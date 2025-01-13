#ifndef NODE_H
#define NODE_H

#include "socket.hpp"

using namespace std;

class Node {
protected:
    TCPSocket* connection;
public:
    virtual void run() = 0;
    virtual void handleMessage(const uint8_t *buffer, size_t size,const string &ip, int32_t port) = 0;
    virtual ~Node() {}
};

#endif
