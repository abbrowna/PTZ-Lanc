#include "mbed.h"
#include "FlashIAPBlockDevice.h"

#define STATIC_REGION_BASE   0xC00000  // 12MB offset
#define HTML_OFFSET          0x000000
#define HTML_SIZE            0x80000   // 512KB
#define CSS_OFFSET           0x80000
#define CSS_SIZE             0x80000   // 512KB
#define JS_OFFSET            0x100000
#define JS_SIZE              0x100000  // 1MB

class StaticStorageClass {
public:
    enum FileType { HTML, CSS, JS };

    int open(FileType type, size_t length) {
        close();

        FlashIAPBlockDevice* dev = getBlockDevice(type);
        if (!dev) return -1;
        int err = dev->init();
        if (err) {
            Serial.print("StaticStorage: BlockDevice init failed: "); Serial.println(err);
            return -2;
        }

        erase_size = dev->get_erase_size(0);
        program_size = dev->get_program_size();
        region_size = getSize(type);

        // Erase region (must be aligned)
        if (region_size % erase_size != 0) {
            Serial.println("StaticStorage: Region size not aligned to erase size!");
            dev->deinit();
            return -3;
        }
        err = dev->erase(0, region_size);
        if (err) {
            Serial.print("StaticStorage: Erase failed: "); Serial.println(err);
            dev->deinit();
            return -4;
        }

        // Write file size header (4 bytes, pad to program_size)
        uint8_t headerBuf[256]; // Use max possible program size
        memset(headerBuf, 0xFF, sizeof(headerBuf));
        uint32_t len32 = (uint32_t)length;
        memcpy(headerBuf, &len32, sizeof(len32));
        err = dev->program(headerBuf, 0, program_size);
        if (err) {
            Serial.print("StaticStorage: Write size header failed: "); Serial.println(err);
            dev->deinit();
            return -5;
        }

        writeOffset = program_size;
        fileType = type;
        bufferIndex = 0;
        memset(buffer, 0xFF, sizeof(buffer));
        isOpen = true;
        return 1;
    }

    // Buffered write
    size_t write(uint8_t c) {
        if (!isOpen) return 0;
        buffer[bufferIndex++] = c;
        if (bufferIndex == program_size) {
            if (!flushBuffer()) return 0;
        }
        return 1;
    }

    void close() {
        if (!isOpen) return;
        flushBuffer(true);
        for (int i = 0; i < 3; ++i) {
            if (blockDevices[i]) blockDevices[i]->deinit();
        }
        bufferIndex = 0;
        writeOffset = 0;
        isOpen = false;
    }

    // Read file size (from header)
    size_t getFileSize(FileType type) {
        FlashIAPBlockDevice* dev = getBlockDevice(type);
        if (!dev) return 0;
        if (dev->init() != 0) return 0;
        uint32_t len32 = 0;
        dev->read(&len32, 0, sizeof(len32));
        dev->deinit();
        return (size_t)len32;
    }

    // Read a file into a buffer (returns actual file size)
    size_t read(FileType type, uint8_t* outBuffer, size_t maxLen) {
        FlashIAPBlockDevice* dev = getBlockDevice(type);
        if (!dev) return 0;
        if (dev->init() != 0) return 0;
        uint32_t len32 = 0;
        dev->read(&len32, 0, sizeof(len32));
        size_t toRead = (size_t)len32;
        if (toRead > maxLen) toRead = maxLen;
        dev->read(outBuffer, program_size, toRead);
        dev->deinit();
        return toRead;
    }

    // Stream a file in chunks to a callback (for serving)
    template<typename Callback>
    void stream(FileType type, Callback cb, size_t chunkSize = 1024) {
        FlashIAPBlockDevice* dev = getBlockDevice(type);
        if (!dev) return;
        if (dev->init() != 0) return;
        uint32_t len32 = 0;
        dev->read(&len32, 0, sizeof(len32));
        size_t fileSize = (size_t)len32;
        size_t offset = program_size;
        static const size_t MAX_CHUNK = 1024;
        uint8_t chunkBuf[MAX_CHUNK];
        while (fileSize > 0) {
            size_t toRead = (fileSize > MAX_CHUNK) ? MAX_CHUNK : fileSize;
            dev->read(chunkBuf, offset, toRead);
            cb(chunkBuf, toRead);
            offset += toRead;
            fileSize -= toRead;
        }
        dev->deinit();
    }

    size_t getSize(FileType type) {
        switch (type) {
            case HTML: return HTML_SIZE;
            case CSS:  return CSS_SIZE;
            case JS:   return JS_SIZE;
            default:   return 0;
        }
    }

private:
    FileType fileType;
    size_t writeOffset = 0;
    size_t erase_size = 4096;
    size_t program_size = 256;
    size_t region_size = 0;
    static const size_t MAX_PROGRAM_SIZE = 256;
    uint8_t buffer[MAX_PROGRAM_SIZE];
    size_t bufferIndex = 0;
    bool isOpen = false;

    bool flushBuffer(bool final = false) {
        FlashIAPBlockDevice* dev = getBlockDevice(fileType);
        if (!dev) return false;
        size_t toWrite = bufferIndex;
        if (final && bufferIndex > 0 && bufferIndex < program_size) {
            memset(buffer + bufferIndex, 0xFF, program_size - bufferIndex);
            toWrite = program_size;
        }
        if (toWrite > 0) {
            int err = dev->program(buffer, writeOffset, toWrite);
            if (err) {
                Serial.print("StaticStorage: flushBuffer() failed: "); Serial.println(err);
                return false;
            }
            writeOffset += toWrite;
            bufferIndex = 0;
        }
        return true;
    }

    // One block device per region
    FlashIAPBlockDevice* getBlockDevice(FileType type) {
        if (!blockDevices[0]) {
            blockDevices[0] = new FlashIAPBlockDevice(XIP_BASE + STATIC_REGION_BASE + HTML_OFFSET, HTML_SIZE);
            blockDevices[1] = new FlashIAPBlockDevice(XIP_BASE + STATIC_REGION_BASE + CSS_OFFSET, CSS_SIZE);
            blockDevices[2] = new FlashIAPBlockDevice(XIP_BASE + STATIC_REGION_BASE + JS_OFFSET, JS_SIZE);
        }
        switch (type) {
            case HTML: return blockDevices[0];
            case CSS:  return blockDevices[1];
            case JS:   return blockDevices[2];
            default:   return nullptr;
        }
    }
    FlashIAPBlockDevice* blockDevices[3] = {nullptr, nullptr, nullptr};
};

// Global instance
StaticStorageClass StaticStorage;