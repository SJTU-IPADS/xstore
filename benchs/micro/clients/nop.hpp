namespace fstore {

using namespace platforms;

namespace bench {

RdmaCtrl&
global_rdma_ctrl();
RegionManager&
global_memory_region();

extern volatile bool running;

using Worker = Thread<double>;

class NopClient
{
public:
  static std::vector<Worker*> bootstrap_all(std::vector<Statics>& statics,
                                            const Addr& server_addr,
                                            const std::string& server_host,
                                            const u64& server_port,
                                            PBarrier& bar,
                                            u64 num_threads,
                                            u64 my_id,
                                            u64 concurrency = 1)
  {
    LOG(4) << "client #" << my_id << " bootstrap " << num_threads
           << " threads.";
    std::vector<Worker*> handlers;
    for (uint thread_id = 0; thread_id < num_threads; ++thread_id) {
      handlers.push_back(new Worker([&]() {
                                      bar.wait();
                                      while( running)
                                        sleep(1);
                                      return 0;
      }));
    }
    return handlers;
  }
};

}

}
