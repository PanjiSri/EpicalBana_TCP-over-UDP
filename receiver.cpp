#include "receiver.hpp"
#include "helper.hpp"
#include "segment.hpp"
#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <limits>
#include <cstdio>   

using namespace std;

static uint32_t computeCustomChecksum(const vector<uint8_t> &data) {
    uint64_t sum = 0;  
    for (auto b : data) {
        sum += b;
    }
    while (sum >> 32) {
        sum = (sum & 0xFFFFFFFF) + (sum >> 32);
    }
    return (uint32_t)sum;
}

Receiver::Receiver(const string &host, int32_t localPort, const string &senderIp)
: host(host), localPort(localPort), senderIp(senderIp)
{
    connection = new TCPSocket();
    RWS = 16; 
    LFR = 0; 
    baseSeqNum = generateSecureRandomNumber();
    LAF = LFR + RWS;
    initialAckNum = 0;
    timeoutInterval = 5000;
    maxHandshakeRetry = 5; 
}

void Receiver::run() {
    cout << "[+] Node is now a receiver" << endl;
    cout << "[?] Input the sender program's port: ";
    cin >> senderPort;
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); 

    if (senderIp == "localhost") {
        senderIp = "127.0.0.1";
    }

    cout << "[i] Binding to " << host << ":" << localPort << endl;
    if (!connection->bindSocket(host, localPort)) {
        cerr << "[-] Failed to bind socket." << endl;
        exit(EXIT_FAILURE);
    }

    int broadcastEnable = 1;
    if (setsockopt(connection->getSocketFD(), SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("[-] Failed to set broadcast");
        exit(EXIT_FAILURE);
    }

    cout << "[DEBUG] Sending REQUEST to " << senderIp << ":" << senderPort << endl;
    bool reqOk = connection->sendTo(senderIp, senderPort, "REQUEST", 7);
    if (!reqOk) {
        cerr << "[-] Failed to send REQUEST." << endl;
    }

    // Lakukan handshake
    bool hsOk = performHandshake(senderIp, senderPort);
    if (!hsOk) {
        cerr << "[-] Handshake failed after multiple tries. Exiting." << endl;
        return;
    }

    LFR = initialAckNum - 1;
    LAF = LFR + RWS;

    cout << "[DEBUG] Start receiving data (SACK approach)..." << endl;
    while (true) {
        uint8_t buffer[65536];
        string sIp;
        int32_t sPort;
        ssize_t rec = connection->recvFrom(buffer, sizeof(buffer), sIp, sPort);

        if (rec < 20) {
            if (rec < 0) {
                perror("[-] recv error");
            }
            continue;
        }
        if (sIp != senderIp || sPort != senderPort) {
            cerr << "[!] Packet from unknown source => ignoring." << endl;
            continue;
        }

        Segment seg;
        memcpy(&seg, buffer, 20);
        size_t payloadSize = rec - 20;
        if (payloadSize > 0 && payloadSize <= MAX_PAYLOAD_SIZE) {
            memcpy(seg.payload, buffer+20, payloadSize);
            seg.payloadSize = payloadSize;
        } else {
            seg.payloadSize = 0;
        }

        if (!isValidChecksum(seg)) {
            cerr << "[!] Corrupt segment => drop." << endl;
            continue;
        }

        if (seg.flags & FIN_FLAG) {
            cout << "[DEBUG] FIN received seq=" << seg.seqNum << ". Send FIN-ACK, break loop." << endl;
            sendAckSegment(seg.seqNum); 
            break;
        }

        uint32_t seqNum = seg.seqNum;
        if (seqNum <= LFR) {
            sendSack(); 
            continue;
        }
        if (seqNum > LAF) {
            cerr << "[DEBUG] seq=" << seqNum << " > LAF=" << LAF << " => drop." << endl;
            continue;
        }

        outOfOrderMap[seqNum] = vector<uint8_t>(seg.payload, seg.payload + seg.payloadSize);

        // majuin LFR kalau seqNum == LFR+1
        if (seqNum == LFR+1) {
            while (outOfOrderMap.find(LFR+1) != outOfOrderMap.end()) {
                LFR++;
            }
            LAF = LFR + RWS;
        }
        // kirim SACK
        sendSack();
    }

    cout << "[DEBUG] Reconstruct data from 1..LFR" << endl;
    size_t totalData = 0;
    for (auto &kv : outOfOrderMap) {
        totalData += kv.second.size();
    }
    vector<uint8_t> fullData;
    fullData.reserve(totalData);

    if (!outOfOrderMap.empty()) {
        uint32_t minSeq = outOfOrderMap.begin()->first;
        uint32_t maxSeq = outOfOrderMap.rbegin()->first;
        for (uint32_t s = minSeq; s <= maxSeq; s++) {
            if (outOfOrderMap.find(s) != outOfOrderMap.end()) {
                auto &payload = outOfOrderMap[s];
                fullData.insert(fullData.end(), payload.begin(), payload.end());
            }
        }
    }

    cout << "[+] Data size = " << fullData.size() << endl;

    uint32_t customCsum = computeCustomChecksum(fullData);
    cout << "[DEBUG] My custom checksum = " << customCsum << " (unsigned 32-bit)" << endl;
    cout << "[DEBUG] My custom checksum (hex) = 0x" << hex << customCsum << dec << endl;

    cout << "[?] Do you want to save the received data into a file? (y/n): ";
    char choice;
    cin >> choice;
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); 

    if (choice == 'y' || choice == 'Y') {
        cout << "[?] Please enter output filename (will be saved in 'received/' folder): ";
        string outFilename;
        getline(cin, outFilename);

        if (outFilename.empty()) {
            outFilename = "output.bin";
        }

        string fullPath = "received/" + outFilename;
        FILE *fOut = fopen(fullPath.c_str(), "wb");
        if (!fOut) {
            cerr << "[-] Cannot open " << fullPath << " for writing." << endl;
        } else {
            size_t written = fwrite(fullData.data(), 1, fullData.size(), fOut);
            fclose(fOut);
            cout << "[+] Successfully saved " << written 
                 << " bytes to " << fullPath << endl;
        }
    } else {
        cout << "[DEBUG] User chose not to save. Displaying data in console." << endl;
        cout << "-----BEGIN DATA-----" << endl;
        for (uint8_t b : fullData) {
            cout << (char)b;
        }
        cout << endl << "------END DATA------" << endl;
    }

    connection->closeSocket();
    cout << "[i] Receiver closed." << endl;
}

bool Receiver::performHandshake(const string &sIp, int32_t sPort) {
    cout << "[+] Starting three-way handshake on receiver side..." << endl;

    int sockfd = connection->getSocketFD();
    uint16_t srcPort = localPort;
    uint32_t seqNum  = baseSeqNum;
    uint8_t dataOffset = 5;
    uint8_t flags = SYN_FLAG;

    int attempt = 0;
    bool done = false;

    while (attempt < maxHandshakeRetry) {
        attempt++;
        cout << "[Handshake] Attempt " << attempt << "/" << maxHandshakeRetry << endl;

        Segment synSeg = createSegment(srcPort, sPort, seqNum, 0, dataOffset, flags, 0, 0, nullptr, 0);

        uint8_t sendBuf[20];
        memcpy(sendBuf, &synSeg, 20);

        bool sendOk = connection->sendTo(sIp, sPort, sendBuf, 20);
        if (!sendOk) {
            cerr << "[-] Failed to send SYN." << endl;
        } else {
            cout << "[Handshake] SYN sent seq=" << seqNum << endl;
            connection->changeStatus(SYN_SENT);
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        timeval tv;
        tv.tv_sec  = 2 + attempt; 
        tv.tv_usec = 0;

        int ret = select(sockfd+1, &readfds, NULL, NULL, &tv);
        if (ret <= 0) {
            cerr << "[Handshake] Timeout waiting for SYN-ACK." << endl;
            continue;
        }

        if (!FD_ISSET(sockfd, &readfds)) {
            continue;
        }

        uint8_t buf[65536];
        string ipR;
        int32_t pR;
        ssize_t rc = connection->recvFrom(buf, sizeof(buf), ipR, pR);
        if (rc < 20 || ipR != sIp || pR != sPort) {
            cerr << "[-] Possibly invalid or mismatch for SYN-ACK." << endl;
            continue;
        }

        Segment rx;
        memcpy(&rx, buf, 20);
        size_t payLen = rc - 20;
        if (payLen > 0 && payLen <= MAX_PAYLOAD_SIZE) {
            memcpy(rx.payload, buf + 20, payLen);
            rx.payloadSize = payLen;
        } else {
            rx.payloadSize = 0;
        }

        if (!isValidChecksum(rx)) {
            cerr << "[Handshake] Corrupt SYN-ACK segment." << endl;
            continue;
        }

        if (!( (rx.flags & SYN_FLAG) && (rx.flags & ACK_FLAG))) {
            cerr << "[Handshake] Not a SYN-ACK." << endl;
            continue;
        }

        cout << "[Handshake] SYN-ACK seq=" << rx.seqNum << ", ack=" << rx.ackNum << endl;

        uint8_t ackFlag = ACK_FLAG;
        Segment ackSeg = createSegment(srcPort, sPort, seqNum, rx.seqNum+1, dataOffset, ackFlag, 0, 0, nullptr, 0);

        memcpy(sendBuf, &ackSeg, 20);
        bool ackOk = connection->sendTo(sIp, sPort, sendBuf, 20);
        if (!ackOk) {
            cerr << "[-] Fail to send final ACK." << endl;
            continue;
        }
        cout << "[Handshake] ACK sent ack=" << ackSeg.ackNum << endl;
        connection->changeStatus(ESTABLISHED);

        initialAckNum = ackSeg.ackNum;
        done = true;
        break;
    }

    return done;
}

void Receiver::sendAckSegment(uint32_t ackNum) {
    cout << "[DEBUG] sendAckSegment => FIN-ACK for seq=" << ackNum << endl;

    Segment ackSeg = createSegment(localPort, senderPort,
                                   baseSeqNum, ackNum+1,
                                   5, ACK_FLAG,
                                   0, 0,
                                   nullptr, 0);
    uint8_t buf[20];
    memcpy(buf, &ackSeg, 20);
    connection->sendTo(senderIp, senderPort, buf, 20);
}

void Receiver::sendSack() {
    char msg[1024];
    sprintf(msg, "SACK %u", LFR);

    int count = 0;
    for (auto &kv : outOfOrderMap) {
        uint32_t s = kv.first;
        if (s > LFR) {
            char tmp[32];
            sprintf(tmp, " %u", s);
            strcat(msg, tmp);
            count++;
            if (count >= 30) break;
        }
    }

    size_t len = strlen(msg);
    connection->sendTo(senderIp, senderPort, msg, len);
    cout << "[DEBUG] sendSack => \"" << msg << "\"" << endl;
}

void Receiver::handleMessage(const uint8_t *buffer, size_t size,
                             const string &receiverIp, int32_t receiverPort)
{
    cout << "[DEBUG] handleMessage() => Not used for now." << endl;
}
