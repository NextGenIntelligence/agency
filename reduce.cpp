#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cassert>
#include <iostream>
#include <agency/execution_policy.hpp>


template<class Iterator, class T, class BinaryFunction>
T reduce(agency::sequential_execution_policy, Iterator first, Iterator last, T init, BinaryFunction binary_op)
{
  return std::accumulate(first, last, init, binary_op);
}


template<class Iterator, class T, class BinaryFunction>
T reduce(Iterator first, Iterator last, T init, BinaryFunction binary_op)
{
  auto n = std::distance(first, last);

  // chop up data into partitions
  auto partition_size = 10000;
  auto num_partitions = (n + partition_size - 1) / partition_size;
  std::vector<T> partial_sums(num_partitions);

  agency::bulk_invoke(agency::par(num_partitions), [=,&partial_sums](agency::parallel_agent& g)
  {
    auto i = g.index();

    auto partition_begin = first + partition_size * i;
    auto partition_end   = std::min(last, partition_begin + partition_size);

    partial_sums[i] = reduce(agency::seq, partition_begin + 1, partition_end, *partition_begin, binary_op);
  });

  return reduce(agency::seq, partial_sums.begin(), partial_sums.end(), init, binary_op);
}


int main()
{
  size_t n = 1 << 20;
  std::vector<int> data(n);
  std::generate(data.begin(), data.end(), [](){ return std::rand() % 20; });

  assert(reduce(data.begin(), data.end(), 0, std::plus<int>()) == accumulate(data.begin(), data.end(), 0, std::plus<int>()));

  std::cout << "OK" << std::endl;

  return 0;
}

