#ifndef SOCKET_H
#define SOCKET_H

#include <string>
#include <cstdint>

using namespace std;

enum TCPStatusEnum {
    LISTEN=0,
    SYN_SENT,
    SYN_RECEIVED,
    ESTABLISHED,
    FIN_WAIT_1,
    FIN_WAIT_2,
    CLOSE_WAIT,
    CLOSING,
    LAST_ACK,
    CLOSED
};

class TCPSocket {
private:
    string ip;
    int32_t port;
    int32_t sockfd;
    TCPStatusEnum status;

public:
    TCPSocket();
    ~TCPSocket();

    bool bindSocket(const string &ip, int32_t port);
    bool sendTo(const string &ip, int32_t port, const void *data, size_t dataSize);
    ssize_t recvFrom(void *buffer, size_t length, string &senderIp, int32_t &senderPort);

    void closeSocket();
    int32_t getSocketFD() const;
    void changeStatus(TCPStatusEnum st);
};

#endif