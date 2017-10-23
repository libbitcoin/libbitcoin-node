[![Build Status](https://travis-ci.org/libbitcoin/libbitcoin-node.svg?branch=master)](https://travis-ci.org/libbitcoin/libbitcoin-node)

[![Coverage Status](https://coveralls.io/repos/libbitcoin/libbitcoin-node/badge.svg)](https://coveralls.io/r/libbitcoin/libbitcoin-node)

# Libbitcoin Node

*Bitcoin full node based on libbitcoin-blockchain*

Make sure you have installed [libbitcoin-blockchain](https://github.com/libbitcoin/libbitcoin-blockchain) and [libbitcoin-network](https://github.com/libbitcoin/libbitcoin-network) beforehand according to their build instructions.

```sh
$ ./autogen.sh
$ ./configure
$ make
$ sudo make install
$ sudo ldconfig
```

libbitcoin-node is now installed in `/usr/local/`.

In version2 the `bitcoin-node` console app is for demonstration purposes only. See [libbitcoin-server](https://github.com/libbitcoin/libbitcoin-server) for release quality full node functionality.

## Installation

### Macintosh

#### Using Homebrew

##### Installing from Formula

Instead of building, libbitcoin-node can be installed from a formula:
```sh
$ brew install libbitcoin-node
```
or
```sh
$ brew install bn
```
