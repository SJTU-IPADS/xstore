#pragma once

// whether the model can be concurrently updated
#define COMPACT_SYNC 1

namespace fstore {

template <typename Model>
class CSM {
public:
  u32 train_watermark = 0;
  Model ml;

  i32 min_error;
  i32 max_error;

  u32 max_entries;
  std::vector<u64> page_table;

#if COMPACT_SYNC
  std::mutex lock;
  u32 seq = 1;
#endif

  CSM()
  {
    // page_table.reserve(max_page_table_entry);
    page_table.clear();
  }

  u64 model_sz() const
  {
    return sizeof(float) + sizeof(float) + sizeof(i8) * 2 + sizeof(u32);
  }

  i64 get_predict(const u64& key)
  {
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

};

}
