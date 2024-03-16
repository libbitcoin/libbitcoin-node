/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_EXECUTOR_HPP
#define LIBBITCOIN_NODE_EXECUTOR_HPP

#include <atomic>
#include <future>
#include <iostream>
#include <unordered_map>
#include <bitcoin/database.hpp>
#include <bitcoin/node.hpp>

namespace libbitcoin {
namespace node {

class executor
{
public:
    DELETE_COPY(executor);

    executor(parser& metadata, std::istream&, std::ostream& output,
        std::ostream& error);

    /// Invoke the menu command indicated by the metadata.
    bool menu();

private:
    using rotator_t = database::file::stream::out::rotator;

    void logger(const auto& message) const;
    void console(const auto& message) const;
    void stopper(const auto& message);

    static void initialize_stop() NOEXCEPT;
    static void stop(const system::code& ec);
    static void handle_stop(int code);

    void handle_started(const system::code& ec);
    void handle_subscribed(const system::code& ec, size_t key);
    void handle_running(const system::code& ec);
    bool handle_stopped(const system::code& ec);

    bool do_help();
    bool do_settings();
    bool do_version();
    bool do_initchain();
    bool do_restore();
    bool do_flags();
    bool do_measure();
    bool do_buckets();
    bool do_collisions();
    bool do_read();
    bool do_write();
    bool do_run();

    rotator_t create_log_sink() const;
    system::ofstream create_event_sink() const;
    void subscribe_log(std::ostream& sink);
    void subscribe_events(std::ostream& sink);
    void subscribe_capture();
    void subscribe_connect();
    void subscribe_close();

    void scan_flags() const;
    void measure_size() const;
    void scan_buckets() const;
    void scan_collisions() const;
    void read_test() const;
    void write_test();

    static const std::string name_;
    static const std::string close_;
    static const std::string backup_;
    static const std::unordered_map<uint8_t, bool> defined_;
    static const std::unordered_map<uint8_t, std::string> display_;
    static const std::unordered_map<std::string, uint8_t> keys_;
    static const std::unordered_map<database::event_t, std::string> events_;
    static const std::unordered_map<database::table_t, std::string> tables_;
    static constexpr size_t logs = add1(network::levels::quit);
    static std::promise<system::code> stopping_;
    static std::atomic_bool cancel_;

    parser& metadata_;
    full_node::ptr node_{};
    full_node::store store_;
    full_node::query query_;
    std::promise<system::code> stopped_{};

    std::istream& input_;
    std::ostream& output_;
    network::logger log_{};
    network::capture capture_{ input_, close_ };
    std_array<std::atomic_bool, logs> toggle_
    {
        true,  // application
        network::levels::news_defined,
        false, //network::levels::objects_defined,
        network::levels::session_defined,
        false, //network::levels::protocol_defined,
        false, //network::levels::proxy_defined,
        false, //network::levels::wire_defined,
        network::levels::remote_defined,
        network::levels::fault_defined,
        network::levels::quit_defined
    };
};

} // namespace node
} // namespace libbitcoin

#endif
