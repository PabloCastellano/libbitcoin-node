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
#include <bitcoin/node/protocol_header_sync.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <bitcoin/network.hpp>
#include <bitcoin/node/p2p_node.hpp>

INITIALIZE_TRACK(bc::node::protocol_header_sync);

namespace libbitcoin {
namespace node {

#define NAME "header_sync"
#define CLASS protocol_header_sync

using namespace bc::config;
using namespace bc::chain;
using namespace bc::message;
using namespace bc::network;
using std::placeholders::_1;
using std::placeholders::_2;

// TODO: move to config.
static constexpr size_t header_period_seconds = 10;

static constexpr size_t full_headers = 2000;
static const asio::seconds header_period(header_period_seconds);

protocol_header_sync::protocol_header_sync(p2p& network,
    channel::ptr channel, uint32_t minimum_rate, size_t first_height,
    hash_list& hashes, const checkpoint::list& checkpoints)
  : protocol_timer(network, channel, true, NAME),
    hashes_(hashes),
    current_second_(0),
    minimum_rate_(minimum_rate),
    start_size_(hashes.size()),
    first_height_(first_height),
    last_height_(last_height(first_height, hashes, checkpoints)),
    checkpoints_(checkpoints),
    CONSTRUCT_TRACK(protocol_header_sync)
{
    // TODO: convert to exception (public API).
    BITCOIN_ASSERT_MSG(!hashes_.empty(), "The starting header must be set.");
}

// Utilities
// ----------------------------------------------------------------------------

size_t protocol_header_sync::last_height(size_t first_height,
    hash_list& headers, const checkpoint::list& checkpoints)
{
    const auto current_block = first_height + headers.size() - 1;
    return checkpoints.empty() ? current_block :
        std::max(checkpoints.back().height(), current_block);
}

size_t protocol_header_sync::current_height() const
{
    return hashes_.size() + first_height_;
}

size_t protocol_header_sync::current_rate() const
{
    return (hashes_.size() - start_size_) / current_second_;
}

bool protocol_header_sync::linked(const header& header,
    const hash_digest& hash) const
{
    return header.previous_block_hash == hash;
}

bool protocol_header_sync::checks(const hash_digest& hash, size_t height) const
{
    if (height > last_height_)
        return true;

    return checkpoint::validate(hash, height, checkpoints_);
}

bool protocol_header_sync::proof_of_work(const header& header,
    size_t height) const
{
    if (height <= last_height_)
        return true;

    // TODO: determine if the PoW for this block is valid.
    // This allows us to collect headers beyond the last checkpoint.
    // We can attempt to fill these blocks and the the sync will terminate at
    // the last validated block (i.e. above the last checkpoint).
    return true;
}

// TODO: ensure that this is correct (using valid block sync).
void protocol_header_sync::rollback()
{
    if (!checkpoints_.empty())
    {
        for (auto it = checkpoints_.rbegin(); it != checkpoints_.rend(); ++it)
        {
            auto match = std::find(hashes_.begin(), hashes_.end(), it->hash());
            if (match != hashes_.end())
            {
                hashes_.erase(++match, hashes_.end());
                return;
            }
        }
    }

    hashes_.resize(1);
}

// It's not necessary to roll back for invalid PoW. We just stop and move to
// another peer. As long as we are getting valid PoW there is no way to know
// we aren't of on a fork, so moving on is sufficient (and generally faster).
bool protocol_header_sync::merge_headers(const headers& message)
{
    auto previous = hashes_.back();

    for (const auto& header: message.elements)
    {
        const auto current = header.hash();

        if (!linked(header, previous) || !checks(current, current_height()))
        {
            rollback();
            return false;
        }

        if (!proof_of_work(header, current_height()))
            return false;

        previous = current;
        hashes_.push_back(current);
    }

    return true;
}

// Start sequence.
// ----------------------------------------------------------------------------

void protocol_header_sync::start(event_handler handler)
{
    const auto peer_top = peer_version().start_height;

    if (peer_top < last_height_)
    {
        log::info(LOG_NETWORK)
            << "Start height (" << peer_top << ") below header sync target ("
            << last_height_ << ") from [" << authority() << "]";

        // This is a successful vote.
        handler(error::success);
        return;
    }

    auto complete = synchronize(BIND2(headers_complete, _1, handler), 1, NAME);
    protocol_timer::start(header_period, BIND2(handle_event, _1, complete));

    SUBSCRIBE3(headers, handle_receive, _1, _2, complete);

    // This is the end of the start sequence.
    send_get_headers(complete);
}

// Header sync sequence.
// ----------------------------------------------------------------------------

void protocol_header_sync::send_get_headers(event_handler complete)
{
    if (stopped())
        return;

    BITCOIN_ASSERT_MSG(!hashes_.empty(), "The start header must be set.");
    const get_headers packet{ { hashes_.back() }, null_hash };

    SEND2(packet, handle_send, _1, complete);
}

void protocol_header_sync::handle_send(const code& ec, event_handler complete)
{
    if (stopped())
        return;

    if (ec)
    {
        log::debug(LOG_PROTOCOL)
            << "Failure sending get headers to sync [" << authority() << "] "
            << ec.message();
        complete(ec);
    }
}

bool protocol_header_sync::handle_receive(const code& ec,
    const headers& message, event_handler complete)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_PROTOCOL)
            << "Failure receiving headers from sync ["
            << authority() << "] " << ec.message();
        complete(ec);
        return false;
    }

    if (!merge_headers(message))
    {
        log::warning(LOG_PROTOCOL)
            << "Failure merging headers from [" << authority() << "]";
        complete(error::previous_block_invalid);
        return false;
    }

    log::info(LOG_PROTOCOL)
        << "Synced headers " << current_height() - message.elements.size()
        << "-" << current_height() - 1 << " from [" << authority() << "]";

    // If we received fewer than 2000 the peer is exhausted.
    if (message.elements.size() < full_headers)
    {
        // If we reached the last height the sync is a success.
        const auto success = current_height() > last_height_;
        complete(success ? error::success : error::operation_failed);
        return false;
    }

    send_get_headers(complete);
    return true;
}

// This is fired by the base timer and stop handler.
void protocol_header_sync::handle_event(const code& ec, event_handler complete)
{
    if (ec == error::channel_stopped)
    {
        complete(ec);
        return;
    }

    if (ec && ec != error::channel_timeout)
    {
        log::warning(LOG_PROTOCOL)
            << "Failure in header sync timer for [" << authority() << "] "
            << ec.message();
        complete(ec);
        return;
    }

    // It was a timeout, so ten more seconds have passed.
    current_second_ += header_period_seconds;

    // Drop the channel if it falls below the min sync rate averaged over all.
    if (current_rate() < minimum_rate_)
    {
        log::info(LOG_PROTOCOL)
            << "Header sync rate (" << current_rate()
            << "/sec) from [" << authority() << "]";
        complete(error::channel_timeout);
        return;
    }
}

void protocol_header_sync::headers_complete(const code& ec,
    event_handler handler)
{
    // This is end of the header sync sequence.
    handler(ec);

    // The session does not need to handle the stop.
    stop(error::channel_stopped);
}

} // namespace network
} // namespace libbitcoin
