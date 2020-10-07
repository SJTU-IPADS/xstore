#include<iostream>
#include "assert.h"
#include <unistd.h>
#include "common.h"
//#include "zhelpers.hpp"

int main(){
    void *server_ctx = zmq_ctx_new();
    assert(server_ctx);
    void *sock = zmq_socket(server_ctx,ZMQ_PUB);
    int rc=0;
    //rc = zmq_connect(sock,server_addr);
    rc = zmq_bind(sock,server_addr);
    assert(rc==0);

    while (1)
    {
        //s_sendmore(sock,"A");
        rc = s_send(sock,"A");
        assert(rc>0);
        std::cout<<"rc "<<rc<<std::endl;
        //s_sendmore(sock,"B");
        rc = s_send(sock,"B");
        assert(rc>0);
        sleep(1);
    }
    zmq_close(sock);
    zmq_term(server_ctx);
    return 0;
}