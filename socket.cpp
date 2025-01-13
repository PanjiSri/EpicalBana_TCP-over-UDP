#include "socket.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

using namespace std;

TCPSocket::TCPSocket() : sockfd(-1), status(CLOSED) {}

TCPSocket::~TCPSocket() {
    closeSocket();
}

bool TCPSocket::bindSocket(const string &ip, int32_t port) {
    cout << "[DEBUG] Creating UDP socket..." << endl;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("[-] Failed to create socket");
        return false;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());

    cout << "[DEBUG] Binding socket to IP " << ip << " and port " << port << "..." << endl;
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[-] Bind failed");
        close(sockfd);
        return false;
    }

    this->ip = ip;
    this->port = port;
    status = LISTEN;

    cout << "[DEBUG] Socket bound successfully." << endl;
    return true;
}

bool TCPSocket::sendTo(const string &ip, int32_t port, const void *data, size_t dataSize) {
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());

    ssize_t sentBytes = sendto(sockfd, data, dataSize, 0, (struct sockaddr *)&addr, sizeof(addr));
    if (sentBytes != (ssize_t)dataSize) {
        perror("[-] Failed to send data");
        return false;
    }
    return true;
}

ssize_t TCPSocket::recvFrom(void *buffer, size_t length, string &senderIp, int32_t &senderPort) {
    sockaddr_in senderAddr;
    socklen_t addrLen = sizeof(senderAddr);
    ssize_t recvBytes = recvfrom(sockfd, buffer, length, 0, (struct sockaddr *)&senderAddr, &addrLen);

    if (recvBytes >= 0) {
        senderIp   = inet_ntoa(senderAddr.sin_addr);
        senderPort = ntohs(senderAddr.sin_port);
    } else {
        perror("[-] Failed to receive data");
    }
    return recvBytes;
}

void TCPSocket::closeSocket() {
    if (sockfd >= 0) {
        cout << "[DEBUG] Closing socket..." << endl;
        close(sockfd);
        sockfd = -1;
        status = CLOSED;
        cout << "[DEBUG] Socket closed." << endl;
    }
}

int32_t TCPSocket::getSocketFD() const {
    return sockfd;
}

void TCPSocket::changeStatus(TCPStatusEnum st){
    status = st;
}