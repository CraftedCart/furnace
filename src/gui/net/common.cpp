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

#include "common.h"

namespace NetCommon {
  const char* statusToString(StatusCode code) {
    switch (code) {
      case StatusCode::OK:
        return "Ok";
      case StatusCode::METHOD_NOT_FOUND:
        return "RPC method not found";
      case StatusCode::METHOD_WRONG_ARGS:
        return "Wrong arguments for RPC method";
      default:
        return "Unknown error";
    }
  }

  Request Request::from(RequestOrResponse&& other) {
    assert(other.kind == MessageKind::REQUEST);

    Request req;
    req.kind = other.kind;
    req.id = other.id;
    other.methodOrStatus.convert(req.methodName);
    req.args = other.argsOrResult;

    return req;
  }

  Response Response::from(RequestOrResponse&& other) {
    assert(other.kind == MessageKind::RESPONSE);

    Response resp;
    resp.kind = other.kind;
    resp.id = other.id;
    other.methodOrStatus.convert(resp.status);
    resp.result = other.argsOrResult;

    return resp;
  }

  ClientId ClientId::fromMessage(const zmq::message_t& message) {
    ClientId id;
    id.id.resize(message.size());
    memcpy(id.id.data(), message.data(), message.size());

    return id;
  }

  bool ClientId::operator==(const ClientId& other) const {
    return id == other.id;
  }

  bool ClientId::operator!=(const ClientId& other) const {
    return id != other.id;
  }
}
