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
/* Exact nearest-neighbour stepping: division is outside the pixel loop.
 * Opaque pixels use one32-bit store, including colour conversion. */
static inline uint32_t zui_pixel(uint32_t v,uint32_t format) {
 if(format==ZUI_BGRA)return v;
 if(format==ZUI_RGBA)return (v&0xff00ff00u)|((v&255u)<<16)|((v>>16)&255u);
 if(format==ZUI_ARGB)return __builtin_bswap32(v);
 return (v>>8)|(v<<24);
}
struct zui_axis {uint32_t at,remainder,whole,fraction,denominator;};
static inline struct zui_axis zui_axis_make(uint32_t source,uint32_t span,uint32_t dest,uint32_t skip) {
 uint64_t offset=(uint64_t)skip*span;
 struct zui_axis a={source+(uint32_t)(offset/dest),(uint32_t)(offset%dest),span/dest,span%dest,dest};
 return a;
}
static inline void zui_axis_next(struct zui_axis *a) {
 a->at+=a->whole;a->remainder+=a->fraction;
 if(a->remainder>=a->denominator){a->remainder-=a->denominator;a->at++;}
}
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
 struct zui_axis first=zui_axis_make((uint32_t)p->source.x,(uint32_t)p->source.w,
  (uint32_t)p->dest.w,(uint32_t)(p->clip.x-p->dest.x));
 struct zui_axis yaxis=zui_axis_make((uint32_t)p->source.y,(uint32_t)p->source.h,
  (uint32_t)p->dest.h,(uint32_t)(p->clip.y-p->dest.y));
 for(int32_t y=p->clip.y;y<p->clip.y+p->clip.h;y++,zui_axis_next(&yaxis)) {
  const uint8_t *row=s->pixels+s->offset+(uint64_t)yaxis.at*s->stride;
  uint8_t *dst=d->pixels+(uint64_t)y*d->stride+(uint64_t)p->clip.x*4;
  struct zui_axis xaxis=first;
  for(int32_t x=0;x<p->clip.w;x++,dst+=4,zui_axis_next(&xaxis)) {
   uint32_t raw,pixel;__builtin_memcpy(&raw,row+(uint64_t)xaxis.at*4,4);
   pixel=zui_pixel(raw,s->format);
   if(mode) {
    uint32_t old;__builtin_memcpy(&old,dst,4);
    uint32_t alpha=pixel>>24,out=0xff000000u;
    for(unsigned k=0;k<3;k++) {
     uint32_t v=((pixel>>(8*k))&255u)+(((old>>(8*k))&255u)*(255-alpha)+127)/255;
     out|=(v>255?255:v)<<(8*k);
    }
    pixel=out;
   }
   __builtin_memcpy(dst,&pixel,4);
  }
 }
}
#endif
