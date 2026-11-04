#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using IOReturn=int;
enum {kIOReturnSuccess=0,kIOReturnNoSpace=1,kIOReturnUnderrun=2,kIOReturnError=3};
#define SWEET_AKS_LOCK_STATE 4
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"line%d\n",__LINE__);std::exit(1);}}while(0)
struct IOMemoryDescriptor{
 uint8_t bytes[4096]={};uint32_t capacity=4096,prepareCalls=0,completeCalls=0,written=0;int prepareResult=0,completeResult=0;bool shortWrite=false;
 uint32_t getLength(){return capacity;}IOReturn prepare(){prepareCalls++;return prepareResult;}IOReturn complete(){completeCalls++;return completeResult;}
 uint64_t writeBytes(unsigned off,const void*p,unsigned n){CHECK(off==0&&n<=capacity);unsigned count=shortWrite?n-1:n;std::memcpy(bytes,p,count);written=count;return count;}
};
struct IOExternalMethodArguments{IOMemoryDescriptor*structureOutputDescriptor=nullptr;void*structureOutput=nullptr;uint32_t structureOutputSize=0,structureOutputDescriptorSize=0;};
