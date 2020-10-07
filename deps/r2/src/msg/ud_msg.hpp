#pragma once

#include "protocol.hpp"

#include "rlib/ud.hpp"
#include <unordered_map>

#define R2_SOLICITED 0

#include "ud_data.hpp"

namespace r2
{

class UDIncomingIter;
class UdAdapter : public MsgProtocol
{
  friend class UDIncomingIter;

public:
  UdAdapter(const Addr &my_addr, rdmaio::UDQP *sqp, rdmaio::UDQP *qp = nullptr);

  rdmaio::IOStatus connect(const Addr &addr,
                           const rdmaio::MacID &id,
                           int i) override;

  rdmaio::IOStatus send_async(const Addr &addr,
                              const char *msg,
                              int size) override;

  rdmaio::IOStatus flush_pending() override;

  int poll_all(const MsgProtocol::msg_callback_t &f) override;

  Iter_p_t get_iter() override;

  /*!
    Below methods handle connect related works.
  */
  rdmaio::Buf_t get_my_conninfo() override;

  rdmaio::IOStatus connect_from_incoming(const Addr &addr,
                                         const rdmaio::Buf_t &connect_info);

  void disconnect(const Addr &addr);

  void reset_key(const u64 &lkey)  {
    sender_.reset_key(lkey);
  }

public:
  const Addr my_addr;

private:
  // QP to receive requests
  rdmaio::UDQP *qp_ = nullptr;
  rdmaio::UDQP *send_qp_ = nullptr;

  std::unordered_map<Addr_id_t, UdConnectInfo> connect_infos_;

  UdSender sender_;
  UdReceiver receiver_;

  int current_idle_recvs_ = 0;

  DISABLE_COPY_AND_ASSIGN(UdAdapter);
}; // end class UdAdapter

} // end namespace r2
