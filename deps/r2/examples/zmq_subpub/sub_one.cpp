#include <iostream>
#include "assert.h"
#include "common.h"
//#include "zhelpers.hpp"

int main(){
    void *ctx = zmq_ctx_new();
    assert(ctx);
    //void *sock[CLIENT_CONNECTION];
    void *sock = zmq_socket(ctx,ZMQ_SUB);
    //zmq_msg_t msg;
    int rc=0;
    //rc = zmq_bind(sock,server_addr);
    rc = zmq_connect(sock,server_addr);
    assert(rc==0);

    //set and filter
    rc = zmq_setsockopt(sock,ZMQ_SUBSCRIBE,"B",1);
    assert(rc==0);
    //rc = zmq_msg_recv(&msg,sock,0);
    while (1)
    {
        //char *addr = s_recv(sock);
        char *contents = s_recv(sock);
        assert(contents!=NULL);
        std::cout<<"r "<<std::string(contents)<<std::endl;
        sleep(1);
    }
    zmq_close(sock);
    zmq_term(ctx);
    return 0;
}