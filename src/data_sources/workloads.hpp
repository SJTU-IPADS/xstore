#pragma once

#include "../common.hpp"

/*!
  Workloads provide a template BASE class for different workload generator.
  The workloads basically generate keys requests to the KV store.
 */
namespace fstore {

namespace sources {

/**
 *   We use static polymorphism, since these code may results on the critical path
 * the code's execution.
 */
template <class Derived>
class BaseWorkload {
 public:
  u64 next_key() {
    return static_cast<Derived*>(this)->next_key_impl();
  }
};

}

}
