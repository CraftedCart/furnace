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
#include <zmq.hpp>

/**
 * @brief Stuff shared between client and server
 */
namespace NetCommon {
  enum class StatusCode {
    OK = 0,
    METHOD_NOT_FOUND = 1,
    METHOD_WRONG_ARGS = 2,
  };

  enum class MessageKind {
    REQUEST = 0,
    RESPONSE = 1,
  };
}

// Needs to be in the global namespace
MSGPACK_ADD_ENUM(NetCommon::StatusCode);
MSGPACK_ADD_ENUM(NetCommon::MessageKind);

namespace NetCommon {
  struct RequestOrResponse {
    MessageKind kind;
    uint64_t id;
    msgpack::object methodOrStatus;
    msgpack::object argsOrResult;

    MSGPACK_DEFINE_ARRAY(kind, id, methodOrStatus, argsOrResult);
  };

  struct Request {
    MessageKind kind;
    uint64_t id;
    String methodName;
    msgpack::object args;

    MSGPACK_DEFINE_ARRAY(kind, id, methodName, args);

    /**
     * @raise msgpack::type_error if conversion fails
     */
    static Request from(RequestOrResponse&& other);
  };

  struct Response {
    MessageKind kind;
    uint64_t id;
    StatusCode status;
    msgpack::object result;

    MSGPACK_DEFINE_ARRAY(kind, id, status, result);

    /**
     * @raise msgpack::type_error if conversion fails
     */
    static Response from(RequestOrResponse&& other);
  };

  struct ClientId {
    std::vector<uint8_t> id;

    static ClientId fromMessage(const zmq::message_t& message);

    bool operator==(const ClientId& other) const;
    bool operator!=(const ClientId& other) const;
  };

  /**
   * @brief Contains RPC method names
   */
  namespace Method {
    constexpr const char* GET_FILE = "getFile";
    constexpr const char* EXEC_COMMAND = "execCommand";
  }

  /**
   * @brief Takes a status code and returns a friendly string describing the error
   */
  const char* statusToString(StatusCode code);
}

namespace std {
  template<>
  struct hash<NetCommon::ClientId> {
    inline std::size_t operator()(const NetCommon::ClientId& k) const {
      int i = 0;
      std::size_t hash = 0;

      for (uint8_t byte : k.id) {
        hash <<= 1;
        hash ^= byte;

        // Prevent longer IDs from consuming too much time here
        if (i == 16) break;

        i++;
      }

      return hash;
    }
  };
}

#endif
