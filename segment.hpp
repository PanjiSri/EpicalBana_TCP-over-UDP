#ifndef SEGMENT_H
#define SEGMENT_H

#include <cstdint>
#include <cstddef>

using namespace std;

const size_t MAX_PAYLOAD_SIZE = 1460;

struct Segment {
    uint16_t sourcePort;
    uint16_t destPort;
    uint32_t seqNum;
    uint32_t ackNum;
    uint8_t  dataOffset_reserved;
    uint8_t  flags;
    uint16_t windowSize;
    uint16_t checksum;
    uint16_t urgentPointer;
    uint8_t  payload[MAX_PAYLOAD_SIZE];
    size_t   payloadSize;
};

const uint8_t FIN_FLAG = 0x01;
const uint8_t SYN_FLAG = 0x02;
const uint8_t RST_FLAG = 0x04;
const uint8_t PSH_FLAG = 0x08;
const uint8_t ACK_FLAG = 0x10;
const uint8_t URG_FLAG = 0x20;
const uint8_t ECE_FLAG = 0x40;
const uint8_t CWR_FLAG = 0x80;

Segment createSegment(
    uint16_t sourcePort,
    uint16_t destPort,
    uint32_t seqNum,
    uint32_t ackNum,
    uint8_t  dataOffset,
    uint8_t  flags,
    uint16_t windowSize,
    uint16_t urgentPointer,
    const uint8_t *data = nullptr,
    size_t dataSize = 0
);

uint16_t calculateChecksum(const Segment &segment);
bool isValidChecksum(const Segment &segment);

#endif