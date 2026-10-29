/* Xiaomi 24856283 ICNSS2 QMI HOST_CAP GPIO_INFO, not APSS GPIO MMIO.
 * cnss_utils/wlan_firmware_service_v01.c: TLV0x2d, u8 count + u32 array.
 * icnss2/qmi.c: indices WLAN_EN/BT_EN/HOST_SOL/TARGET_SOL; absent=0xffff.
 * The caller binds the discovered WLFW service and native ziyi boot epoch.
 * Validate the whole original packet before writing any byte. */
static __attribute__((noinline)) int
ziyi_wlfw_host_cap(uint8_t *packet, size_t length, size_t capacity,
    unsigned wlan_en, int bound)
{
 if(!bound || wlan_en!=25u)return -EPERM;
 if(!packet || length<7 || length>capacity || length>65542u)return -EINVAL;
 if(packet[0]!=0 || packet[3]!=0x34 || packet[4]!=0 ||
    (size_t)(packet[5]|packet[6]<<8)!=length-7)return -EINVAL;
 for(size_t at=7;at<length;){
  if(length-at<3)return -EINVAL;
  size_t bytes=(size_t)(packet[at+1]|packet[at+2]<<8);
  if(bytes>length-at-3 || packet[at]==0x2d)return -EINVAL;
  at+=3+bytes;
 }
 if(capacity-length<20 || length-7>65535u-20u)return -ENOSPC;
 uint8_t *p=packet+length;
 p[0]=0x2d;p[1]=17;p[2]=0;p[3]=4;
 const uint32_t gpio[4]={25u,0xffffu,0xffffu,0xffffu};
 for(unsigned i=0;i<4;i++)for(unsigned b=0;b<4;b++)
  p[4+4*i+b]=(uint8_t)(gpio[i]>>(8*b));
 size_t payload=length-7+20;
 packet[5]=(uint8_t)payload;packet[6]=(uint8_t)(payload>>8);
 return (int)(length+20);
}
