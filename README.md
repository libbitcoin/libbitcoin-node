[![Build Status](https://github.com/libbitcoin/libbitcoin-node/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/libbitcoin/libbitcoin-node/actions/workflows/ci.yml?branch=master)
[![Coverage Status](https://coveralls.io/repos/github/libbitcoin/libbitcoin-node/badge.svg?branch=master)](https://coveralls.io/github/libbitcoin/libbitcoin-node?branch=master)

# libbitcoin-node

*The high performance Bitcoin full node based on [libbitcoin-system](https://github.com/libbitcoin/libbitcoin-system), [libbitcoin-database](https://github.com/libbitcoin/libbitcoin-database), and [libbitcoin-network](https://github.com/libbitcoin/libbitcoin-network).*

## tl;dr

This is the **node of libbitcoin v4**. It integrates the basic components and offers a high performance Bitcoin full node that makes heavy use of the available hardware, internet connection and availabe RAM and CPU.

If you follow this README you will be able to build, run and monitor your own libbitcoin node. As libbitcoin is a multi platform software it works on Linux, Windows and OSX, on Intel and on ARM.

**The current state of node is not a drop in replacement for your Core node.** Not yet. What the node currently does: It serves as a full node, and it does so very well. It receives and distributes Blocks in conformity with the Bitcoin protocol as fast as your hardware allows it. We are currently working on the connection layer that will let you connect your libbitcoin node to other systems like Sparrow Wallet (via `ElectrumX`) or Stratum V1 (via `json-rpc`). The node's capabilities in that matter change with every commit so stay tuned.

Now, If you want to see how fast your setup can sync the Bitcoin Blockchain, read on and get your libbitcoin node running today. It's pretty easy actually.

**License Overview**

All files in this repository fall under the license specified in [COPYING](COPYING). The project is licensed as [AGPL with a lesser clause](https://wiki.unsystem.net/en/index.php/Libbitcoin/License). It may be used within a proprietary project, but the core library and any changes to it must be published online. Source code for this library must always remain free for everybody to access.

**About libbitcoin**

The libbitcoin toolkit is a set of cross platform C++ libraries for building bitcoin applications. The toolkit consists of several libraries, most of which depend on the foundational [libbitcoin-system](https://github.com/libbitcoin/libbitcoin-system) library. Each library's repository can be cloned and built separately. There are no packages yet in distribution, however each library includes an installation script (described below) which is regularly verified via [github actions](https://github.com/features/actions).

## Contents
- [Installation](#installation)
  - [Linux](#linux)
  - [macOS](#macos)
  - [Windows](#windows)
- [Running libbitcoin](#running-libbitcoin)
  -  [Startup Process](#startup-process)
  -  [Configuration](#configuration)
  -  [Logging](#logging)
  -  [Using the Console](#using-the-console)
  -  [Performance Analysis](#performance-analysis)
  -  [Memory Management](#memory-management)
- [Known Limitiations](#known-limitations)

## Installation

Depending on what operating system you use, the build process will be different. Detailed instructions are provided below.

- [Linux (Ubuntu)](#linux)
- [macOS](#macos)
- [Windows](#windows)

### Linux

Linux users build libbitcoin node with the provided installation script `install.sh` that uses Autotools and pkg-config to first build the needed dependencies (boost, libpsec256) and then fetches the latest libbitcoin projects (libbitcoin-system, libbitcoin-database, libbitcoin-network) from github and builds the stack bottom up to the libbitcoin-node binary - bn.

You can issue the following command to check for further parameterization of the install script:

```bash
$ install.sh --help
```

which brings up the following options:

```
Usage: ./install.sh [OPTION]...
Manage the installation of libbitcoin-node.
Script options:
  --with-icu               Compile with International Components for Unicode.
                             Since the addition of BIP-39 and later BIP-38 
                             support, libbitcoin conditionally incorporates ICU 
                             to provide BIP-38 and BIP-39 passphrase 
                             normalization features. Currently 
                             libbitcoin-explorer is the only other library that 
                             accesses this feature, so if you do not intend to 
                             use passphrase normalization this dependency can 
                             be avoided.
  --build-icu              Build ICU libraries.
  --build-boost            Build Boost libraries.
  --build-secp256k1        Build libsecp256k1 libraries.
  --build-dir=<path>       Location of downloaded and intermediate files.
  --prefix=<absolute-path> Library install location (defaults to /usr/local).
  --disable-shared         Disables shared library builds.
  --disable-static         Disables static library builds.
  --help, -h               Display usage, overriding script execution.

All unrecognized options provided shall be passed as configuration options for 
all dependencies.
```

In order to successfully execute `install.sh` a few requirements need to be met. This walkthrough is based on a 'current' installation of the following components. (That doesn't necessarily mean these are the minimum requirements though).

#### Requirements
* Base OS: Ubuntu 24.04.3 LTS (Noble Numbat)
* C++13 compiler (gcc version 13.3.0 (Ubuntu 13.3.0-6ubuntu2~24.04))
* [Autoconf](https://www.gnu.org/software/autoconf/)
* [Automake](https://www.gnu.org/software/automake/)
* [libtool](https://www.gnu.org/software/libtool/)
* [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/)
* [git](https://git-scm.com/)
* [curl](https://www.gnu.org/software/curl/)

The corresponding packages can be installed with the following command:

```bash
$ sudo apt install build-essential curl git automake pkg-config libtool
```

Create a new directory (e.g. `/home/user/development/libbitcoin`), then use git to fetch the latest repository from github by issuing the following command from within this directory:

```bash
git clone https://github.com/libbitcoin/libbitcoin-node
```

Enter the project directory `cd libbitcoin-node` and start the build with the following command:

```bash
$ ./install.sh --prefix=/home/user/development/libbitcoin/build/release_static/ --build-secp256k1 --build-boost --disable-shared
```

This will build libbitcoin-node as a static release with no [CPU extensions](#cpu-extensions) built in. Use only absolute path names for prefix dir. A successful build will create the following directory/file structure:

```
~/development/libbitcoin/
~/development/libbitcoin/build/
~/development/libbitcoin/build/release_static/
~/development/libbitcoin/build/release_static/bin/
~/development/libbitcoin/build/release_static/etc/
~/development/libbitcoin/build/release_static/include/
~/development/libbitcoin/build/release_static/lib/
~/development/libbitcoin/build/release_static/share/
~/development/libbitcoin/build/release_static/share/doc/
~/development/libbitcoin/build/release_static/share/doc/libbitcoin-database/
~/development/libbitcoin/build/release_static/share/doc/libbitcoin-network/
~/development/libbitcoin/build/release_static/share/doc/libbitcoin-node/
~/development/libbitcoin/build/release_static/share/doc/libbitcoin-system/
~/development/libbitcoin/libbitcoin-node
```

Now enter the bin directory and [fire up](#running-libbitcoin) your node.

### CPU Extensions

CPU Extensions are specific optimizations your CPU might or might not have. libbitcoin can be built with support for the following CPU extensions if your architecture supports them:

* [SHA-NI](https://en.wikipedia.org/wiki/SHA_instruction_set)
* [SSE4.1](https://en.wikipedia.org/wiki/SSE4)
* [AVX2](https://en.wikipedia.org/wiki/Advanced_Vector_Extensions)
* [AVX512](https://en.wikipedia.org/wiki/AVX-512)

To build libbitcoin-node with these extensions use the `--enable-feature` parameters like:

- --enable-shani
- --enable-avx2
- --enable-sse41
- --enable-avx512

This command will create a static release build with all supported CPU extensions (if supported by the system):

```bash
$ CFLAGS="-O3" CXXFLAGS="-O3" ./install.sh --prefix=/home/eynhaender/development/libbitcoin/build/release_static/ --build-dir=/home/eynhaender/development/libbitcoin/build/temp --build-secp256k1 --build-boost --disable-shared --enable-ndebug --enable-shani --enable-avx2 --enable-sse41 --enable-avx512
```

You can check if the optimizations have been built in your binary with the following command:

```bash
$ ./bn --hardware
```

which will show your CPU architecture as well as the built in status of the supported optimizations:

```
Hardware configuration...
arm..... platform:0.
intel... platform:1.
avx512.. platform:1 compiled:1.
avx2.... platform:1 compiled:1.
sse41... platform:1 compiled:1.
shani... platform:1 compiled:1.
```

### macOS
TODO

### Windows
TODO

## Running libbitcoin
Let's see what options we have for the node:

```bash
$ ./bn --help
```

The response should look like this:

```
Usage: bn [-abdfhiklnrstvw] [--config value]                             

Info: Runs a full bitcoin node.                                          

Options (named):

-a [--slabs]         Scan and display store slab measures.               
-b [--backup]        Backup to a snapshot (can also do live).            
-c [--config]        Specify path to a configuration settings file.      
-d [--hardware]      Display hardware compatibility.                     
-f [--flags]         Scan and display all flag transitions.              
-h [--help]          Display command line options.                       
-i [--information]   Scan and display store information.                 
-k [--buckets]       Scan and display all bucket densities.              
-l [--collisions]    Scan and display hashmap collision stats (may exceed
                     RAM and result in SIGKILL).                         
-n [--newstore]      Create new store in configured directory.           
-r [--restore]       Restore from most recent snapshot.                  
-s [--settings]      Display all configuration settings.                 
-t [--test]          Run built-in read test and display.                 
-v [--version]       Display version information.                        
-w [--write]         Run built-in write test and display.
```

Did you check if all supported [CPU Extensions](#cpu-extensions) are built in (`./bn --hardware`)?

### Startup Process

Starting the node without passing any parameters like:

```
$ ./bn
```

will do the following:
- If there is no data store at `./bitcoin/`, the node will create one. The location can be changed in the [config](#config-file) file (`bn.cfg`).
- Start multiple threads to setup peer connections, to download and to process blocks -> [Theory of operation](#TODO)
- Expose an interactive console to do basic communication with the node -> [Using the console](#using-the-console)
- Write various logfiles -> [Logging](#logging)

Furthermore the standard parameters will perform a **milestone sync** with the Bitcoin Blockchain, as opposed to a **full validation sync**.

Although this works in principle, you may want to do at least a few basic settings. You do this by passing a [config file](#config-file) to the node:

```sh
$ ./bn --config ./bn.cfg
```

The node will now expect to find a valid [config file](#config-file) in the current folder.

Starting a **full validation sync** on the node requires to set the latest milestone block back to genesis. This is done by adding the following lines to the config file:

```
[bitcoin]
checkpoint   = 000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f:0
milestone = 000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f:0
```

### Config File

libbitcoin node reads its basic configuration from a config file that can be passed to the function as a parameter `-c`. The config file has the following structure:

```
[database]
path = /mnt/blockchain/bitcoin
#path = /mnt/blockchain/bitcoin_tn
#path = /mnt/blockchain/zcash

[network]
threads = 16
#threads = 8
seed = cup.nop.lol:8333
#seed = dnsseed.bitcoin.dashjr.org:8333

[node]
#threads = 32

[server]
inbound_connections = 0
bind = 0.0.0.0:80

```
A current config template is generated during build and saved at `/prefix-dir/etc/libbitcoin/bn.cfg`.

### Logging

libbitcoin-node creates the following files at start:

* events.log
* bn_begin.log
* bn_end.log
* hosts.cache

The events.log file displays all information about the sync process, providing the current status and speed of the sync. Read [Performance Analysis](#performance-analysis) to check your sync speed through the logs.

### Using the console

Pressing **`m` + Enter** while running will bring up the interactive console. This interface is mainly user for low level logging and configuration and it looks like this:

```
2025-10-17T23:47:58Z.0 Toggle: [v]erbose
2025-10-17T23:47:58Z.0 Toggle: [o]bjects
2025-10-17T23:47:58Z.0 Toggle: [q]uitting
2025-10-17T23:47:58Z.0 Toggle: [f]ault
2025-10-17T23:47:58Z.0 Toggle: [r]emote
2025-10-17T23:47:58Z.0 Toggle: [x]proxy
2025-10-17T23:47:58Z.0 Toggle: [p]rotocol
2025-10-17T23:47:58Z.0 Toggle: [s]ession
2025-10-17T23:47:58Z.0 Toggle: [n]ews
2025-10-17T23:47:58Z.0 Toggle: [a]pplication
2025-10-17T23:47:58Z.0 Option: [z]eroize disk full error
2025-10-17T23:47:58Z.0 Option: [w]ork distribution
2025-10-17T23:47:58Z.0 Option: [t]est built-in case
2025-10-17T23:47:58Z.0 Option: [m]enu of options and toggles
2025-10-17T23:47:58Z.0 Option: [i]nfo about store
2025-10-17T23:47:58Z.0 Option: [h]old network communication
2025-10-17T23:47:58Z.0 Option: [g]o network communication
2025-10-17T23:47:58Z.0 Option: [e]rrors in store
2025-10-17T23:47:58Z.0 Option: [c]lose the node
2025-10-17T23:47:58Z.0 Option: [b]ackup the store
```

### Performance Analysis

To check the net sync performance of your node you will have to let it run until a specific milestone block is reached. The events.log will then tell you how long it took to reach this block height. If you want to check how long it took for the node to reach a specific Block Height, you check the events.log. 

Let's check for the **organization** of Block **700,000**

```bash
$ cat ./events.log | grep 'block_organized..... 700000'
```

which will give you something like this, the number on the right representing the seconds from start of the process to 700k:

```
block_organized..... 700000 11053
```

In this case it took the node 11053 seconds or 184 minutes to get to Block 700k.

As the node aims to sync the Blockchain as fast as possible, we assume that if the computer is powerful enough, the limiting factor for syncing the chain is your internet connection - you simply can't sync faster than the time needed to download the Blockchain.

The following table shows what your node should be capable of on a given internet connection. In other words, these are our numbers:

| Network            | OS           | Milestone  | Seconds/Minutes | Setup                        |
|--------------------|--------------|------------|-----------------|------------------------------|
| 260MBit/s VDSL     | Linux/Ubuntu |       900k |                 | Ryzen7/32GB RAM/WD Black SSD |
| 400MBit/s Starlink | Linux/Ubuntu |       900k |      16,292/271 | Ryzen7/96GB RAM/WD Black SSD |
| 400 + 300MBit/s LB | Linux/Ubuntu |       900k |      11,188/187 | Ryzen7/96GB RAM/WD Black SSD |
| 2.3GBit/s          | Windows      |       900k |         3600/60 |                              |

### Memory Management

In order to improve the performance on your Linux system, you probably have to alter the kernel parameters that determine the memory management of the system ([Memory Paging](https://en.wikipedia.org/wiki/Memory_paging)).

#### Kernel VM Tuning Parameters

These sysctl parameters optimize Linux VM dirty page writeback for high I/O workloads (e.g., databases). Set via `/etc/sysctl.conf` or `sudo sysctl`. This table shows the stock values of the current Ubuntu LTS.

| Parameter                      | Value  | Description                                                       |
|--------------------------------|--------|-------------------------------------------------------------------|
| `vm.dirty_background_ratio`    | `10`   | Initiates background writeback at 10% dirty RAM (lowers latency). |
| `vm.dirty_ratio`               | `20`   | Blocks apps at 20% dirty RAM (prevents OOM).                      |
| `vm.dirty_expire_centisecs`    | `3000` | Marks pages "old" after 30s (1/100s units; triggers writeback).   |
| `vm.dirty_writeback_centisecs` | `500`  | Wakes kworker every 5s to flush old pages.                        |

Check your actual kernel parameters by issuing the following command:

```sh
$ sysctl vm.dirty_background_ratio vm.dirty_ratio vm.dirty_expire_centisecs vm.dirty_writeback_centisecs
```

The following parameters have been identified to be the most effective for a 'standard' 32GB RAM / SSD computer:

* vm.dirty_background_ratio = 50
* vm.dirty_ratio = 80
* vm.dirty_expire_centisecs = 6000
* vm.dirty_writeback_centisecs = 1000

**Tune:** Adjust ratios for RAM size; test under load. Set them with the following commands:

```sh
$ sudo sysctl vm.dirty_background_ratio=50
$ sudo sysctl vm.dirty_ratio=80
$ sudo sysctl vm.dirty_expire_centisecs=6000 #60 seconds/1 minute
$ sudo sysctl vm.dirty_writeback_centisecs=1000 #10 seconds
```

If set this way the settings are only temporary and reset after reboot. Although there are options to set them permanently we suggest to play around with session based parameters until you found the setting that works best for your system. Note that these parameters apply to the OS and therefore to all running applications.

## Known Limitations

libbitcoin is designed to use the maximum available RAM and, whenever necessary, writes this data to the disk using mmap. The question of whether this is necessary is determined by the operating system (Dirty Paging). During development we noticed that Windows' paging is significantly more performant (because it's dynamic) than either OSX or Linux. To achieve similar performance under Linux as under Windows, the Linux kernel parameters must be adapted to the underlying hardware/the requirements of the node.

Check [Memory Management](#memory-management) and [Performance Analysis](#performance-analysis) to learn how to change your settings.