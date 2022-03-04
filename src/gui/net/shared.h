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

#ifndef _GUI_NET_SHARED_H
#define _GUI_NET_SHARED_H

#include "common.h"
#include "../task_queue.h"
#include "../../ta-utils.h"
#include "../../ta-log.h"
#include <zmq.hpp>
#include <optional>
#include <thread>
#include <cstdint>

// Forward declarations
class FurnaceGUI;
struct UndoAction;

/**
 * @brief Base class for `NetServer` and `NetClient`
 */
class NetShared {
  protected:
    using MethodFunc = void(*)(NetShared*, const msgpack::object&, msgpack::sbuffer&);

    /**
     * @brief List of RPC methods a client can invoke on a server, or a server can invoke on a client
     */
    static const std::unordered_map<String, MethodFunc> METHODS;

  protected:
    /** Non-owning pointer */
    FurnaceGUI* gui;

    zmq::context_t zmqContext{1};

    /** Must be created on net thread */
    zmq::socket_t socket;

    /**
     * @brief Task queue run on the net thread
     */
    TaskQueue taskQueue;

    /**
     * @brief Thread that networking takes place on
     */
    std::optional<std::thread> thread;

    /**
     * @brief Should the net thread be stopped (set to `true` on destruction)
     */
    bool stopThread;

  public:
    NetShared(FurnaceGUI* gui);
    virtual ~NetShared();

  protected:
    void handleRequest(const NetCommon::Request& reqMessage, msgpack::sbuffer& respondBuffer);

    /**
     * @brief Download the file from the server
     */
    std::vector<uint8_t> recvGetFile();

    virtual msgpack::type::nil_t recvDoAction(const UndoAction& action);
};

#endif
