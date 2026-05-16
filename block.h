#pragma once

#include <string>

struct BLOCK_T {
    int height; // Incremental ID of the block in the chain
    int timestamp;
    unsigned int hash;
    unsigned int prev_hash;
    int difficulty; // Number of preceding zeroes
    int nonce; // Incremental int to change the hash value
    int relayed_by; // Miner ID
};

inline constexpr int LEN_MESSAGE_MAX = 512;
inline constexpr int LEN_HEADER = 4;
inline constexpr int MAX_MINERS = 4;
inline const std::string HEADER_BLOCK = "BLK:";
inline const std::string HEADER_SUBSCRIBE = "SUB:";
inline const std::string CONFIG_PATH = "/mnt/mta/mtacoin.conf";
inline const std::string MINER_PIPE_PREFIX = "/mnt/mta/miner_pipe_";
inline const std::string SERVER_PIPE = "/mnt/mta/server_pipe";

unsigned int calculateChecksum(const BLOCK_T &block);
bool validateDifficulty(unsigned int checksum, int difficulty);
void log_message(const std::string &message);
void log_error(const std::string &message);