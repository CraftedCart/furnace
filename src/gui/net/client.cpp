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

#include "client.h"
#include "../gui.h"
#include "../../ta-log.h"
#include <rpc/rpc_error.h>

NetClient::NetClient(FurnaceGUI* gui, const String& address, uint16_t port) :
  gui(gui),
  client(address, port),
  thread([&]() { runThread(); }) {}

  NetClient::~NetClient() {
    stopThread = true;
    thread.join();
  }

const char* NetClient::getConnectionStateStr() const {
  switch (client.get_connection_state()) {
    case rpc::client::connection_state::initial:
      return "Waiting to connect...";
    case rpc::client::connection_state::connected:
      return "Connected";
    case rpc::client::connection_state::disconnected:
      return "Lost connection";
    case rpc::client::connection_state::reset:
      return "Connection reset";
  }
}

void NetClient::downloadFileAsync() {
  taskQueue.enqueue<void>([&]() {
    try {
      std::vector<uint8_t> file = client.call("getFile").as<std::vector<uint8_t>>();

      // Copy the file into a new buffer, since `DivEngine::load` expects to be able to `delete[]` it
      uint8_t* buf = new uint8_t[file.size()];
      memcpy(buf, file.data(), file.size());

      gui->runOnGuiThread<void>([&]() {
        if (!gui->getEngine()->load(buf, file.size())) {
          logE("Error loading file gotten from RPC (in downloadFileAsync)\n");
        }
      }).get();
    } catch (rpc::rpc_error& e) {
      // TODO: Handle errors more user-visibly
      logE("RPC error (in downloadFileAsync): %s\n", e.what());
    } catch (rpc::timeout& e) {
      // TODO: Handle errors more user-visibly
      logE("RPC timeout (in downloadFileAsync): %s\n", e.what());
    }
  });
}

void NetClient::runThread() {
  while (!stopThread) {
    taskQueue.processTasks();
    std::this_thread::yield();
  }
}
