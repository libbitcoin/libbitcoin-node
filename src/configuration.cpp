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
#include <bitcoin/node/configuration.hpp>

#include <bitcoin/node/define.hpp>
#include <bitcoin/node/settings.hpp>

namespace libbitcoin {
namespace node {

// Construct with defaults derived from given context.
configuration::configuration(system::chain::selection context) NOEXCEPT
  : bitcoin(context),
    database(context),
    network(context),
    node(context)
{
}

// Silent payment indexing requires witness data, which a pruned store does not
// archive for checkpointed or milestoned (bypassed) blocks.
code configuration::initialize() NOEXCEPT
{
    const auto silent = node.silent_start_height != max_uint32;
    if (silent && !node.require_witness)
        return network::error::invalid_configuration;

    const auto checkpoint = bitcoin.top_checkpoint().height();
    const auto milestone = bitcoin.milestone.height();
    const auto bypassed = std::max(checkpoint, milestone);
    if (silent && node.limited_blocks && node.silent_start_height <= bypassed)
        return network::error::invalid_configuration;

    database.initialize(bitcoin, node.limited_blocks, node.silent_start_height);
    return network.initialize();
}

} // namespace node
} // namespace libbitcoin
