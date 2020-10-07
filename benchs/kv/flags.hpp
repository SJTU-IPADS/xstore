#pragma once

#include <gflags/gflags.h>

/*!
  This file provides declarations of command-line flags defined in flags.cc .
 */
namespace kv {

DECLARE_int64(threads);
DECLARE_int64(page_sz_m);
DECLARE_string(model_config);

DECLARE_string(workload_type);
DECLARE_int64(seed);
DECLARE_int64(running_time);
DECLARE_int64(batch_size);

DECLARE_int64(start_warehouse);
DECLARE_int64(end_warehouse);

DECLARE_int64(num);

DECLARE_string(txt_name);

}
