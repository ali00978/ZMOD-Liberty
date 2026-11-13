#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <random>
#include "../glue/rexglue-sdk-main/src/graphics/gta4_native/native_color_clear_fastpath.h"
using namespace rex::graphics::gta4_native;
static_assert(NativeClearRectCoversTarget(0, 0, 1920, 1080, 1920, 1080));
static_assert(NativeClearRectCoversTarget(-10, -20, 2000, 1100, 1920, 1080));
static_assert(!NativeClearRectCoversTarget(1, 0, 1920, 1080, 1920, 1080));
static_assert(!NativeClearRectCoversTarget(0, 0, 1919, 1080, 1920, 1080));
static_assert(!NativeClearRectCoversTarget(0, 0, 1920, 1080, 0, 1080));
static_assert(!NativeClearRectCoversTarget(0, 0, 1920, 1080, UINT32_MAX, 1080));
static_assert(NativeFullColorClearMask(0x3Fu, 0x5u, 0, 0, 1920, 1080, 1920, 1080) == 5);
static_assert(NativeFullColorClearMask(0x3Fu, 0x5u, 1, 0, 1920, 1080, 1920, 1080) == 0);
static_assert(NativeFullColorClearMask(0x10u, 0xFu, 0, 0, 1920, 1080, 1920, 1080) == 0);
static_assert(NativeClearRectHasNoPixels(0, 0, 0, 1024, 1920, 1080));
static_assert(NativeClearRectHasNoPixels(1920, 0, 1925, 1024, 1920, 1080));
static_assert(!NativeClearRectHasNoPixels(-1, -1, 1, 1, 1920, 1080));
static_assert(!NativeClearRectHasNoPixels(0, 0, 0, 0, 0, 1080));
int main() {
  std::mt19937 rng(925231);
  for (unsigned i=0;i<250000;++i) {
    uint32_t w=1u+(rng()%4096u), h=1u+(rng()%4096u);
    int32_t l=int32_t(rng()%8000u)-2000, t=int32_t(rng()%8000u)-2000;
    int32_t r=int32_t(rng()%8000u)-2000, b=int32_t(rng()%8000u)-2000;
    const int64_t L=std::max<int64_t>(0,l), T=std::max<int64_t>(0,t);
    const int64_t R=std::min<int64_t>(w,r), B=std::min<int64_t>(h,b);
    assert(NativeClearRectHasNoPixels(l,t,r,b,w,h) == (L>=R || T>=B));
    const bool full=l<=0 && t<=0 && r>=int32_t(w) && b>=int32_t(h);
    assert(NativeClearRectCoversTarget(l,t,r,b,w,h)==full);
    uint32_t flags=rng(), targets=rng();
    const auto actual=NativeFullColorClearMask(flags,targets,l,t,r,b,w,h);
    assert(actual==(full?(flags&targets&15u):0u));
  }
}
