/**
 * Furnace Tracker - multi-system chiptune tracker
 * Copyright (C) 2021-2022 tildearrow and contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _GUI_NET_COMMON_H
#define _GUI_NET_COMMON_H

#include "../../ta-utils.h"
#include <msgpack.hpp>

/**
 * @brief Stuff shared between client and server
 */
namespace NetCommon {
  enum class StatusCode {
    OK = 0,
    METHOD_NOT_FOUND = 1,
    METHOD_WRONG_ARGS = 2,
  };
}

// Needs to be in the global namespace
MSGPACK_ADD_ENUM(NetCommon::StatusCode);

namespace NetCommon {
  using Request = msgpack::type::tuple<
    String, // methodName
    msgpack::object // arguments (should be a msgpack array)
  >;

  using Response = msgpack::type::tuple<
    StatusCode, // status
    msgpack::object // result (nil if `status` is not `OK`)
  >;

  /**
   * @brief Contains RPC method names
   */
  namespace Method {
    constexpr const char* GET_FILE = "getFile";
    constexpr const char* DO_ACTION = "doAction";
  }

  /**
   * @brief Takes a status code and returns a friendly string describing the error
   */
  const char* statusToString(StatusCode code);
}

#endif
