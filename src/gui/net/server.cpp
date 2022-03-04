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
#include <type_traits>

/**
 * @brief Helper to invoke a member function wrapped by `wrapMethod`
 *
 * `wrapMethod` only has one template argument, `auto func`, to make it simple to actually construct the template,
 * however we do need to be able get the return type and argument types from `func`, so `wrapMethod` passes its template
 * argument type over to this struct (which has a more specialized variant).
 */
template<typename T>
struct MethodInvoker;

template<typename R, typename... ArgTs>
struct MethodInvoker<R(NetServer::*)(ArgTs...)> {
  using ReturnT = R;
  using ArgumentTuple = std::tuple<std::decay_t<ArgTs>...>;

  static ReturnT call(NetServer* server, R(NetServer::*memberFunc)(ArgTs...), ArgumentTuple&& args) {
    return std::apply(memberFunc, std::tuple_cat(std::make_tuple(server), args));
  }
};

/**
 * @brief Wrap a `NetServer` member function into something that can be stored in `METHODS`
 *
 * Wraps a `NetServer` member function into something that can take a MsgPack object, deserialize it into the function
 * arguments, call the function, and serialize the function's return value.
 */
template<auto func>
void wrapMethod(NetServer* server, const msgpack::object& argsObject, msgpack::sbuffer& returnBuffer) {
  using Sig = MethodInvoker<typeof(func)>;

  // Deserialize args
  typename Sig::ArgumentTuple argsTuple;
  argsObject.convert(argsTuple);

  // Call the wrapped function
  typename Sig::ReturnT returnVal = Sig::call(server, func, std::move(argsTuple));

  // Serialize the return value
  msgpack::pack(returnBuffer, returnVal);
}

using MethodFunc = void(*)(NetServer*, const msgpack::object&, msgpack::sbuffer&);

/**
 * @brief List of RPC methods a client can invoke
 */
static const std::unordered_map<String, MethodFunc> METHODS = {
  {NetCommon::Method::GET_FILE, &wrapMethod<&NetServer::rpcGetFile>},
  {NetCommon::Method::DO_ACTION, &wrapMethod<&NetServer::rpcDoAction>},
};

NetServer::NetServer(FurnaceGUI* gui) : gui(gui) {}

NetServer::~NetServer() {
  if (thread.has_value()) {
    stopThread = true;
    thread->join();
  }
}

void NetServer::start(uint16_t port) {
  assert(!thread.has_value() && "Tried to start net server even though it was already running");

  // Start the server thread
  logI("Starting net server\n");
  thread.emplace([this, port]() { runThread(port); });
}

void NetServer::runThread(uint16_t port) {
  zmq::context_t zmqContext{1};
  zmq::socket_t socket{zmqContext, zmq::socket_type::rep};

  try {
    socket.bind(std::string("tcp://*:") + std::to_string(port));
  } catch (zmq::error_t& e) {
    logE("Error binding socket: %s\n", e.what());
    return;
  }

  while (!stopThread) {
    try {
      // Receive a request from client
      zmq::message_t request;
      while (!socket.recv(request, zmq::recv_flags::dontwait).has_value()) {
        if (stopThread) return;
        std::this_thread::yield();
      }

      msgpack::sbuffer respondBuffer;

      try {
        // Try deserialize it
        msgpack::object_handle reqObjectHandle = msgpack::unpack(request.data<char>(), request.size());
        msgpack::object reqObject = reqObjectHandle.get();
        NetCommon::Request reqMessage;
        reqObject.convert(reqMessage);

        logI("RPC: client >> %s\n", reqMessage.get<0>().c_str());

        // Look up the method
        auto methodIter = METHODS.find(reqMessage.get<0>());
        if (methodIter != METHODS.end()) {
          // Method exists, let's try call it
          msgpack::packer<msgpack::sbuffer> packer(respondBuffer);
          packer.pack_array(2);
          packer.pack(NetCommon::StatusCode::OK);
          methodIter->second(this, reqMessage.get<1>(), respondBuffer);
        } else {
          // Method does not exist
          logE("Client tries to call non-existent method %s\n", reqMessage.get<0>().c_str());

          respondBuffer.clear();
          msgpack::pack(respondBuffer, NetCommon::Response(NetCommon::StatusCode::METHOD_NOT_FOUND, msgpack::type::nil_t()));
        }
      } catch (msgpack::type_error& e) {
        logE("MsgPack type error in server: %s\n", e.what());

        respondBuffer.clear();
        msgpack::pack(respondBuffer, NetCommon::Response(NetCommon::StatusCode::METHOD_WRONG_ARGS, msgpack::type::nil_t()));
      }

      // Send the reply to the client
      while (!socket.send(zmq::buffer(respondBuffer.data(), respondBuffer.size()), zmq::send_flags::dontwait).has_value()) {
        if (stopThread) return;
        std::this_thread::yield();
      }
    } catch (zmq::error_t& e) {
      logE("ZMQ error in server: %s\n", e.what());
    }

    std::this_thread::yield();
  }

  socket.close();
}

std::vector<uint8_t> NetServer::rpcGetFile() {
  return gui->runOnGuiThread<std::vector<uint8_t>>([&]() {
    SafeWriter* writer = gui->getEngine()->saveFur();

    std::vector<uint8_t> data(writer->size());
    memcpy(data.data(), writer->getFinalBuf(), writer->size());

    writer->finish();
    delete writer;

    return data;
  }).get();
}

msgpack::type::nil_t NetServer::rpcDoAction(const UndoAction& action) {
  return gui->runOnGuiThread<msgpack::type::nil_t>([&]() {
      gui->doRedoAction(action);

      // TODO: Broadcast changes to all connected clients somehow

    return msgpack::type::nil_t();
  }).get();
}
