#ifndef RECEIVER_H
#define RECEIVER_H

#include "node.hpp"
#include "socket.hpp"
#include <map>
#include <vector>
#include <cstdint>
#include <string>

using namespace std;

class Receiver : public Node {
private:
    string host;
    int32_t localPort;
    string senderIp;
    int32_t senderPort;

    uint32_t RWS;         
    uint32_t LFR;         
    uint32_t LAF;         
    uint32_t baseSeqNum;  
    uint32_t initialAckNum;

    map<uint32_t, vector<uint8_t>> outOfOrderMap;

    int timeoutInterval;
    int maxHandshakeRetry;

    bool performHandshake(const string &sIp, int32_t sPort);

    void sendAckSegment(uint32_t ackNum);

    void sendSack();

public:
    Receiver(const string &host, int32_t localPort, const string &senderIp);

    void run() override;
    void handleMessage(const uint8_t *buffer, size_t size, const string &receiverIp, int32_t receiverPort) override;
};

#endif