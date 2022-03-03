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

#ifndef _GUI_NET_CLIENT_H
#define _GUI_NET_CLIENT_H

#include "../../ta-utils.h"
#include "../task_queue.h"
#include <rpc/client.h>
#include <thread>
#include <cstdint>

// Forward declarations
class FurnaceGUI;

class NetClient {
  private:
    /** Non-owning pointer */
    FurnaceGUI* gui;
    rpc::client client;

    /**
     * @brief Task queue run on the client thread
     */
    TaskQueue taskQueue;
    std::thread thread;

    /**
     * @brief Should the client thread be stopped (set to `true` on destruction)
     */
    bool stopThread;

  public:
    NetClient(FurnaceGUI* gui, const String& address, uint16_t port);
    ~NetClient();

    /**
     * @brief Get the currnet connection state as a friendly string
     */
    const char* getConnectionStateStr() const;

    void downloadFileAsync();

  private:
    void runThread();
};

#endif
