#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdlib>
#include <forward_list>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "rmi.h"

#if !defined(LEARNED_INDEX_H)
#define LEARNED_INDEX_H

#if !defined(COUT_THIS)
#define COUT_THIS(this) std::cerr << this << std::endl
#endif // COUT_THIS

struct Predicts
{
#if 1
  learned_addr_t pos;
  learned_addr_t start;
  learned_addr_t end;
#else
  int pos;
  int start;
  int end;
#endif
  friend std::ostream& operator<<(std::ostream& output, const Predicts& p)
  {
    output << "(" << p.start << "," << p.pos << "," << p.end << ")";
    return output;
  }
};

template<class Val_T, class Weight_T>
class LearnedRangeIndexSingleKey
{
public:
  LearnedRangeIndexSingleKey(const RMIConfig& rmi_config)
    : rmi(rmi_config)
  {}

  LearnedRangeIndexSingleKey(const std::vector<std::string>& first,
                             const RMIConfig& rmi_config) : rmi(first,rmi_config) {

  }

  LearnedRangeIndexSingleKey(const std::vector<std::string>& first,
                             const std::vector<std::string>& second,
                             uint64_t num,
                             uint64_t key_n = 0)
    : rmi(first, second)
    , sorted_array_size(num)
  {
    rmi.key_n = key_n;
  }

  void reset() { sorted_array.clear(); }

  ~LearnedRangeIndexSingleKey()
  {
    // for (auto& head : sorted_array) {
    //   if(head.is_val == false) {
    //     delete head.val_or_ptr.next;
    //   }
    // }
  }

  LearnedRangeIndexSingleKey(const LearnedRangeIndexSingleKey&) = delete;
  LearnedRangeIndexSingleKey(LearnedRangeIndexSingleKey&) = delete;

  void insert(const uint64_t key, const Val_T value)
  {
    // Record record = {.key = static_cast<uint64_t>(key), .value = value};
    Record record = { .key = key, .value = value };
    sorted_array.push_back(record);
    rmi.insert(static_cast<double>(key));
  }

  void insert(const uint64_t key, const Val_T value, learned_addr_t addr)
  {
    Record record = { .key = key, .value = value };
    sorted_array.push_back(record);
    rmi.insert_w_idx(static_cast<double>(key), addr);
  }

  void finish_insert(bool train_first = true)
  {
    struct RecordComparitor
    {
      bool operator()(Record i, Record j) { return i.key < j.key; }
    } comp;

    sort(sorted_array.begin(), sorted_array.end(), comp);
    sorted_array_size = sorted_array.size();
    rmi.finish_insert(train_first);

    // for (auto &kv : data) {
    //   rmi.insert(kv.first);
    //   sorted_array.push_back(kv.second);
    // }
    // rmi.finish_insert();
    // data.clear();
  }

  void finish_train() { rmi.finish_train(); }

  Predicts predict(const double key)
  {
    Predicts res;
    rmi.predict_pos(key, res.pos, res.start, res.end);
    // std::cout << "get predict: " << res << " for key: "<< key << "; total key
    // n : " << rmi.key_n;
    res.start = std::min(std::max(res.start, static_cast<int64_t>(0)),
                         static_cast<int64_t>(rmi.key_n));
    res.end = std::min(res.end, static_cast<int64_t>(rmi.key_n));
    res.pos = std::max(res.pos, static_cast<int64_t>(0));
    if(!(res.end >= 0 && res.end <= rmi.key_n)) {
      fprintf(stderr,"%ld get res end\n",res.end);
      assert(false);
    }
    return res;
  }

  inline Predicts predict_w_model(const double& key, const unsigned& model)
  {
    Predicts res;
    rmi.predict_pos_w_model(key, model, res.pos, res.start, res.end);
    return res;
  }

  inline LinearRegression &get_lr_model(const unsigned &model) const {
    auto second_stage = reinterpret_cast<LRStage *>(rmi.second_stage);
    return second_stage->models[model];
  }

  int predict_pos(const double key)
  {
    Predicts res;
    int pos, start, end;
    rmi.predict(key, pos, start, end);
    res.pos = pos;
    // res.start = std::max(res.start,0);
    // res.end   = std::min(res.end,sorted_array_size);
    return res.pos;
  }

  /*!
    return which keys belong to this model
   */
  int get_model(const double key)
  {
    // TODO: not implemented
    return rmi.pick_model_for_key(key);
  }

  learned_addr_t get_logic_addr(const double key)
  {
    // get position prediction and fetch model errors
    learned_addr_t pos, start, end, mid;
    rmi.predict_pos(key, pos, start, end);

    // bi-search positions should be valid
    start = start < 0 ? 0 : start;
    end = end > sorted_array_size ? sorted_array_size : end;
    mid = pos >= start && pos < end ? pos : (start + end) / 2;

    // use binary search to locate record
    while (end > start) {
      // printf("check key: %lu at : %d,from %d -> %d (%lu ->
      // %lu)\n",sorted_array[mid].key,mid,
      //     start,end,
      // sorted_array[start].key,sorted_array[end].key);
      if (sorted_array[mid].key == key) {
        return mid;
      }

      if (sorted_array[mid].key > key) {
        end = mid;
      } else {
        start = mid + 1;
      }

      mid = (start + end) >> 1;
    }

    COUT_THIS("bi-search failed!");
    assert(0); // not found!
  }

  Val_T get(const double key)
  {
    // get position prediction and fetch model errors
    learned_addr_t pos, start, end, mid;
    rmi.predict_pos(key, pos, start, end);

    // bi-search positions should be valid
    start = start < 0 ? 0 : start;
    end = end > sorted_array_size ? sorted_array_size : end;
    mid = pos >= start && pos < end ? pos : (start + end) / 2;

    // use binary search to locate record
    while (end > start) {
      if (sorted_array[mid].key == key) {
        return sorted_array[mid].value;

        // if (sorted_array[mid].is_val) {
        //   return sorted_array[mid].val_or_ptr.val;
        // } else {  // return the oldest value
        //   return sorted_array[mid].val_or_ptr.next->val;
        // }
      }

      if (sorted_array[mid].key > key) {
        end = mid;
      } else {
        start = mid + 1;
      }

      mid = (start + end) >> 1;
    }

    //COUT_THIS("bi-search failed! for key");
    std::cerr << "bi-search failed for key: " << key;
    assert(0); // not found!
  }

  Val_T *get_as_ptr(const double key)
  {
    // get position prediction and fetch model errors
    learned_addr_t pos, start, end, mid;
    rmi.predict_pos(key, pos, start, end);

    // bi-search positions should be valid
    start = start < 0 ? 0 : start;
    end = end > sorted_array_size ? sorted_array_size : end;
    mid = pos >= start && pos < end ? pos : (start + end) / 2;

    // use binary search to locate record
    while (end > start) {
      if (sorted_array[mid].key == key) {
        return &sorted_array[mid].value;

        // if (sorted_array[mid].is_val) {
        //   return sorted_array[mid].val_or_ptr.val;
        // } else {  // return the oldest value
        //   return sorted_array[mid].val_or_ptr.next->val;
        // }
      }

      if (sorted_array[mid].key > key) {
        end = mid;
      } else {
        start = mid + 1;
      }

      mid = (start + end) >> 1;
    }

    //COUT_THIS("bi-search failed! for key");
    std::cerr << "bi-search failed for key: " << key;
    assert(0); // not found!
    return nullptr;
  }


  Val_T binary_search(const double key, int pos, int start, int end)
  {
    // bi-search positions should be valid
    start = start < 0 ? 0 : start;
    end = end > sorted_array_size ? sorted_array_size : end;
    int mid = pos >= start && pos < end ? pos : (start + end) / 2;

    // use binary search to locate record
    while (end > start) {
      if (sorted_array[mid].key == key) {
        return sorted_array[mid].value;
      }

      if (sorted_array[mid].key > key) {
        end = mid;
      } else {
        start = mid + 1;
      }

      mid = (start + end) >> 1;
    }

    COUT_THIS("bi-search failed! key:" << key << " " << int64_t(key));
    assert(0); // not found!
  }

private:
  struct Trail
  {
    Val_T val;
    Trail* next;
  };

  struct Head
  {
    double key = 0.0;
    union
    {
      Val_T val;
      Trail* next;
    } val_or_ptr;
    bool is_val = false;
  };

#if 0
  struct Record {
    double key;
    Val_T value;
  };
#endif
  struct Record
  {
    uint64_t key;
    Val_T value;
  };

public:
  uint64_t sorted_array_size = 0;

#ifdef MixTop
  RMIMixTop<Weight_T> rmi;
#else
  RMINew<Weight_T> rmi;
#endif
  std::vector<Record> sorted_array;
  // std::map<double, Head> data;  // valid during inserts
};

// template <class Val_T>
// class LearnedSeparateChainingHashMapSingleKey {
//  public:
//   LearnedSeparateChainingHashMapSingleKey(const int bucket_num)
//       : buckets(std::vector<std::forward_list<Record>>(bucket_num)),
//         bucket_num(bucket_num) {}

//   void insert(const double key, const Val_T value) {
//     temp_store.insert(std::pair<double, Val_T>(key, value));
//     rmi_p->insert(key);
//   }

//   void finish_insert() {
//     rmi_p->finish_insert();

//     for (auto it = temp_store.begin(); it != temp_store.end(); ++it) {
//       double key = it->first;
//       Val_T value = it->second;
//       std::forward_list<Record> &bucket = hash_to_bucket(key);
//       bucket.emplace_front(key, value);
//     }

//     temp_store.clear();
//   }

//   Val_T get(const double key) {
//     std::forward_list<Record> &bucket = hash_to_bucket(key);
//     for (auto it = bucket.begin(); it != bucket.end(); ++it) {
//       if (it->key == key) return it->value;
//     }

//     assert(0);
//   }

//  protected:
//   std::unique_ptr<RMI> rmi_p;

//  private:
//   struct Record {
//     double key;
//     Val_T value;
//     Record(double key, Val_T value) : key(key), value(value) {}
//   };

//   std::forward_list<Record> &hash_to_bucket(double key) {
//     double cdf = rmi_p->predict_cdf(key);
//     int bucket = cdf * bucket_num;
//     while (bucket < 0) bucket += bucket_num;
//     return buckets[bucket % bucket_num];
//   }

//   const int bucket_num;
//   std::multimap<double, Val_T> temp_store;  // not valid after calling
//   std::vector<std::forward_list<Record>> buckets;
// };

#endif // LEARNED_INDEX_H
