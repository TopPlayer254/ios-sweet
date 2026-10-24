#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "u14_ui.h"
static inline void zui_reference_render(const struct zui_image *s,const struct zui_target *d,
 const struct zui_plan *p,unsigned mode) {
 if(!mode&&s->format==ZUI_BGRA&&p->source.w==p->dest.w&&p->source.h==p->dest.h) {
  for(int32_t y=p->clip.y;y<p->clip.y+p->clip.h;y++) {
   uint32_t sy=(uint32_t)(p->source.y+y-p->dest.y),sx=(uint32_t)(p->source.x+p->clip.x-p->dest.x);
   __builtin_memcpy(d->pixels+(uint64_t)y*d->stride+(uint64_t)p->clip.x*4,
    s->pixels+s->offset+(uint64_t)sy*s->stride+(uint64_t)sx*4,(size_t)p->clip.w*4);
  }
  return;
 }
 for(int32_t y=p->clip.y;y<p->clip.y+p->clip.h;y++) {
  uint32_t sy=(uint32_t)(p->source.y+((int64_t)y-p->dest.y)*p->source.h/p->dest.h);
  for(int32_t x=p->clip.x;x<p->clip.x+p->clip.w;x++) {
   uint32_t sx=(uint32_t)(p->source.x+((int64_t)x-p->dest.x)*p->source.w/p->dest.w);
   const uint8_t *src=s->pixels+s->offset+(uint64_t)sy*s->stride+(uint64_t)sx*4;
   uint8_t *dst=d->pixels+(uint64_t)y*d->stride+(uint64_t)x*4;uint32_t c[4];
   zui_channels(src,s->format,c);
   for(unsigned k=0;k<3;k++) {
    uint32_t v=mode?c[k]+((uint32_t)dst[k]*(255-c[3])+127)/255:c[k];
    dst[k]=(uint8_t)(v>255?255:v);
   }
   dst[3]=mode?255:(uint8_t)c[3];
  }
 }
}

static uint32_t state=0x9753abc1;
static uint32_t next(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
int main(void){
 uint8_t src[32*36*4+256],a[32*36*4+256],b[sizeof(a)];unsigned cases=0;
 uint32_t formats[]={ZUI_BGRA,ZUI_RGBA,ZUI_ARGB,ZUI_ABGR};
 for(unsigned n=0;n<10000;n++){
  unsigned w=next()%32+1,h=next()%32+1,stride=w*4+next()%8,off=next()%8;
  unsigned dw=next()%32+1,dh=next()%32+1,ds=dw*4+next()%8;
  for(unsigned k=0;k<sizeof(src);k++){src[k]=(uint8_t)next();a[k]=(uint8_t)next();}memcpy(b,a,sizeof(a));
  struct zui_image s={src,sizeof(src),off,w,h,stride,formats[n%4]};
  struct zui_target x={a+16,sizeof(a)-32,dw,dh,ds},y={b+16,sizeof(b)-32,dw,dh,ds};
  int sx=(int)(next()%w),sy=(int)(next()%h);
  struct zui_rect sr={sx,sy,(int)(next()%(w-sx)+1),(int)(next()%(h-sy)+1)};
  struct zui_rect dr={(int)(next()%65)-32,(int)(next()%65)-32,(int)(next()%64+1),(int)(next()%64+1)};
  struct zui_plan plan;if(zui_prepare(&s,&x,sr,dr,&plan))continue;
  zui_render(&s,&x,&plan,(n>>2)%2);zui_reference_render(&s,&y,&plan,(n>>2)%2);
  assert(!memcmp(a,b,sizeof(a)));cases++;
 }
 printf("U14_BIT_EXACT_REFERENCE_PASS randomPreparedCases=%u attempted=10000 formats=4 alpha=2 guards=PASS\n",cases);
 unsigned sw=1125,sh=2436,dw=1080,dh=2400;
 uint8_t *source=malloc(sw*sh*4),*dest=malloc(dw*dh*4);assert(source&&dest);
 for(unsigned i=0;i<sw*sh*4;i++)source[i]=(uint8_t)next();
 struct zui_image s={source,(uint64_t)sw*sh*4,0,sw,sh,sw*4,ZUI_BGRA};
 struct zui_target d={dest,(uint64_t)dw*dh*4,dw,dh,dw*4};struct zui_plan plan;
 assert(!zui_prepare(&s,&d,(struct zui_rect){0,0,sw,sh},(struct zui_rect){0,0,dw,dh},&plan));
 clock_t begin=clock();for(unsigned i=0;i<20;i++)zui_reference_render(&s,&d,&plan,0);
 double oldms=1000.0*(clock()-begin)/CLOCKS_PER_SEC/20;
 begin=clock();for(unsigned i=0;i<20;i++)zui_render(&s,&d,&plan,0);
 double newms=1000.0*(clock()-begin)/CLOCKS_PER_SEC/20;
 printf("U14_HOST_ONLY_BENCHMARK old_ms=%.3f new_ms=%.3f ratio=%.2f NO_PHONE_FPS_CLAIM\n",oldms,newms,oldms/newms);
 free(source);free(dest);return 0;
}
