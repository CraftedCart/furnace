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

NetClient::NetClient(FurnaceGUI* gui) :
  gui(gui) {}

NetClient::~NetClient() {
  if (thread.has_value()) {
    stopThread = true;
    thread->join();
  }
}

void NetClient::start(const String& address) {
  assert(!thread.has_value() && "Tried to start net client even though it was already running");

  // Start the client thread
  logI("Starting net client\n");
  thread.emplace([this, address]() { runThread(address); });
}

bool NetClient::isDownloadingFile() const {
  return downloadingFile;
}

void NetClient::downloadFileAsync() {
  assert(!downloadingFile && "Tried to get file even though we're already waiting to get one");

  downloadingFile = true;

  taskQueue.enqueue<void>([=]() {
    std::optional<std::vector<uint8_t>> file = rpcCall<std::vector<uint8_t>>(NetCommon::Method::GET_FILE);
    if (!file.has_value()) return;

    // Copy the file into a new buffer, since `DivEngine::load` expects to be able to `delete[]` it
    uint8_t* buf = new uint8_t[file->size()];
    memcpy(buf, file->data(), file->size());

    gui->runOnGuiThread<void>([&]() {
      if (!gui->getEngine()->load(buf, file->size())) {
        logE("Error loading file gotten from RPC (in downloadFileAsync)\n");
      }

      downloadingFile = false;
    }).get();
  });
}

void NetClient::sendActionAsync(const UndoAction& action) {
  taskQueue.enqueue<void>([=]() {
    rpcCall<msgpack::type::nil_t>(NetCommon::Method::DO_ACTION, action);
  });
}

void NetClient::runThread(const String& address) {
  zmq::context_t zmqContext{1};
  socket = zmq::socket_t{zmqContext, zmq::socket_type::dealer};

  try {
    socket.connect(std::string("tcp://") + address);
  } catch (zmq::error_t& e) {
    logE("Error connecting to socket: %s\n", e.what());
    return;
  }

  while (!stopThread) {
    taskQueue.processTasks();
    std::this_thread::yield();
  }

  socket.close();
}
