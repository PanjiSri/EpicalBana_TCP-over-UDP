#include "sender.hpp"
#include "segment.hpp"
#include "helper.hpp"
#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <algorithm>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sstream>
#include <cstdio>

using namespace std;


Sender::Sender(const string &host, int32_t port) : host(host), port(port)
{
    connection = new TCPSocket();

    SWS = 4;  
    baseSeqNum = generateSecureRandomNumber();
    LAR = baseSeqNum;
    LFS = baseSeqNum;
    MaxSeqNum = 4000000000;

    RTO = 1200;
    alpha = 0.125;
    beta  = 0.25;
    SRTT = 0;
    RTTVAR = 0;

    lastAckNum = baseSeqNum;
    dupAckCount = 0;

    maxHandshakeRetry = 10;
    maxFinRetry       = 5;
}

void Sender::run() {
    cout << "[DEBUG] Sender::run() start..." << endl;

    int mode;
    cout << "[+] Node is now a sender" << endl;
    cout << "[?] Please choose the sending mode" << endl;
    cout << "[?] 1. User input" << endl;
    cout << "[?] 2. File input" << endl;
    cout << "[?] Input: ";
    cin >> mode;
    cin.ignore();

    if (mode == 1) {
        cout << "[?] Input mode chosen, please enter your input: ";
        getline(cin, dataToSend);
        cout << "[+] User input has been successfully received." << endl;
    } else if (mode == 2) {
        string filePath;
        cout << "[?] File mode chosen, please enter the file path: ";
        getline(cin, filePath);
        FILE *file = fopen(filePath.c_str(), "rb");
        if (!file) {
            cerr << "[-] Failed to open file." << endl;
            exit(EXIT_FAILURE);
        }
        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        rewind(file);

        char *buffer = new char[fileSize];
        fread(buffer, 1, fileSize, file);
        fclose(file);

        dataToSend.assign(buffer, fileSize);
        delete[] buffer;
        cout << "[+] File has been successfully read." << endl;
    } else {
        cerr << "[-] Invalid mode selected." << endl;
        exit(EXIT_FAILURE);
    }

    cout << "[i] Binding to " << host << ":" << port << endl;
    if (!connection->bindSocket(host, port)) {
        cerr << "[-] Failed to bind socket." << endl;
        exit(EXIT_FAILURE);
    }

    int broadcastEnable = 1;
    if (setsockopt(connection->getSocketFD(), SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("[-] Failed to set socket options for broadcast");
        exit(EXIT_FAILURE);
    }

    size_t offset = 0;
    uint32_t seq = baseSeqNum + 1;
    while (offset < dataToSend.size()) {

        size_t chunkSize = min((size_t)MAX_PAYLOAD_SIZE, dataToSend.size() - offset);

        Packet pkt;
        pkt.seqNum = seq++;
        pkt.payload.assign((uint8_t*)dataToSend.data() + offset, (uint8_t*)dataToSend.data() + offset + chunkSize);

        sendBuffer.push_back(pkt);
        offset += chunkSize;
    }

    cout << "[DEBUG] Total segments prepared: " << sendBuffer.size() << endl;
    for (size_t i = 0; i < sendBuffer.size(); i++) {
        cout << "  [DEBUG] sendBuffer[" << i << "].seqNum = " << sendBuffer[i].seqNum << ", payloadSize=" << sendBuffer[i].payload.size() << endl;
    }

    cout << "[DEBUG] Waiting for REQUEST from receiver..." << endl;
    while (true) {
        char buffer[1024];
        string senderIp;
        int32_t senderPort;
        ssize_t received = connection->recvFrom(buffer, sizeof(buffer), senderIp, senderPort);

        cout << "[DEBUG] recvFrom() => " << received << " bytes from " << senderIp << ":" << senderPort << endl;

        if (received > 0) {
            buffer[received] = '\0';
            cout << "[DEBUG] The message content: " << buffer << endl;

            if (strcmp(buffer, "REQUEST") == 0) {
                cout << "[+] Got REQUEST from " << senderIp << ":" << senderPort << endl;

                receiverIp   = senderIp;
                receiverPort = senderPort;

                cout << "[DEBUG] Attempting handshake with " << receiverIp << ":" << receiverPort << endl;

                bool hs = doHandshake(senderIp, senderPort);
                if (!hs) {
                    cerr << "[-] Handshake repeatedly failed. Exiting." << endl;
                    break;
                }

                cout << "[DEBUG] Handshake established. Now sending window..." << endl;
                sendWindow();

                while (LAR < (baseSeqNum + sendBuffer.size())) {
                    bool ackReceived = waitForAckOrTimeout();
                    if (!ackReceived) {
                        cout << "[!] Timeout - resending from LAR+1 => " << (LAR + 1) << endl; 
                        resendFrom(LAR + 1);
                    }
                }

                cout << "[DEBUG] All data ACKed. Proceed to send FIN..." << endl;
                bool finOk = sendFinAndWait();
                if (finOk) {
                    cout << "[+] FIN-ACK received, connection closed gracefully." << endl;
                } else {
                    cout << "[!] FIN-ACK not received after retries, closing anyway." << endl;
                }
                break;
            } else {
                // Mungkin message lain?
                cout << "[DEBUG] Unknown message => " << buffer << endl;
            }
        } else {
            cerr << "[DEBUG] No data => keep waiting for REQUEST..." << endl;
            // usleep(100000); 
        }
    }

    connection->closeSocket();
    cout << "[i] Sender done." << endl;
}


bool Sender::doHandshake(const string &rIp, int32_t rPort) {
    cout << "[DEBUG] doHandshake() start, rIp=" << rIp << ", rPort=" << rPort << ", maxHandshakeRetry=" << maxHandshakeRetry << endl;

    int sockfd = connection->getSocketFD();
    bool handshakeDone = false;

    for (int attempt = 1; attempt <= maxHandshakeRetry; attempt++) {
        cout << "[Handshake] Attempt " << attempt << "/" << maxHandshakeRetry << endl;

        uint64_t waitMs = 2000ULL * (1ULL << (attempt - 1));
        uint64_t startTime = getCurrentTimeMillis();

        cout << "[DEBUG] handleSyn() => waitMs=" << waitMs << " ms (exponential)" << endl;

        bool synOk = handleSyn(sockfd, startTime, waitMs, rIp, rPort);
        if (!synOk) {
            cerr << "[Handshake] handleSyn() failed => next attempt." << endl;
            continue;
        }

        bool gotFinalAck = false;
        uint64_t endTime = startTime + waitMs;

        while (!gotFinalAck && getCurrentTimeMillis() < endTime) {
            uint64_t now = getCurrentTimeMillis();
            uint64_t timeLeft = (now < endTime) ? (endTime - now) : 0;

            struct timeval tv;
            tv.tv_sec  = timeLeft / 1000;
            tv.tv_usec = (timeLeft % 1000) * 1000ULL;

            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(sockfd, &readfds);

            int ret = select(sockfd+1, &readfds, NULL, NULL, &tv);
            if (ret < 0) {
                perror("[Handshake] select error");
                continue;
            } 
            else if (ret == 0) {
                cerr << "[Handshake] partial timeout => can re-send SYN-ACK if needed." << endl;
                continue;
            }

            if (!FD_ISSET(sockfd, &readfds)) {
                continue;
            }

            uint8_t buffer[65536];
            string ipRecv;
            int32_t portRecv;
            ssize_t rec = connection->recvFrom(buffer, sizeof(buffer), ipRecv, portRecv);

            if (rec < 20 || ipRecv != rIp || portRecv != rPort) {
                cerr << "[Handshake] Possibly not our final ACK => ignore." << endl;
                continue;
            }

            Segment ackSegment;
            memcpy(&ackSegment, buffer, 20);

            size_t payloadLen = rec - 20;
            if (payloadLen > 0 && payloadLen <= MAX_PAYLOAD_SIZE) {
                memcpy(ackSegment.payload, buffer+20, payloadLen);
                ackSegment.payloadSize = payloadLen;
            } else {
                ackSegment.payloadSize = 0;
            }

            if (!isValidChecksum(ackSegment)) {
                cerr << "[Handshake] final ACK corrupt => ignore." << endl;
                continue;
            }
            if (ackSegment.flags & ACK_FLAG) {
                cout << "[Handshake] Final ACK => ESTABLISHED." << endl;
                connection->changeStatus(ESTABLISHED);
                gotFinalAck   = true;
                handshakeDone = true;
            }
        }

        if (handshakeDone) {
            break;
        } else {
            cerr << "[Handshake] Attempt #" << attempt 
                 << " => final ACK not received => next attempt." << endl;
        }
    }

    if (!handshakeDone) {
        cerr << "[-] Handshake repeatedly failed after "
             << maxHandshakeRetry << " attempts." << endl;
    }
    return handshakeDone;
}


bool Sender::handleSyn(int sockfd, uint64_t &startTime, uint64_t handshakeTimeoutMs, const string &rIp, int32_t rPort)
{
    cout << "[DEBUG] handleSyn() => handshakeTimeoutMs=" << handshakeTimeoutMs << ", rIp=" << rIp << ", rPort=" << rPort << endl;

    while (true) {
        uint64_t now = getCurrentTimeMillis();
        if (now - startTime > handshakeTimeoutMs) {
            cerr << "[-] Timeout waiting for SYN in handleSyn()." << endl;
            return false;
        }

        // sisa waktu
        uint64_t elapsed = now - startTime;
        uint64_t timeLeft = (handshakeTimeoutMs > elapsed) ? (handshakeTimeoutMs - elapsed) : 0;

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        struct timeval tv;
        tv.tv_sec  = timeLeft / 1000;
        tv.tv_usec = (timeLeft % 1000) * 1000ULL;

        int ret = select(sockfd+1, &readfds, NULL, NULL, &tv);
        if (ret < 0) {
            perror("[handleSyn] select error");
            continue;
        }
        if (ret == 0) {
            cerr << "[handleSyn] Timeout => no SYN => return false." << endl;
            return false;
        }

        if (!FD_ISSET(sockfd, &readfds)) {
            continue;
        }

        // Terima seg
        uint8_t buffer[60000];
        string senderIp;
        int32_t senderPort;
        ssize_t received = connection->recvFrom(buffer, sizeof(buffer), senderIp, senderPort);

        if (received < 20) {
            cerr << "[handleSyn] Possibly too small => ignore." << endl;
            continue;
        }
        if (senderIp != rIp || senderPort != rPort) {
            cerr << "[handleSyn] Not from the intended ip/port => ignore." << endl;
            continue;
        }

        Segment synSeg;
        memcpy(&synSeg, buffer, 20);

        size_t payLen = received - 20;
        if (payLen > 0 && payLen <= MAX_PAYLOAD_SIZE) {
            memcpy(synSeg.payload, buffer + 20, payLen);
            synSeg.payloadSize = payLen;
        } else {
            synSeg.payloadSize = 0;
        }

        if (!isValidChecksum(synSeg)) {
            cerr << "[handleSyn] SYN segment corrupt => ignore." << endl;
            continue;
        }
        if (!(synSeg.flags & SYN_FLAG)) {
            cerr << "[handleSyn] Not a SYN => ignore." << endl;
            continue;
        }

        // Kirim SYN-ACK
        cout << "[Handshake] SYN seq=" << synSeg.seqNum << " => sending SYN-ACK." << endl;
        connection->changeStatus(SYN_RECEIVED);

        uint16_t srcPort  = port;
        uint32_t seqNum   = baseSeqNum;
        uint8_t dataOffset = 5;
        uint8_t flags      = SYN_FLAG | ACK_FLAG;

        Segment synAck = createSegment(
            srcPort, rPort,
            seqNum,
            synSeg.seqNum + 1,
            dataOffset, flags,
            0, 0,
            nullptr, 0
        );

        uint8_t buf[20];
        memcpy(buf, &synAck, 20);

        bool ok = connection->sendTo(rIp, rPort, buf, 20);
        if (!ok) {
            cerr << "[-] Failed to send SYN-ACK." << endl;
            return false;
        }

        cout << "[Handshake] [S=" << synAck.seqNum << "] [A=" << synAck.ackNum << "] SYN-ACK sent." << endl;

        return true;
    }

    return false;
}

bool Sender::waitForAckOrTimeout() {
    cout << "[DEBUG] waitForAckOrTimeout() => RTO=" << RTO << " ms" << endl;

    int sockfd = connection->getSocketFD();
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    struct timeval tv;
    tv.tv_sec  = RTO / 1000;
    tv.tv_usec = (RTO % 1000) * 1000;

    cout << "[DEBUG] select() => " << tv.tv_sec << "s " << tv.tv_usec << "us" << endl;

    int ret = select(sockfd+1, &readfds, NULL, NULL, &tv);
    cout << "[DEBUG] select() => ret=" << ret << endl;

    if (ret <= 0) {
        cout << "[DEBUG] Timeout or error => ret=" << ret << endl;
        return false; // "timeout"
    }
    if (!FD_ISSET(sockfd, &readfds)) {
        cout << "[DEBUG] FD not set => no data => return false" << endl;
        return false;
    }

    // Baca data
    uint8_t buf[65536];
    string ip;
    int32_t port;
    ssize_t recvLen = connection->recvFrom(buf, sizeof(buf), ip, port);
    cout << "[DEBUG] recvFrom => " << recvLen << " bytes from " << ip << ":" << port << endl;

    if (recvLen <= 0) {
        perror("[-] recvFrom error");
        return false;
    }
    
    // Cek apakah dari receiver kita
    if (ip != receiverIp || port != receiverPort) {
        cout << "[DEBUG] Packet from unknown => ignoring." << endl;
        return true;
    }

    // cek SACK
    if (recvLen >= 4 && buf[0] == 'S' && buf[1] == 'A' && buf[2] == 'C' && buf[3] == 'K')
    {
        string sackMsg((char*)buf, recvLen);
        cout << "[DEBUG] handleSackMessage => " << sackMsg << endl;
        handleSackMessage(sackMsg);
        return true;
    }
    else if (recvLen >= 20) {
        Segment seg;
        memcpy(&seg, buf, 20);

        size_t payLen = recvLen - 20;
        if (payLen > 0 && payLen <= MAX_PAYLOAD_SIZE) {
            memcpy(seg.payload, buf+20, payLen);
            seg.payloadSize = payLen;
        } else {
            seg.payloadSize = 0;
        }

        if (!isValidChecksum(seg)) {
            cerr << "[DEBUG] Corrupt 20-byte segment => ignoring." << endl;
            return true;
        }

        if ((seg.flags & ACK_FLAG) && seg.payloadSize == 0) {
            cout << "[DEBUG] 20-byte pure ACK => handleAck("<< seg.ackNum << ")" << endl;
            handleAck(seg.ackNum);
        }
        else if (seg.payloadSize >= 4) {
            string possibleSack((char*)seg.payload, seg.payloadSize);
            if (possibleSack.rfind("SACK", 0) == 0) {
                cout << "[DEBUG] Received SACK in segment => "<< possibleSack << endl;
                handleSackMessage(possibleSack);
            } else {
                cout << "[DEBUG] 20-byte seg, flags=0x"<< hex << (int)seg.flags << dec << ", unknown => ignoring." << endl;
            }
        } else {
            cout << "[DEBUG] 20-byte seg but no recognized flags => ignoring." << endl;
        }
        return true;
    }
    else if (recvLen == 4) {
        uint32_t ackNumNet;
        memcpy(&ackNumNet, buf, 4);
        uint32_t ackNum = ntohl(ackNumNet);
        cout << "[DEBUG] handleAck(" << ackNum << ") => raw ack int" << endl;
        handleAck(ackNum);
        return true;
    }
    else {
        if (recvLen >= 5) {
            string maybe((char*)buf, recvLen);
            if (maybe.rfind("SACK", 0) == 0) {
                cout << "[DEBUG] handleSackMessage => " << maybe << endl;
                handleSackMessage(maybe);
            } else {
                cout << "[DEBUG] unrecognized " << recvLen << " bytes => " << maybe << endl;
            }
        } else {
            cout << "[DEBUG] <4 bytes => ignoring." << endl;
        }
        return true;
    }
}


void Sender::handleMessage(const uint8_t *buffer, size_t size, const string &rxIp, int32_t rxPort)
{
    cout << "[DEBUG] handleMessage() => Not used for now." << endl;
}

void Sender::handleSackMessage(const string &sackMsg) {
    cout << "[DEBUG] handleSackMessage => " << sackMsg << endl;

    vector<string> tokens;
    {
        stringstream ss(sackMsg);
        string t;
        while (ss >> t) {
            tokens.push_back(t);
        }
    }
    if (tokens.size() < 2) return; 

    // tokens[0] = "SACK"
    // tokens[1] = <LFR>
    // tokens[2..] = seq out-of-order
    uint32_t newLFR = (uint32_t) stoul(tokens[1]);
    vector<uint32_t> sackedSeq;

    for (size_t i = 2; i < tokens.size(); i++) {
        uint32_t x = (uint32_t) stoul(tokens[i]);
        sackedSeq.push_back(x);
    }

    handleSack(newLFR, sackedSeq);
}


void Sender::handleSack(uint32_t newLFR, const vector<uint32_t> &sackedSeq) {
    cout << "[DEBUG] handleSack => newLFR=" << newLFR << ", sackedSeq.size=" << sackedSeq.size() << endl;

    if (newLFR > LAR && newLFR <= (baseSeqNum + sendBuffer.size())) {
        for (uint32_t seq = LAR + 1; seq <= newLFR; seq++) {
            auto it = segmentMap.find(seq);
            if (it != segmentMap.end() && !it->second.acked) {
                cout << "[DEBUG] Marking seg=" << seq
                     << " as acked (step A)" << endl;

                it->second.acked = true;
                if (!it->second.isRetrans) {
                    uint64_t rtt = timeDiffMillis( it->second.sendTimestamp, getCurrentTimeMillis());
                    cout << "[DEBUG] updateRTO(rtt=" << rtt << ") for seg=" << seq << endl;
                    updateRTO(rtt);
                }
            }
        }

        // Update LAR
        LAR = newLFR;
        cout << "[DEBUG] LAR updated => " << LAR << endl;
    }

    for (auto s : sackedSeq) {
        if (s >= LAR && s <= (baseSeqNum + sendBuffer.size())) {
            auto it = segmentMap.find(s);
            if (it != segmentMap.end() && !it->second.acked) {
                cout << "[DEBUG] Marking seg=" << s << " as acked (step B)" << endl;

                it->second.acked = true;
                if (!it->second.isRetrans) {
                    uint64_t rtt = timeDiffMillis(it->second.sendTimestamp, getCurrentTimeMillis());
                    cout << "[DEBUG] updateRTO(rtt=" << rtt << ") for seg=" << s << endl;
                    updateRTO(rtt);
                }
            }
        }
    }

    uint32_t maxS = 0;
    for (auto s : sackedSeq) {
        if (s > maxS) maxS = s;
    }
    if (maxS > LAR) {
        cout << "[DEBUG] Step C => Checking missing in range (" << (LAR + 1) << ".." << maxS << ")" << endl;

        for (uint32_t seq = LAR + 1; seq <= maxS; seq++) {
            auto it = segmentMap.find(seq);
            if (it != segmentMap.end()) {
                if (!it->second.acked) {
                    // ilang => fast retransmit
                    cout << "[DEBUG] SACK indicates missing seg " << seq << ", fast retransmit" << endl;
                    uint32_t idx = seq - (baseSeqNum + 1);
                    if (idx < sendBuffer.size()) {
                        sendSegment(idx, true);
                    } else {
                        cout << "[DEBUG] idx=" << idx << " out-of-range => skip" << endl;
                    }
                }
            } else {
                // Belum pernah dikirim => skip
                cout << "[DEBUG] seg=" << seq << " not found => not sent yet => skip" << endl;
            }
        }
    }

    // Kirim segmen baru kalau window masih muat
    if (LFS < (baseSeqNum + sendBuffer.size())) {
        cout << "[DEBUG] Step D => calling sendWindow()" << endl;
        sendWindow();
    }
}


void Sender::sendSegment(uint32_t idx, bool isRetrans) {
    if (idx >= sendBuffer.size()) {
        cerr << "[DEBUG] sendSegment() => idx out of range: " << idx << " / " << sendBuffer.size() << endl;
        return;
    }

    uint32_t seqNum = sendBuffer[idx].seqNum;
    vector<uint8_t> &pl = sendBuffer[idx].payload;

    Segment seg = createSegment(
        port, receiverPort,
        seqNum,
        0,
        5,
        PSH_FLAG,
        0, 0,
        pl.data(),
        pl.size()
    );

    const size_t headerSize = 20;
    uint8_t sendBuf[headerSize + MAX_PAYLOAD_SIZE];
    memcpy(sendBuf, &seg, headerSize);
    memcpy(sendBuf + headerSize, seg.payload, seg.payloadSize);

    SegmentInfo si;
    si.seqNum        = seqNum;
    si.sendTimestamp = getCurrentTimeMillis();
    si.acked         = false;
    si.isRetrans     = isRetrans;
    segmentMap[seqNum] = si;

    cout << "[DEBUG] Sending segment seq=" << seqNum << (isRetrans ? " [RETRANS]" : "") << ", payloadSize=" << pl.size() << endl;

    bool ok = connection->sendTo(receiverIp, receiverPort, sendBuf, headerSize + seg.payloadSize);

    if (!ok) {
        cerr << "[-] sendSegment => fail sending seq=" << seqNum << endl;
    }
}

void Sender::resendFrom(uint32_t seqNum) {
    cout << "[DEBUG] resendFrom(" << seqNum << ") => up to LFS=" << LFS << endl;

    for (uint32_t s = seqNum; s <= LFS; s++) {
        uint32_t idx = s - (baseSeqNum + 1);
        if (idx < sendBuffer.size()) {
            sendSegment(idx, true);
        } else {
            cout << "[DEBUG] resendFrom => idx=" << idx << " out-of-range => skip" << endl;
        }
    }

    RTO = min(RTO * 2, (uint64_t)30000);
    cout << "[DEBUG] RTO doubled => " << RTO << " ms" << endl;
}

void Sender::handleAck(uint32_t ackNum) {
    cout << "[DEBUG] handleAck(" << ackNum << ") => lastAckNum=" << lastAckNum << ", LAR=" << LAR << endl;

    if (ackNum == lastAckNum) {
        dupAckCount++;
        cout << "[DEBUG] Duplicate ACK => dupAckCount=" << dupAckCount << endl;

        if (dupAckCount == 3) {
            cout << "[DEBUG] 3 duplicate ACK => fast retransmit => " << (ackNum + 1) << endl;
            resendFrom(ackNum + 1);
        }
    } else if (ackNum > lastAckNum) {
        lastAckNum  = ackNum;
        dupAckCount = 0;
    }

    // update LAR
    if (ackNum > LAR && ackNum <= (baseSeqNum + sendBuffer.size())) {
        auto it = segmentMap.find(ackNum);
        if (it != segmentMap.end()) {
            if (!it->second.acked && !it->second.isRetrans) {
                // RTT
                uint64_t rtt = timeDiffMillis(it->second.sendTimestamp, getCurrentTimeMillis());
                cout << "[DEBUG] Measured RTT => " << rtt << " => updateRTO()" << endl;
                updateRTO(rtt);
            }

            it->second.acked = true;
        }

        LAR = ackNum;
        cout << "[DEBUG] LAR => " << LAR << endl;

        if (LAR < (baseSeqNum + sendBuffer.size())) {
            sendWindow();
        }

    } else {
        cout << "[DEBUG] ACK " << ackNum << " <= LAR or out-of-range => no update." << endl;
    }
}


void Sender::updateRTO(uint64_t measuredRTT) {
    cout << "[DEBUG] updateRTO() => measuredRTT=" << measuredRTT << ", SRTT=" << SRTT << ", RTTVAR=" << RTTVAR << endl;

    if (SRTT < 0.0001) {
        SRTT   = (double)measuredRTT;
        RTTVAR = (double)measuredRTT / 2.0;
        RTO    = (uint64_t)(SRTT + 4.0 * RTTVAR);

        if (RTO < 300) RTO = 300;
        cout << "[DEBUG] RTO init => " << RTO << " ms" << endl;
        return;
    }

    double rtt = (double)measuredRTT;
    double absErr = (SRTT > rtt) ? (SRTT - rtt) : (rtt - SRTT);

    RTTVAR = (1.0 - beta) * RTTVAR + beta * (absErr);
    SRTT   = (1.0 - alpha)*SRTT + alpha*rtt;

    double newRTO = SRTT + 4.0 * RTTVAR;
    if (newRTO < 300.0)   newRTO = 300.0;
    if (newRTO > 60000.0) newRTO = 60000.0;
    RTO = (uint64_t)newRTO;

    cout << "[DEBUG] updateRTO => SRTT=" << SRTT << ", RTTVAR=" << RTTVAR << ", RTO=" << RTO << " ms" << endl;
}

bool Sender::sendFinAndWait() {
    cout << "[DEBUG] sendFinAndWait() => maxFinRetry=" << maxFinRetry << endl;
    uint32_t seqNum = baseSeqNum + (uint32_t)sendBuffer.size() + 1;

    for (int i = 0; i < maxFinRetry; i++) {
        cout << "[DEBUG] FIN attempt #" << (i + 1) << " => seqNum=" << seqNum << endl;

        Segment finSeg = createSegment(port, receiverPort,
                                       seqNum, 0,
                                       5, FIN_FLAG,
                                       0, 0,
                                       nullptr, 0);

        uint8_t finBuf[20];
        memcpy(finBuf, &finSeg, 20);

        bool sentOk = connection->sendTo(receiverIp, receiverPort, finBuf, 20);

        cout << "[DEBUG] send FIN => " << (sentOk ? "ok" : "fail") << endl;

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(connection->getSocketFD(), &readfds);

        timeval tv;
        tv.tv_sec  = (RTO / 1000) + i;
        tv.tv_usec = (RTO % 1000) * 1000ULL;

        cout << "[DEBUG] Wait for FIN-ACK => select() " << tv.tv_sec << "s + " << tv.tv_usec << "us" << endl;

        int ret = select(connection->getSocketFD() + 1, &readfds, NULL, NULL, &tv);

        cout << "[DEBUG] select() ret=" << ret << endl;

        if (ret <= 0) {
            continue;
        }

        if (!FD_ISSET(connection->getSocketFD(), &readfds)) {
            continue;
        }

        uint8_t buf[65536];
        string ip;
        int32_t p;
        ssize_t r = connection->recvFrom(buf, sizeof(buf), ip, p);

        cout << "[DEBUG] recvFrom => r=" << r << " from " << ip << ":" << p << endl;

        if (r < 20 || ip != receiverIp || p != receiverPort) {
            cerr << "[DEBUG] Possibly not our FIN-ACK." << endl;
            continue;
        }

        Segment seg;
        memcpy(&seg, buf, 20);

        size_t payLen = r - 20;
        if (payLen > 0 && payLen <= MAX_PAYLOAD_SIZE) {
            memcpy(seg.payload, buf + 20, payLen);
            seg.payloadSize = payLen;
        } else {
            seg.payloadSize = 0;
        }

        cout << "[DEBUG] FIN-ACK seg => flags=0x" << hex << (int)seg.flags << dec << ", seqNum=" << seg.seqNum << ", ackNum=" << seg.ackNum << ", payloadSize=" << seg.payloadSize << endl;

        if (!isValidChecksum(seg)) {
            cerr << "[DEBUG] Corrupt FIN-ACK => ignoring." << endl;
            continue;
        }
        if (seg.flags & ACK_FLAG) {
            cout << "[DEBUG] Received FIN-ACK => done." << endl;
            return true;
        }
    }
    return false;
}

void Sender::sendWindow() {
    cout << "[DEBUG] sendWindow() => LAR=" << LAR
         << ", LFS=" << LFS
         << ", SWS=" << SWS << endl;

    // Selama LFS < total segmen, dan belum lebih gede dari ukuran window
    while ((LFS < (baseSeqNum + sendBuffer.size())) && ((LFS - LAR) < SWS))
    {
        uint32_t idx = LFS - (baseSeqNum + 1);
        cout << "[DEBUG] sendWindow loop => LFS=" << LFS << ", LAR=" << LAR << ", idx=" << idx << endl;

        sendSegment(idx, false);
        LFS++;
    }

    cout << "[DEBUG] sendWindow() done => LFS=" << LFS << ", LAR=" << LAR << endl;
}