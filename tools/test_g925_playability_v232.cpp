#include <array>
#include <cassert>
#include <cstdint>
#include <random>
#include <span>
#include <vector>
#include "../glue/rexglue-sdk-main/src/graphics/gta4_native/native_sparse_index_batch.h"
using namespace rex::graphics::gta4_native;
static_assert(ShouldBatchNativeSparseIndexDraws(3,1,1024,true));
static_assert(!ShouldBatchNativeSparseIndexDraws(3,1,1025,true));
static_assert(!ShouldBatchNativeSparseIndexDraws(3,2,400,true));
static_assert(!ShouldBatchNativeSparseIndexDraws(3,1,400,false));
static_assert(!ShouldBatchNativeSparseIndexDraws(2,1,400,true));
static_assert(ShouldBatchNativeSparseIndexDraws(4,2,4096,true));
static_assert(!ShouldBatchNativeSparseIndexDraws(4,3,4096,true));
int main() {
  std::mt19937 rng(925232);
  for (unsigned i=0; i<100000; ++i) {
    const uint32_t each=3*(1+rng()%180u);
    const std::array<NativeSparseIndexRange,3> ranges{{
        {0,each},{each+5,each},{2*each+31,each}}};
    const uint32_t count=3*each;
    assert(ShouldBatchNativeSparseIndexDraws(
        3,1,count,NativeSparseTriangleBatchHasGap(ranges))==
        (count<=kNativeSmallSparseBatchMaxIndices));
    std::vector<uint32_t> words(count+64);
    for(auto& w:words) w=rng();
    std::vector<uint32_t> out(count+2,0xBAADF00D);
    assert(WriteNativeSparseTriangleIndices(
      std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(words.data()),
                               words.size()*sizeof(uint32_t)),
      ranges,true,
      std::span<uint8_t>(reinterpret_cast<uint8_t*>(out.data()),
                         count*sizeof(uint32_t))));
    assert(out[count]==0xBAADF00D && out[count+1]==0xBAADF00D);
    size_t cursor=0;
    for(const auto& range:ranges)
      for(uint32_t j=0;j<range.count;++j)
        assert(out[cursor++]==words[range.first+j]);
  }
}
