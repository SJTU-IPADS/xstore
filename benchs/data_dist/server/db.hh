#pragma once

#ifndef USE_NN
#define USE_NN 1
#endif
#define USE_MV 0

#include "../schema.hh"

#include "../../../xcache/src/submodel_trainer.hh"

#include "../../../xkv_core/src/xalloc.hh"
#include "../../../xkv_core/src/xtree/page_iter.hh"

#include "../../../xutils/cdf.hh"
#include "../../../xutils/file_loader.hh"
#include "../../../xutils/xy_data.hh"

#include "../../../deps/progress-cpp/include/progresscpp/ProgressBar.hpp"

#include "../../../deps/r2/src/random.hh"
#include "../../../deps/r2/src/timer.hh"

namespace xstore {

DEFINE_bool(skip_first, false, "whether to skip the first layer search");

DEFINE_bool(vlen, false, "whether to use variable length value");
DEFINE_int32(len, 8, "average length of the value");
DEFINE_int32(nmodels, 1, "number submodel used");
DEFINE_bool(verbose,
            false,
            "whether to dump prediction results from the model");

using namespace xcache;
using namespace xml;
using namespace xkv::xtree;
using namespace util;

// core DB
XAlloc<sizeof(DBTree::Leaf)>* xalloc = nullptr;
DBTree db;
DBTreeV dbv;
std::unique_ptr<XCache> cache = nullptr;
std::vector<XCacheTT> tts;

// avaliable serialze buf, main.cc init this
u64 val_buf = 0;
u64 model_buf = 0;
u64 tt_buf = 0;
u64 buf_end = 0;

#if USE_MAP
auto
load_map(const usize& nkeys) -> usize
{
  std::ifstream map("smap.txt");
  std::string line;

  auto prev_key = KK(0, 0);

  usize count = 0;
  while (getline(map, line)) {
    std::stringstream ss(line);
    float lon;
    float lat;

    ss >> lon >> lat;
    db.insert_w_alloc(KK(lat, lon), lat, *xalloc);
    //    ASSERT(KK(lat, lon) != prev_key);
    prev_key = KK(lat, lon);
    count++;
    if (count > nkeys) {
      break;
    }
  }
  LOG(4) << "Total Map data sz: " << db.sz_inner() << "B"
         << " w entries: " << count;
  return count;
}
#endif

#if USE_TPCC
auto
load_tpcc(const usize& nkeys, ::r2::util::FastRandom& rand)
{
  XYData<u64, u64> cdf;
  std::ifstream myfile("tpcc.txt");
  std::string line;

  usize count = 0;
  while (getline(myfile, line)) {
    // parse
    std::stringstream ss(line);
    u64 w;
    u64 d;
    u64 o;
    u64 c;

    ss >> w;
    ss >> d;
    ss >> o;
    ss >> c;
    //    LOG(4) << "encode: " << w << " " << d << " " << o << " " << c;
    //    KK(w, d, o, c).to_feature();
    db.insert_w_alloc(KK(w, d, o, c), w, *xalloc);
    cdf.add(KK(w, d, o, c).to_scalar(), count);

    count += 1;
    if (count >= nkeys) {
      break;
    }
  }

end:
  cdf.dump_as_np_data("tpcc_cdf.py");
  LOG(4) << "Total TPCC data sz: " << db.sz_inner() << "B"
         << " w entries: " << count;
  auto all = db.sz_all();
  for (auto s : all) {
    LOG(4) << "sz: " << s;
  }
  return count;
}
#endif

DEFINE_bool(load_from_file, false, "whether to load DB from the file");

#if USE_AR
auto
load_ar(const usize& nkeys) -> usize
{
  std::ifstream myfile("ar_r.txt");

  std::string line;
  usize count = 0;
  while (getline(myfile, line)) {
    // parse
    std::stringstream ss(line);
    u64 r;
    u64 p;
    char c;

    ss >> r;
    ss >> c;
    ASSERT(c == ',');
    ss >> p;

    db.insert_w_alloc(KK(p, r), r, *xalloc);

    count += 1;
    if (count >= nkeys) {
      break;
    }
  }
  return count;
}
#endif

auto
load_from_file(const usize& nkeys) -> usize
{
  //  FileLoader loader("lognormal_uni_100m.txt");
  FileLoader loader("log_data.txt");

  char* cur_val_ptr = reinterpret_cast<char*>(val_buf);

  for (usize i = 0; i < nkeys; ++i) {
    auto key = loader.next_key<u64>(FileLoader::default_converter<u64>);
    if (key) {
      auto k = key.value();
      db.insert_w_alloc(KK(k), k, *xalloc);
    } else {
      break;
    }
  }
  return nkeys;
}

const bool verbose = true;
CDF<double> err_data("key_err.py");

auto
page_updater(const u64& label,
             const u64& predict,
             const int& cur_min,
             const int& cur_max) -> std::pair<int, int>
{
  if (LogicAddr::decode_logic_id<kNPageKey>(predict) ==
      LogicAddr::decode_logic_id<kNPageKey>(label)) {
    return std::make_pair(cur_min, cur_max);
  }

  auto target_id = LogicAddr::decode_logic_id<kNPageKey>(label);
  if (LogicAddr::decode_logic_id<kNPageKey>(predict + cur_min) <= target_id &&
      LogicAddr::decode_logic_id<kNPageKey>(predict + cur_max) >= target_id) {
    return std::make_pair(cur_min, cur_max);
  }

  auto err = static_cast<i64>(label - predict);
  if (verbose) {
    err_data.insert(err);
  }
  if (err < cur_min || err > cur_max) {
    LOG(0) << "label: " << label << " ; predict: " << predict
           << "; err: " << static_cast<i64>(label - predict);
  }

  /*
    auto new_min = std::min(static_cast<i64>(cur_min),
                            static_cast<i64>(label) -
    static_cast<i64>(predict)); auto new_max =
    std::max(static_cast<i64>(cur_max), static_cast<i64>(label) -
    static_cast<i64>(predict));
                            */
  int step = 0;
  if (err < 0) {
    step = -1;
  } else {
    step = 1;
  }
  int gap = 0;

  while (LogicAddr::decode_logic_id<kNPageKey>(predict + gap) != target_id) {
    gap += step;
  }
  auto new_min = std::min(cur_min, gap);
  auto new_max = std::max(cur_max, gap);
  ASSERT(new_min <= cur_min && new_max >= new_max);

  return std::make_pair(new_min, new_max);
}

template<typename K>
using PS = PageSampler<kNPageKey, K>;

#define SAM DefaultSample
//#define SAM PS

auto
train_db(const std::string& config)
{
  // TODO: load model configuration from the file
  const int num_sub = FLAGS_nmodels;

  if (cache == nullptr) {
    // init
#if USE_LOG && USE_TNN
    XNN<KK>::NNN net(
      ::tiny_dnn::make_mlp<::tiny_dnn::activation::sigmoid>({ 1, 32, 1 }));

    cache = std::unique_ptr<XCache>(new XCache(num_sub, net));
    ::r2::util::FastRandom rand;
    for (uint i = 0; i < 1000; ++i) {
      cache->first_layer.predict_raw(KK(rand.next()));
    }
#else
    cache = std::make_unique<XCache>(num_sub);
#endif

    // init sub
    for (uint i = 0; i < num_sub; ++i) {
      tts.emplace_back();
    }

    // train
    // 1. first layer
    {
      ASSERT(cache != nullptr);
      r2::Timer t;

      StepSampler<KK> ss(1);

      if (!FLAGS_skip_first) {
        if (FLAGS_vlen) {

          // cache->train_first<DBTreeIterV, SS>(dbv,ss);
          cache->default_train_first<DBTreeIterV>(dbv);
        } else {
          // cache->default_train_first<DBTreeIter>(db);
          // cache->first_layer.train<DBTreeIter, SS>(db,ss);
          auto num = cache->train_first<DBTreeIter, StepSampler>(db, ss);
          LOG(4) << "train first layer done using: " << t.passed_sec()
                 << " secs; for: " << num << " keys";
        }
      } else {
        // load from file
        cache->load_first_from_file("xfirst", FLAGS_nkeys);
      }
    }
    // 2. second layer

    usize max = 0;
    int idx = -1;
    std::vector<std::map<KK, u64>> trainer_labels;

    if (!FLAGS_vlen) {
      auto trainers = cache->dispatch_keys_to_trainers<DBTreeIter>(db);
      {
        CDF<int> num_cdf("");
        CDF<int> error_cdf("");
        CDF<int> page_cdf("");
        CDF<int> nkeys_cdf("");

        XYData<int, int> nkeys_per_model;
        XYData<int, int> num_error;

        usize null_model = 0;

        progresscpp::ProgressBar bar(trainers.size(), 70);

        for (uint i = 0; i < trainers.size(); ++i) {
          auto& trainer = trainers[i];
          if (FLAGS_verbose) {
            SAM<KK> s;
            auto it = TrainIter::from_tt(db, &(tts[i]));

            std::map<KK, u64> mapping;
            const auto& tl =
              trainer.snapshpot_train_labels<TrainIter, SAM>(it, db, s);
            for (uint i = 0; i < std::get<0>(tl).size(); ++i) {
              mapping.insert(
                std::make_pair(std::get<0>(tl)[i], std::get<1>(tl)[i]));
            }
            trainer_labels.push_back(mapping);
          }

          if (trainer.nkeys_update > 0) {
            nkeys_cdf.insert(trainer.nkeys_update);
          } else {
            // LOG(4) << "trainer: " << i << " has zero keys";
          }

          auto it = TrainIter::from_tt(db, &(tts[i]));

          SAM<KK> s;
          StepSampler<KK> ss(1);

          trainer.set_name(std::to_string(i));
          cache->second_layer[i] =
            trainer.train_w_it_w_shrink<TrainIter, SAM, StepSampler, SML>(
              it, db, s, ss, page_updater, FLAGS_verbose);

          if (FLAGS_verbose) {
            nkeys_per_model.add(i, trainer.nkeys_update);
          }

          if (tts[i].size() != 0) {
            num_error.add(trainer.nkeys_update,
                          cache->second_layer[i]->total_error());
            error_cdf.insert(cache->second_layer[i]->total_error());
            //            LOG(4) << "total error:" <<
            //            cache->second_layer[i]->total_error();
            page_cdf.insert(tts[i].size());

            if (cache->second_layer[i]->max > max) {
              max = cache->second_layer[i]->max;
              idx = i;
            }
          } else {
            null_model += 1;
          }
          ++bar;
          // bar.display();
        }
        bar.done();
        error_cdf.finalize();
        page_cdf.finalize();
        nkeys_cdf.finalize();

        LOG(4) << "model: " << idx << " has max key: " << max
               << "; ratio: " << (double)max / FLAGS_nkeys;

        LOG(4) << "nkeys data: "
               << "[average: " << nkeys_cdf.others.average
               << ", min: " << nkeys_cdf.others.min
               << ", max: " << nkeys_cdf.others.max;
        LOG(4) << nkeys_cdf.dump_as_np_data() << "\n";

        LOG(4) << "Error data: "
               << "[average: " << error_cdf.others.average
               << ", min: " << error_cdf.others.min
               << ", max: " << error_cdf.others.max;
        LOG(4) << error_cdf.dump_as_np_data() << "\n";

        LOG(4) << "Page entries:" << page_cdf.dump_as_np_data() << "\n";

        LOG(4) << "total " << num_sub << " models; null models:" << null_model;
        num_error.finalize().dump_as_np_data("ne.py");

        if (FLAGS_verbose) {
          {
            std::ofstream out("key_err.py");
            out << err_data.finalize().dump_as_np_data();
            out.close();
          }
          err_data.clear();
          cache->dump_all<DBTreeSIter>(db);
          cache->dump_first<DBTreeSIter>(db);
          cache->dump_cdf<DBTreeSIter>(db);
          cache->dump_labels<DBTreeSIter>(db, trainer_labels);

          {
            std::ofstream out("key_num.py");
            out << nkeys_per_model.dump_as_np_data();
            out.close();
          }
        }
      }
    } else {
      auto trainers = cache->dispatch_keys_to_trainers<DBTreeIterV>(dbv);
      {
        CDF<int> error_cdf("");
        CDF<int> page_cdf("");

        usize null_model = 0;

        for (uint i = 0; i < trainers.size(); ++i) {
          auto& trainer = trainers[i];
          auto it = TrainIterV::from_tt(dbv, &(tts[i]));

          // DefaultSample<XKey> s;
          PS<KK> s;
          StepSampler<KK> ss(2);

          cache->second_layer[i] =
            trainer.train_w_it_w_shrink<TrainIterV, PS, StepSampler, SML>(
              it, dbv, s, ss, page_updater);

          if (tts[i].size() != 0) {
            error_cdf.insert(cache->second_layer[i]->total_error());
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

auto
serialize_db()
{
  char* cur_ptr = (char*)model_buf;
  for (uint i = 0; i < cache->second_layer.size(); ++i) {
    auto s = cache->second_layer[i]->serialize();
    ASSERT(s.size() + (u64)cur_ptr <= buf_end);
    memcpy(cur_ptr, s.data(), s.size());
    cur_ptr += s.size();
  }

  tt_buf = (u64)cur_ptr;
  tt_buf = round_up<u64>(tt_buf, sizeof(u64));
  cur_ptr = (char*)tt_buf;

  LOG(4) << "after seriaize, tt buf: " << tt_buf
         << "; model sz:  " << tt_buf - model_buf;

  // update the TT
  for (uint i = 0; i < tts.size(); ++i) {
    auto s = tts[i].serialize();
    ASSERT(s.size() + (u64)cur_ptr <= buf_end);
    memcpy(cur_ptr, s.data(), s.size());

    cur_ptr += s.size();
  }
  buf_end = (u64)cur_ptr;
}

} // namespace xstore
