"""Only the exact seven-instruction slot validator wrapper has a short body.
All other functions retain the global >=32-byte, non-return-stub requirement.
"""
import struct
PINS=bytes.fromhex('01cacafc48ae96f061caa325a294a91024e626403b97fa556a9bcc42e4de6768c66d1886619a370a91193d3dcd65bb70f7ab89d4c75872a459440bf407e3f1271a5fdbf458ea547c5459b301ab577a83461797b77bde9501fb48fc62d2d5f185')
def slot_wrapper(raw,p):
 name='_g24_slot_view';entries=p['symbols'][name];assert len({(e['address'],e['section']) for e in entries})==1;e=entries[0];start=e['address'];sec=p['sections'][e['section']];stop=min(v for v in p['addresses'][e['section']] if v>start);assert stop-start==28
 off=sec['offset']+start-sec['address'];sample=raw[off:off+28];w=struct.unpack('<7I',sample);assert w[0]==0xaa0003e3 and w[5]==0x52a00021
 def address(adr,add,pc,reg):
  assert adr&0x9f00001f==0x90000000|reg and add&0xffc003ff==0x91000000|(reg<<5)|reg
  imm=((adr>>29)&3)|((adr>>5&0x7ffff)<<2);return (pc&~4095)+(((imm^0x100000)-0x100000)<<12)+(add>>10&4095)
 slot=address(w[1],w[2],start+4,0);pins=address(w[3],w[4],start+12,2)
 assert w[6]&0xfc000000==0x14000000;imm=w[6]&0x3ffffff;target=start+24+4*((imm^0x2000000)-0x2000000);assert target==p['symbols']['_g24_capsule_view'][0]['address']
 at=32;found=[];pin_values=[]
 for _ in range(struct.unpack_from('<I',raw,16)[0]):
  cmd,size=struct.unpack_from('<II',raw,at);assert size>=8 and at+size<=32+struct.unpack_from('<I',raw,20)[0]
  if cmd==0x19:
   va,vs,fo,fs=struct.unpack_from('<4Q',raw,at+24);prot,nsects=struct.unpack_from('<II',raw,at+60)
   if va<=pins and 96<=va+fs-pins and pins+96<=va+vs and not prot&4:pin_values.append(raw[fo+pins-va:fo+pins-va+96])
   for i in range(nsects):
    q=at+72+80*i
    if raw[q:q+16].split(b'\0')[0]!=b'__ziyi_g24':continue
    address_,length,fileoff,align=struct.unpack_from('<QQII',raw,q+32)
    assert raw[q+16:q+32].split(b'\0')[0] in (b'__DATA_CONST',b'__DATA') and not prot&4 and align>=14 and address_%16384==0 and fileoff%16384==0
    assert length==65536 and address_==slot and fileoff-fo==slot-va and va<=slot and length<=va+vs-slot and fo<=fileoff and length<=fo+fs-fileoff and fileoff+length<=len(raw)
    assert struct.unpack_from('<I',raw,q+64)[0]&255==0 and raw[fileoff:fileoff+length]==b'ZIYI_G24_CP_FW01'+bytes(65520);found.append(fileoff)
  at+=size
 assert len(found)==1 and pin_values==[PINS]
 return dict(name=name,address=start,section=e['section'],segment=sec['segment'],bytesToNextTextSymbol=28,sample=sample)
def audited_function(raw,parsed,name,function_bytes):
 return slot_wrapper(raw,parsed) if name=='_g24_slot_view' else function_bytes(raw,parsed,name)
