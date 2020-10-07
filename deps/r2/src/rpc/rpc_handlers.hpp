#include "rpc.hpp"

#include <ctime>

namespace r2 {

namespace rpc {

using namespace rdmaio;
using namespace std;

enum {
  START_HS = 0,
  END_HS
};

/*!
  We use two RPC handler to handle hand-shake messages.
 */
class RPCHandler {
 public:
  static void start_handshake_handler(RPC &rpc,const Req::Meta &ctx,const char *msg,u32 size) {

    //DISPLAY(2) << "At [" << cur_time() << "]: " <<
    //  "receive start handshake from: (" << ctx.dest.to_str() << ")";

    Buf_t info = Buf_t(msg,size);
    if(rpc.msg_handler_->connect_from_incoming(ctx.dest,info) != SUCC) {
      // TODO, handle such errors
      ASSERT(false);
    }
    char *reply_buf = rpc.get_buf_factory().get_inline_buf();
    ASSERT(rpc.reply(ctx,reply_buf,0) == SUCC);
  }

  static void stop_handshake_handler(RPC &rpc,const Req::Meta &ctx,const char *msg,u32 size) {
    //DISPLAY(2) << "At [" << cur_time() << "]: "
    //<< "receive stop handshake from: " << ctx.dest.to_str();
    rpc.msg_handler_->disconnect(ctx.dest);
  }

 private:
  /*!
    Return the current time at server.
   */
  static std::string cur_time() {
    time_t rawtime;
    struct tm * timeinfo;
    char buffer[64];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer,sizeof(buffer),"%d-%m-%Y %H:%M:%S",timeinfo);
    return std::string(buffer);
  }
};

} // end namespace rpc

} // end namespace r2
