#include <iostream>
#include <agency/execution_policy.hpp>
#include <agency/coordinate.hpp>
#include <agency/cuda/execution_policy.hpp>

const agency::cuda::detail::basic_execution_policy<agency::cuda::parallel_agent_2d, agency::cuda::parallel_executor> par2d{};

struct functor
{
  __device__
  void operator()(agency::cuda::parallel_agent_2d& self)
  {
    printf("Hello world from agent {%d, %d}\n", agency::get<0>(self.index()), agency::get<1>(self.index()));
  }
};

int main()
{
  auto exec = par2d({0,0}, {2,2});

  agency::cuda::bulk_invoke(exec, functor());

  cudaError_t error = cudaDeviceSynchronize();

  std::cerr << "CUDA error: " << cudaGetErrorString(error) << std::endl;

  return 0;
}

