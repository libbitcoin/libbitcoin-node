[![Build Status](https://travis-ci.org/libbitcoin/libbitcoin-node.svg?branch=version2)](https://travis-ci.org/libbitcoin/libbitcoin-node)

# Libbitcoin Node

*Bitcoin full node based on libbitcoin-blockchain*

Note that you need g++ 4.7 or higher. For this reason Ubuntu 12.04 and older are not supported. Make sure you have installed [libbitcoin-blockchain](https://github.com/libbitcoin/libbitcoin-blockchain) beforehand according to its build instructions.

```sh
$ ./autogen.sh
$ ./configure
$ make
$ sudo make install
$ sudo ldconfig
```

libbitcoin-node is now installed in `/usr/local/`.