#pragma once

#include "model_config.hpp"
#include "table.hpp"

#include "../loaders/loader.hpp"

#include "../src/xcache/seralize.hh"

namespace fstore {

namespace server {

DECLARE_int64(seed);
DECLARE_int64(ycsb_num);

DEFINE_uint32(step, 1, "Training-step");

class XLearner
{
public:
  static void train_x(Table &tab, const std::string &model) {
    auto loaded_model = ModelConfig::load(model);
    auto x = new X(loaded_model.stage_configs[1].model_n);
    BStream it(tab.data, 0);
    bool whether_dump = false;
    //bool  whether_dump = true;
    x->train_dispatcher(it, whether_dump);

    r2::Timer t;
    auto trained = x->train_submodels(tab.data, nullptr, 0,FLAGS_step, true, whether_dump);
    LOG(4) << "trained model ratio: " << (double)trained / loaded_model.stage_configs[1].model_n;

    auto passed_msec = t.passed_msec();
    LOG(4) << "Trained " << trained << " models using: " << passed_msec << "msec" << "; training thpt: " << trained / passed_msec * 1000000;


    {
      auto model = x->select_submodel( 3541629798);
      LOG(4) << "santiy check key: " << " use model: "<< model << "; page table entries: "<<  x->sub_models[model].page_table.size();
    }
#if 1
    auto new_keys = Aug::aug(x->sub_models);
    new_keys.clear();
#endif
#if 1
    // train after aug
    LOG(4) << Aug::aug_zero(x->sub_models) << " models augmented";

    for(uint i = 0;i < new_keys.size(); ++i) {
      if (new_keys[i]) {
        auto new_range = new_keys[i].value();
        auto &m = x->sub_models[i];
        m.lock.lock();
        m.reset_keys(std::get<0>(new_range),
                     std::get<1>(new_range));
        m.notify_watermark += 1;
        m.lock.unlock();
      }
    }

    x->train_submodels(tab.data, nullptr, 0, FLAGS_step);
#endif

    tab.xcache = x;

#if 0
    {
      auto &m = x->sub_models[2669142];
      LOG(4) << "sanity check range: "<< m.start_key << " "<< m.end_key;
    }
#endif
  }

  // assumption: tab's model has been trained priori to serialize to buf
  // ret: total memory comsuption of xcache

  static std::pair<u64,u64> serialize_x(Table &tab) {

    auto allocator = r2::AllocatorMaster<0>::get_thread_allocator();
    u64 size = static_cast<u64>(Serializer::sizeof_submodel<LRModel<>>() + sizeof(u64) + sizeof(u64)) * tab.xcache->sub_models.size();
    LOG(4) << "allocate serialize buf num : "<< tab.xcache->sub_models.size();
    //ASSERT(size < std::numeric_limits<u32>::max()) << " buf sz too large: " << size / (1024 * 1024 * 1024.0) << " GB" << " " << sizeof(size_t);

    char* buf =
      (char*)(allocator->alloc_large(size));
    ASSERT(buf != nullptr);

    u64 model_sum = 0;
    u64 page_table_sum = 0;

    for (u64 i = 0; i < tab.xcache->sub_models.size();++i) {
      auto &m = tab.xcache->sub_models[i];
      //ASSERT(m.page_table.size() != 0) << " model: " << i << " page entry 0 ";
      char* m_buf =
        i * (Serializer::sizeof_submodel<LRModel<>>() + sizeof(u64) + sizeof(u64)) + buf;
      Serializer::serialize_submodel<SBM>(m, m_buf); // serialize m -> buf

      model_sum += m.model_sz();
      page_table_sum += m.page_table_sz();
    }

    // reset the tab's buf
    tab.submodel_buf = buf;

    return std::make_pair(model_sum,page_table_sum);
  }

  static void train_with_static(Table &tab, const std::string file_name) {

  }

  static void train(Table& tab) {
#if 0
    auto start_page = tab.data.find_leaf_page(0);
    auto end_page = tab.data.find_leaf_page(std::numeric_limits<u64>::max());
    //LOG(4) << "start end page id: " << BlockPager<Leaf>::page_id(start_page)
    //<< " " << BlockPager<Leaf>::page_id(end_page);

    start_page->sanity_check();
    end_page->sanity_check();
    //LOG(4) << "train start page end page: " << start_page << " " << end_page;

    tab.model = new SBM(start_page->start_key(), end_page->end_key());
    tab.model->train(tab.data);
    LOG(4) << "min_max error: " << tab.model->min_error << " " << tab.model->max_error
           << "; model: " << tab.model->ml.w << " " << tab.model->ml.b;
#endif
  }

  static void sainty_check(Table& tab) {
#if 1
    YCSBHashGenereator it(0, FLAGS_ycsb_num, FLAGS_seed);
    int count = 0;
    for (it.begin(); it.valid(); it.next(), count += 1) {
      // get
      //LOG(4) << "sanity check key: " << it.key();
      auto &model = tab.xcache->get_model(it.key());
      auto page_range = model.get_page_span(it.key());
      // TODO

      //LOG(4) << "search phy range: "<< std::get<0>(page_range) << " " << std::get<1>(page_range);
      bool found = false;
      for(uint p = std::get<0>(page_range); p <= std::get<1>(page_range);p++) {
        auto page_id = model.lookup_page_phy_addr(p);
        // fetch the page
        auto *page = BlockPager<Leaf>::get_page_by_id(page_id);

        //LOG(4) << "sanity check page range:" << p << " phy: " << page_id << " " << page;

        for(uint i = 0;i < page->num_keys;++i) {
          //LOG(4) << "check key: " << page->keys[i];
          if (page->keys[i] == it.key()) {
            found = true;
            break;
          }
        }

      }
      ASSERT(found) << "failed to found key: " << it.key() << " @count: " << count;
    }
    LOG(4) << "santiy check passes " << "for " << count << " keys";
#endif
  }
};

}

}
