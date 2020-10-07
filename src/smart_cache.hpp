#pragma once

#include "mega_iter.hpp"
#include "mega_pager_v2.hpp"

#include "page_fetch.hpp"

#include "stores/mod.hpp"

#include "./marshal.hpp"
#include "./model_serialize.hpp"

namespace fstore {

struct LidxDesc
{
  u64 total_keys;
  u64 key_n;
  u64 first_stage_buf_sz;
  u64 second_stage_buf_sz;
};

/*!
  Smart cache is a cache which predicts key-value position using learned index.
  \note: currently we only support u64 as the key type,
  and we assume use the MegaPageV for indirection to a KV.

  \param K: the type of key. now only u64 is supported.
  \param V: the type of value.
  \param PAGE_TYPE: the underlying data structure to store the real data.
  should be something like Page<K,V>.
  \param PAGEAlloc: the allocator for assigning pages.
 */
template<typename K,
         typename V,
         typename PAGE_TYPE,
         template<class>
         class PAGEAlloc>
class SCache
{
  usize pre_trained_keys = 0;
public:
  // char is just a dummy place holder. We donot store any values in learned
  // index.
  using Learned_index_t = LearnedRangeIndexSingleKey<K, char>;
  using Fetcher = PageFetcher<K, V, PAGE_TYPE, PAGEAlloc>;
  using Tree = ::fstore::store::NaiveTree<K, V, PAGEAlloc>;
  using PageStream = ::fstore::store::BPageStream<K, V, PAGEAlloc>;

  //std::shared_ptr<Learned_index_t>        index;
  //std::shared_ptr<MegaPagerV<PAGE_TYPE>>  mp;
  Learned_index_t *index;
  MegaPagerV<PAGE_TYPE> *mp;
  // TODO: may add a bloom filter

  SCache(const RMIConfig& rmi)
    //: index(std::make_shared<Learned_index_t>(rmi))
    //, mp(std::make_shared<MegaPagerV<PAGE_TYPE>>(1024))
    : index(new Learned_index_t(rmi)),
      mp(new MegaPagerV<PAGE_TYPE>(1024))
  {
  }

  SCache(const std::vector<std::string>& first,
         const std::vector<std::string>& second,
         u64 num,
         u64 key_n,
         const std::string& mega)
    //: index(std::make_shared<Learned_index_t>(first, second, num, key_n))
    //, mp(std::make_shared<MegaPagerV<PAGE_TYPE>>(mega))
    :index(new Learned_index_t(first,second,num,key_n)),
     mp(new MegaPagerV<PAGE_TYPE>(mega))
  {
    //LOG(4) << "new SCache with page entries: " << mp->total_num() << "; second stage num: "
    //<< second.size();
  }

  ~SCache() {
    delete index;
    delete mp;
  }

  /*!
    Given a logic addr, return the *local* stored physical page
  */
  PAGE_TYPE* get_page(u64 logic_addr) const
  {
    auto physical_id = mp->mapped_page_id(mp->decode_mega_to_entry(logic_addr));
    return PAGEAlloc<PAGE_TYPE>::get_page_by_id(physical_id);
  }

  /*!
    Check two predicts, verify if
    [p0.start,p0.end] ∩ [p1.start,p1.end] == ∅.
    Note that we assume that p0 <= P1
   */
  inline bool intersect(const Predicts& p0, const Predicts& p1)
  {
    if (mp->decode_mega_to_entry(p0.end) < mp->decode_mega_to_entry(p1.start)) {
      return false;
    }
    return true;
  }

  void augment(Tree& t)
  { //
    // XD: fixme: the 0 should be replace to something like K::min();
    //
    LOG(4) << "retrain using augment";

    PageStream stream(t, 0);
    for (stream.begin(); stream.valid(); stream.next()) {
      auto page = stream.value();
      ASSERT(page != nullptr);
      page->sanity_check();

      auto first_key_model_id = index->rmi.pick_model_for_key(page->keys[0]);
      for (uint i = 1; i < page->num_keys; ++i) {
        auto model = index->rmi.pick_model_for_key(page->keys[i]);
        if (model != first_key_model_id) {
          index->rmi.augment_model(page->keys[i], first_key_model_id);
        }
      }
      // augment the previous page / next page
      auto prev = page->left;
      if (prev) {
        auto model = index->rmi.pick_model_for_key(prev->end_key());
        if (model != first_key_model_id)
          index->rmi.augment_model(prev->end_key(), first_key_model_id);
      }
      auto next = page->right;
      if (next) {
        auto model = index->rmi.pick_model_for_key(next->start_key());
        if (model != first_key_model_id)
          index->rmi.augment_model(next->start_key(), first_key_model_id);
      }
    }
  }

  std::pair<std::shared_ptr<Learned_index_t>,
            std::shared_ptr<MegaPagerV<PAGE_TYPE>>>
  retrain(RMIConfig &model,
          datastream::StreamIterator<page_id_t, PAGE_TYPE*>* iter,
          Tree& t)
  {
    // first we adjust the stage configs using the pre_trained keys
    //if (0) {
    if(1) {
      // warning!! this code is very hard-coded
      // and the magic number is calculated based on experiments
      // todo: making it more automatic

      //double magic_ratio = 0.4;
      double magic_ratio = 0.01;
      usize magic_increase_num = 10000;
      usize magic_max_num = 4000000;

      // make some sanity checks to allow direct (brute-force) modifications
      assert(model.stage_configs.size() == 2);
      auto estimated_models = static_cast<usize>(std::ceil(magic_ratio * (pre_trained_keys)));
      estimated_models = ::fstore::utils::round_up(estimated_models,static_cast<usize>(4096));
      auto real_models =
        std::max(model.stage_configs[1].model_n, estimated_models);
      real_models = std::min(real_models,magic_max_num);
      LOG(4) << "retrain adjust model from: " << model.stage_configs[1].model_n << " -> " << real_models;
      model.stage_configs[1].model_n = real_models;
    }
    auto new_index = std::make_shared<Learned_index_t>(model);
    auto new_mp = std::make_shared<MegaPagerV<PAGE_TYPE>>(1024);

    MegaStream<PAGE_TYPE> it(iter, new_mp.get());

    u64 num = 0;
    u64 max_key = 0;
    for (it.begin(); it.valid(); it.next(),num += 1) {
      new_index->insert(it.key(), 0, it.value());
      // index.insert(it.key(),0);
      max_key = std::max(max_key,it.key());
    }
    LOG(4) << "total " << num << " keys retrained, max key: " << max_key;
    pre_trained_keys = num;

    new_index->finish_insert();

    // TODO: augmenting process to it
    new_index->finish_train();

    // make some sanity checks
    for (uint i = 0; i < 5; ++i) {
      auto& model = new_index->get_lr_model(i);
      LOG(3)
        << "check model error: " << model.min_error <<  " " << model.max_error;
    }

    return std::make_pair(new_index, new_mp);
  }

  void retrain_with_known_model(datastream::StreamIterator<page_id_t, PAGE_TYPE*>* iter, Tree& t) {
    // first we init the mapping
    mp->reset();

    MegaStream<PAGE_TYPE> it(iter, mp);
    // MegaStream<PAGE_TYPE> it(iter, mp.get());

    u64 total_keys = 0;
    for (it.begin(); it.valid(); it.next()) {
      total_keys += 1;
      it.value();
    }

    // then we load the model
    std::ifstream meta;
    meta.open("meta", std::ios::in | std::ios::binary);

    LidxDesc desc;
    meta.read((char *)(&desc), sizeof(LidxDesc));
    //ASSERT(total_keys == desc.total_keys);

    char *first_stage = new char[desc.first_stage_buf_sz];
    char *second_stage = new char[desc.second_stage_buf_sz];

    // read from the file
    std::ifstream fst_file;
    fst_file.open("fst", std::ios::in | std::ios::binary);
    fst_file.read(first_stage, desc.first_stage_buf_sz);

    std::ifstream snd_file;
    snd_file.open("snd", std::ios::in | std::ios::binary);
    snd_file.read(second_stage, desc.second_stage_buf_sz);

    auto fst = ModelDescGenerator::deserialize_one_stage(first_stage);
    auto snd = ModelDescGenerator::deserialize_one_stage(second_stage);

    this->index = new Learned_index_t(fst,snd, desc.total_keys,desc.key_n);

    delete first_stage; delete second_stage;
  }

  void retrain(datastream::StreamIterator<page_id_t, PAGE_TYPE*>* iter, Tree& t)
  {
    mp->reset();

    MegaStream<PAGE_TYPE> it(iter, mp);
    //MegaStream<PAGE_TYPE> it(iter, mp.get());

    for (it.begin(); it.valid(); it.next()) {
      index->insert(it.key(), 0, it.value());
      // index.insert(it.key(),0);
    }
    LOG(4) << "Trained with: " << index->sorted_array.size() << " tuples";

    index->finish_insert();

    /*
      XD: Be careful: there is a race condition here:
      If there is concurrent inserts, then the dataset scanned by the augment
      may be different. We must keep a static mapping after finish insert, and
      use it to augument.
    */
    //augment(t);

    // TODO: augmenting process to it
    index->finish_train();

    for (uint i = 0; i < std::min((unsigned int)5,index->rmi.second_stage->get_model_n()); ++i) {
      auto& model = index->get_lr_model(i);
      LOG(3) << "check model error: " << model.min_error << " "
             << model.max_error
             <<"; check model training_set_num: " << model.num_training_set;
    }
  }

  V* get(const K& k)
  {
    auto predict = index->predict(MegaFaker::encode(k));
    ASSERT(predict.start >= 0);
    ASSERT(predict.end >= predict.start);
    // LOG(4) << "get predict: " << predict.start << " => " << predict.end
    // << " from scache";
    return get_from_predicts(k, predict);
  }

  Predicts get_predict(const K& k)
  {
    return index->predict(MegaFaker::encode(k));
  }

  V* get_from_predicts(const K& k, const Predicts& predict)
  {

    /**
     * A fast path: since the position is exactly predicted.
     */
#if 1
    if (predict.start == predict.end) {
      // LOG(4) << "fast path on " << k;
      return PageFetcher<K, V, PAGE_TYPE, PAGEAlloc>::exact_fetch(
        mp->mapped_page_id(mp->decode_mega_to_entry(predict.start)),
        mp->get_offset(predict.start));
    }
#endif

    /**
     * Slow path: we need to scan the page for the value
     */
    auto page_start = mp->decode_mega_to_entry(predict.start);
    auto page_end = mp->decode_mega_to_entry(predict.end);

    for (auto p = page_start; p <= page_end; p += 1) {
      auto res = PageFetcher<K, V, PAGE_TYPE, PAGEAlloc>::fetch_from_one_page(
        k, mp->mapped_page_id(p));
      if (res)
        return res;
    }
    LOG(4) << "predict start: " << predict.start << " : " << predict.end;
    LOG(4) << "failed to find key, from page: " << page_start << " : "
           << page_end;
    return nullptr;
  }

private:
  // helper functions
  inline PAGE_TYPE* get_local_page_from_mid(mega_id_t mega_id)
  {
    auto pid = mp->get_page_id(mega_id);
    return PAGEAlloc<PAGE_TYPE>::get_page_by_id(pid);
  }



  DISABLE_COPY_AND_ASSIGN(SCache);
};

} //
