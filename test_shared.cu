#include <agency/cuda/execution_policy.hpp>
#include <agency/cuda/grid_executor.hpp>
#include <thrust/device_vector.h>
#include <cstdio>
#include <cassert>


template<class T>
__device__
T fetch_and_add(T* ptr, T value)
{
#ifdef __NVCC__
  return atomicAdd(ptr, value);
#else
  return __sync_fetch_and_add(ptr, value);
#endif
}


using cuda_thread = agency::parallel_group<agency::cuda::concurrent_agent>;


struct functor
{
  __device__
  void operator()(cuda_thread& self, int* outer_result, int& outer_shared, int& inner_shared)
  {
    printf("idx: {%d, %d}\n", self.outer().index(), self.inner().index());
    printf("outer_shared: %d\n", outer_shared);
    printf("inner_shared: %d\n", inner_shared);

    fetch_and_add(&inner_shared, 1);
    self.inner().wait();

    assert(inner_shared == self.inner().group_size() + 2);

    auto result = fetch_and_add(&outer_shared, 1);

    // exactly one agent will see this result
    if(result == (2 * 2))
    {
      *outer_result = result + 1;
    }
  }
};


int main()
{
  using cuda_thread = agency::parallel_group<agency::cuda::concurrent_agent>;

  auto policy = agency::cuda::par(2, agency::cuda::con(2));

  thrust::device_vector<int> outer_result(1);

  agency::cuda::bulk_invoke(policy, functor(), thrust::raw_pointer_cast(outer_result.data()), agency::share<0>(1), agency::share<1>(2));

  assert(outer_result[0] == (2 * 2 + 1));

  std::cout << "OK" << std::endl;

  return 0;
}

