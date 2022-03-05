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

#include "server.h"
#include "common.h"
#include "../gui.h"
#include "../../ta-log.h"
#include <cinttypes>

NetServer::NetServer(FurnaceGUI* gui) : NetShared(gui) {}

void NetServer::start(uint16_t port) {
  assert(!thread.has_value() && "Tried to start net server even though it was already running");

  // Start the server thread
  logI("Starting net server\n");
  thread.emplace([this, port]() { runThread(port); });
}

void NetServer::sendExecCommand(const EditAction::Command& cmd) {
  msgpack::zone zone;
  msgpack::object cmdObject = cmd.serialize(zone);

  taskQueue.enqueue<void>([this, zone = std::move(zone), cmdObject = std::move(cmdObject)]() {
    for (const NetCommon::ClientId& client : connectedClients) {
      rpcCall(client, NetCommon::Method::EXEC_COMMAND, cmdObject);
    }
  });
}

void NetServer::runThread(uint16_t port) {
  socket = zmq::socket_t{zmqContext, zmq::socket_type::router};

  try {
    socket.bind(std::string("tcp://*:") + std::to_string(port));
  } catch (zmq::error_t& e) {
    logE("Error binding socket: %s\n", e.what());
    return;
  }

  while (!stopThread) {
    currentClient = std::nullopt;

    std::this_thread::yield();

    taskQueue.processTasks();

    try {
      // Receive a request from client
      // First we receive an identifier for the client
      zmq::message_t requestFrom;
      while (!socket.recv(requestFrom, zmq::recv_flags::dontwait).has_value()) {
        if (stopThread) return;
        taskQueue.processTasks();
        std::this_thread::yield();
      }
      // Then we receive the client's payload
      zmq::message_t request;
      while (!socket.recv(request, zmq::recv_flags::dontwait).has_value()) {
        if (stopThread) return;
        taskQueue.processTasks();
        std::this_thread::yield();
      }

      NetCommon::ClientId clientId = NetCommon::ClientId::fromMessage(requestFrom);
      currentClient = clientId;

      // Add the client to our list of connected clients (if it isn't already there)
      connectedClients.insert(clientId);

      msgpack::sbuffer respondBuffer;

      try {
        // Try deserialize it
        msgpack::object_handle reqObjectHandle = msgpack::unpack(request.data<char>(), request.size());
        msgpack::object reqObject = reqObjectHandle.get();
        NetCommon::RequestOrResponse reqOrRespMessage;
        reqObject.convert(reqOrRespMessage);

        switch (reqOrRespMessage.kind) {
          case NetCommon::MessageKind::REQUEST: {
            msgpack::sbuffer respondBuffer;
            handleRequest(NetCommon::Request::from(std::move(reqOrRespMessage)), respondBuffer);

            // First we send the client identifier we want to send to
            while (!socket.send(requestFrom, zmq::send_flags::dontwait | zmq::send_flags::sndmore).has_value()) {
              if (stopThread) return;
              taskQueue.processTasks();
              std::this_thread::yield();
            }
            // Then we send the payload to the client
            while (!socket.send(zmq::buffer(respondBuffer.data(), respondBuffer.size()), zmq::send_flags::dontwait).has_value()) {
              if (stopThread) return;
              taskQueue.processTasks();
              std::this_thread::yield();
            }
            break;
          }

          case NetCommon::MessageKind::RESPONSE: {
            handleResponse(NetCommon::Response::from(std::move(reqOrRespMessage)));
            break;
          }

          default: {
            logE("Invalid message kind from client\n");
            break;
          }
        }
      } catch (msgpack::type_error& e) {
        logE("MsgPack type error in server (not enough info to respond to client): %s\n", e.what());
        continue;
      }
    } catch (zmq::error_t& e) {
      logE("ZMQ error in server: %s\n", e.what());
    }
  }

  currentClient = std::nullopt;
  socket.close();
}

void NetServer::recvExecCommand(EditAction::Command& cmd) {
  NetShared::recvExecCommand(cmd);

  // Propagate message to other clients
  msgpack::zone zone;
  msgpack::object cmdObject = cmd.serialize(zone);

  for (const NetCommon::ClientId& client : connectedClients) {
    if (client == currentClient) continue;

    rpcCall(client, NetCommon::Method::EXEC_COMMAND, cmdObject);
  }
}
