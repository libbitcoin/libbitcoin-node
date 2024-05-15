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
#include "test.hpp"

using namespace bc::system;
using namespace bc::network;

BOOST_AUTO_TEST_SUITE(configuration_tests)

BOOST_AUTO_TEST_CASE(configuration__construct1__none_context__expected)
{
    const node::configuration instance(chain::selection::none);

    BOOST_REQUIRE(instance.file.empty());
    BOOST_REQUIRE(!instance.help);
    BOOST_REQUIRE(!instance.hardware);
    BOOST_REQUIRE(!instance.settings);
    BOOST_REQUIRE(!instance.version);
    BOOST_REQUIRE(!instance.newstore);
    BOOST_REQUIRE(!instance.backup);
    BOOST_REQUIRE(!instance.restore);
    BOOST_REQUIRE(!instance.flags);
    BOOST_REQUIRE(!instance.information);
    BOOST_REQUIRE(!instance.slabs);
    BOOST_REQUIRE(!instance.buckets);
    BOOST_REQUIRE(!instance.collisions);
    BOOST_REQUIRE(!instance.test);
    BOOST_REQUIRE(!instance.write);

    BOOST_REQUIRE_EQUAL(instance.log.application, levels::application_defined);
    BOOST_REQUIRE_EQUAL(instance.log.news, levels::news_defined);
    BOOST_REQUIRE_EQUAL(instance.log.session, levels::session_defined);
    BOOST_REQUIRE_EQUAL(instance.log.protocol, false /*levels::protocol_defined*/);
    BOOST_REQUIRE_EQUAL(instance.log.proxy, false /*levels::proxy_defined*/);
    BOOST_REQUIRE_EQUAL(instance.log.remote, levels::remote_defined);
    BOOST_REQUIRE_EQUAL(instance.log.fault, levels::fault_defined);
    BOOST_REQUIRE_EQUAL(instance.log.quitting, false /*levels::quitting_defined*/);
    BOOST_REQUIRE_EQUAL(instance.log.objects, false /*levels::objects_defined*/);
    BOOST_REQUIRE_EQUAL(instance.log.verbose, false /*levels::verbose_defined*/);

    BOOST_REQUIRE_EQUAL(instance.log.maximum_size, 1'000'000_u32);
    BOOST_REQUIRE_EQUAL(instance.log.log_file1(), "bn_end.log");
    BOOST_REQUIRE_EQUAL(instance.log.log_file2(), "bn_begin.log");
    BOOST_REQUIRE_EQUAL(instance.log.events_file(), "events.log");
    BOOST_REQUIRE_EQUAL(instance.log.path, "");
#if defined(HAVE_MSC)
    BOOST_REQUIRE_EQUAL(instance.log.symbols, "");
#endif
}

BOOST_AUTO_TEST_SUITE_END()
