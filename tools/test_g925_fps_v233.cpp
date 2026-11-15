// G925 2.33: exact two-draw index repacking, with bounds and canary checks.
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <random>
#include <span>
#include <vector>
#include "../glue/rexglue-sdk-main/src/graphics/gta4_native/native_sparse_index_batch.h"
using namespace rex::graphics::gta4_native;

static_assert(ShouldBatchNativeSparseIndexDraws(2,1,384,true));
static_assert(ShouldBatchNativeSparseIndexDraws(2,1,6,true));
static_assert(!ShouldBatchNativeSparseIndexDraws(2,1,kNativePairedSparseBatchMaxIndices+1,true));
static_assert(!ShouldBatchNativeSparseIndexDraws(2,1,384,false));
static_assert(!ShouldBatchNativeSparseIndexDraws(2,2,384,true));
static_assert(ShouldBatchNativeSparseIndexDraws(3,1,1024,true));
static_assert(!ShouldBatchNativeSparseIndexDraws(3,1,kNativeSmallSparseBatchMaxIndices+1,true));
static_assert(ShouldBatchNativeSparseIndexDraws(4,2,4000,true));

template <class Index> void VerifyPaired(uint32_t seed) {
  std::mt19937 random(seed);
  constexpr size_t kSourceElements=2048;
  std::vector<Index> source(kSourceElements);
  for (Index& v:source) v=Index(random());
  for (unsigned step=0;step<25000;++step) {
    // Always triangle-aligned, with a genuine gap and <384 indices total.
    const uint32_t first_count=3u*(1u+random()%60u);
    const uint32_t second_count=3u*(1u+random()%60u);
    const uint32_t total=first_count+second_count;
    const uint32_t first=1u+random()%300u;
    const uint32_t second=900u+random()%600u;
    const std::array<NativeSparseIndexRange,2> ranges{{{first,first_count},{second,second_count}}};
    assert(NativeSparseTriangleBatchHasGap(ranges));
    assert(NativeSparseTriangleIndexCount(ranges,source.size())==total);
    assert(ShouldBatchNativeSparseIndexDraws(2,1,total,true));
    std::vector<Index> output(total+2,Index(0xCECE));
    assert(WriteNativeSparseTriangleIndices(
      {reinterpret_cast<const uint8_t*>(source.data()),source.size()*sizeof(Index)},
      ranges,sizeof(Index)==4,
      {reinterpret_cast<uint8_t*>(output.data()),total*sizeof(Index)}));
    assert(output[total]==Index(0xCECE) && output[total+1]==Index(0xCECE));
    for (uint32_t i=0;i<first_count;++i) assert(output[i]==source[first+i]);
    for (uint32_t i=0;i<second_count;++i) assert(output[first_count+i]==source[second+i]);
  }
}
int main(){VerifyPaired<uint16_t>(92516);VerifyPaired<uint32_t>(92532);}
