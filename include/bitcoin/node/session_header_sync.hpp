/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_NODE_SESSION_HEADER_SYNC_HPP
#define LIBBITCOIN_NODE_SESSION_HEADER_SYNC_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/settings.hpp>

namespace libbitcoin {
namespace node {

class BCN_API session_header_sync
  : public network::session_batch, track<session_header_sync>
{
public:
    typedef std::shared_ptr<session_header_sync> ptr;

    session_header_sync(network::p2p& network, hash_list& hashes,
        const config::checkpoint& top, const settings& settings,
        const blockchain::settings& chain_settings);

    void start(result_handler handler);

private:
    void handle_started(const code& ec, result_handler handler);
    void new_connection(network::connector::ptr connect,
        result_handler handler);
    void start_syncing(const code& ec, const config::authority& host,
        network::connector::ptr connect, result_handler handler);
    void handle_connect(const code& ec, network::channel::ptr channel,
        network::connector::ptr connect, result_handler handler);
    void handle_complete(const code& ec, network::connector::ptr connect,
        result_handler handler);

    void handle_channel_start(const code& ec, network::connector::ptr connect,
        network::channel::ptr channel, result_handler handler);
    void handle_channel_stop(const code& ec);

    const size_t start_height_;
    const settings& settings_;

    // These are guarded by the non-restartable session constraint.
    size_t votes_;
    config::checkpoint::list checkpoints_;
    hash_list& hashes_;
};

} // namespace node
} // namespace libbitcoin

#endif

