/* Bounded CPU scanout. Source-over is opt-in: swap plane alpha semantics
 * must be measured, never inferred from an IOSurface's fourth byte. */
#ifndef ZIYI_U12_UI_H
#define ZIYI_U12_UI_H
#include <stdint.h>
#include <stddef.h>
#define ZUI_BGRA 0x42475241u
#define ZUI_RGBA 0x52474241u
#define ZUI_ARGB 0x41524742u
#define ZUI_ABGR 0x41424752u
struct zui_image { const uint8_t *pixels; uint64_t bytes,offset; uint32_t width,height,stride,format; };
struct zui_target { uint8_t *pixels; uint64_t bytes; uint32_t width,height,stride; };
struct zui_rect { int32_t x,y,w,h; };
struct zui_plan { struct zui_rect source,dest,clip; };
static inline int zui_format(uint32_t f) {
 return f==ZUI_BGRA||f==ZUI_RGBA||f==ZUI_ARGB||f==ZUI_ABGR;
}
static inline int zui_prepare(const struct zui_image *s,const struct zui_target *d,
 struct zui_rect a,struct zui_rect b,struct zui_plan *out) {
 if(!s||!d||!out||!s->pixels||!d->pixels||!zui_format(s->format))return 1;
 if(!s->width||!s->height||s->width>16384||s->height>16384||
    !d->width||!d->height||d->width>16384||d->height>16384||
    s->stride<(uint64_t)s->width*4||d->stride<(uint64_t)d->width*4)return 2;
 uint64_t slast=(uint64_t)(s->height-1)*s->stride+(uint64_t)s->width*4;
 uint64_t dlast=(uint64_t)(d->height-1)*d->stride+(uint64_t)d->width*4;
 if(s->offset>s->bytes||slast>s->bytes-s->offset||dlast>d->bytes)return 3;
 if(a.x<0||a.y<0||a.w<=0||a.h<=0||b.w<=0||b.h<=0||
    a.w>16384||a.h>16384||b.w>32768||b.h>32768||
    (int64_t)a.x+a.w>s->width||(int64_t)a.y+a.h>s->height)return 4;
 int64_t x0=b.x>0?b.x:0,y0=b.y>0?b.y:0;
 int64_t x1=(int64_t)b.x+b.w,y1=(int64_t)b.y+b.h;
 if(x1>d->width)x1=d->width;
 if(y1>d->height)y1=d->height;
 if(x0>=x1||y0>=y1)return 5;
 out->source=a;out->dest=b;
 out->clip=(struct zui_rect){(int32_t)x0,(int32_t)y0,(int32_t)(x1-x0),(int32_t)(y1-y0)};
 return 0;
}
static inline void zui_channels(const uint8_t *p,uint32_t f,uint32_t c[4]) {
 if(f==ZUI_BGRA){c[0]=p[0];c[1]=p[1];c[2]=p[2];c[3]=p[3];}
 else if(f==ZUI_RGBA){c[0]=p[2];c[1]=p[1];c[2]=p[0];c[3]=p[3];}
 else if(f==ZUI_ARGB){c[0]=p[3];c[1]=p[2];c[2]=p[1];c[3]=p[0];}
 else {c[0]=p[1];c[1]=p[2];c[2]=p[3];c[3]=p[0];}
}
/* mode0: hardware-style opaque plane copy; mode1: premultiplied source-over.
 * No allocation, global state, MMIO, or access outside the prepared rectangles. */
static inline void zui_render(const struct zui_image *s,const struct zui_target *d,
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
#endif
