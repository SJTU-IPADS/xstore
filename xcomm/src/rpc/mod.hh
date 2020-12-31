#pragma once

#include <functional>

#include "../transport/trait.hh"

#include "./op.hh"

#include "../../../deps/r2/src/sshed.hh"

namespace xstore {
namespace rpc {

/*!
  An RPC framework assuming a given SendTrait, and RecvTrait
  for sending and receiving RPC messages.
 */
template<class SendTrait, class RecvTrait, class Manager>
struct RPCCore
{
  using rpc_func_t = std::function<
    void(const Header& rpc_header, const MemBlock& args, SendTrait* replyc)>;

  using ST = SendTrait;

  SessionManager<Manager, SendTrait, RecvTrait> session_manager;

  ReplyStation reply_station;
  std::vector<rpc_func_t> callbacks;

  explicit RPCCore(int num_cors)
    : reply_station(num_cors)
  {}

  auto execute(const RPCOp& op, SendTrait* sender) -> Result<std::string>
  {
    return sender->send(op.msg);
  }

  auto execute_w_key(const RPCOp& op, const u32& k, SendTrait* sender)
    -> Result<std::string>
  {
    return sender->send_w_key(op.msg, k);
  }

  auto reg_callback(rpc_func_t callback) -> usize
  {
    auto id = callbacks.size();
    callbacks.push_back(callback);
    return id;
  }

  /*!
    \ret the number of RPC call and replies received.
    On reply, filling the reply statis's buf
   */
  auto recv_event_loop(RecvTrait* recv) -> usize
  {
    usize num = 0;
    for (recv->begin(); recv->has_msgs(); recv->next()) {
      num += 1;
      auto cur_msg = recv->cur_msg();
      auto session_id = recv->cur_session_id();
      ASSERT(cur_msg.sz >= sizeof(Header));

      // parse the RPC header
      Header& h = *(reinterpret_cast<Header*>(cur_msg.mem_ptr));
      ASSERT(h.payload + sizeof(Header) <= cur_msg.sz)
        << "cur msg sz: " << cur_msg.sz << "; payload: " << h.payload
        << "; session id: " << session_id; // sanity check header and content

      MemBlock payload((char*)cur_msg.mem_ptr + sizeof(Header), h.payload);
      switch (h.type) {
        case Req: {
          try {
            auto reply_channel =
              this->session_manager.incoming_sesions[session_id].get();
            auto f = callbacks.at(h.rpc_id);
            f(h, payload, reply_channel);
          } catch (...) {
            ASSERT(false) << "rpc called failed with rpc id " << h.rpc_id;
          }
        } break;
        case Reply: {
          // pass
          auto ret = this->reply_station.append_reply(h.cor_id, payload);
          ASSERT(ret) << "add reply error: " << h;
        } break;
        case Connect: {
          this->session_manager.add_new_session(session_id, payload, *recv);
        } break;
        default:
          ASSERT(false) << "not implemented";
      }
    }
    recv->end();
    return num;
  }

  // register a future to poll RPC replies
  auto reg_poll_future(::r2::SScheduler& sshed, RecvTrait* recv)
  {
    poll_func_t poll_future =
      [&sshed, this, recv]() -> Result<std::pair<::r2::Routine::id_t, usize>> {
      for (recv->begin(); recv->has_msgs(); recv->next()) {
        auto cur_msg = recv->cur_msg();
        auto session_id = recv->cur_session_id();
        ASSERT(cur_msg.sz >= sizeof(Header));

        // parse the RPC header
        Header& h = *(reinterpret_cast<Header*>(cur_msg.mem_ptr));
        ASSERT(h.payload + sizeof(Header) <= cur_msg.sz)
          << "cur msg sz: " << cur_msg.sz << "; payload: " << h.payload
          << "; session id: " << session_id; // sanity check header and content

        MemBlock payload((char*)cur_msg.mem_ptr + sizeof(Header), h.payload);

        switch (h.type) {
          case Req: {
            try {
              auto reply_channel =
                this->session_manager.incoming_sesions[session_id].get();
              callbacks.at(h.rpc_id)(h, payload, reply_channel);
            } catch (...) {
              ASSERT(false) << "rpc called failed with rpc id " << h.rpc_id;
            }
          } break;
          case Reply: {
            // pass
            auto ret = this->reply_station.append_reply(h.cor_id, payload);
            ASSERT(ret) << "add reply for: " << h.cor_id << " error";

            if (this->reply_station.cor_ready(h.cor_id)) {
              sshed.addback_coroutine(h.cor_id);
            }
          } break;
          case Connect: {
            this->session_manager.add_new_session(session_id, payload, *recv);
          } break;
          default:
            ASSERT(false) << "not implemented";
        }
      }
      recv->end();
      // this future in principle should never return
      return NotReady(std::make_pair<::r2::Routine::id_t>(0u, 0u));
    };

    sshed.emplace_future(poll_future);
  }
};

} // namespace rpc
} // namespace xstore
