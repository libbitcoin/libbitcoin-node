[![Build Status](https://github.com/libbitcoin/libbitcoin-node/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/libbitcoin/libbitcoin-node/actions/workflows/ci.yml?branch=master)
[![Coverage Status](https://coveralls.io/repos/github/libbitcoin/libbitcoin-node/badge.svg?branch=master)](https://coveralls.io/github/libbitcoin/libbitcoin-node?branch=master)

# libbitcoin-node

*The high performance Bitcoin full node based on [libbitcoin-system](https://github.com/libbitcoin/libbitcoin-system), [libbitcoin-database](https://github.com/libbitcoin/libbitcoin-database), and [libbitcoin-network](https://github.com/libbitcoin/libbitcoin-network).*

<div align="center">

<a href="/docs/images/libbitcoin-components-node.svg">
  <img src="/docs/images/libbitcoin-components-node.svg" alt="libbitcoin components" width="500" />
</a>

</div>

## tl;dr
This is the **node component of libbitcoin v4**. It integrates the basic components and offers a **high performance Bitcoin full node**. It makes heavy use of the available hardware, internet connection and availabe RAM and CPU so you can sync the Bitcoin blockchain from genesis, today and in the future.

**This component contains no executable** as it moved up to libbitcoin-server. If you want to build and run your own libbitcoin server, check out the corresponding module `bs` at [libbitcoin-server](https://github.com/libbitcoin/libbitcoin-server).

**License Overview**

All files in this repository fall under the license specified in [COPYING](COPYING). The project is licensed as [AGPL with a lesser clause](https://wiki.unsystem.net/en/index.php/Libbitcoin/License). It may be used within a proprietary project, but the core library and any changes to it must be published online. Source code for this library must always remain free for everybody to access.

**About libbitcoin**

The libbitcoin toolkit is a set of cross platform C++ libraries for building Bitcoin applications. The toolkit consists of several libraries, most of which depend on the foundational [libbitcoin-system](https://github.com/libbitcoin/libbitcoin-system) library. Each library's repository can be cloned and built separately. There are no packages yet in distribution, however each library includes an installation script (described below) which is regularly verified via [github actions](https://github.com/features/actions).