#pragma once

#include "../../x_ml/src/lib.hh"
#include "../../xkv_core/src/iter_trait.hh"

#include "./sample_trait.hh"

#include "../../xutils/marshal.hh"

namespace xstore {

namespace xcache {

using namespace xstore::xkv;

/*!
  The dispatcher will route a key to some number between [1,n),
  using an ML model.
 */
template<template<typename> class ML, typename KeyType>
struct Dispatcher
{
  ML<KeyType> model;
  usize dispatch_num;
  u32 up_bound = 0;

  explicit Dispatcher(const usize& dn)
    : dispatch_num(dn)
    , model()
  {
    ASSERT(dispatch_num > 0 && dispatch_num < 4000000)
      << " dispatch number must be larger than zero";
  }

  template<class... Args>
  Dispatcher(const usize& dn, Args... args)
    : dispatch_num(dn)
    , model(args...)
  {}

  explicit Dispatcher(const ::xstore::string_view& d, bool from_file = false)
    : model()
  {
    {
      char* cur_ptr = (char*)d.data();
      this->dispatch_num =
        ::xstore::util::Marshal<u32>::deserialize(cur_ptr, d.size());
      cur_ptr += sizeof(u32);

      this->up_bound =
        ::xstore::util::Marshal<u32>::deserialize(cur_ptr, d.size());
      cur_ptr += sizeof(this->up_bound);

      ASSERT(d.size() > sizeof(u32) + sizeof(this->up_bound));

      auto next = xstore::string_view(
        cur_ptr, d.size() - sizeof(u32) - sizeof(this->up_bound));

      if (!from_file) {
        // load directly from the buffer

        // sanity check the sz
        ASSERT(d.size() == this->serialize().size())
          << "expect sz: " << this->serialize().size() << "; d sz: " << d.size()
          << " " << d;
        this->model_from_serialize(next);
      } else {
        this->model.from_file_view(next);
      }
    }
  }

  void load_from_file(const std::string& name, const u64& up)
  {
    this->model.from_file_view(name);
    this->up_bound = up;
  }

  void model_from_serialize(const ::xstore::string_view& s)
  {
    this->model.from_serialize(s);
  }

  auto predict(const KeyType& key, const u32& max) -> usize
  {
    auto res = this->model.predict(key);
    if (res < 0) {
      res = 0;
    }
    if (res >= max) {
      res = this->dispatch_num - 1;
    } else {
      res = (static_cast<double>(res) / max) * this->dispatch_num;
    }
    return static_cast<usize>(res);
  }

  auto predict_raw(const KeyType& key) -> double
  {
    return this->model.predict(key);
  }

  auto predict_verbose(const KeyType& key, const u32& max) -> usize
  {
    auto res = this->model.predict(key);
    LOG(4) << "predict " << key << "; "
           << "res:" << res;
    return this->predict(key, max);
  }

  /*!
    Train the dispatcher with the KV
    The KV must implements the KeyIterTrait.
    S must implement the SampleTrait.

    \ret how many keys trained
   */
  template<class IT, template<typename> class S>
  auto train(typename IT::KV& kv, S<KeyType>& s) -> usize
  {
    std::vector<KeyType> train_set;
    std::vector<u64> train_label;

    // 1. fill in the train_set, train_label
    auto it = IT::from(kv);
    // FIXME: assumes the totoal KVS is smaller than usize's limit
    usize count = 0;
    for (it.begin(); it.has_next(); it.next()) {
      // filter whether we should ad the key
      s.add_to(it.cur_key(), count, train_set, train_label);
      count += 1;
    }
    up_bound = count;
    // LOG(4) << "check trainset: " << train_set[train_set.size() -1] << " " <<
    // train_label[train_label.size() -1];

    // 2. train the ml model
    this->model.train(train_set, train_label);

    return train_set.size();
  }

  // the default sampler will train all the keys
  template<class IT>
  auto default_train(typename IT::KV& kv) -> usize
  {
    DefaultSample<KeyType> s;
    return this->train<IT, DefaultSample>(kv, s);
  }

  auto serialize() -> std::string
  {
    return this->meta_serailize() + this->model.serialize();
  }

  auto serialize_to_file(const std::string& name) -> std::string
  {
    return this->meta_serailize() + this->model.serialize_to_file(name);
  }

  auto meta_serailize() -> std::string
  {
    std::string res;

    // first serialize the dispatch_num
    res += ::xstore::util::Marshal<u32>::serialize_to(this->dispatch_num);
    res += ::xstore::util::Marshal<u32>::serialize_to(this->up_bound);
    return res;
  }

  friend std::ostream& operator<<(std::ostream& out, const Dispatcher& k)
  {
    return out << "Dispatcher model: " << k.model << "; dispatch num "
               << k.dispatch_num << "; up_bound:" << k.up_bound;
  }
};

} // namespace xcache

} // namespace xstore
