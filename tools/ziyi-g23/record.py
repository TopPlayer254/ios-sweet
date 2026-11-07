"""Independent GST1 little-endian decoder. Actual observations only, no repair."""
import struct,zlib
def decode(data):
 if len(data)!=768:raise ValueError('GST1 size')
 w=struct.unpack('<192I',data)
 if w[:3]!=(0x31545347,1,768) or w[3]!=zlib.crc32(data[:12]+bytes(4)+data[16:]):raise ValueError('GST1 identity/CRC')
 state,error,current,calls,acks=w[4:9];key,seen=w[19:21];qvalues=list(w[21:24]+w[186:189]);bell=w[179]
 if state>4 or current>5 or calls>5 or acks>calls or bell&~31 or w[11]&~127 or w[14]>1 or w[15]>1 or key>3 or seen&~63 or any(w[189:]):raise ValueError('GST1 schema')
 if any(v for i,v in enumerate(qvalues) if not seen&(1<<i)):raise ValueError('unobserved queue value')
 wide=lambda i:w[i]|w[i+1]<<32
 if wide(184) not in [0,19200000] or wide(182)<wide(180):raise ValueError('GST1 clock')
 headers=[0x203b04,0x30a003,0x40020e,0x50010a,0x60041e];lengths=[59,160,2,1,4];steps=[]
 for i in range(5):
  h,crc,status,count,declared,complete=w[24+i],w[29+i],w[34+i],w[39+i],w[174+i],w[164+i]
  raw=list(w[44+19*i:44+19*i+19]);qw,qr,aw,polls,stores=w[139+i],w[144+i],w[149+i],w[154+i],w[159+i]
  if count>19 or declared>255 or complete>1 or polls>50000 or stores>162 or max(qw,qr,aw)>=1024 or any(raw[count:]):raise ValueError('GST1 step bounds')
  if h and h!=headers[i] or count and declared!=(raw[0]>>8&255):raise ValueError('GST1 packet identity')
  if complete and (count<3 or count!=declared or not bell&(1<<i) or status or raw[1]!=h):raise ValueError('GST1 ACK')
  aligned=(lengths[i]+3)&~3
  steps.append(dict(index=i,header=h,packetCRC=f'{crc:08x}',doorbellReturned=bool(bell&(1<<i)),rawDeclared=declared,raw=raw[:count],status=status if count>=3 else None,polls=polls,successfulMemoryStores=stores,queue0PlannedWrite=qw,queue0Published=stores>=aligned+1,queue1PlannedRead=qr,queue1Advanced=stores>=aligned+2,queue1ObservedWrite=aw,complete=bool(complete)))
 if state==3 and (error or calls!=5 or acks!=5 or current!=5 or bell!=31 or w[11]!=127 or not wide(17) or w[14:16]!=(1,1) or not all(s['complete'] for s in steps)):raise ValueError('GST1 success')
 return dict(schema=1,crc32=f'{w[3]:08x}',state=state,error=error,current=current,doorbellCalls=calls,completedAcks=acks,resourceCRC=f'{w[9]:08x}',headerCRC=f'{w[10]:08x}',found=w[11],entries=w[12],peakKbps=w[13],headerEqual=bool(w[14]),resourcesEqual=bool(w[15]),databaseWordReads=w[16],epoch=wide(17),queue=dict(kind=['table-for-q0','q0','table-for-q1','q1'][key],validMask=seen,values=[v if seen&(1<<i) else None for i,v in enumerate(qvalues)]),steps=steps,lastHealth=list(w[169:174]),startTick=wide(180),lastObservedTick=wide(182),frequency=wide(184),nativePixels=False)
