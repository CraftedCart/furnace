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

#ifndef _GUI_NET_SERVER_H
#define _GUI_NET_SERVER_H

#include "shared.h"
#include <msgpack.hpp>
#include <zmq.hpp>
#include <unordered_set>

// Forward declarations
class FurnaceGUI;

class NetServer : public NetShared {
  private:
    std::unordered_set<NetCommon::ClientId> connectedClients;

    /**
     * @brief The client that the server is currently responding to
     *
     * Should only be accessed from the net thread
     */
    std::optional<NetCommon::ClientId> currentClient = std::nullopt;

    uint64_t lastRequestId = 0;

  public:
    NetServer(FurnaceGUI* gui);

    /**
     * @brief Start the server on another thread
     */
    void start(uint16_t port);

  protected:
    virtual msgpack::type::nil_t recvDoAction(const UndoAction& action) override;

  private:
    void runThread(uint16_t port);
};

#endif
