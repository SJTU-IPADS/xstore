#pragma once

#include "../../../xcache/src/submodel_trainer.hh"

#include "../../../xkv_core/src/xalloc.hh"
#include "../../../xkv_core/src/xtree/page_iter.hh"

#include "../../../xutils/cdf.hh"
#include "../../../xutils/file_loader.hh"

#include "../schema.hh"

namespace xstore {

DEFINE_bool(vlen, false, "whether to use variable length value");
DEFINE_int32(len, 8, "average length of the value");
DEFINE_int32(nmodels, 10000, "number submodel used");

using namespace xcache;
using namespace xml;
using namespace xkv::xtree;
using namespace util;

// core DB
XAlloc<sizeof(DBTree::Leaf)> *xalloc = nullptr;
DBTree db;
DBTreeV dbv;
std::unique_ptr<XCache> cache = nullptr;
std::vector<XCacheTT> tts;

// avaliable serialze buf, main.cc init this
u64 val_buf = 0;
u64 model_buf = 0;
u64 tt_buf = 0;
u64 buf_end = 0;

auto load_linear(const u64 &nkeys) {
  char *cur_val_ptr = reinterpret_cast<char *>(val_buf);
  for (u64 k = 0; k < nkeys; ++k) {
    // db.insert(XKey(k), k);
    if (!FLAGS_vlen) {
      db.insert_w_alloc(XKey(k), k, *xalloc);
    } else {
      ASSERT((u64)(cur_val_ptr + FLAGS_len) <= model_buf) << " insert k: " << k << " failed; "
                                                          << "total alloced:"  << (u64)(cur_val_ptr - val_buf);
      dbv.insert_w_alloc(XKey(k), FatPointer(cur_val_ptr, FLAGS_len), *xalloc);
      cur_val_ptr += FLAGS_len;
    }
  }
}

DEFINE_bool(load_from_file, true, "whether to load DB from the file");
DEFINE_string(data_file, "lognormal_uni_100m.txt", "data file name");

auto load_from_file(const usize &nkeys) {
  FileLoader loader(FLAGS_data_file);
  char *cur_val_ptr = reinterpret_cast<char *>(val_buf);

  for (usize i = 0;i < nkeys; ++i) {
    auto key = loader.next_key<u64>(FileLoader::default_converter<u64>);
    if (key) {
      auto k = key.value();
      if (!FLAGS_vlen) {
        db.insert_w_alloc(XKey(k), k, *xalloc);
      } else {
        ASSERT((u64)(cur_val_ptr + FLAGS_len) <= model_buf)
            << " insert k: " << k << " failed; "
            << "total alloced:" << (u64)(cur_val_ptr - val_buf);
        dbv.insert_w_alloc(XKey(k), FatPointer(cur_val_ptr, FLAGS_len),
                           *xalloc);
        cur_val_ptr += FLAGS_len;
      }
    }
  }
}

auto page_updater(const u64 &label, const u64 &predict, const int &cur_min,
                  const int &cur_max) -> std::pair<int, int> {

  if (LogicAddr::decode_logic_id<kNPageKey>(predict) ==
      LogicAddr::decode_logic_id<kNPageKey>(label)) {
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
  const int num_sub = FLAGS_nmodels;

  if (cache == nullptr) {
    // init
    cache = std::make_unique<XCache>(num_sub);

    // init sub
    for (uint i = 0; i < num_sub; ++i) {
      tts.emplace_back();
    }

    // train
    // 1. first layer
    {
      r2::Timer t;
      if (FLAGS_vlen) {
        cache->default_train_first<DBTreeIterV>(dbv);
      } else {
        cache->default_train_first<DBTreeIter>(db);
        LOG(4) << "train first layer done using: " << t.passed_sec() << " secs";
      }
    }
    // 2. second layer

    if (!FLAGS_vlen) {
      auto trainers = cache->dispatch_keys_to_trainers<DBTreeIter>(db);
      {
        CDF<int> error_cdf("");
        CDF<int> page_cdf("");

        usize null_model = 0;

        for (uint i = 0; i < trainers.size(); ++i) {
          auto &trainer = trainers[i];
          auto it = TrainIter::from_tt(db, &(tts[i]));

          // DefaultSample<XKey> s;
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

        LOG(4) << "Error data: " << "[average: " << error_cdf.others.average
               << ", min: " << error_cdf.others.min << ", max: " << error_cdf.others.max;
        LOG(4) << error_cdf.dump_as_np_data() << "\n";

        LOG(4) << "Page entries:" << page_cdf.dump_as_np_data() << "\n";

        LOG(4) << "total " << num_sub << " models; null models:" << null_model;
      }
    } else {
      auto trainers = cache->dispatch_keys_to_trainers<DBTreeIterV>(dbv);
      {
        CDF<int> error_cdf("");
        CDF<int> page_cdf("");

        usize null_model = 0;

        for (uint i = 0; i < trainers.size(); ++i) {
          auto &trainer = trainers[i];
          auto it = TrainIterV::from_tt(dbv, &(tts[i]));

          // DefaultSample<XKey> s;
          PS<XKey> s;
          StepSampler<XKey> ss(2);

          cache->second_layer[i] =
              trainer.train_w_it_w_shrink<TrainIterV, PS, StepSampler, LR>(
                  it, dbv, s, ss, page_updater);
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

        LOG(4) << "total " << num_sub << " models; null models:" << null_model;
      }
    }
    // done
  }
}


template<typename T>
static constexpr T
round_up(const T& num, const T& multiple)
{
  assert(multiple && ((multiple & (multiple - 1)) == 0));
  return (num + multiple - 1) & -multiple;
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
  tt_buf = round_up<u64>(tt_buf,sizeof(u64));
  cur_ptr = (char *)tt_buf;

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
