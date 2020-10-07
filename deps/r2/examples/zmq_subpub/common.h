#pragma once
#include <iostream>
#include "../include/zmq.h"
#include <string>
#include<cstring>
#include <stdlib.h>
#include<unistd.h>
#include <typeinfo>

const int client_connect = 10;
const int zmq_client = 1;
const int zmq_server = 8;
const char server_addr[] = "tcp://127.0.0.1:7777";

void coutError(int rc){
    std::cout<<rc<<std::endl;
    std::cout<<zmq_errno()<<std::endl;
    std::cout<<zmq_strerror(zmq_errno())<<std::endl;
}

// static bool
// s_sendmore (void & socket, const std::string & string) {

//     std::string message(string.size());
//     memcpy (message.data(), string.data(), string.size());

//     bool rc = zmq_send(socket , message, message.size(), ZMQ_SNDMORE);
//     return (rc);
// }

static int
s_send (void *socket, char *str) {
    std::cout<<str<<" "<<strlen(str)<<std::endl;
    zmq_msg_t msg;
    zmq_msg_init_data(&msg, str,strlen(str) ,NULL,NULL);

    int rc = zmq_send(socket,str,strlen(str),ZMQ_DONTWAIT);
    return (rc);
}

static char *
s_recv (void *socket) {
    int rc=0;
    char a[]= "";
    rc = zmq_recv(socket,a,1,ZMQ_BLOCKY);
    assert(rc>0);
    std::cout<<strlen(a)<<std::endl;
    std::cout<<"a "<<std::string(a)<<std::endl;   
    return strdup(a);
}