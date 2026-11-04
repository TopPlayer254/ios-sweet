#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using UInt32=uint32_t;using IOFixed=int32_t;
#define TRUE 1
#define SWEET_TOUCH_FINGERS 10
#define SWEET_TOUCH_EDGE_PX 48
enum{ kIOHIDDigitizerEventRange=1,kIOHIDDigitizerEventTouch=2,kIOHIDDigitizerEventPosition=4,kIOHIDDigitizerEventIdentity=32,kIOHIDDigitizerEventFromEdgeTip=2048 };
enum {kIOHIDDigitizerTransducerTypeHand=3,kIOHIDDigitizerTransducerTypeFinger=2};
enum{kIOHIDEventFieldDigitizerCollection,kIOHIDEventFieldDigitizerIndex,kIOHIDEventFieldDigitizerIdentity,kIOHIDEventFieldDigitizerTouch,kIOHIDEventFieldDigitizerEventMask,kIOHIDEventFieldDigitizerX,kIOHIDEventFieldDigitizerY,kIOHIDEventFieldDigitizerRange,kIOHIDEventFieldDigitizerButtonMask};
static unsigned checks,locked,allocs,failAt,appendCount,failAppend,live,dispatched;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"line%d\n",__LINE__);std::exit(1);}}while(0)
static void IOLockLock(int*){CHECK(!locked);locked=1;}
static void IOLockUnlock(int*){CHECK(locked);locked=0;}
static uint64_t mach_absolute_time(){return 1234567;}
class IOHIDEvent;
class OSArray{public:std::vector<IOHIDEvent*>v;unsigned getCount(){return (unsigned)v.size();}};
class IOHIDEvent{public:
 int refs=1;uint64_t stamp;UInt32 type;int64_t f[9]={};OSArray children;
 static IOHIDEvent* digitizerEvent(uint64_t t,UInt32 index,UInt32 typ,bool range,UInt32 buttons,IOFixed x,IOFixed y){if(++allocs==failAt)return nullptr;auto*p=new IOHIDEvent;live++;p->stamp=t;p->type=typ;p->f[kIOHIDEventFieldDigitizerIndex]=index;p->f[kIOHIDEventFieldDigitizerRange]=range;p->f[kIOHIDEventFieldDigitizerButtonMask]=buttons;p->f[kIOHIDEventFieldDigitizerX]=x;p->f[kIOHIDEventFieldDigitizerY]=y;return p;}
 void setIntegerValue(unsigned k,int64_t n){CHECK(k<9);f[k]=n;}void setFixedValue(unsigned k,IOFixed n){setIntegerValue(k,n);}
 void appendChild(IOHIDEvent*c){if(++appendCount==failAppend)return;c->refs++;children.v.push_back(c);}OSArray*getChildren(){return children.v.empty()?nullptr:&children;}
 void release(){if(--refs)return;for(auto*p:children.v)p->release();live--;delete this;}
};
struct Captured{uint64_t stamp;UInt32 type;int64_t f[9];};static std::vector<Captured>last;
class SweetHIDTouch{public:
 int lock=0,*fLock=&lock,fLogicalW=1125,fLogicalH=2436,fOffX=22,fOffY=18,fFitCanvas=1;
 bool fDown[10]={},fEdge[10]={};IOFixed fX[10]={},fY[10]={};
 bool touchFrame(const UInt32*,const UInt32*,UInt32);
 void dispatchEvent(IOHIDEvent*p){CHECK(!locked);last.clear();for(auto*q:p->children.v){Captured c;c.stamp=q->stamp;c.type=q->type;std::memcpy(c.f,q->f,sizeof(c.f));last.push_back(c);}CHECK(p->stamp==1234567);dispatched++;}
};
