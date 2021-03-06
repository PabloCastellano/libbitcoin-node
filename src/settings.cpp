/**
 * Copyright (c) 2011-2016 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/settings.hpp>

namespace libbitcoin {
namespace node {

static const settings settings_defaults()
{
    settings value;
    value.threads = 4;
    value.quorum = 8;
    value.block_bytes_per_second = 100000;
    value.headers_per_second = 10000;
    value.peers = { {} };
    return value;
};


const settings settings::defaults = settings_defaults();

} // namespace node
} // namespace libbitcoin
