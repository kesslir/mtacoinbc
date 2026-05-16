#include <iostream>
#include <list>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstring>
#include <csignal>
#include <ctime>
#include <cerrno>
#include "../block.h"

struct ServerState {
    int difficulty = 0;
    BLOCK_T blockToMine = {};
    std::list<std::pair<int, int> > minerSubscribed; // (minerID, pipeFD)
    std::list<BLOCK_T> blockchain;
};

volatile sig_atomic_t running = 1;

void sigint_handler(int /*signum*/) {
    running = 0;
}

void setDefaultDifficulty(ServerState &state);
void readDifficulty(ServerState &state);
BLOCK_T createGenesisBlock(int difficulty);
BLOCK_T createNewBlock(const BLOCK_T &mined, int chainSize, int difficulty);
bool sendBlockToMiner(int minerFD, const BLOCK_T &newBlock);
bool blockIsValid(unsigned int checksum, const ServerState &state, const BLOCK_T &minedBlock);
void handleSubscriptionRequest(const char *buffer, ServerState &state);
void handleMinedBlockRequest(const char *buffer, ServerState &state);
void run(ServerState &state, int serverFD);
void server_func();

void setDefaultDifficulty(ServerState &state) {
    log_message("Setting difficulty to 16 (default)");
    state.difficulty = 16;
}

void readDifficulty(ServerState &state) {
    //open config file
    int configFD = open(CONFIG_PATH.c_str(), O_RDONLY);
    if (configFD == -1) {
        log_error("ERROR: No configuration file");
        setDefaultDifficulty(state);
        return;
    }


    char buf[256];
    ssize_t n = read(configFD, buf, sizeof(buf) - 1);

    close(configFD);

    if (n <= 0) {
        log_error("ERROR: No data in configuration file");
        setDefaultDifficulty(state);
        return;
    }

    buf[n] = '\0';
    log_message("Reading " + CONFIG_PATH + "..");

    char *eq = strstr(buf, "DIFFICULTY") ? strchr(buf, '=') : nullptr;
    if (!eq) {
        log_error("ERROR: Invalid format in config file");
        setDefaultDifficulty(state);
        return;
    }

    char *end;
    long difficulty = strtol(eq + 1, &end, 10);

    if (eq + 1 == end) {
        log_error("ERROR: No value for difficulty in config file");
        setDefaultDifficulty(state);
    } else if (difficulty >= 0 && difficulty <= 31) {
        log_message("Difficulty set to " + std::to_string(difficulty));
        state.difficulty = static_cast<int>(difficulty);
    } else {
        log_error("ERROR: Difficulty must be between 0 and 31");
        setDefaultDifficulty(state);
    }
}

BLOCK_T createGenesisBlock(int difficulty) {
    BLOCK_T genesisBlock = {};

    genesisBlock.difficulty = difficulty;
    genesisBlock.hash = 0xAAAAAAAA;
    genesisBlock.timestamp = static_cast<int>(time(nullptr));
    genesisBlock.relayed_by = -1;

    return genesisBlock;
}

BLOCK_T createNewBlock(const BLOCK_T &mined, int chainSize, int difficulty) {
    BLOCK_T block = {};

    block.prev_hash = mined.hash;
    block.height = chainSize;
    block.difficulty = difficulty;

    return block;
}

bool sendBlockToMiner(int minerFD, const BLOCK_T &newBlock) {
    ssize_t n = write(minerFD, &newBlock, sizeof(BLOCK_T));
    if (n < 0) {
        log_error("ERROR: Failed to send block to miner: " + std::string(strerror(errno)));
        return false;
    }

    return true;
}

bool blockIsValid(unsigned int checksum, const ServerState &state, const BLOCK_T &minedBlock) {
    char buffer[LEN_MESSAGE_MAX];

    if (!validateDifficulty(checksum, state.difficulty)) {
        std::sprintf(buffer, "Miner #%d provided bad hash (0x%x) for block.", minedBlock.relayed_by,
                     minedBlock.hash);
        log_error(buffer);
        return false;
    }

    if (checksum != minedBlock.hash) {
        std::sprintf(buffer, "Miner #%d provided hash (0x%x) but server calculated (0x%x).",
                     minedBlock.relayed_by, minedBlock.hash, checksum);
        log_error(buffer);
        return false;
    }

    if (minedBlock.prev_hash != state.blockchain.front().hash || minedBlock.height != state.blockToMine.height) {
        std::sprintf(
            buffer,
            "Miner #%d provided incorrect prev_hash (0x%x), does not reference most recent block in blockchain (0x%x).",
            minedBlock.relayed_by, minedBlock.prev_hash, state.blockchain.front().hash);
        log_error(buffer);
        return false;
    }

    return true;
}

void handleSubscriptionRequest(const char *buffer, ServerState &state) {
    std::string line(buffer);
    std::size_t pos = line.find('#');

    if (pos == std::string::npos) {
        return;
    }

    int minerID;
    try {
        minerID = std::stoi(line.substr(pos + 1));
    } catch (const std::exception &e) {
        log_error("ERROR: Failed to parse miner ID: " + std::string(e.what()));
        return;
    }

    if (minerID <= 0 || minerID > MAX_MINERS) {
        log_error("ERROR: Invalid miner ID: " + std::to_string(minerID));
        return;
    }

    std::string pipeName = MINER_PIPE_PREFIX + std::to_string(minerID);

    int minerPipeFD = open(pipeName.c_str(), O_WRONLY); // could hang if miner 'dies' in-between handling
    if (minerPipeFD == -1) {
        log_error("ERROR: Failed to open miner #" + std::to_string(minerID) + " pipe");
        return;
    }

    state.minerSubscribed.emplace_back(minerID, minerPipeFD);
    log_message("Received connection request from miner #" + std::to_string(minerID) + ", pipe name: " + pipeName);

    if (!sendBlockToMiner(minerPipeFD, state.blockToMine)) {
        close(minerPipeFD);
        state.minerSubscribed.pop_back();
    }
}

void handleMinedBlockRequest(const char *buffer, ServerState &state) {
    BLOCK_T minedBlock = {};
    memcpy(&minedBlock, buffer + LEN_HEADER, sizeof(BLOCK_T));
    unsigned int checksum = calculateChecksum(minedBlock);

    if (!blockIsValid(checksum, state, minedBlock)) {
        return;
    }

    char logBuff[LEN_MESSAGE_MAX];
    std::sprintf(
        logBuff,
        "New block added by %d, attributes: height(%d), timestamp (%d), hash(0x%x), prev_hash(0x%x), difficulty(%d), nonce(%d)",
        minedBlock.relayed_by, minedBlock.height, minedBlock.timestamp, minedBlock.hash, minedBlock.prev_hash,
        state.difficulty, minedBlock.nonce);
    log_message(logBuff);

    state.blockchain.push_front(minedBlock);
    state.blockToMine = createNewBlock(minedBlock, static_cast<int>(state.blockchain.size()), state.difficulty);

    //send new block to subscribed miners
    for (auto it = state.minerSubscribed.begin(); it != state.minerSubscribed.end();) {
        if (sendBlockToMiner(it->second, state.blockToMine)) {
            ++it;
        } else {
            close(it->second);
            it = state.minerSubscribed.erase(it);
        }
    }
}

void run(ServerState &state, int serverFD) {
    char buffer[LEN_MESSAGE_MAX];

    while (running) {
        ssize_t bytesRead = read(serverFD, buffer, sizeof(buffer) - 1);

        if (bytesRead < 0) {
            if (errno != EINTR) {
                log_error("ERROR: read failed: " + std::string(strerror(errno)));
            }

            break;
        }

        if (bytesRead == 0) {
            log_message("All miners disconnected.");
            break;
        }

        //check which type of data was read by checking the header
        if (strncmp(buffer, HEADER_SUBSCRIBE.c_str(), LEN_HEADER) == 0) {
            buffer[bytesRead] = '\0';
            handleSubscriptionRequest(buffer, state);
        } else if (strncmp(buffer, HEADER_BLOCK.c_str(), LEN_HEADER) == 0) {
            handleMinedBlockRequest(buffer, state);
        }
    }
}

void server_func() {
    ServerState state;

    readDifficulty(state);

    mkfifo(SERVER_PIPE.c_str(), 0666);
    int serverFD = open(SERVER_PIPE.c_str(), O_RDONLY);

    if (serverFD == -1) {
        log_error("ERROR: Failed to open server pipe");
        unlink(SERVER_PIPE.c_str());
        return;
    }

    log_message("Listening on " + SERVER_PIPE);

    BLOCK_T genesisBlock = createGenesisBlock(state.difficulty);
    state.blockchain.push_front(genesisBlock);

    //create new block to mine based on genesis' data
    state.blockToMine.prev_hash = state.blockchain.front().hash;
    state.blockToMine.height = 1;
    state.blockToMine.difficulty = state.difficulty;

    run(state, serverFD);

    log_message("Server shutting down.");
    for (auto &m: state.minerSubscribed)
        close(m.second);

    close(serverFD);
    unlink(SERVER_PIPE.c_str());
}

int main() {
    struct sigaction sa = {};
    sa.sa_handler = sigint_handler;

    if (sigaction(SIGINT, &sa, nullptr) == -1 || sigaction(SIGTERM, &sa, nullptr) == -1) {
        log_error("Failed to set signal handler");
        return 1;
    }

    struct sigaction sa_pipe = {};
    sa_pipe.sa_handler = SIG_IGN;

    if (sigaction(SIGPIPE, &sa_pipe, nullptr) == -1) {
        log_error("Failed to ignore SIGPIPE");
        return 1;
    }

    server_func();

    return 0;
}
