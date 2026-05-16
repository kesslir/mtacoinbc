#pragma once

#include <string>
#include <fstream>

struct BLOCK_T {
    int height; // Incremental ID of the block in the chain
    int timestamp;
    unsigned int hash;
    unsigned int prev_hash;
    int difficulty; // Number of preceding zeroes
    int nonce; // Incremental int to change the hash value
    int relayed_by; // Miner ID
};

inline constexpr int LEN_HEADER = 4;
inline constexpr int MAX_MINERS = 4;
const std::string HEADER_BLOCK = "BLK:";
const std::string HEADER_SUBSCRIBE = "SUB:";
const std::string LOG_PATH = "/var/log/mtacoin.log";
const std::string CONFIG_PATH = "/mnt/mta/mtacoin.conf";
const std::string MINER_PIPE_PREFIX = "/mnt/mta/miner_pipe_";
const std::string SERVER_PIPE = "/mnt/mta/server_pipe";

unsigned int calculateChecksum(const BLOCK_T &block);
bool validateDifficulty(unsigned int checksum, int difficulty);
void log_message(std::ofstream &logFile, const std::string &message);