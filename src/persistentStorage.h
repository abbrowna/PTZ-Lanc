#pragma once
#include "mbed.h"
#include "FlashIAPBlockDevice.h"

#define PERSIST_BLOCK_ADDR   (XIP_BASE + 0xC00000 - 4096)
#define PERSIST_BLOCK_SIZE   4096
#define HOSTNAME_OFFSET      0
#define HOSTNAME_SIZE        32

// Read the entire persistent block into buf (must be at least 4096 bytes)
inline void readPersistentBlock(uint8_t* buf) {
    FlashIAPBlockDevice dev(PERSIST_BLOCK_ADDR, PERSIST_BLOCK_SIZE);
    dev.init();
    dev.read(buf, 0, PERSIST_BLOCK_SIZE);
    dev.deinit();
}

// Write the entire persistent block from buf (must be at least 4096 bytes)
inline void writePersistentBlock(const uint8_t* buf) {
    FlashIAPBlockDevice dev(PERSIST_BLOCK_ADDR, PERSIST_BLOCK_SIZE);
    dev.init();
    dev.erase(0, PERSIST_BLOCK_SIZE);
    size_t program_size = dev.get_program_size();
    for (size_t offset = 0; offset < PERSIST_BLOCK_SIZE; offset += program_size) {
        dev.program(buf + offset, offset, program_size);
    }
    dev.deinit();
}

// Read hostname (null-terminated)
inline void readHostnameFromPersistent(char* out) {
    uint8_t buf[PERSIST_BLOCK_SIZE];
    readPersistentBlock(buf);
    memcpy(out, buf + HOSTNAME_OFFSET, HOSTNAME_SIZE);
    out[HOSTNAME_SIZE-1] = 0;
}

// Write hostname (null-terminated)
inline void writeHostnameToPersistent(const char* hostname) {
    uint8_t buf[PERSIST_BLOCK_SIZE];
    readPersistentBlock(buf); // preserve other variables
    memset(buf + HOSTNAME_OFFSET, 0, HOSTNAME_SIZE);
    strncpy((char*)(buf + HOSTNAME_OFFSET), hostname, HOSTNAME_SIZE-1);
    writePersistentBlock(buf);
}