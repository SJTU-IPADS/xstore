#pragma once

#include "common.hpp"
#include "model_config.hpp"
#include "model_serialize.hpp"
#include "stores/mod.hpp"

#include "utils/spin_lock.hpp"

#include "db_traits.hpp"
#include "smart_cache.hpp"

#include "r2/src/allocator_master.hpp"

#include "xcache/mod.hh"

#include <string>

namespace fstore {

namespace server {

using namespace store;

using SC = SCache<KeyType, ValType, Leaf, BlockPager>;

using PageStream = BPageStream<KeyType, ValType, BlockPager>;
using BStream = BNaiveStream<KeyType, ValType, BlockPager>;

using SBM = SubModel<::fstore::LRModel<>, PageStream, Tree>;
  using CBM = CSM<::fstore::LRModel<>>;

using X = XCache<::fstore::LRModel<>, SBM>;

class Table
{
public:
  //utils::SpinLock guard;
  std::mutex *guard;

  Tree data;
  SC* sc;

  X *xcache = nullptr;
  // buf to store the xcache submodels
  char* submodel_buf = nullptr;

  const std::string name;

  // legacy fields for old smart cache, will be removed later
  ModelDescGenerator::buf_desc_t first_model_buf;
  ModelDescGenerator::buf_desc_t second_model_buf;
  ModelDescGenerator::buf_desc_t mega_buf;

  /*!
    \param: model: the file name which defines the model
   */
  explicit Table(const std::string& name, const std::string& model)
    : name(name)
    //, sc(new SC(ModelConfig::load(model)))
    , sc(nullptr)
    , first_model_buf(std::make_tuple(nullptr, 0))
    , second_model_buf(std::make_tuple(nullptr, 0))
    , guard(new std::mutex())
  {}

  void prepare_models()
  {
    auto allocator = r2::AllocatorMaster<0>::get_thread_allocator();
    ASSERT(allocator != nullptr);

#if 1
    if (std::get<0>(first_model_buf) != nullptr) {
      //ASSERT(std::get<0>(second_model_buf) != nullptr);
     // return;
      LOG(4) << "warning: the newly prepared model will replace the old one";
      allocator->dealloc(std::get<0>(first_model_buf));
      allocator->dealloc(std::get<0>(second_model_buf));
      allocator->dealloc(std::get<0>(mega_buf));
    }
#endif

    std::vector<std::string> first, second;
    for (auto& m : sc->index->rmi.first_stage->models) {
      first.push_back(LinearRegression::serialize_hardcore(m));
    }

    for (auto& m : sc->index->rmi.second_stage->models) {
      second.push_back(LinearRegression::serialize_hardcore(m));
    }

    auto res_bufs = ModelDescGenerator::generate_two_stage_desc<r2::Allocator>(
      *allocator, first, second);
    first_model_buf = res_bufs[0];
    second_model_buf = res_bufs[1];

    // then, the mega buf
    mega_buf = ModelDescGenerator::generate_mega_buf<r2::Allocator>(
      *allocator, sc->mp->serialize_to_buf());
    return;
  }


  void dump_table_models() {
    std::ofstream meta;
    meta.open("meta",std::ios::out | std::ios::binary);

    // prepare the buf
    LidxDesc desc = { .total_keys =
                        (u64)(this->sc->index->sorted_array_size),
                      .key_n = this->sc->index->rmi.key_n,
                      .first_stage_buf_sz = std::get<1>(first_model_buf),
                      .second_stage_buf_sz = std::get<1>(second_model_buf) };
    meta.write((char *)(&desc), sizeof(LidxDesc));
    meta.close();

    // the write the following stages
    std::ofstream fst;
    fst.open("fst", std::ios::out | std::ios::binary);
    fst.write(std::get<0>(first_model_buf), std::get<1>(first_model_buf));
    fst.close();

    std::ofstream snd;
    fst.open("snd", std::ios::out | std::ios::binary);
    fst.write(std::get<0>(second_model_buf), std::get<1>(second_model_buf));
    fst.close();
  }
}; // end class Tables

} // server

} // fstore
