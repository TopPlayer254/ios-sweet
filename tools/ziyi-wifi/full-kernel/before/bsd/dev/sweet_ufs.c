/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Ziyi U7: image-prefix disk, exact GPT/root hash gates, opt-in FUA writes.
 * No SMMU/PHY reset or persistent logger. No writes before both hash gates.
 */
#if defined(ARM64_BOARD_CONFIG_SWEET)
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <miscfs/devfs/devfs.h>
#include <kern/locks.h>
#include <kern/clock.h>
#include <arm/caches_internal.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <pexpert/pexpert.h>
#include "ziyi_u1_core.h"
#include "ziyi_u3_policy.h"
#include "ziyi_u1_sha.inc"
#include "ziyi_u4_verify.h"
#include "ziyi_u7_write_policy.h"
#include "ziyi_u8_read_policy.h"
#include "ziyi_u9_install_policy.h"
extern vm_offset_t ml_io_map(vm_offset_t,vm_size_t);
extern ppnum_t pmap_find_phys(pmap_t,addr64_t);
extern void sweet_sample_start(void);
void ziyi_ufs_probe(void);
int sweet_ufs_rootdev(int32_t *dev);
static int register_disk(void);
void sweet_log_append(const char *text,int bytes) { (void)text;(void)bytes; }
uint32_t sweet_log_copy(uint64_t *pos,char *out,uint32_t max) { (void)pos;(void)out;(void)max;return 0; }
uint64_t sweet_log_position(void) { return 0; }

/* Addresses and SID from the saved ziyi firmware DT, not a donor profile. */
#define U1_HCI 0x01d84000ull
#define U1_SMMU 0x15000000ull
#define U1_SID 0x20u
static struct {
    vm_offset_t hci,smmu,dma;
    uint64_t pa,blocks,deadline;
    uint32_t bs,smr[128],s2cr[128],cr0,id0;
    unsigned lun,tag,commands,limit;
    struct u1_partition part;
    struct u3_smmu identity;
    uint32_t secsize;
    dev_t disk;
    int ready,major,verified,wake;
    int poison,owned;
    struct u7_volume volume;
    uint8_t write_block[U1_BLOCK_MAX];
    uint8_t headers[2][U1_BLOCK_MAX],tables[2][U1_TABLE_MAX],sample[U1_BLOCK_MAX];
} u1;
static uint32_t hr(unsigned off) { return *(volatile uint32_t *)(u1.hci+off); }
static void hw(unsigned off,uint32_t v) { *(volatile uint32_t *)(u1.hci+off)=v;__builtin_arm_dsb(0xf); }
static uint32_t sr(unsigned off) { return *(volatile uint32_t *)(u1.smmu+off); }
static void sync_dma(void) { flush_dcache64((addr64_t)u1.pa,U1_DMA_BYTES,TRUE);__builtin_arm_dsb(0xf); }
static uint64_t after_ms(unsigned ms) {
    uint64_t t;nanoseconds_to_absolutetime((uint64_t)ms*1000000,&t);return mach_absolute_time()+t;
}
static int wait_reg(unsigned off,uint32_t mask,uint32_t want,unsigned ms) {
    uint64_t end=after_ms(ms);
    for(unsigned i=0;i<ms*1000+1;i++) {
        if((hr(off)&mask)==want) return 1;
        if(mach_absolute_time()>=end) break;
        delay(1);
    }
    return 0;
}
static void digest(const char *what,const void *p,size_t n) {
    uint8_t h[32];char hex[65];static const char digits[]="0123456789abcdef";
    u1_sha256(p,n,h);
    for(unsigned i=0;i<32;i++) { hex[2*i]=digits[h[i]>>4];hex[2*i+1]=digits[h[i]&15]; }
    hex[64]=0;printf("ZIYI U1 SHA %s bytes=%lu %s\n",what,(unsigned long)n,hex);
}
static int bypass(void) {
    struct u3_smmu a,b;
    struct u3_smmu *s=&a;
    for(unsigned pass=0;pass<2;pass++,s=&b) {
        bzero(s,sizeof(*s));s->cr0=sr(0);s->cr2=sr(8);s->id0=sr(0x20);
        s->id1=sr(0x24);s->id2=sr(0x28);s->id7=sr(0x3c);
        for(unsigned i=0;i<U3_MATCHES;i++) {s->smr[i]=sr(0x800+4*i);s->s2cr[i]=sr(0xc00+4*i);}
        s->cbar=sr(0x1000);s->cba2r=sr(0x1800);s->sctlr=sr(0x80000);s->tcr2=sr(0x80010);
        s->ttbr0lo=sr(0x80020);s->ttbr0hi=sr(0x80024);s->ttbr1lo=sr(0x80028);
        s->ttbr1hi=sr(0x8002c);s->tcr=sr(0x80030);__builtin_arm_dsb(0xf);
    }
    u1.cr0=a.cr0;u1.id0=a.id0;
    if(!u1_equal(&a,&b,sizeof(a))||!u3_identity(&a)) return 0;
    if(u1.owned&&!u1_equal(&a,&u1.identity,sizeof(a))) return 0;
    u1.identity=a;return 1;
}
static int old_queue_readonly(void) {
    uint64_t pa=hr(0x50)|(uint64_t)hr(0x54)<<32;
    unsigned slots=(hr(0)&31u)+1u,used=0;
    if((pa&1023u)||!u3_firmware_span(pa,slots*32u)||hr(0x58)||hr(0x78)) return 0;
    vm_offset_t ring=ml_io_map(pa,slots*32u);if(!ring) return 0;
    /* Exact completed powerdown + poison padding profile observed in U5.
     * Never read CAP-sized RAM as descriptors when it is actually UCD data. */
    uint8_t prefix[256],*saved=u1.sample;
    for(unsigned j=0;j<256;j++) prefix[j]=*(volatile uint8_t *)(ring+j);
    uint64_t first=u1_le64(prefix+16);
    if(!(first&127u)&&u3_firmware_span(first,4096)) {
        vm_offset_t va=ml_io_map(first,4096);
        if(!va)return 0;
        for(unsigned j=0;j<4096;j++)saved[j]=*(volatile uint8_t *)(va+j);
        if(u6_firmware_profile(prefix,saved,pa)) {
            for(unsigned j=0;j<256;j++)if(prefix[j]!=*(volatile uint8_t *)(ring+j))return 0;
            for(unsigned j=0;j<4096;j++)if(saved[j]!=*(volatile uint8_t *)(va+j))return 0;
            if(hr(0x58)||hr(0x78)||pa!=(hr(0x50)|(uint64_t)hr(0x54)<<32))return 0;
            u1.wake=1;printf("ZIYI U6 completed firmware powerdown profile PASS; no replay\n");return 1;
        }
    }
    for(unsigned i=0;i<slots;i++) {
        uint8_t d[32];
        for(unsigned j=0;j<32;j++) d[j]=*(volatile uint8_t *)(ring+i*32+j);
        for(unsigned j=0;j<32;j++) if(d[j]!=*(volatile uint8_t *)(ring+i*32+j)) return 0;
        if(u4_inert_slot(d)) continue;
        uint64_t ucd;unsigned poff,n;
        if(!u3_old_utrd(d,&ucd,&poff,&n)) {
            printf("ZIYI U4 TRD %x %x %x %x %x %x %x %x\n",u1_le32(d),u1_le32(d+4),u1_le32(d+8),u1_le32(d+12),u1_le32(d+16),u1_le32(d+20),u1_le32(d+24),u1_le32(d+28));
            printf("ZIYI U3 STOP inherited slot=%u hdr=%x ocs=%x ptr=%llx\n",i,u1_le32(d),u1_le32(d+8),u1_le64(d+16));return 0;
        }
        vm_offset_t va=ml_io_map(ucd,4096);if(!va) return 0;
        uint8_t u[4096];
        for(unsigned j=0;j<4096;j++) u[j]=*(volatile uint8_t *)(va+j);
        if(!u3_old_ucd(u,poff,n)) {
            printf("ZIYI U4 UPIU %x %x %x %x %x %x %x %x\n",u1_le32(u),u1_le32(u+4),u1_le32(u+8),u1_le32(u+12),u1_le32(u+16),u1_le32(u+20),u1_le32(u+24),u1_le32(u+28));printf("ZIYI U3 STOP inherited UCD slot=%u op=%x flags=%x\n",i,u[16],u[1]);return 0;}
        for(unsigned j=0;j<4096;j++) if(u[j]!=*(volatile uint8_t *)(va+j)) return 0;
        for(unsigned j=0;j<32;j++) if(d[j]!=*(volatile uint8_t *)(ring+i*32+j)) return 0;
        used++;
    }
    printf("ZIYI U3 inherited queue readonly PASS slots=%u used=%u\n",slots,used);
    return used&&pa==(hr(0x50)|(uint64_t)hr(0x54)<<32)&&!hr(0x58)&&!hr(0x78);
}
/* A timeout permanently poisons this probe. Its allocation is NEVER freed,
 * including if abort/queue stop cannot be confirmed; late DMA has owned RAM. */
static void stop_queue(void) {
    if(!u1.owned) return;
    if(hr(0x58)&1) hw(0x5c,~1u);
    hw(0x60,0);
    int stopped=wait_reg(0x60,1,0,10),empty=wait_reg(0x58,~0u,0,10);
    printf("ZIYI U1 queue stopped=%d empty=%d retained DMA=%llx\n",stopped,empty,u1.pa);
}
static int transfer(unsigned op,uint64_t lba,uint32_t count,const uint8_t *data) {
    struct u1_cmd c;
    if(u1.poison||++u1.commands>u1.limit||mach_absolute_time()>=u1.deadline||
       (op==0x2a ? (!data||!u7_write_command(&c,&u1.volume,lba,count)) :
        (data||!u8_command(&c,op,u1.lun,lba,count,u1.bs,u1.blocks)))) return 0;
    if(!bypass()||hr(0x58)||hr(0x78)||!(hr(0x60)&1)||
       hr(0x50)!=(uint32_t)u1.pa||hr(0x54)!=(uint32_t)(u1.pa>>32)) {
        u1.poison=1;printf("ZIYI U1 STOP ownership/identity changed\n");return 0;
    }
    bzero((void *)u1.dma,U1_DMA_BYTES);
    uint32_t *utrd=(uint32_t *)u1.dma;
    uint8_t *ucd=(uint8_t *)(u1.dma+0x1000),*resp=ucd+512;
    uint32_t *prdt=(uint32_t *)(ucd+1024);
    unsigned tag=(++u1.tag)&255;
    ucd[0]=1;ucd[1]=c.bytes?(op==0x2a?0x20:0x40):0;ucd[2]=c.lun;ucd[3]=(uint8_t)tag;
    u1_putbe32(ucd+12,c.bytes);bcopy(c.cdb,ucd+16,16);
    utrd[0]=(1u<<28)|(1u<<24)|(c.bytes?((op==0x2a?1u:2u)<<25):0);
    utrd[2]=15;utrd[4]=(uint32_t)(u1.pa+0x1000);utrd[5]=(uint32_t)((u1.pa+0x1000)>>32);
    utrd[6]=(128u<<16)|128u;
    if(c.bytes) {
        utrd[7]=(256u<<16)|1u;prdt[0]=(uint32_t)(u1.pa+U1_DATA_OFF);
        prdt[1]=(uint32_t)((u1.pa+U1_DATA_OFF)>>32);prdt[3]=c.bytes-1;
    }
    if(op==0x2a)bcopy(data,(void *)(u1.dma+U1_DATA_OFF),c.bytes);
    sync_dma();hw(0x58,1);
    if(!wait_reg(0x58,1,0,op==0x1b?5000:1000)) {
        u1.poison=1;printf("ZIYI U1 TIMEOUT op=%x lun=%u\n",op,u1.lun);stop_queue();return 0;
    }
    sync_dma();
    if((utrd[2]&255)||resp[0]!=0x21||resp[2]!=c.lun||resp[3]!=tag||resp[6]||
       resp[7]||(resp[1]&0x60)||u1_be32(resp+12)) {
        printf("ZIYI U1 SCSI op=%x lun=%u ocs=%x rsp=%x status=%x\n",op,u1.lun,utrd[2]&255,resp[0],resp[7]);
        /* SCSI check condition can be retried (TUR), malformed completions cannot. */
        if((utrd[2]&255)||resp[0]!=0x21||resp[2]!=c.lun||resp[3]!=tag) u1.poison=1;
        return 0;
    }
    return 1;
}
static int command(unsigned op,uint64_t lba,uint32_t count) {
    return transfer(op,lba,count,NULL);
}
static int write_verified(uint64_t lba,const uint8_t *data) {
    /* data must survive transfer() clearing its DMA arena. Caller uses write_block. */
    if(data!=u1.write_block||!transfer(0x2a,lba,1,data)||!command(0x28,lba,1)||
       !u1_equal(data,(void *)(u1.dma+U1_DATA_OFF),u1.bs)) {
        u1.poison=1;u1.volume.writable=0;stop_queue();
        printf("ZIYI U7 STOP write/readback failure; retained DMA\n");return 0;
    }
    return 1;
}
static int read_twice(uint64_t lba,uint8_t *out) {
    if(!command(0x28,lba,1)) return 0;
    bcopy((void *)(u1.dma+U1_DATA_OFF),out,u1.bs);
    if(!command(0x28,lba,1)) return 0;
    if(!u1_equal(out,(void *)(u1.dma+U1_DATA_OFF),u1.bs)) {
        u1.poison=1;printf("ZIYI U1 STOP repeated read mismatch\n");return 0;
    }
    return 1;
}
static int table_read(const struct u1_gpt *g,uint8_t *out) {
    uint32_t bytes=g->count*128;
    for(uint32_t off=0;off<bytes;off+=u1.bs) {
        if(!read_twice(g->entries_lba+off/u1.bs,u1.sample)) return 0;
        bcopy(u1.sample,out+off,MIN(u1.bs,bytes-off));
    }
    return 1;
}
static int probe_lun(void) {
    int ready=0;
    for(unsigned i=0;i<3&&!u1.poison;i++) if(command(0,0,0)) { ready=1;break; }
    if(!ready||!command(0x25,0,0)) return 0;
    uint8_t *p=(uint8_t *)(u1.dma+U1_DATA_OFF);size_t n=8;
    if(u1_be32(p)==UINT32_MAX) { if(!command(0x9e,0,0)) return 0;n=32; }
    if(!u1_capacity(p,n,&u1.blocks,&u1.bs)) return 0;
    printf("ZIYI U1 capacity lun=%u blocks=%llu block=%u\n",u1.lun,u1.blocks,u1.bs);
    struct u1_gpt g[2];struct u1_partition part;
    if(!read_twice(1,u1.headers[0])||!read_twice(u1.blocks-1,u1.headers[1])||
       !u1_header(u1.headers[0],u1.bs,1,u1.blocks,&g[0])||
       !u1_header(u1.headers[1],u1.bs,u1.blocks-1,u1.blocks,&g[1])) return 0;
    if(!table_read(&g[0],u1.tables[0])||!table_read(&g[1],u1.tables[1])||
       !u1_tables(&g[0],&g[1],u1.tables[0],u1.tables[1],&part)) return 0;
    printf("ZIYI U1 GPT BOTH CRC PASS lun=%u userdata first=%llu last=%llu bytes=%llu\n",
           u1.lun,part.first,part.last,(part.last-part.first+1)*u1.bs);
    digest("primary-header",u1.headers[0],u1.bs);digest("backup-header",u1.headers[1],u1.bs);
    digest("GPT-entries",u1.tables[0],g[0].count*128);
    if(!read_twice(part.first,u1.sample)) return 0;
    digest("userdata-first-block",u1.sample,u1.bs);
    if(!read_twice(part.last,u1.sample)) return 0;
    digest("userdata-last-block",u1.sample,u1.bs);
    u1.part=part;
    uint32_t mib=0;char text[65]={0};uint8_t wanted[32],actual[32];
    if(PE_parse_boot_argn("ziyi_ufs_verify_mib",&mib,sizeof(mib))&&
       (mib==3072||mib==8192)) {
        u1.volume=(struct u7_volume){part.first,part.last,(uint64_t)mib<<20,u1.bs,u1.lun,0,0,0};
        if(!u7_volume_valid(&u1.volume))return 0;
        /* Header blocks plus exact primary entries bind capacity, both GPT
         * headers, disk/partition GUID, geometry and complete partition table. */
        struct u4_hash state;u4_hash_init(&state);
        int ok=u4_hash_blocks(&state,u1.headers[0],u1.bs)&&
               u4_hash_blocks(&state,u1.headers[1],u1.bs);
        uint32_t n=g[0].count*128;
        for(uint32_t off=0;ok&&off<n;off+=4096)
            ok=u4_hash_blocks(&state,u1.tables[0]+off,MIN(4096,n-off));
        if(!ok||!u4_hash_final(&state,(uint64_t)u1.bs*2+n,actual))return 0;
        if(PE_parse_boot_argn("ziyi_ufs_gpt_hash",text,sizeof(text))&&
           u4_expected_hash(text,wanted)&&u1_equal(actual,wanted,32))u1.volume.gpt_verified=1;
        printf("ZIYI U7 GPT identity match=%u prefixMiB=%u\n",u1.volume.gpt_verified,mib);
        u1.part.last=part.first+u1.volume.image_bytes/u1.bs-1;
    }
    return 1;
}
void ziyi_ufs_probe(void) {
    static int once;uint32_t enabled=0,root=0;
    PE_parse_boot_argn("ziyi_ufs_probe",&enabled,sizeof(enabled));
    PE_parse_boot_argn("ziyi_ufs_root",&root,sizeof(root));
    if((enabled!=1&&root!=1)||once++) return;
    printf("ZIYI U3 BEGIN: native readonly disk, root opt-in\n");
    u1.hci=ml_io_map(U1_HCI,0x4000);
    if(!u1.hci) return;
    uint32_t cap=hr(0),hcs=hr(0x30),hce=hr(0x34),ver=hr(8);
    printf("ZIYI U1 HCI ver=%x cap=%x hce=%x hcs=%x dbr=%x tm=%x\n",ver,cap,hce,hcs,hr(0x58),hr(0x78));
    if(!(hce&1)||(hcs&3)!=3||hr(0x58)||hr(0x78)||!ver||ver==~0u) {
        printf("ZIYI U1 STOP host not ready/idle\n");return;
    }
    u1.smmu=ml_io_map(U1_SMMU,0x100000);
    if(!u1.smmu) return;
    int identity=bypass();
    printf("ZIYI U1 SMMU cr0=%x id0=%x sid=20 context-identity=%d\n",u1.cr0,u1.id0,identity);
    printf("ZIYI U3 live cr2=%x id1=%x id2=%x cb=%x/%x sctlr=%x\n",sr(8),sr(0x24),sr(0x28),sr(0x1000),sr(0x1800),sr(0x80000));
    if(!identity) { printf("ZIYI U1 STOP DMA mapping unproved; no queue writes\n");return; }
    if(!old_queue_readonly()) { printf("ZIYI U3 STOP old slot not proven readonly\n");return; }
    if(kmem_alloc_contig(kernel_map,&u1.dma,U1_DMA_BYTES,PAGE_MASK,0,0,
                        KMA_DATA|KMA_ZERO|KMA_PERMANENT,VM_KERN_MEMORY_IOKIT)!=KERN_SUCCESS) return;
    u1.pa=ptoa_64(pmap_find_phys(kernel_pmap,(addr64_t)u1.dma));
    if(!u1.pa||u1.pa%PAGE_SIZE||u1.pa>UINT64_MAX-U1_DMA_BYTES) return;
    for(unsigned off=0;off<U1_DMA_BYTES;off+=PAGE_SIZE)
        if(ptoa_64(pmap_find_phys(kernel_pmap,(addr64_t)u1.dma+off))!=u1.pa+off) return;
    if(!u3_dma_range(&u1.identity,u1.pa,U1_DMA_BYTES,cap)) return;
    if(!bypass()||hr(0x58)||hr(0x78)) return;
    hw(0x60,0);
    if(!wait_reg(0x60,1,0,10)||hr(0x58)||hr(0x78)||!bypass()) return;
    hw(0x24,0);sync_dma();
    hw(0x50,(uint32_t)u1.pa);hw(0x54,(uint32_t)(u1.pa>>32));
    if(hr(0x50)!=(uint32_t)u1.pa||hr(0x54)!=(uint32_t)(u1.pa>>32)) return;
    u1.owned=1;hw(0x60,1);u1.deadline=after_ms(30000);u1.limit=256;
    if(u1.wake) {
        u1.lun=0xd0;
        /* Clear reset unit attention without executing any data-LUN command. */
        if(!command(0,0,0)&&!u1.poison)command(3,0,0);
        int awake=0;
        for(unsigned attempt=0;attempt<3&&!u1.poison;attempt++) {
            if(command(0x1b,0,0)){awake=1;break;}
            if(!u1.poison)command(3,0,0);
        }
        printf("ZIYI U6 ACTIVE W-LUN success=%d poisoned=%d\n",awake,u1.poison);
        if(!awake){stop_queue();u1.poison=1;return;}
    }
    int success=0;
    for(u1.lun=0;u1.lun<8&&!u1.poison;u1.lun++) {
        u1.blocks=0;u1.bs=0;
        if(probe_lun()) { success=1;break; }
    }
    if(success&&!u1.poison&&register_disk()==0) u1.ready=1;
    if(!u1.ready) stop_queue();
    printf("ZIYI U1 END success=%d poisoned=%d commands=%u; no media writes\n",success,u1.poison,u1.commands);
}

static LCK_GRP_DECLARE(u3_group,"ziyi_ufs_readonly");
static LCK_MTX_DECLARE(u3_lock,&u3_group);
static int disk_open(__unused dev_t dev,int flags,__unused int type,__unused struct proc *p) {
    return !u1.ready||u1.poison?ENXIO:((flags&FWRITE)&&!u1.volume.writable?EROFS:0);
}
static int disk_close(__unused dev_t dev,__unused int flags,__unused int type,__unused struct proc *p) {return 0;}
static void disk_strategy(buf_t bp) {
    int error=0;caddr_t va=NULL;uint32_t count=buf_count(bp);uint64_t off=0,bytes=0;
    buf_setresid(bp,count);
    lck_mtx_lock(&u3_lock);
    if(!u1.ready||u1.poison) {error=ENXIO;goto unlock;}
    int writing=!(buf_flags(bp)&B_READ);
    if(writing&&!u1.volume.writable){error=EROFS;goto unlock;}
    if(!u3_partition_io(u1.part.first,u1.part.last,u1.bs,u1.secsize,
                        buf_blkno(bp),count,&off,&bytes)) {error=EINVAL;goto unlock;}
    if(!count) goto unlock;
    if(buf_map(bp,&va)) {error=EIO;goto unlock;}
    u1.commands=0;u1.limit=3*(count/u1.bs+2);u1.deadline=after_ms(30000);
    for(uint32_t copied=0;copied<count;) {
        uint64_t pos=off+copied;
        uint32_t skip=(uint32_t)(pos%u1.bs),take=MIN(count-copied,u1.bs-skip);
        uint64_t lba=u1.part.first+pos/u1.bs;
        if(writing) {
            if(skip||take<u1.bs) {
                if(!command(0x28,lba,1)){error=EIO;break;}
                bcopy((void *)(u1.dma+U1_DATA_OFF),u1.write_block,u1.bs);
            }
            bcopy(va+copied,u1.write_block+skip,take);
            if(!write_verified(lba,u1.write_block)){error=EIO;break;}
        } else {
            uint32_t blocks=1;
            if(!skip&&count-copied>=u1.bs){blocks=MIN((count-copied)/u1.bs,U8_READ_MAX/u1.bs);take=blocks*u1.bs;}
            if(!command(0x28,lba,blocks)){error=EIO;break;}
            bcopy((void *)(u1.dma+U1_DATA_OFF+skip),va+copied,take);
        }
        copied+=take;buf_setresid(bp,count-copied);
    }
    buf_unmap(bp);
unlock:
    lck_mtx_unlock(&u3_lock);
    if(error) buf_seterror(bp,error);
    buf_biodone(bp);
}
static int disk_ioctl(__unused dev_t dev,u_long cmd,caddr_t data,__unused int flags,
                      __unused struct proc *p,int is_char) {
    int error=0;uint32_t *v=(uint32_t *)data;uint64_t *w=(uint64_t *)data;
    lck_mtx_lock(&u3_lock);
    if(!u1.ready||u1.poison) {error=ENXIO;goto out;}
    switch(cmd) {
        case DKIOCGETBLOCKSIZE:*v=u1.secsize;break;
        case DKIOCSETBLOCKSIZE:
            if(is_char) error=ENODEV;
            else if(*v<512||*v>u1.bs||(*v&(*v-1))) error=EINVAL;
            else u1.secsize=*v;
            break;
        case DKIOCGETPHYSICALBLOCKSIZE:*v=u1.bs;break;
        case DKIOCGETBLOCKCOUNT:*w=(u1.part.last-u1.part.first+1)*u1.bs/u1.secsize;break;
        case DKIOCISWRITABLE:*v=u1.volume.writable;break;
        case DKIOCGETMAXBLOCKCOUNTREAD:*w=U3_REQUEST_MAX/u1.secsize;break;
        case DKIOCGETMAXBLOCKCOUNTWRITE:*w=u1.volume.writable?U3_REQUEST_MAX/u1.secsize:0;break;
        case DKIOCGETMAXSEGMENTCOUNTREAD:*w=1;break;
        case DKIOCGETMAXSEGMENTCOUNTWRITE:*w=u1.volume.writable?1:0;break;
        case DKIOCGETFEATURES:*v=DK_FEATURE_FORCE_UNIT_ACCESS;break;
        case DKIOCSYNCHRONIZE: {
            const dk_synchronize_t *s=(const dk_synchronize_t *)data;
            if(!s||!u9_sync_valid(u1.volume.image_bytes,s->offset,s->length,s->options,s->reserved0160))error=EINVAL;
            else if(!u1.volume.writable)error=EROFS;
            /* Same synchronous lock and FUA/readback invariant as legacy sync. */
            break;
        }
        case DKIOCSYNCHRONIZECACHE:
            /* Every completed media write used FUA and an independent READ.
             * The same mutex serializes this with every strategy operation. */
            if(!u1.volume.writable)error=EROFS;
            break;
        default:error=ENOTTY;break;
    }
out:lck_mtx_unlock(&u3_lock);return error;
}
static int disk_bioctl(dev_t d,u_long c,caddr_t a,int f,struct proc *p) {return disk_ioctl(d,c,a,f,p,0);}
static int disk_cioctl(dev_t d,u_long c,caddr_t a,int f,struct proc *p) {return disk_ioctl(d,c,a,f,p,1);}
static int disk_psize(__unused dev_t d) {
    return u1.ready?(int)MIN((u1.part.last-u1.part.first+1)*u1.bs/u1.secsize,INT_MAX):-1;
}
static int disk_read(dev_t d,struct uio *u,__unused int f) {
    return !u1.ready?ENXIO:physio(disk_strategy,NULL,d,B_READ,minphys,u,(int)u1.secsize);
}
static int disk_write(dev_t d,struct uio *u,__unused int f) {
    return !u1.ready||u1.poison?ENXIO:!u1.volume.writable?EROFS:
        physio(disk_strategy,NULL,d,B_WRITE,minphys,u,(int)u1.secsize);
}
static const struct bdevsw disk_bdevsw={
    .d_open=disk_open,.d_close=disk_close,.d_strategy=disk_strategy,.d_ioctl=disk_bioctl,
    .d_dump=eno_dump,.d_psize=disk_psize,.d_type=D_DISK
};
static const struct cdevsw disk_cdevsw={
    .d_open=disk_open,.d_close=disk_close,.d_read=disk_read,.d_write=disk_write,.d_ioctl=disk_cioctl,
    .d_stop=eno_stop,.d_reset=eno_reset,.d_ttys=NULL,.d_select=eno_select,.d_mmap=eno_mmap,
    .d_strategy=eno_strat,.d_reserved_1=eno_getc,.d_reserved_2=eno_putc,.d_type=D_DISK
};
static int register_disk(void) {
    u1.secsize=DEV_BSIZE;
    int b=bdevsw_add(-1,&disk_bdevsw);if(b<0) return ENXIO;
    int c=cdevsw_add_with_bdev(-1,&disk_cdevsw,b);if(c<0) return ENXIO;
    u1.major=b;u1.disk=makedev(b,0);
    if(!devfs_make_node(u1.disk,DEVFS_BLOCK,UID_ROOT,GID_OPERATOR,0640,"ufsroot0")||
       !devfs_make_node(makedev(c,0),DEVFS_CHAR,UID_ROOT,GID_OPERATOR,0640,"rufsroot0")) return ENXIO;
    printf("ZIYI U3 readonly BSD disk ready major=%d lun=%u bytes=%llu\n",b,u1.lun,(u1.part.last-u1.part.first+1)*u1.bs);
    return 0;
}
/* Root entry is conditional on a bounded complete readback of the prepared
 * headless image. Expected SHA comes from private audited boot args, not donor
 * addresses. A digest mismatch cannot enter Apple userspace. */
static int verify_installed_root(void) {
    char hex[65]={0};uint8_t expected[32],actual[32],ids[2][28];
    uint64_t bytes=u1.volume.image_bytes;
    if(!u1.ready||u1.poison||!u1.volume.gpt_verified||bytes!=(8ull<<30)||
       !PE_parse_boot_argn("ziyi_ufs_volume_hash",hex,sizeof(hex))||!u4_expected_hash(hex,expected))return 0;
    for(unsigned i=0;i<2;i++) {
        uint64_t off=i?bytes-1024:1024;
        u1.commands=0;u1.limit=8;u1.deadline=after_ms(30000);
        if(!read_twice(u1.part.first+off/u1.bs,u1.sample)||
           !u9_hfs_identity(u1.sample+off%u1.bs,u1.bs-off%u1.bs,bytes,ids[i]))return 0;
    }
    if(!u1_equal(ids[0],ids[1],28))return 0;
    u1_sha256(ids[0],28,actual);
    int ok=u1_equal(actual,expected,32);
    printf("ZIYI U9 installed HFS identity primary+alternate match=%d; no whole-volume scan\n",ok);
    return ok;
}
static int verify_apple_root(void) {
    char mode[16]={0};
    if(PE_parse_boot_argn("ziyi_ufs_mode",mode,sizeof(mode))) {
        if(!strcmp(mode,"installed"))return verify_installed_root();
        if(strcmp(mode,"full"))return 0;
    }

    uint32_t mib=0;char hex[65]={0};uint8_t expected[32],actual[32];
    if(!PE_parse_boot_argn("ziyi_ufs_verify_mib",&mib,sizeof(mib))||(mib!=3072&&mib!=8192)||
       !PE_parse_boot_argn("ziyi_ufs_hash",hex,sizeof(hex))||!u4_expected_hash(hex,expected)) return 0;
    const uint64_t bytes=(uint64_t)mib<<20;
    if(!u1.ready||u1.poison||!u1.volume.gpt_verified||u1.volume.image_bytes!=bytes||
       (u1.part.last-u1.part.first+1)*u1.bs!=bytes) return 0;
    struct u4_hash state;u4_hash_init(&state);
    uint64_t global=after_ms(1800000);
    printf("ZIYI U4 full root readback BEGIN bytes=%llu readonly\n",bytes);
    printf("ZIYI U8 readback batching max=%u existing owned DMA=%u\n",U8_READ_MAX,U1_DMA_BYTES);
    for(uint64_t off=0;off<bytes;) {
        if(mach_absolute_time()>=global)return 0;
        if(off%((uint64_t)U8_READ_MAX*128)==0){
            u1.commands=0;u1.limit=128;u1.deadline=MIN(global,after_ms(30000));
        }
        uint32_t n=(uint32_t)MIN(bytes-off,U8_READ_MAX);
        if(n%u1.bs||!command(0x28,u1.part.first+off/u1.bs,n/u1.bs))return 0;
        for(uint32_t local=0;local<n;local+=u1.bs)
            if(!u4_hash_blocks(&state,(const uint8_t *)(u1.dma+U1_DATA_OFF+local),u1.bs))return 0;
        off+=n;
        if(off%0x4000000==0)printf("ZIYI U8 root readback MiB=%llu/%u\n",off>>20,mib);
    }
    if(!u4_hash_final(&state,bytes,actual)) return 0;
    char value[65];static const char digits[]="0123456789abcdef";
    for(unsigned i=0;i<32;i++) {value[2*i]=digits[actual[i]>>4];value[2*i+1]=digits[actual[i]&15];}
    value[64]=0;printf("ZIYI U4 full root SHA256 %s match=%d\n",value,u1_equal(actual,expected,32));
    return u1_equal(actual,expected,32);
}
int sweet_ufs_rootdev(int32_t *dev) {
    uint32_t enabled=0;
    if(!dev||!PE_parse_boot_argn("ziyi_ufs_root",&enabled,sizeof(enabled))||enabled!=1) return ENXIO;
    ziyi_ufs_probe();
    if(!u1.ready||u1.poison) return ENXIO;
    if(!u1.verified) {
        u1.verified=verify_apple_root();
        if(!u1.verified) {printf("ZIYI U4 STOP root readback failed; no Apple entry\n");stop_queue();u1.poison=1;return EIO;}
        /* The early storage-free profile removed the donor UFS caller.
         * Restore only the RAM-only sampler, once, after verified root.
         * Persistent logger, PHY and USB remain excluded. ziyi input is opt-in. */
        extern void sweet_keys_init(void),sweet_touch_init(void);
        sweet_keys_init();sweet_touch_init();
        char sample_names[128]={0};
        if(PE_parse_boot_argn("sweetsample",sample_names,sizeof(sample_names))&&sample_names[0]&&sample_names[0]!='-') {
            printf("ZIYI U10 requesting original RAM-only process sampler after verified root\n");
            sweet_sample_start();
        }
    }
    u1.volume.root_verified=1;
    uint32_t rw=0;
    if(PE_parse_boot_argn("ziyi_ufs_rw",&rw,sizeof(rw))&&rw==1&&u1.volume.gpt_verified==1)
        u1.volume.writable=1;
    printf("ZIYI U7 ROOT READY bytes=%llu writable=%u; configured root identity/GPT verified\n",
           u1.volume.image_bytes,u1.volume.writable);
    *dev=u1.disk;return 0;
}
#endif
