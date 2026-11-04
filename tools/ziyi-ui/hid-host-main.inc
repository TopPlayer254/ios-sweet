static void reset(){CHECK(!live&&!locked);allocs=failAt=appendCount=failAppend=dispatched=0;last.clear();}
int main(){UInt32 xs[10],ys[10];for(unsigned i=0;i<10;i++){xs[i]=100+i*70;ys[i]=1000+i*100;}
 for(unsigned count=1;count<=10;count++){
  reset();SweetHIDTouch s;UInt32 mask=(1u<<count)-1;
  CHECK(s.touchFrame(xs,ys,mask));CHECK(dispatched==1&&last.size()==count);
  for(unsigned i=0;i<count;i++){CHECK(last[i].f[kIOHIDEventFieldDigitizerIndex]==i);CHECK(last[i].f[kIOHIDEventFieldDigitizerIdentity]==i+2);CHECK(last[i].f[kIOHIDEventFieldDigitizerTouch]==1);CHECK(last[i].stamp==1234567);}
  CHECK(s.touchFrame(xs,ys,mask)&&dispatched==1);
  xs[count-1]++;CHECK(s.touchFrame(xs,ys,mask)&&dispatched==2&&last.size()==count);xs[count-1]--;
  CHECK(s.touchFrame(xs,ys,0)&&dispatched==3&&last.size()==count);
  for(unsigned i=0;i<count;i++)CHECK(!last[i].f[kIOHIDEventFieldDigitizerTouch]&&(last[i].f[kIOHIDEventFieldDigitizerEventMask]&3)==3&&!s.fDown[i]);
  CHECK(s.touchFrame(xs,ys,0)&&dispatched==3&&!live);
 }
 for(unsigned fault=1;fault<=11;fault++){
  reset();SweetHIDTouch s;failAt=fault;CHECK(!s.touchFrame(xs,ys,1023));CHECK(!dispatched&&!live&&!locked);for(bool down:s.fDown)CHECK(!down);
  failAt=0;CHECK(s.touchFrame(xs,ys,1023)&&dispatched==1);CHECK(s.touchFrame(xs,ys,0)&&dispatched==2&&!live);
 }
 for(unsigned fault=1;fault<=10;fault++){
  reset();SweetHIDTouch s;failAppend=fault;CHECK(!s.touchFrame(xs,ys,1023));CHECK(!dispatched&&!live&&!locked);for(bool down:s.fDown)CHECK(!down);
  failAppend=0;CHECK(s.touchFrame(xs,ys,1023)&&dispatched==1);CHECK(s.touchFrame(xs,ys,0)&&dispatched==2&&!live);
 }
 reset();SweetHIDTouch s;xs[0]=540;ys[0]=2399;CHECK(s.touchFrame(xs,ys,1));CHECK(last[0].f[kIOHIDEventFieldDigitizerEventMask]&2048);ys[0]=1200;CHECK(s.touchFrame(xs,ys,1));CHECK(last[0].f[kIOHIDEventFieldDigitizerEventMask]&2048);CHECK(s.touchFrame(xs,ys,0));CHECK(s.touchFrame(xs,ys,1));CHECK(!(last[0].f[kIOHIDEventFieldDigitizerEventMask]&2048));CHECK(s.touchFrame(xs,ys,0));
 for(unsigned repeat=0;repeat<100;repeat++){CHECK(s.touchFrame(xs,ys,1));CHECK(s.touchFrame(xs,ys,0));CHECK(!s.fDown[0]);}
 unsigned before=dispatched;xs[3]=1080;CHECK(!s.touchFrame(xs,ys,8)&&dispatched==before&&!live);xs[3]=100;CHECK(!s.touchFrame(xs,ys,1024)&&dispatched==before);
 reset();SweetHIDTouch t;t.fFitCanvas=0;xs[0]=540;ys[0]=1200;CHECK(t.touchFrame(xs,ys,1));CHECK(last[0].f[kIOHIDEventFieldDigitizerX]==((int64_t)(540+22)<<16)/1125);CHECK(last[0].f[kIOHIDEventFieldDigitizerY]==((int64_t)(1200+18)<<16)/2436);CHECK(t.touchFrame(xs,ys,0)&&!live&&!locked);
 printf("FULL_TCM_HID_FRAME_HOST_PASS checks=%u contacts1to10=atomic allocationAppendFailures=transactional repeatedTapUp=100 normalizedCanvasAndEdges=preserved nativeOutcome=unproven\n",checks);
}
