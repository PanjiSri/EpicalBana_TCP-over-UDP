#include "helper.hpp"

uint32_t generateSecureRandomNumber() {
    uint32_t randomNumber;
    int urandom = open("/dev/urandom", O_RDONLY);
    if (urandom < 0) {
        cerr << "[-] Failed to open /dev/urandom" << endl;
        exit(EXIT_FAILURE);
    }
    read(urandom, &randomNumber, sizeof(randomNumber));
    close(urandom);
    return randomNumber;
}

uint64_t getCurrentTimeMillis() {
    timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t ms = (uint64_t)tv.tv_sec * 1000ULL + (tv.tv_usec / 1000ULL);
    return ms;
}

uint64_t timeDiffMillis(uint64_t start, uint64_t end) {
    if (end >= start) {
        return end - start;
    } else {
        return 0ULL;
    }
}