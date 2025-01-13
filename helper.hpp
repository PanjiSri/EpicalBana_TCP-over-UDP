#ifndef HELPER_H
#define HELPER_H

#include <iostream>
#include <cstring>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

using namespace std;

/**
 * Fungsi untuk menghasilkan bilangan acak 32-bit dari /dev/urandom
 */
uint32_t generateSecureRandomNumber();

/**
 * Buat ngambil current time (millisecond)
 */
uint64_t getCurrentTimeMillis();

/**
 * Buat ngitung selisih waktu (ms) antara dua timestamp
 */
uint64_t timeDiffMillis(uint64_t start, uint64_t end);

#endif
