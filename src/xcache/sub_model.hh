#pragma once

#include <mutex>

#include "../stores/naive_tree.hpp"
#include "../mega_iter.hpp"

// whether to shrink the training_set leveraging linear model's property
#define COMPACT 1

#define NON_EXIST 0

#define INVALID_PAGE_ID 0
#define INVALID_SEQ 0

namespace fstore {

const usize max_page_table_entry = 512;

template<typename Model, typename PageIter, typename Tree>
class SubModel
{
public:

  // This submodel is responsible for predicting keys from [start_key, end_key]
  u64 start_key = 0;
  u64 end_key   = 0;

  //bool need_train = false;
  u64  notify_watermark = 0;
  u64  train_watermark  = 0;

  //char padding[3];

  std::mutex lock;
  bool first_trained = false;

  // model for predict
  Model ml;

  // min-max error
  i32 min_error;
  i32 max_error;

  u32 max_entries;

  std::vector<u64> page_table;

  u16 seq = 1;

  SubModel() {
    //page_table.reserve(max_page_table_entry);
    page_table.clear();
  }

  SubModel(const SubModel& x) : min_error(x.min_error), max_error(x.max_error),
    max_entries(x.max_entries), ml(x.ml)
  {
    for(uint i = 0;i < x.page_table.size();++i) {
      this->page_table.push_back(x.page_table[i]);
    }
  }

  SubModel(u64 s, u64 end) : start_key(s), end_key(end) {
    //page_table.reserve(max_page_table_entry);
    page_table.clear();
  }

  u64 total_sz() const {
    return sizeof(float) + sizeof(float) +
      sizeof(i8) + sizeof(i8)
      + sizeof(u32) + page_table.size() * sizeof(u64);
  }

  u64 page_table_sz() const {
    return page_table.size() * sizeof(u64);
  }

  u64 model_sz() const {
    return sizeof(float) + sizeof(float) + sizeof(i8) * 2 + sizeof(u32);
  }

  bool eq(SubModel &s) {
    if (!s.ml.eq(ml)) {
      return false;
    }

    if (this->min_error != s.min_error) {
      LOG(4) << "min error error: " << this->min_error << " " << s.min_error;
      return false;
    }

    if (this->max_error != s.max_error) {
      return false;
    }

    if (this->max_entries != s.max_entries) {
      return false;
    }

    if (this->page_table.size() != s.page_table.size()) {
      return false;
    }

    for (uint i = 0; i < this->page_table.size(); ++i) {
      if (this->page_table[i] != s.page_table[i] ) {
        return false;
      }
    }
    return true;
  }

  // must hold the lock
  void reset_keys(u64 s, u64 e) {
    //this->lock.lock();
    this->start_key = s;
    this->end_key   = e;
    //this->need_train = true;
    //this->lock.unlock();
  }

  i64 get_predict(const u64 &key) {
    return static_cast<i64>(std::max(ml.predict(key), 0.0));
  }

  std::pair<u64, u64> get_page_span(const u64 &key) {

    auto predict = static_cast<i64>(std::max(ml.predict(key),0.0));

    //LOG(4) << "predict: "<< predict << " ";
#if NON_EXIST
    auto p_start = std::min(
      std::max(static_cast<i64>(predict + min_error - 1), static_cast<i64>(0)),
      static_cast<i64>(this->max_entries));
    auto p_end = std::min(
      std::max(static_cast<i64>(predict + max_error + 1), static_cast<i64>(0)),
      static_cast<i64>(this->max_entries));
#else
    auto p_start = std::min(std::max(static_cast<i64>(predict + min_error), static_cast<i64>(0)),static_cast<i64>(this->max_entries));
    auto p_end   = std::min(std::max(static_cast<i64>(predict + max_error),static_cast<i64>(0)), static_cast<i64>(this->max_entries));
#endif

    if (this->min_error == 0 && this->max_error == 0 && 0) {
      LOG(4) << "predict range: " << p_start << " " << p_end
             << " for key: " << key << " pos: " << predict
             << " max: " << this->max_entries << " [" << p_start << ":" << p_end
             << "]"
             << " " << this->min_error << " " << this->max_error;
      ASSERT(p_end >= p_start)
        << p_start << " " << p_end << "; predict: " << predict
        << ", min max:" << this->min_error << " " << this->max_error;
    }

    // decode the page addr
    return std::make_pair(p_start / IM, p_end / IM);
  }

  u64 lookup_page_phy_addr(const u64 &page_id) {
    auto ret = lookup_page_entry(page_id).value();
    return SeqEncode::decode_id(ret);
  }

  void invalidate_page_entry(const u64 &page_id) {
    ASSERT(page_id < page_table.size())
      << "page id: " << page_id << "; total: " << page_table.size()
      << " max: " << this->max_entries;
    page_table[page_id] = INVALID_PAGE_ID;
  }

  Option<u64> lookup_page_entry(const u64 &page_id) {
    //ASSERT(page_id < page_table.size()) << "page id: " << page_id << "; total: " << page_table.size() << " max: " << this->max_entries;
    if (page_id >= page_table.size()) {
      return {};
    }
    return page_table[page_id];
  }

  usize num_keys_responsible = 0;

  bool train(Tree& t, int step, Option<usize> dump = {})
  {
    num_keys_responsible = 0;
#if 1
    if (this->notify_watermark == this->train_watermark) {
      return  false;
    }
#endif
    auto prev_seq = this->seq;
    this->seq = 0;
    r2::compile_fence();

    //this->lock.lock();
    //this->lock.unlock();

    page_table.clear();

    ::fstore::utils::DataMap<u64, u64> train_space("");
    ::fstore::utils::DataMap<u64, double> predict_space("");

    // take a snapshot of the current node
    PageIter pt(t, start_key);
    ASSERT(pt.valid());
    auto start_node = (pt.value());

    auto end_node = t.safe_find_leaf_page(this->end_key);
    //LOG(4) << "start node -> end_node " << start_node << " " << end_node;
    i32 cur_page_id = 0;

    // prepare the training-set
    std::vector<u64> training_data;
    std::vector<u64> training_label;

#define MAGIC 0

#if MAGIC
    // magic
    {
      auto gap = (end_key - start_key) / IM * 2;
    }
#endif

    usize total_keys_responsible;
#if !MAGIC
    for (auto node = start_node; pt.valid() ; pt.next()) {
      //ASSERT(node != nullptr) << " start -> end: " << start_node << " " << end_node << " [ " << this->start_key << " : " << this->end_key << "]";
      ASSERT(pt.valid());
      //LOG(4) << "iter node: " << node;
    retry:
      node = pt.value();
      total_keys_responsible += node->num_keys;

      auto node_v = *node;
      node_v.sanity_check();

#if !COMPACT
      for(uint i = 0; i < node_v.num_keys;++i) {
        training_data.push_back(static_cast<double>(node_v.keys[i]));
        training_label.push_back(cur_page_id * IM + i);
        if (dump) {
          train_space.insert(node_v.keys[i], cur_page_id * IM + i);
        }
      }
#else
      training_data.push_back(node_v.start_key());
      training_label.push_back(cur_page_id * IM);
      if (node_v.num_keys > 1) {
        training_data.push_back(node_v.end_key());
        training_label.push_back(cur_page_id * IM + node_v.num_keys - 1);
      } else {
        ASSERT(false);
      }

#endif
      num_keys_responsible += node_v.num_keys;
      cur_page_id += 1;

      this->max_entries = cur_page_id * IM - 1;
      ASSERT(pt.key() != 0);
      auto entry = SeqEncode::encode(1, node_v.seq, pt.key());
      ASSERT(entry != INVALID_PAGE_ID);

      r2::compile_fence();
      if (unlikely(node_v.seq != node->seq)) {
        goto retry;
      }

      //if (page_table.size() < max_page_table_entry) {
        this->page_table.push_back(entry);
        //}

      if (node == end_node)
        break;
    }
#endif
    ASSERT(training_data.size() == training_label.size())
      << " data sz: " << training_data.size() << " " << training_label.size();
    ASSERT(training_data.size() > 1);

    //LOG(4) << "training-set sz: "<< training_data.size();

    // training-data done, now train
    ml.train(training_data, training_label, step);
    if (training_data.size() > 100) {
      //ASSERT(false) << start_key << " " << end_key << " " << training_data.size();
    }

    // calculate the min_max
    this->min_error = 0;
    this->max_error = 0;

    bool check_flag = false;

    bool whether_dump = false;

    for (uint i = 0;i < training_data.size();++i) {
      auto predicted_pos = static_cast<i64>(ml.predict(training_data[i]));
      if (predicted_pos < 0) {
        predicted_pos = 0;
      }

      int error = 0;

      if ((static_cast<i64>(predicted_pos) / IM) != training_label[i] / IM) {
#if 1
        if (1) {
          this->min_error =
            std::min(static_cast<i64>(this->min_error),
                     static_cast<i64>(training_label[i]) - predicted_pos);
          this->max_error =
            std::max(static_cast<i64>(this->max_error),
                     static_cast<i64>(training_label[i]) - predicted_pos);
        }
#else
        if (predicted_pos < training_label[i]) {
          while (static_cast<i64>((predicted_pos) + error) / IM != (training_label[i] / IM)) {
            error += 1;
          }
          ASSERT(error <= static_cast<i64>(training_label[i]) -
                 predicted_pos)
            << "predicted pos: " << predicted_pos << " " << "label: " << training_label[i] << " " << error;

          this->max_error = std::max(static_cast<i64>(this->max_error),
                                     static_cast<i64>(error));
          this->min_error = std::min(static_cast<i64>(this->min_error), static_cast<i64>(0));

        } else {
          while ((static_cast<i64>((predicted_pos) + error) / IM) !=
                 (training_label[i] / IM)) {
            error -= 1;
          }
          ASSERT(error >= static_cast<i64>(training_label[i]) - predicted_pos);
          this->min_error = std::min(static_cast<i64>(this->min_error),
                                     static_cast<i64>(error));
          this->max_error = std::max(static_cast<i64>(this->max_error), static_cast<i64>(0));
        }
#endif
      } else {
        // 0 error case
#if 0
        this->min_error =
          std::min(static_cast<i64>(this->min_error),
                   static_cast<i64>(0));
        this->max_error =
          std::max(static_cast<i64>(this->max_error),
                   static_cast<i64>(0));
#endif
      }

#if 0
      if (training_data[i] == 2174970225) {
        LOG(4) << "predict label for the key: " << training_label[i];
      }
#endif

      if (dump) {
        //predict_space.insert(training_data[i], predicted_pos);
        predict_space.insert(training_data[i], ml.predict(training_data[i]));

      }
    }

    if (this->min_error == std::numeric_limits<i32>::max()) {
      this->min_error = 0;
    }
    if (this->max_error == std::numeric_limits<i32>::min()) {
      this->max_error = 0;
    }

    if (check_flag) {
      LOG(4) << "check min max error: "<< this->min_error << " " << this->max_error;
    }

#if 0
    if (this->min_error == 0) {
      this->min_error -= 1;
    }
    if (this->max_error == 0) {
      this->max_error += 1;
    }
#endif
    //this->min_error -= 1;
    //this->max_error += 1;

    if (dump) {
      if (dump.value() < 10) {
      //if (this->max_error - this->min_error > 20) {
      //if (total_keys_responsible >= 100 && total_keys_responsible < 300) {

        FILE_WRITE(std::to_string(dump.value()) + "_train_space.py",
                   std::ofstream::out)
          << train_space.dump_as_np_data();
        FILE_WRITE(std::to_string(dump.value()) + "_predict_space.py",
                   std::ofstream::out)
          << predict_space.dump_as_np_data();
        LOG(4) << "model: " << dump.value()
               << " min max error: " << this->min_error << " "
               << this->max_error;
        whether_dump = true;
      }
        //}
    }

    //this->max_entries = training_data.size();

#if 0
    this->lock.lock();
    // update the start/end key
    if (training_data.size() > 0) {
      this->update_start_end_key(training_data[0]);
      this->update_start_end_key(training_data[training_data.size() - 1]);
    }
    this->lock.unlock();
#endif
    r2::compile_fence();
    this->train_watermark += 1;
    r2::compile_fence();
    //this->seq = 0;
    this->seq = prev_seq + 1;

    return whether_dump;
  }

  void update_start_end_key(const u64 &k) {
    //this->lock.lock();
    if (this->first_trained) // has already been trained
    {
      this->start_key = std::min(this->start_key, k);
      this->end_key = std::max(this->end_key, k);
    } else {
      this->start_key = k;
      this->end_key   = k;
      this->first_trained = true;
    }
    //this->lock.unlock();
  }
};

}
