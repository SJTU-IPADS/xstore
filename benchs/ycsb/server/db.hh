#pragma once

#include "../../../xcache/src/submodel_trainer.hh"

#include "../../../xkv_core/src/xtree/page_iter.hh"
#include "../../../xkv_core/src/xalloc.hh"

#include "../../../xutils/cdf.hh"

#include "../schema.hh"

namespace xstore {

using namespace xcache;
using namespace xml;
using namespace xkv::xtree;
using namespace util;

// core DB
XAlloc<sizeof(DBTree::Leaf)> *xalloc = nullptr;
DBTree db;
std::unique_ptr<XCache> cache = nullptr;
std::vector<XCacheTT> tts;

// avaliable serialze buf, main.cc init this
u64 model_buf = 0;
u64 tt_buf    = 0;
u64 buf_end   = 0;

auto load_linear(const u64 &nkeys) {
  for (u64 k = 0; k < nkeys; ++k) {
    //db.insert(XKey(k), k);
    db.insert_w_alloc(XKey(k),k,*xalloc);
  }
}

auto page_updater(const u64 &label, const u64 &predict, const int &cur_min,
                      const int &cur_max) -> std::pair<int, int> {
  if (predict / kNPageKey == label / kNPageKey) {
    return std::make_pair(cur_min, cur_max);
  }

  auto new_min = std::min(static_cast<i64>(cur_min),
                          static_cast<i64>(label) - static_cast<i64>(predict));
  auto new_max = std::max(static_cast<i64>(cur_max),
                          static_cast<i64>(label) - static_cast<i64>(predict));

  return std::make_pair(new_min, new_max);
}

auto train_db(const std::string &config) {
  // TODO: load model configuration from the file
  const int num_sub = 10000;

  if (cache == nullptr) {
    // init
    cache = std::make_unique<XCache>(num_sub);

    // init sub
    for (uint i = 0;i < num_sub; ++i)  {
      tts.emplace_back();
    }

    // train
    // 1. first layer
    {
      r2::Timer t;
      cache->default_train_first<DBTreeIter>(db);
      LOG(4) << "train first layer done using: " << t.passed_sec() << " secs";
    }
    // 2. second layer
    auto trainers =
        cache->dispatch_keys_to_trainers<DBTreeIter>(db);
    {
      CDF<int> error_cdf("");
      CDF<int> page_cdf("");

      usize null_model = 0;

      for (uint i = 0;i < trainers.size();++i) {
        auto &trainer = trainers[i];
        auto it = TrainIter::from_tt(db,&(tts[i]));

        //DefaultSample<XKey> s;
        PS<XKey> s;
        StepSampler<XKey> ss(2);

        cache->second_layer[i] =
            trainer.train_w_it_w_shrink<TrainIter, PS, StepSampler, LR>(
                it, db, s, ss, page_updater);
        if (tts[i].size() != 0) {
          error_cdf.insert(cache->second_layer[i].total_error());
          page_cdf.insert(tts[i].size());
        } else {
          null_model += 1;
        }
      }
      error_cdf.finalize();
      page_cdf.finalize();

      LOG(4) << "Error data:";
      LOG(4) << error_cdf.dump_as_np_data() << "\n";

      LOG(4) << "Page entries:" << page_cdf.dump_as_np_data() << "\n";

      LOG(4) << "total " << num_sub
             << " models; null models:" << null_model;
    }
    // done
  }
}

auto serialize_db() {
  char *cur_ptr = (char *)model_buf;
  for (uint i = 0;i < cache->second_layer.size();++i) {
    auto s = cache->second_layer[i].serialize();
    ASSERT(s.size() + (u64)cur_ptr <= buf_end);
    memcpy(cur_ptr, s.data(),s.size());
    cur_ptr += s.size();
  }

  tt_buf = (u64)cur_ptr;

  LOG(4) << "after seriaize, tt buf: " << tt_buf << "; model sz:  "
         << tt_buf - model_buf;

  // update the TT
  for (uint i = 0;i < tts.size();++i) {
    auto s = tts[i].serialize();
    ASSERT(s.size() + (u64)cur_ptr <= buf_end);
    memcpy(cur_ptr,s.data(),s.size());
    cur_ptr += s.size();
  }
  buf_end = (u64)cur_ptr;
}

} // namespace xstore
