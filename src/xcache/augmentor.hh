#pragma once

#include "./sub_model.hh"

namespace fstore {

class Aug
{
public:
  template<typename S>
  static std::vector<Option<std::pair<u64,u64>>> aug(std::vector<S>& ms) {
    std::vector<Option<std::pair<u64,u64>>> new_keys;
    for(uint i = 0;i < ms.size(); ++i) {
      auto &m = ms[i];
      if (m.page_table.size() == 0) {
        new_keys.push_back({});
        continue;
      }

      u64 new_start_key = m.start_key;
      u64 new_end_key   = m.end_key;

      // first update the start key
      int j = i;
      while (j - 1 > 0) {
        auto &pm = ms[j - 1];
        if (pm.page_table.size() > 0) {

          new_start_key = std::min(new_start_key, pm.end_key);
          if (i == 2669142) {
            LOG(4) << "aug start key from: " << m.start_key << " " << new_start_key;
          }
          break;
        }
        j -= 1;
      }

      // then the end key
      j = i;
      while (j + 1 < ms.size()) {
        auto &nm = ms[j + 1];
        if (nm.page_table.size() != 0) {
          new_end_key = std::max(new_end_key, nm.start_key);
          break;
        }
        j += 1;
      }
      new_keys.push_back(std::make_pair(new_start_key, new_end_key));
    }

    ASSERT(new_keys.size() == ms.size());
    for(uint i = 0;i < ms.size();++i) {
      if (new_keys[i]) {

      }
    }
    return new_keys;
  }

    // must be called after
    // - xcache.train_dispatcher(..);
    // - xcache.train_submodels(..);
    // - Aug::zug_zero(xcache.submodels);
  template<typename S>
  static usize aug_zero(std::vector<S>& ms)
  {
    usize counter = 0;
    for (uint i = 0; i < ms.size(); ++i) {
      // check whether need to aug
      auto& m = ms[i];
      if (i == 1) {
        LOG(4) << "check model :" << i << " whether need train: " << m.page_table.size();
      }
      if (m.page_table.size() == 0) {
        if (i == 1) {
          LOG(4) << "aug model " << i;
        }
        // need aug
        counter += 1;

        u64 start_key = 0;
        u64 end_key = std::numeric_limits<u64>::max();

        // find the previous non-empty model
        int j = i;
        while (j - 1 >= 0) {
          auto& pm = ms[j - 1];
          if (pm.page_table.size() > 0) {
            start_key = pm.end_key;
            goto step2;
          }
          j -= 1;
        }
      step2:

        j = i;
        // find the next
        while (j + 1 < ms.size()) {
          auto& pm = ms[j + 1];
          if (pm.page_table.size() > 0) {
            end_key = pm.start_key;
            goto reset;
          }
          j += 1;
        }
      reset:
        // reset
        ms[i].lock.lock();
        ms[i].reset_keys(start_key, end_key);
        ms[i].notify_watermark += 1;
        ms[i].lock.unlock();
        //ms[i].need_train = true;
        // end find  the target model to aug
      }
    }
    //    ASSERT(ms[1].need_train);
    // end augmentation model
    return counter;
  }
};
}
