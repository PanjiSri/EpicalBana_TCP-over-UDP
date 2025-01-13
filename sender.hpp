#ifndef SENDER_H
#define SENDER_H

#include "node.hpp"
#include "socket.hpp"
#include <string>
#include <vector>
#include <map>
#include <cstdint>

using namespace std;

class Sender : public Node {
private:
    string host;        
    int32_t port;       

    string dataToSend;

    uint32_t SWS;       
    uint32_t LAR;       
    uint32_t LFS;       
    uint32_t baseSeqNum;
    uint32_t MaxSeqNum; 

    string receiverIp;

    int32_t receiverPort;

    uint64_t RTO;     

    double alpha, beta; 

    double SRTT, RTTVAR;

    uint32_t lastAckNum;

    int dupAckCount;    

    int maxHandshakeRetry; 
    
    int maxFinRetry;       

    struct Packet {
        uint32_t seqNum;
        vector<uint8_t> payload;
    };
    vector<Packet> sendBuffer;

    struct SegmentInfo {
        uint32_t seqNum;
        uint64_t sendTimestamp;
        bool     acked;
        bool     isRetrans;
    };
    map<uint32_t, SegmentInfo> segmentMap;

    bool doHandshake(const string &rIp, int32_t rPort);

    bool handleSyn(int sockfd, uint64_t &startTime, uint64_t handshakeTimeoutMs, const string &rIp, int32_t rPort);

    bool sendFinAndWait();

    bool waitForAckOrTimeout();

    void sendSegment(uint32_t idx, bool isRetrans = false);

    void sendWindow();

    void resendFrom(uint32_t seqNum);

    void handleAck(uint32_t ackNum);

    void updateRTO(uint64_t measuredRTT);

    void handleSackMessage(const string &sackMsg);

    void handleSack(uint32_t newLFR, const vector<uint32_t> &sackedSeq);

public:
    Sender(const string &host, int32_t port);

    void run() override;

    void handleMessage(const uint8_t *buffer, size_t size, const string &receiverIp, int32_t receiverPort) override;
};

#endif