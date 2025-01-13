#include "segment.hpp"
#include <cstring>
#include <arpa/inet.h>

using namespace std;

Segment createSegment(
    uint16_t sourcePort,
    uint16_t destPort,
    uint32_t seqNum,
    uint32_t ackNum,
    uint8_t  dataOffset,
    uint8_t  flags,
    uint16_t windowSize,
    uint16_t urgentPointer,
    const uint8_t *data,
    size_t dataSize
) {
    Segment segment;
    segment.sourcePort = sourcePort;
    segment.destPort   = destPort;
    segment.seqNum     = seqNum;
    segment.ackNum     = ackNum;
    segment.dataOffset_reserved = (dataOffset << 4);
    segment.flags      = flags;
    segment.windowSize = windowSize;
    segment.urgentPointer = urgentPointer;

    if (dataSize > MAX_PAYLOAD_SIZE) {
        dataSize = MAX_PAYLOAD_SIZE;
    }
    if (data && dataSize > 0) {
        memcpy(segment.payload, data, dataSize);
    }
    segment.payloadSize = dataSize;

    segment.checksum = 0;
    segment.checksum = calculateChecksum(segment);
    return segment;
}

uint16_t calculateChecksum(const Segment &segment) {
    uint32_t sum = 0;
    size_t headerSize = 20;

    uint8_t header[20];
    memset(header, 0, headerSize);

    uint16_t srcPort = htons(segment.sourcePort);
    uint16_t dstPort = htons(segment.destPort);
    uint32_t seqNum  = htonl(segment.seqNum);
    uint32_t ackNum  = htonl(segment.ackNum);
    uint16_t do_rf   = (segment.dataOffset_reserved << 8) | segment.flags;
    uint16_t winSize = htons(segment.windowSize);
    uint16_t chksum  = 0; 
    uint16_t urgPtr  = htons(segment.urgentPointer);

    memcpy(header,      &srcPort, 2);
    memcpy(header + 2,  &dstPort, 2);
    memcpy(header + 4,  &seqNum,  4);
    memcpy(header + 8,  &ackNum,  4);
    memcpy(header + 12, &do_rf,   2);
    memcpy(header + 14, &winSize, 2);
    memcpy(header + 16, &chksum,  2);
    memcpy(header + 18, &urgPtr,  2);

    for (size_t i = 0; i < headerSize; i += 2) {
        uint16_t word = (header[i] << 8) + header[i + 1];
        sum += word;
    }

    for (size_t i = 0; i < segment.payloadSize; i += 2) {
        uint16_t word = (segment.payload[i] << 8);
        if (i + 1 < segment.payloadSize) {
            word |= segment.payload[i + 1];
        }
        sum += word;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    uint16_t result = ~((uint16_t)sum);
    return result;
}

bool isValidChecksum(const Segment &segment) {
    uint16_t receivedChecksum = segment.checksum;
    Segment temp = segment;
    temp.checksum = 0;
    uint16_t calc = calculateChecksum(temp);
    return (receivedChecksum == calc);
}
