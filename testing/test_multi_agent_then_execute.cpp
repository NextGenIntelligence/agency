#include <agency/future.hpp>
#include <agency/new_executor_traits.hpp>
#include <iostream>
#include <cassert>
#include <algorithm>
#include <mutex>

struct my_executor {};

int main()
{
  {
    // then_execute returning user-specified container
    
    my_executor exec;

    size_t n = 100;

    auto past = agency::detail::make_ready_future(13);

    std::future<std::vector<int>> fut = agency::new_executor_traits<my_executor>::template then_execute<std::vector<int>>(exec, past, [](int& past, size_t idx)
    {
      return past;
    },
    n);

    auto got = fut.get();

    assert(got == std::vector<int>(n, 13));
  }

  {
    // then_execute returning default container
    
    my_executor exec;

    size_t n = 100;

    auto past = agency::detail::make_ready_future(13);

    auto fut = agency::new_executor_traits<my_executor>::then_execute(exec, past, [](int& past, size_t idx)
    {
      return past;
    },
    n);

    auto result = fut.get();

    std::vector<int> ref(n, 13);
    assert(std::equal(ref.begin(), ref.end(), result.begin()));
  }

  {
    // then_execute returning void
    
    my_executor exec;

    size_t n = 100;

    auto past = agency::detail::make_ready_future(13);

    int increment_me = 0;
    std::mutex mut;
    auto fut = agency::new_executor_traits<my_executor>::then_execute(exec, past, [&](int& past, size_t idx)
    {
      mut.lock();
      increment_me += past;
      mut.unlock();
    },
    n);

    fut.wait();

    assert(increment_me == n * 13);
  }

  std::cout << "OK" << std::endl;

  return 0;
}
