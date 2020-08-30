/// Try evaluating the optimal performance of XStore read/write
/// For each request, the benchmark will do as follows:
/// - 1. read sizeof(XNode) in XTree
/// - 2. read sizeof(payload) given the benchmark payload

#include <gflags/gflags.h>

DEFINE_int64(threads, 1, "num client thread to use");
DEFINE_int64(coros, 1, "num client coroutine used per threads");
DEFINE_string(addr, "localhost:8888", "server address");

namespace bench {}

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  // TODO
}
