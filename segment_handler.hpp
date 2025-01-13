// #ifndef segment_handler_h
// #define segment_handler_h

// using namespace std;
// #include "segment.hpp"

// const size_t MAX_PAYLOAD_SIZE = 1460; 
// const uint32_t WAIT_RESPOND_TIME = 0.5;

// class SegmentHandler
// {
// private:
//     uint8_t windowSize;
//     uint32_t currentSeqNum;
//     uint32_t currentAckNum;
//     void *dataStream;
//     uint32_t dataSize;
//     uint32_t dataIndex;
//     Segment *segmentBuffer; 

//     void generateSegments(uint32_t sourcePort, uint32_t destPort, uint8_t flags);
//     void sendBatch();
//     void waitAck();

// public:
//     SegmentHandler(uint8_t windowSize);
//     void setDataStream(uint8_t *dataStream, uint32_t dataSize);
//     uint8_t getWindowSize();
//     Segment *advanceWindow(uint8_t size);
//     void initSend();
//     void initRecv();
// };

// #endif