#pragma once

#include "model_config.hpp"
#include "table.hpp"

namespace fstore {

namespace server {

using PageStream = BPageStream<KeyType, ValType, BlockPager>;
using BStream = BNaiveStream<KeyType, ValType, BlockPager>;

class Learner
{
public:
  static void train(Table& tab)
  {
    PageStream it(tab.data, 0);
    tab.sc->retrain(&it, tab.data);
  }

  static void retrain_static(Table &tab, const std::string &model) {
    tab.guard->lock();
    PageStream it(tab.data, 0);
    tab.sc->retrain_with_known_model(&it,tab.data);
    tab.guard->unlock();
  }

  static void retrain(Table& tab,const std::string &model) {
    PageStream it(tab.data, 0);
    auto loaded_model = ModelConfig::load(model);

    tab.guard->lock();
    auto res =
      tab.sc->retrain(loaded_model, &it, tab.data);

    //LOG(2) << "[XX] retrain locked " << tab.guard;
    // replace the table's mp and index with the new index
    // because training is very slow, we cannot afford to put the retrain with the lock
    tab.sc->index = std::get<0>(res).get();
    tab.sc->mp = std::get<1>(res).get();
    //LOG(4) << "sanity check mp 0 entry: " << SeqEncode::decode_id(tab.sc->mp->mapped_page_id(95179));
    LOG(4) << "new index with models: " << tab.sc->index->rmi.second_stage->models.size() << "; total mp entries: "
           << tab.sc->mp->total_num();
    tab.prepare_models();
    //LOG(2) << "[XX] retrain unlock " << tab.guard;
    tab.guard->unlock();
  }

  /*!
    Just for test only
    increment the sequence number of all pages of one table,
    so that client will invalid all pages
   */
  static void mark_all_invalid(Table &tab) {
    PageStream it(tab.data,0);
    for(it.begin();it.valid();it.next()) {
      auto page_p = it.value();
      page_p->seq += 1;
    }
  }

  static void clear_model(Table& tab, const std::string& model)
  {
    // TODO: we cannot free the sc now, since others may reference to it.
    // we must use an epoch based scheme for clear this
    tab.sc = new SC(ModelConfig::load(model));
  }

  /*!
    Verify that the learned index in the table keeps the same semantic
    of original B+tree.
   */
  static bool verify_table(Table& tab) { return false; }
};

} // server

} // fstore
