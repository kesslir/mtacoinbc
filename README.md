<h1 align="center">
  <a><img src="https://github.com/kesslir/mtacoinbc/blob/main/img/mtacoin.png" width="300"></a>
  <br>
  MTACoin Blockchain
  <br>
</h1>

<p align="center">Dockerized blockchain mining simulation using IPC</p>

<p align="center">
  <a href="#key-features">Key Features</a> •
  <a href="#how-to-use">How To Use</a> •
  <a href="#credits">Credits</a> •
  <a href="#license">License</a>
</p>

## Key Features

* Simulates a blockchain mining process
  - Miner containers continuously receive, mine (using CRC32) and submit blocks to the server for validation
* Per-container logging
  - Each container maintains its own log file, isolated from other containers
* Configurable difficulty
  - Control the number of leading zeros required in a valid block hash via a config file
* IPC via named pipes
  - Containers communicate through named pipes over a shared volume
* Graceful shutdown
  - Handles SIGINT and SIGTERM cleanly, miners and server exit with proper cleanup

## How To Use

To clone and run this application, you'll need [Git](https://git-scm.com), [Docker](https://github.com/docker), and [CMake](https://cmake.org):
 
```bash
# Clone repository
$ git clone https://github.com/kesslir/mtacoinbc
 
# Go into the directory
$ cd mtacoinbc
 
# Install dependencies
$ sudo apt-get install docker cmake zlib1g-dev
```

### Build
 
```bash
# Generate build system and compile
$ cmake -S . -B build
$ cmake --build build
 
# Copy binaries to their respective directories
$ cp build/Server/Server.out Server/
$ cp build/Miner/Miner.out Miner/
```

### Run
 
```bash
# Start 1 server and 4 miner containers
$ docker compose up -d
 
# Stop all containers
$ docker compose down
```

### Configuration
 
Edit `conf/mta/mtacoin.conf` to set the difficulty before starting:
 
```
DIFFICULTY=X
```
 
Valid range is 0-31. Defaults to 16 if the file is missing or invalid.
 
## Notes
* Supports up to `MAX_MINERS` concurrent miner containers (default is 4, configurable in `block.h`)
* Server container must be running before miners (`depends_on` in `compose.yaml` handles this automatically)
* `Server/` contains all files relevant to the server container
* `Miner/` contains all files relevant to the miner container
* `conf/mta/` contains the `mtacoin.conf` configuration file
* Log file is kept at `/var/log/mtacoin.log` within each container

## Credits

This software uses the following open source packages:

- [zlib](https://github.com/madler/zlib)
- [Docker](https://github.com/docker)

## License

MIT