#pragma once

#include "../common.hpp"
#include "ud_msg.hpp"
#include "ud_data.hpp"

namespace r2
{

class UDIncomingIter : public IncomingIter
{
public:
  UDIncomingIter(UdAdapter *adapter) : adapter(adapter),
                                       poll_result(ibv_poll_cq(adapter->qp_->recv_cq_, MAX_UD_RECV_SIZE, adapter->receiver_.wcs_))
  {
    if (unlikely(poll_result < 0))
    {
      auto &wc = adapter->receiver_.wcs_[0];
      LOG_IF(4, wc.status != IBV_WC_SUCCESS) << "poll till completion error: " << wc.status << " " << ibv_wc_status_str(wc.status);
      // re-set the poll result
      poll_result = 0;
    }
  }

  ~UDIncomingIter()
  {
    adapter->receiver_.post_recvs(adapter->qp_, poll_result);
  }

  IncomingMsg next() override
  {
    Addr addr;
    addr.from_u32(adapter->receiver_.wcs_[idx_].imm_data);
    return {
        .msg = (char *)(adapter->receiver_.wcs_[idx_++].wr_id + GRH_SIZE),
        .size = MAX_UD_RECV_SIZE,
        .from = addr};
  }

  bool has_next() override
  {
    return idx_ < poll_result;
  }

private:
  UdAdapter *adapter = nullptr;
  int idx_ = 0;
  int poll_result = 0;
};

} // end namespace r2
