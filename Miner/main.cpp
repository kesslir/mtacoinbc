#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstring>
#include <csignal>
#include <ctime>
#include "../block.h"

struct MinerState {
    int minerID = -1;
    int difficulty = 0;
    BLOCK_T blockToMine = {};
};

volatile sig_atomic_t running = 1;

void sigint_handler(int /*signum*/) {
    running = 0;
}

int claimMinerID();
bool subscribeAndRequestBlock(MinerState &state, int serverFD, std::ofstream &log);
void mineBlocks(MinerState &state, int serverFD, int minerFD, std::ofstream &log);
void miner_func();

int claimMinerID() {
    for (int i = 1; i <= MAX_MINERS; i++) {
        std::string pipe = MINER_PIPE_PREFIX + std::to_string(i);

        if (mkfifo(pipe.c_str(), 0666) == 0) {
            return i;
        }

        if (errno != EEXIST) {
            break;
        }
    }

    return -1; // no available ID's
}

bool subscribeAndRequestBlock(MinerState &state, int serverFD, std::ofstream &log) {
    std::string pipeName = MINER_PIPE_PREFIX + std::to_string(state.minerID);
    std::string msg = HEADER_SUBSCRIBE + "Miner #" + std::to_string(state.minerID) +
                      " sent connect request on " + pipeName;

    ssize_t n = write(serverFD, msg.c_str(), msg.length());
    if (n < 0) {
        log_message(log, "ERROR: Failed to send subscription request: " + std::string(strerror(errno)));
        return false;
    }

    log_message(log, msg.substr(LEN_HEADER));

    // open pipe and wait for response from server
    int minerFD = open(pipeName.c_str(), O_RDONLY);
    if (minerFD == -1) {
        log_message(log, std::string("ERROR: Failed to open miner #" + std::to_string(state.minerID) + " pipe"));
        return false;
    }

    n = read(minerFD, &state.blockToMine, sizeof(BLOCK_T));
    close(minerFD);

    if (n < 0) {
        if (errno != EINTR) {
            log_message(log, "ERROR: read failed: " + std::string(strerror(errno)));
        }

        return false;
    }

    if (n != static_cast<ssize_t>(sizeof(BLOCK_T))) {
        log_message(log, "ERROR: Failed to read block from server");
        return false;
    }

    char logBuffer[512];
    std::sprintf(
            logBuffer,
            "Miner #%d received first block: relayed_by(%d), height(%d), timestamp (%d), hash(0x%x), prev_hash(0x%x), difficulty(%d), nonce(%d)",
            state.minerID, state.blockToMine.relayed_by, state.blockToMine.height, state.blockToMine.timestamp,
            state.blockToMine.hash, state.blockToMine.prev_hash, state.blockToMine.difficulty, state.blockToMine.nonce);
    log_message(log, std::string(logBuffer));

    return true;
}

void mineBlocks(MinerState &state, int serverFD, int minerFD, std::ofstream &log) {
    char logBuff[512];
    char sendBuff[sizeof(BLOCK_T) + LEN_HEADER];

    memcpy(sendBuff, HEADER_BLOCK.c_str(), LEN_HEADER);

    while (running) {
        state.blockToMine.relayed_by = state.minerID;
        state.blockToMine.nonce = 0;
        state.difficulty = state.blockToMine.difficulty;

        while (running) {
            state.blockToMine.nonce++;
            state.blockToMine.timestamp = static_cast<int>(time(nullptr));
            state.blockToMine.hash = calculateChecksum(state.blockToMine);

            if (validateDifficulty(state.blockToMine.hash, state.difficulty)) {
                std::sprintf(logBuff, "Miner #%d mined a new block #%d, with the hash 0x%x, difficulty %d",
                             state.minerID,
                             state.blockToMine.height, state.blockToMine.hash, state.blockToMine.difficulty);
                log_message(log, std::string(logBuff));

                // add block data after HEADER_BLOCK
                memcpy(sendBuff + LEN_HEADER, &state.blockToMine, sizeof(BLOCK_T));
                ssize_t n = write(serverFD, sendBuff, sizeof(sendBuff));
                if (n < 0) {
                    log_message(log, "ERROR: Failed to send mined block: " + std::string(strerror(errno)));
                    return;
                }
            }

            // check if a new block is available
            BLOCK_T incoming = {};
            ssize_t n = read(minerFD, &incoming, sizeof(BLOCK_T));

            if (n == static_cast<ssize_t>(sizeof(BLOCK_T)) && incoming.height != 0 && incoming.height != state.
                    blockToMine.height) {
                state.blockToMine = incoming;
                std::sprintf(logBuff,
                             "Miner #%d received a new block: height(%d), prev_hash(0x%x), difficulty(%d)",
                             state.minerID, incoming.height, incoming.prev_hash, incoming.difficulty);
                log_message(log, logBuff);
                break;
            }
        }
    }
}

void miner_func() {
    MinerState state;

    state.minerID = claimMinerID();
    if (state.minerID == -1) {
        std::cerr << "ERROR: No available miner IDs (max " << MAX_MINERS << ")" << std::endl;
        return;
    }

    std::string pipeName = MINER_PIPE_PREFIX + std::to_string(state.minerID);

    std::ofstream log(LOG_PATH);
    if (!log.is_open()) {
        std::cerr << "Failed to open log file: " << LOG_PATH << std::endl;
        unlink(pipeName.c_str());
        return;
    }

    int serverFD = open(SERVER_PIPE.c_str(), O_WRONLY);
    if (serverFD == -1) {
        log_message(log, "ERROR: Failed to open server pipe");
        log.close();
        unlink(pipeName.c_str());
        return;
    }

    if (!subscribeAndRequestBlock(state, serverFD, log)) {
        close(serverFD);
        log.close();
        unlink(pipeName.c_str());
        return;
    }

    int minerFD = open(pipeName.c_str(), O_RDONLY | O_NONBLOCK);
    if (minerFD == -1) {
        log_message(log, "ERROR: Failed to open miner pipe (non-blocking)");
        close(serverFD);
        log.close();
        unlink(pipeName.c_str());

        return;
    }

    mineBlocks(state, serverFD, minerFD, log);

    log_message(log, "Miner #" + std::to_string(state.minerID) + " shutting down.");
    close(minerFD);
    close(serverFD);
    unlink(pipeName.c_str());
    log.close();
}

int main() {
    struct sigaction sa = {};
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, nullptr) == -1 || sigaction(SIGTERM, &sa, nullptr) == -1) {
        std::cerr << "Failed to set signal handler" << std::endl;
        return 1;
    }

    struct sigaction sa_pipe = {};
    sa_pipe.sa_handler = SIG_IGN;

    if (sigaction(SIGPIPE, &sa_pipe, nullptr) == -1) {
        std::cerr << "Failed to ignore SIGPIPE" << std::endl;
        return 1;
    }

    miner_func();

    return 0;
}