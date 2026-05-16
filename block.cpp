#include "block.h"
#include <zlib.h>

unsigned int calculateChecksum(const BLOCK_T &block) { 
    uLong crc = crc32(0L, Z_NULL, 0);

    crc = crc32(crc, reinterpret_cast<const Bytef *>(&block.height), sizeof(block.height));
    crc = crc32(crc, reinterpret_cast<const Bytef *>(&block.timestamp), sizeof(block.timestamp));
    crc = crc32(crc, reinterpret_cast<const Bytef *>(&block.prev_hash), sizeof(block.prev_hash));
    crc = crc32(crc, reinterpret_cast<const Bytef *>(&block.nonce), sizeof(block.nonce));
    crc = crc32(crc, reinterpret_cast<const Bytef *>(&block.relayed_by), sizeof(block.relayed_by));

    return crc;
}

bool validateDifficulty(unsigned int checksum, int difficulty) {
    int numOfLeadingZeroes = 0;

    //count number of leading zeroes
    for (int i = 31; i >= 0; i--) {
        if ((checksum >> i) & 1)
            break;

        numOfLeadingZeroes++;
    }

    return numOfLeadingZeroes >= difficulty;
}

void log_message(std::ofstream& log, const std::string& message) {
    log << message << std::endl;
}