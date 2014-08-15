#pragma once

#include <sched.h>
#include <future>
#include <typeinfo>
#include <tuple>
#include <functional>
#include <iostream>
#include "variant.hpp"


class cpu_id
{
  public:
    typedef int native_handle_type;

    cpu_id(native_handle_type handle)
      : handle_(handle)
    {}

    // default constructor creates a cpu_id which represents no CPU
    cpu_id()
      : cpu_id(-1)
    {}

    // XXX std::this_thread::native_handle() is not const -- why?
    native_handle_type native_handle() const
    {
      return handle_;
    }

    friend inline bool operator==(cpu_id lhs, const cpu_id& rhs)
    {
      return lhs.handle_ == rhs.handle_;
    }

    friend inline bool operator!=(cpu_id lhs, cpu_id rhs)
    {
      return lhs.handle_ != rhs.handle_;
    }

    friend inline bool operator<(cpu_id lhs, cpu_id rhs)
    {
      return lhs.handle_ < rhs.handle_;
    }

    friend inline bool operator<=(cpu_id lhs, cpu_id rhs)
    {
      return lhs.handle_ <= rhs.handle_;
    }

    friend inline bool operator>(cpu_id lhs, cpu_id rhs)
    {
      return lhs.handle_ > rhs.handle_;
    }

    friend inline bool operator>=(cpu_id lhs, cpu_id rhs)
    {
      return lhs.handle_ >= rhs.handle_;
    }

    friend std::ostream& operator<<(std::ostream &os, const cpu_id& id)
    {
      return os << id.native_handle();
    }

  private:
    native_handle_type handle_;
};


// XXX do we really want to burn a thread_local on this?
//     the alternative would be some sort of this_cpu_t which acts like a cpu_id
//constexpr thread_local cpu_id this_cpu(sched_getcpu());
thread_local cpu_id this_cpu(sched_getcpu());


template<class Function, class... Args>
std::future<typename std::result_of<Function(Args...)>::type>
  async(cpu_id proc, Function&& f, Args&&... args)
{
  // XXX figure out how to forward f & args correctly through std::async
  return std::async(std::launch::async, [=](Args&&... args)
  {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(proc.native_handle(), &set);

    if(sched_setaffinity(0, sizeof(cpu_set_t), &set) < -1)
    {
      throw std::system_error(errno, std::system_category(), "std::async(): error after sched_setaffinity()");
    } // end if

    return f(std::forward<Args>(args)...);
  },
  std::forward<Args>(args)...
  );
}


template<class Function, class... Args>
std::future<void> bulk_async(cpu_id proc, size_t n, Function&& f, Args&&... args)
{
  auto g = std::bind(f, std::placeholders::_1, args...);

  std::vector<std::future<void>> futures(n);
  
  for(size_t i = 0; i < n; ++i)
  {
    futures[i] = async(proc, [=]
    {
      g(i);
    });
  }
  
  return when_all(futures.begin(), futures.end());
}


// XXX should this exist?
template<class Function, class... Args>
typename std::result_of<Function(Args...)>::type
  sync(cpu_id proc, Function&& f, Args&&... args)
{
  // execute immediately if possible
  if(proc == this_cpu)
  {
    return f(std::forward<Args>(args)...);
  }

  return async(proc, std::forward<Function>(f), std::forward<Args>(args)...).get();
}


template<class Function, class... Args>
void bulk_sync(cpu_id proc, size_t n, Function&& f, Args&&... args)
{
  return bulk_async(proc, n, std::forward<Function>(f), std::forward<Args>(args)...).get();
}


class processor_id
{
  public:
    // default constructor creates a processor_id which represents no processor
    // XXX this might be a bad policy -- maybe construct a cpu_id instead
    processor_id()
      : index_(unknown_id{})
    {}

    processor_id(cpu_id id)
      : index_(id)
    {}

    friend inline bool operator==(processor_id lhs, processor_id rhs)
    {
      return lhs.index_ == rhs.index_;
    }

    friend inline bool operator!=(processor_id lhs, processor_id rhs)
    {
      return lhs.index_ != rhs.index_;
    }

    friend inline bool operator<(processor_id lhs, processor_id rhs)
    {
      return lhs.index_ < rhs.index_;
    }

    friend inline bool operator<=(processor_id lhs, processor_id rhs)
    {
      return lhs.index_ <= rhs.index_;
    }

    friend inline bool operator>(processor_id lhs, processor_id rhs)
    {
      return lhs.index_ > rhs.index_;
    }

    friend inline bool operator>=(processor_id lhs, processor_id rhs)
    {
      return lhs.index_ >= rhs.index_;
    }

    friend std::ostream& operator<<(std::ostream& os, const processor_id& id)
    {
      return os << id.index_;
    }

    const std::type_info& type() const
    {
      return index_.type();
    }

  private:
    template<class Function, class... Args>
    friend std::future<typename std::result_of<Function(Args...)>::type>
      async(processor_id proc, Function&& f, Args&&... args);

    template<class Function>
    friend struct __async_processor_visitor;

    struct unknown_id
    {
      inline bool operator==(const unknown_id&) const
      {
        return true;
      }

      inline bool operator<(const unknown_id&) const
      {
        return false;
      }

      friend inline std::ostream& operator<<(std::ostream& os, const unknown_id& id)
      {
        return os << "<unknown processor>";
      }
    };

    variant<unknown_id, cpu_id> index_;
};


// XXX do we really want to burn a thread_local on this?
//     the alternative would be some sort of this_processor_t which acts like a processor_id
// XXX consider making this a function to avoid the thread_local
//constexpr thread_local processor_id this_processor(this_cpu);
thread_local processor_id this_processor(this_cpu);


template<class Function>
struct __async_processor_visitor
{
  Function f;

  typedef typename std::result_of<Function()>::type result_type;

  __async_processor_visitor(Function f)
    : f(f)
  {}

  std::future<result_type> operator()(const processor_id::unknown_id&)
  {
    throw std::runtime_error("std::async: unknown processor");
  }

  template<class ProcessorID>
  std::future<result_type> operator()(const ProcessorID& proc)
  {
    return async(proc, f);
  }
};


template<class Function, class... Args>
auto __make_async_processor_visitor(Function&& f, Args&&... args) ->
  __async_processor_visitor<decltype(std::bind(f, args...))>
{
  auto closure = std::bind(f, args...);

  return __async_processor_visitor<decltype(closure)>(closure);
}


template<class Function, class... Args>
std::future<typename std::result_of<Function(Args...)>::type>
  async(processor_id proc, Function&& f, Args&&... args)
{
  using result_type = typename std::result_of<Function(Args...)>::type;

  auto visitor = __make_async_processor_visitor(std::forward<Function>(f), std::forward<Args>(args)...);

  return apply_visitor(visitor, proc.index_);
}


// XXX should this exist?
template<class Function, class... Args>
typename std::result_of<Function(Args...)>::type
  sync(processor_id proc, Function&& f, Args&&... args)
{
  // execute immediately if possible
  if(proc == this_processor)
  {
    return f(std::forward<Args>(args)...);
  }

  return async(proc, std::forward<Function>(f), std::forward<Args>(args)...).get();
}

