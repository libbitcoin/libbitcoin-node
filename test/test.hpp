/**
 * Copyright (c) 2011-2026 libbitcoin developers
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_NODE_TEST_HPP
#define LIBBITCOIN_NODE_TEST_HPP

#include <filesystem>
#include <bitcoin/node.hpp>

#define TEST_NAME \
    ::libbitcoin::system::unit_test::session::instance().case_name()
#define TEST_DIRECTORY \
    test::directory
#define TEST_PATH \
    TEST_DIRECTORY + "/" + TEST_NAME

#ifdef HAVE_MSC
    BC_DISABLE_WARNING(NO_ARRAY_INDEXING)
    BC_DISABLE_WARNING(NO_GLOBAL_INIT_CALLS)
    BC_DISABLE_WARNING(NO_UNUSED_LOCAL_SMART_PTR)
    BC_DISABLE_WARNING(NO_DYNAMIC_ARRAY_INDEXING)
#endif

using namespace bc;
////using namespace bc::system;
////using namespace bc::network;
using namespace bc::node;

namespace std {

// data_slice -> base16(data)
std::ostream& operator<<(std::ostream& stream,
    const system::data_slice& slice) NOEXCEPT;

// std::vector<Type> -> join(<<Type)
template <typename Type>
std::ostream& operator<<(std::ostream& stream,
    const std::vector<Type>& values) NOEXCEPT
{
    // Ok when testing serialize because only used for error message out.
    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    stream << system::serialize(values);
    BC_POP_WARNING()
    return stream;
}

// array<Type, Size> -> join(<<Type)
template <typename Type, size_t Size>
std::ostream& operator<<(std::ostream& stream,
    const std_array<Type, Size>& values) NOEXCEPT
{
    // Ok when testing serialize because only used for error message out.
    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    stream << system::serialize(values);
    BC_POP_WARNING()
    return stream;
}

} // namespace std

namespace test {

// Common directory for all test file creations.
// Subdirectories and/or files must be differentiated (i.e. by TEST_NAME).
// Total path length cannot exceed MAX_PATH in HAVE_WINDOWS builds.
extern const std::string directory;

bool clear(const std::filesystem::path& directory) NOEXCEPT;
bool folder(const std::filesystem::path& directory) NOEXCEPT;
bool create(const std::filesystem::path& file_path) NOEXCEPT;
bool exists(const std::filesystem::path& file_path) NOEXCEPT;
bool remove(const std::filesystem::path& file_path) NOEXCEPT;

/// Settings with explicit values, independent of the defaults.
database::settings database_settings() NOEXCEPT;
network::settings network_settings() NOEXCEPT;
node::settings node_settings() NOEXCEPT;
node::configuration node_configuration() NOEXCEPT;

struct directory_setup_fixture
{
    DELETE_COPY_MOVE(directory_setup_fixture);

    directory_setup_fixture() NOEXCEPT
    {
        BC_REQUIRE(clear(directory));
    }
    ~directory_setup_fixture() NOEXCEPT
    {
        BC_REQUIRE(clear(directory));
    }
};

// Require success of a synchronous io call (the code is appended).
#define REQUIRE_SUCCESS(object, method, ...) \
{ \
    code require_ec{}; \
    (object).method(__VA_ARGS__ __VA_OPT__(,) require_ec); \
    BC_REQUIRE_EQUAL(require_ec, ::libbitcoin::network::error::success); \
}

// Blocking transfers of test peers (required to succeed).
template <typename Stream, typename Buffers>
inline size_t write(Stream& stream, const Buffers& buffers) NOEXCEPT
{
    code ec{};
    const auto size = network::io::write(stream, buffers, ec);
    BC_REQUIRE_EQUAL(ec, network::error::success);
    return size;
}

template <typename Stream, typename Buffers>
inline size_t read(Stream& stream, const Buffers& buffers) NOEXCEPT
{
    code ec{};
    const auto size = network::io::read(stream, buffers, ec);
    BC_REQUIRE_EQUAL(ec, network::error::success);
    return size;
}

// A loopback endpoint (an ephemeral port by default).
inline network::io::endpoint loopback(uint16_t port=0) NOEXCEPT
{
    return { system::ip::address{ system::ip::ipv4{ { 127, 0, 0, 1 } } }, port };
}

// A loopback endpoint on an ephemeral port with no socket bound to it.
inline network::io::endpoint refuser() NOEXCEPT
{
    network::io::context context{};
    network::io::socket socket{ context };
    REQUIRE_SUCCESS(socket, open, network::io::tcp::v4());
    REQUIRE_SUCCESS(socket, bind, loopback());
    const auto endpoint = socket.local_endpoint();
    REQUIRE_SUCCESS(socket, close);
    return endpoint;
}

} // namespace test

#endif