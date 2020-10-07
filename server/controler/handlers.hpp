#pragma once

#include "r2/src/channel/channel.hpp"
#include "r2/src/rpc/rpc.hpp"

#include "marshal.hpp"

#include "../internal/tables.hpp"
#include "../internal/xcache_learner.hh"

#include "../proto.hpp"

#define COUNT_LAT 0

namespace fstore {

namespace server {

DECLARE_string(model_config);
DECLARE_uint32(step);

//__thread int worker_id;
extern __thread r2::Channel<u32>* update_channel;
extern __thread r2::Channel<u32>* update_channel_1;
extern __thread r2::Channel<u32>* update_channel_2;

__thread u64 insert_count = 0;
__thread u64 invalid_count = 0;


__thread std::queue<int>* overflow_q = nullptr;

std::mutex retrain_lock;
bool whether_retrain = false;
bool in_retrain = false;

bool
enqueue_trained_id_to_channel(X* x, int i)
{
  bool ret = false;
#if 0

  if (i < x->sub_models.size() / 3) {
    ret = update_channel->enqueue(i);
  } else {
    if (i < x->sub_models.size() / 3 * 2) {
      ret = update_channel_1->enqueue(i);
    } else {
      ret = update_channel_2->enqueue(i);
    }
  }
#else
  if (i < x->sub_models.size() / 2) {
    ret = update_channel->enqueue(i);
  } else {
    ret = update_channel_1->enqueue(i);
  }
#endif
  return ret;
}

extern TestLidx test_lidx;

using namespace r2::rpc;

extern Tables global_table;

__thread usize rpc_executed = 0;

/*!
  Various handlers of the data-path, i.e. get(),put() and various data
  requests. The various RPC function defines are in proto.hpp.
 */
class DataHandlers
{
public:
  static void check(RPC& rpc,
                    const Req::Meta& ctx,
                    const char* msg,
                    u32 payload)
  {
    GetPayload* req = (GetPayload*)msg;
    auto& tab = global_table.get_table(req->table_id);
    ASSERT(req->table_id == 0);

    auto key = req->key;

    // check
    auto select = tab.xcache->select_submodel(key);
    auto& model = tab.xcache->get_model(key);
    auto page_range = model.get_page_span(key);
    LOG(4) << "Sanity check model: " << select << " for key: " << key;

    auto val = tab.data.get(key);
    LOG(4) << " whether value exits? : " << val;

    for (uint p = std::get<0>(page_range); p <= std::get<1>(page_range); p++) {
      // auto page_id = model.lookup_page_phy_addr(p);
      auto page_table_entry = model.lookup_page_entry(p).value();
      auto page_id = SeqEncode::decode_id(page_table_entry);
      auto page_seq = SeqEncode::decode_seq(page_table_entry);

      // fetch the page
      auto* page = BlockPager<Leaf>::get_page_by_id(page_id);

      LOG(4) << "snaity check page: " << page_seq << " " << (int)page->seq
             << "; two watermarks: " << model.notify_watermark << " "
             << model.train_watermark;
    }

    model.notify_watermark += 1;
    model.train(tab.data, 1);

    for (uint p = std::get<0>(page_range); p <= std::get<1>(page_range); p++) {
      // auto page_id = model.lookup_page_phy_addr(p);
      auto page_table_entry = model.lookup_page_entry(p).value();
      auto page_id = SeqEncode::decode_id(page_table_entry);
      auto page_seq = SeqEncode::decode_seq(page_table_entry);

      // fetch the page
      auto* page = BlockPager<Leaf>::get_page_by_id(page_id);

      LOG(4) << "snaity re-check page: " << page_seq << " " << (int)page->seq
             << "; two watermarks: " << model.notify_watermark << " "
             << model.train_watermark << "; for page:" << page_id;
    }
    sleep(5);
    ASSERT(false);
  }

  static void get_w_fallback(RPC& rpc,
                             const Req::Meta& ctx,
                             const char* msg,
                             u32 payload)
  {
    ASSERT(payload >= sizeof(GetPayload)) << "wrong payload sz: " << payload;
    GetPayload* req = (GetPayload*)msg;

#if 1 // use B+tree for get
    auto& tab = global_table.get_table(req->table_id);
    ASSERT(req->table_id == 0);
    auto val_p = tab.data.get(req->key);

#else
    // use lidx for get
    // auto& tab = global_table.get_table(req->table_id);
    auto val_p = test_lidx.get_as_ptr(req->key);
    // auto val_p = tab.sc->index->get_as_ptr(req->key);
    // ASSERT(val_p != nullptr);
    // ASSERT(val_p->get_meta() == req->key);
#endif
    // send the reply
    auto& factory = rpc.get_buf_factory();
    char* reply_buf = factory.get_inline();

    // prepare the reply model
    usize cur_model_sz = 0;

    // get the model

    auto& model = tab.xcache->sub_models[req->model_id];
    //ASSERT(req->model_seq <=
    //tab.xcache->sub_models[req->model_id].train_watermark);
#if 0
      if (req->model_seq != 0) {
        //ASSERT(false) << "req model seq: " << req->model_seq;
      }
#endif
    // LOG(4) << "Receive model seq; " << req->model_seq << " for model id:" <<
    // req->model_id; sleep(1);
    if (model.train_watermark > req->model_seq) {
      // serialize
      // const auto gap =
      // Serializer::sizeof_submodel<LRModel<>>() + sizeof(u64) + sizeof(u64);

      // cur_model_sz = gap;
      auto prev_seq = model.seq;
      r2::compile_fence();
      if (prev_seq != 0) {
        cur_model_sz =
          Serializer::direct_serialize(model, reply_buf + sizeof(u32));
        r2::compile_fence();
        if (model.seq != prev_seq) {
          // invalid, being trained
          cur_model_sz = 0;
        }
      } else {
        cur_model_sz = 0;
      }
      r2::compile_fence();
      ASSERT(cur_model_sz < 4000) << "cur model sz:" << cur_model_sz << "; page table num: " << model.page_table.size();
    } else {
      // cur_model_sz = ValType::get_payload();
      auto xx = tab.xcache;
      r2::compile_fence();
      if (req->model_key == (u64)tab.submodel_buf) {
        //if (1) {
        enqueue_trained_id_to_channel(xx, req->model_id);
      }
      cur_model_sz = 0;
    }
    ASSERT(cur_model_sz != 73);
    *((u32*)reply_buf) = cur_model_sz;

    rpc.reply_async(ctx, reply_buf, sizeof(u32) + cur_model_sz);
  }


  static void retrain(RPC &rpc, const Req::Meta &ctx, const char *msg, u32 payload) {
    retrain_lock.lock();
    if (whether_retrain == false) {
    //    if (0) {
      whether_retrain = true;

      r2::compile_fence();

      // retrain

      auto& tab = global_table.get_table(0);

      auto cur_xcache = tab.xcache;
      r2::compile_fence();
      in_retrain = true;
      r2::compile_fence();
      //while(tab.xcache == cur_xcache && tab.submodel_buf == nullptr) {
      while (in_retrain == true) {
        sleep(1);
        r2::compile_fence();
      }
      // retrain done
    }
    retrain_lock.unlock();
    r2::compile_fence();

    auto& factory = rpc.get_buf_factory();
    //ASSERT(false);
    char* reply_buf = factory.get_inline();
    rpc.reply_async(ctx, reply_buf, sizeof(u64));
    //LOG(4) << "Reply done";
    //ASSERT(false);
  }

  static void get(RPC& rpc, const Req::Meta& ctx, const char* msg, u32 payload)
  {

#if COUNT_LAT
    static __thread DistReport<double> get_lat;
    r2::Timer t;
#endif

    ASSERT(payload >= sizeof(GetPayload)) << "wrong payload sz: " << payload;
    GetPayload* req = (GetPayload*)msg;

#if 1 // use B+tree for get
    auto& tab = global_table.get_table(req->table_id);
    ASSERT(req->table_id == 0);
    auto val_p = tab.data.get(req->key);

#else
    // use lidx for get
    // auto& tab = global_table.get_table(req->table_id);
    auto val_p = test_lidx.get_as_ptr(req->key);
    // auto val_p = tab.sc->index->get_as_ptr(req->key);
    // ASSERT(val_p != nullptr);
    // ASSERT(val_p->get_meta() == req->key);
#endif
    // send the reply
    auto& factory = rpc.get_buf_factory();
    char* reply_buf = factory.get_inline();

    if (val_p) {
      memcpy(reply_buf, val_p, ValType::get_payload());
    } else {
      // ASSERT(false);
    }
    rpc.reply_async(ctx, reply_buf, ValType::get_payload());

#if COUNT_LAT
    get_lat.add(t.passed_msec());
    rpc_executed += 1;
    // if (rpc_executed >= 10000000) {
    if (0) {
      LOG(4) << "handler time: " << get_lat.average;
      ASSERT(false);
    }
#endif
  }

  static void put(RPC& rpc, const Req::Meta& ctx, const char* msg, u32 payload)
  {
    ASSERT(payload == sizeof(GetPayload) + sizeof(ValType))
      << "wrong put payload sz: " << payload;

    GetPayload* req = (GetPayload*)msg;

    auto& factory = rpc.get_buf_factory();
    char* reply_buf = factory.get_inline();

#if 1
    auto& tab = global_table.get_table(req->table_id);
    auto val_p = tab.data.get(req->key);
    ASSERT(val_p != nullptr);
    // copy the content to val_p
    // TODO: we donot handle concurrency
#if 1
    //val_p->lock.lock();
    r2::compile_fence();
    memcpy(val_p->data, msg + sizeof(GetPayload), ValType::get_payload());
    r2::compile_fence();
    //val_p->lock.unlock();
#endif
    // send the reply
    Marshal<u64>::serialize_to(reinterpret_cast<u64>(val_p), reply_buf);
#endif

    rpc.reply_async(ctx, reply_buf, sizeof(u64));
  }

  static void insert(RPC& rpc,
                     const Req::Meta& ctx,
                     const char* msg,
                     u32 payload)
  {
    if (overflow_q == nullptr) {
      overflow_q = new std::queue<int>();
    }

    {
      auto& tab = global_table.get_table(0);
      auto xx = tab.xcache;
      // check whether we need to notify
      while (!overflow_q->empty()) {
        auto i = overflow_q->front();
        auto& model = tab.xcache->sub_models[i];
        if (model.notify_watermark > model.train_watermark && !in_retrain) {
          // still need to retrain
          if (enqueue_trained_id_to_channel(xx, i)) {
            overflow_q->pop();
          } else {
            break;
          }
        } else {
          // dequeue
          overflow_q->pop();
        }
      }
    }

    ASSERT(payload == sizeof(GetPayload) + sizeof(ValType))
      << "wrong put payload sz: " << payload;

#if 1
    GetPayload* req = (GetPayload*)msg;

    auto& tab = global_table.get_table(req->table_id);
    auto key = req->key;

    // LOG(4) << "insert:" << req->key;
#if 0
    auto val_p = tab.data.get(req->key);
    if(val_p != nullptr) {
      LOG(4) << "already inserted key: " << req->key; sleep(1);
    }
#endif
    // ASSERT(val_p != nullptr);

    if (1) {
      // auto npage_before_insert = BlockPager<Leaf>::allocated;
      // auto ptr =
      // global_table.get_table(req->table_id).data.safe_get_with_insert(req->key);
      auto res = global_table.get_table(req->table_id)
                   .data.safe_get_with_insert_check_split(req->key);
      auto ptr = std::get<0>(res);
      // auto ptr = global_table.get_table(req->table_id)
      //.data.get_with_insert(req->key);
      // ASSERT(ptr == val_p) << "get two wrong ptr, one: " << val_p << "; one:
      // " << ptr;

      insert_count += 1;

      // check whether we need to retrain the model
      if (std::get<1>(res)) {
        auto down_keys = std::get<0>(std::get<1>(res).value());
        auto up_keys = std::get<1>(std::get<1>(res).value());

        auto& tab = global_table.get_table(req->table_id);
        auto xx = tab.xcache;

        int s = xx->select_submodel(down_keys);
        int e = xx->select_submodel(up_keys);
        for (uint i = s; i <= e; ++i) {
          auto& model = xx->sub_models[i];

          bool need_notify = false;

          model.lock.lock();

          model.update_start_end_key(up_keys);
          model.update_start_end_key(down_keys);
          ASSERT(model.notify_watermark >= model.train_watermark);
          if (model.notify_watermark <= model.train_watermark) {
            model.notify_watermark = model.train_watermark + 1;
            need_notify = true;
          }
          model.lock.unlock();
          // LOG(4) << "invalidate model: " << s << " for key: " << key;
          // sleep(1);

          if (need_notify && (u64)tab.submodel_buf == req->model_key) {
            //if (need_notify) {
            // LOG(4) << "notify: " << i << " to retrain";
            auto ret = enqueue_trained_id_to_channel(tab.xcache, i);
            //if (!ret && overflow_q->size() < 128) {
              overflow_q->push(i);
              //}
          }
          invalid_count += 1;
        }
#if 0
        int i = s - 1;

        // first train s
        {
          auto& model = tab.xcache->sub_models[s];
          model.lock.lock();

          model.update_start_end_key(key);
          model.notify_watermark += 1;
          model.lock.unlock();
          //LOG(4) << "invalidate model: " << s << " for key: " << key;
          //sleep(1);
          while (!
                 update_channel->enqueue(s)) {
            r2::compile_fence();
          }
          invalid_count += 1;
          //LOG(4) << "need retrain: " << s; sleep(1);
        }
#endif
#if 0
        while (i >= 0) {
          auto& model = tab.xcache->sub_models[i];
          model.lock.lock();

          if (model.end_key >= key) {
            // update
            model.update_start_end_key(key);
            model.notify_watermark += 1;
          } else {
            model.lock.unlock();
            break;
          }

          model.lock.unlock();
          invalid_count += 1;
          //LOG(4) << "invalidate model: " << i << " for key: " << key;
          //sleep(1);

          while (!update_channel->enqueue(i)) {
            r2::compile_fence();
          }

          i -= 1;
        }

        // reset
        i = s + 1;

        while (i < tab.xcache->sub_models.size()) {
          auto& model = tab.xcache->sub_models[i];
          model.lock.lock();

          if (model.start_key <= key) {
            // update
            model.update_start_end_key(key);
            model.notify_watermark += 1;
          } else {
            model.lock.unlock();
            break;
          }

          model.lock.unlock();
          //LOG(4) << "invalidate model: " << i << " for key: " << key; sleep(1);
          invalid_count += 1;

          while (!update_channel->enqueue(i)) {
            r2::compile_fence();
          }

          i += 1;
        }
#endif
      }
    }
#endif

    // send the reply
    auto& factory = rpc.get_buf_factory();
    char* reply_buf = factory.get_inline();
    Marshal<u64>::serialize_to(reinterpret_cast<u64>((u64)73), reply_buf);
    rpc.reply_async(ctx, reply_buf, sizeof(u64));
  }

  static void predict(RPC& rpc,
                      const Req::Meta& ctx,
                      const char* msg,
                      u32 payload)
  {
    ASSERT(false);
    ASSERT(payload >= sizeof(GetPayload)) << "wrong payload sz: " << payload;
    GetPayload* req = (GetPayload*)msg;

    auto& tab = global_table.get_table(req->table_id);
    auto predict = tab.sc->get_predict(req->key);

    auto& factory = rpc.get_buf_factory();
    char* reply_buf = factory.get_inline();
    Marshal<Predicts>::serialize_to(predict, reply_buf);
    rpc.reply_async(ctx, reply_buf, sizeof(Predicts));
  }

  static void scan(RPC& rpc, const Req::Meta& ctx, const char* msg, u32 payload)
  {
    ScanPayload* req = (ScanPayload*)msg;

    auto& tab = global_table.get_table(req->table_id);
    ASSERT(req->table_id == 0);
    auto page = tab.data.safe_find_leaf_page(req->start);

    auto& factory = rpc.get_buf_factory();
    char* reply_buf = factory.get_inline();
    char* cur_ptr = reply_buf;

    u64 size = 0;
    // uint page_to_fetch = (req->num / IM) * 2 + 1;
    // uint expected_reply_num = (page_to_fetch * IM) / 4000 + 1;
    int expected_replies =
      std::ceil((req->num * ValType::get_payload()) / 4000.0);
    uint real_reply = 0;
    //    LOG(4) << "get key req: " << req->num << "; " << req->start << ";
    //    expected: " << expected_replies << " replies";

    u64 num = 0; // how many keys replied
    // LOG(4) << "start handling scan with replies: " << expected_replies;

    u64 fetched = 0;
    while (page != nullptr && num < req->num) {
      auto expect_num =
        std::min(req->num - num, static_cast<u64>(page->num_keys));
      auto copied_size =
        expect_num * ValType::get_payload() + expect_num * sizeof(u64);
      // LOG(4) << "add sz: " << copied_size;
      if (copied_size + size > 4000) {
        // LOG(4) << "sent one with sz: " << size;
        // ASSERT(size < 4000);
        // LOG(4) << "sent reply " << (void *)reply_buf << " to " <<
        // ctx.dest.to_str() << " with sz: " << size;
        if (real_reply < expected_replies)
          rpc.reply_async(ctx, reply_buf, size);
        real_reply += 1;

        // re-set the reply buffer
        reply_buf = factory.get_inline();
        cur_ptr = reply_buf;
        size = 0;
      }
      size += copied_size;
      memcpy(cur_ptr, &(page->values[0]), copied_size);

      // reset the cursors
      cur_ptr += copied_size;
      num += expect_num;

      page = page->right;
    }
    //    ASSERT(size < 4000) << "sent sz: " << size << "; fetched num: " <<
    //    req->num
    //                        << " valsize: " << sizeof(ValType);
    if (size > 0 && real_reply < expected_replies) {
      // LOG(4) << "sent sz:"  << size;
      // ASSERT(size < 4000);
      rpc.reply_async(ctx, reply_buf, size);
      real_reply += 1;
    } else {
    }
    // fill the remaining holes at client
    char inline_buf[64];
    while (real_reply < expected_replies) {
      // reply_buf = factory.get_inline();
      // LOG(4) << "sent reply remaining" << (void *)reply_buf << " to " <<
      // ctx.dest.to_str(); rpc.reply_async(ctx, inline_buf + 16, 0);
      rpc.reply(ctx, inline_buf + 16, 0);
      // rpc.reply_async(ctx,reply_buf,0);
      real_reply += 1;
    }
    // LOG(4) << "scan done"; sleep(1);
    // LOG(4) << "scan reply: " << real_reply;
  }

  //static void retrain(RPC &rpc,

  static void null(RPC& rpc, const Req::Meta& ctx, const char*, u32)
  {
    auto& factory = rpc.get_buf_factory();
    // ASSERT(false);
    char* reply_buf = factory.get_inline();
    rpc.reply_async(ctx, reply_buf, 0);
  }

  static void register_all(RPC& rpc)
  {
    rpc.register_callback(GET_ID, get);
    rpc.register_callback(PUT_ID, put);
    //rpc.register_callback(NULL_ID, null);
    rpc.register_callback(NULL_ID, retrain);
    // rpc.register_callback(PREDICT, check);
    rpc.register_callback(PREDICT, get_w_fallback);
    rpc.register_callback(SCAN_RPC, scan);
    rpc.register_callback(INSERT_ID, insert);
    // rpc.register_callback(CHECK, check);
  }
};

} // namespace server

} // namespace fstore
