// #include "segment_handler.hpp"
// #include "helper.hpp"
// #include <cstring>
// #include <arpa/inet.h> 
// #include <stdexcept>
// #include <random>
// #include <unistd.h>

// using namespace std;

// SegmentHandler::SegmentHandler(uint8_t windowsize):windowSize(windowsize) {}

// void SegmentHandler::generateSegments(uint32_t sourcePort, uint32_t destPort, uint8_t flags) {
//     if (!dataStream || dataSize == 0)
//     {
//         throw runtime_error("Data stream is empty or data size is zero");
//     }

//     // Hitung jumlah segmen
//     uint32_t segmentCount = (dataSize + MAX_PAYLOAD_SIZE - 1) / MAX_PAYLOAD_SIZE;

//     // Alokasikan buffer untuk segmen
//     this->segmentBuffer = new Segment[segmentCount];

//     // Potong data jadi segmen
//     dataIndex = 0;
//     for (uint32_t i = 0; i < segmentCount; ++i)
//     {
//         // Hitung ukuran payload untuk segmen
//         uint32_t payloadSize = min(static_cast<uint32_t>(MAX_PAYLOAD_SIZE), dataSize - dataIndex);

//         // Siapkan payload buffer
//         uint8_t *payload = new uint8_t[payloadSize];
//         std::memcpy(payload, static_cast<uint8_t *>(dataStream) + dataIndex, payloadSize);

//         // Inisialisasi segmen
//         segmentBuffer[i] = createSegment(sourcePort, destPort, 0, 0, 20, flags, windowSize, 0, payload, payloadSize);

//         // Update indeks data
//         dataIndex += payloadSize;
//     }

//  // Reset data index untuk memastikan segmen dapat diakses kembali
//     dataIndex = 0;
// }

// void SegmentHandler::setDataStream(uint8_t *dataStream, uint32_t dataSize) {
//     this->dataStream = dataStream;
//     this->dataSize = dataSize;
// }

// uint8_t SegmentHandler::getWindowSize() { return windowSize; }

// void SegmentHandler::initSend() {
//     currentSeqNum = generateSecureRandomNumber();
//     while (dataIndex < dataSize) {
//         sendBatch();
//         waitAck();
//         sleep(WAIT_RESPOND_TIME);
//     }
// }

// void SegmentHandler::initRecv() {
//     return;
// }

// void SegmentHandler::waitAck() {
//     return;
// }

// void SegmentHandler::sendBatch() {
//     return;
//     uint32_t index = dataIndex/MAX_PAYLOAD_SIZE;
//     Segment segmen = segmentBuffer[index];
//  }