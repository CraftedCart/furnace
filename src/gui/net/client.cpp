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
  NetShared(gui) {}

void NetClient::start(const String& address) {
  assert(!thread.has_value() && "Tried to start net client even though it was already running");

  // Start the client thread
  logI("Starting net client\n");
  thread.emplace([this, address]() { runThread(address); });
  workerThread.emplace([this]() { runWorkerThread(); });
}

bool NetClient::isDownloadingFile() const {
  return downloadingFile;
}

void NetClient::sendDownloadFile() {
  assert(!downloadingFile && "Tried to get file even though we're already waiting to get one");

  downloadingFile = true;

  std::future<RpcResponse> futureFile = taskQueue.enqueue<std::future<RpcResponse>>([this]() {
    return rpcCall(NetCommon::Method::GET_FILE);
  }).get();

  workerTaskQueue.enqueue<void>([this, futureFile = std::move(futureFile)]() mutable {
    std::optional<std::vector<uint8_t>> file = futureFile.get().as<std::vector<uint8_t>>();
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

void NetClient::sendExecCommand(const EditAction::Command& cmd) {
  msgpack::zone zone;
  msgpack::object cmdObject = cmd.serialize(zone);

  taskQueue.enqueue<void>([this, zone = std::move(zone), cmdObject = std::move(cmdObject)]() {
    rpcCall(NetCommon::Method::EXEC_COMMAND, cmdObject);
  });
}

void NetClient::runThread(const String& address) {
  socket = zmq::socket_t{zmqContext, zmq::socket_type::dealer};

  try {
    socket.connect(std::string("tcp://") + address);
  } catch (zmq::error_t& e) {
    logE("Error connecting to socket: %s\n", e.what());
    return;
  }

  while (!stopThread) {
    std::this_thread::yield();

    taskQueue.processTasks();

    // See if we have any messages from the server
    try {
      zmq::message_t reply{};
      if (!socket.recv(reply, zmq::recv_flags::dontwait).has_value()) {
        continue;
      }

    // Try deserialize the response
      msgpack::object_handle respObjectHandle = msgpack::unpack(reply.data<char>(), reply.size());
      msgpack::object respObject = respObjectHandle.get();
      NetCommon::RequestOrResponse reqOrRespMessage;
      respObject.convert(reqOrRespMessage);

      switch (reqOrRespMessage.kind) {
        case NetCommon::MessageKind::REQUEST: {
          msgpack::sbuffer respondBuffer;
          handleRequest(NetCommon::Request::from(std::move(reqOrRespMessage)), respondBuffer);

          while (!socket.send(zmq::buffer(respondBuffer.data(), respondBuffer.size()), zmq::send_flags::dontwait).has_value()) {
            if (stopThread) return;
            std::this_thread::yield();
          }
          break;
        }

        case NetCommon::MessageKind::RESPONSE: {
          handleResponse(NetCommon::Response::from(std::move(reqOrRespMessage)));
          break;
        }

        default: {
          logE("Invalid message kind from server\n");
          break;
        }
      }
    } catch (msgpack::type_error& e) {
      logE("MsgPack type error in client: %s\n", e.what());
    } catch (zmq::error_t& e) {
      logE("ZMQ error in client: %s\n", e.what());
    }
  }

  socket.close();
}
