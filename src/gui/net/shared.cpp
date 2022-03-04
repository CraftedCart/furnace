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

#include "shared.h"
#include "../gui.h"
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
struct MethodInvoker<R(NetShared::*)(ArgTs...)> {
  using ReturnT = R;
  using ArgumentTuple = std::tuple<std::decay_t<ArgTs>...>;

  static ReturnT call(NetShared* server, R(NetShared::*memberFunc)(ArgTs...), ArgumentTuple&& args) {
    return std::apply(memberFunc, std::tuple_cat(std::make_tuple(server), args));
  }
};

/**
 * @brief Wrap a `NetShared` member function into something that can be stored in `METHODS`
 *
 * Wraps a `NetShared` member function into something that can take a MsgPack object, deserialize it into the function
 * arguments, call the function, and serialize the function's return value.
 */
template<auto func>
void wrapMethod(NetShared* server, const msgpack::object& argsObject, msgpack::sbuffer& returnBuffer) {
  using Sig = MethodInvoker<typeof(func)>;

  // Deserialize args
  typename Sig::ArgumentTuple argsTuple;
  argsObject.convert(argsTuple);

  // Call the wrapped function
  typename Sig::ReturnT returnVal = Sig::call(server, func, std::move(argsTuple));

  // Serialize the return value
  msgpack::pack(returnBuffer, returnVal);
}

const std::unordered_map<String, NetShared::MethodFunc> NetShared::METHODS = {
  {NetCommon::Method::GET_FILE, &wrapMethod<&NetShared::recvGetFile>},
  {NetCommon::Method::DO_ACTION, &wrapMethod<&NetShared::recvDoAction>},
};

NetShared::NetShared(FurnaceGUI* gui) :
  gui(gui) {}

NetShared::~NetShared() {
  stopThread = true;
  zmqContext.shutdown();
  if (thread.has_value()) thread->join();
}

void NetShared::handleRequest(const NetCommon::Request& reqMessage, msgpack::sbuffer& respondBuffer) {
  uint64_t messageId = reqMessage.id;

  logI("RPC: [%" PRIu64 "] remote >> %s\n", messageId, reqMessage.methodName.c_str());

  // Look up the method
  auto methodIter = METHODS.find(reqMessage.methodName);
  if (methodIter != METHODS.end()) {
    // Method exists, let's try call it
    try {
      msgpack::packer<msgpack::sbuffer> packer(respondBuffer);
      packer.pack_array(4);
      packer.pack(NetCommon::MessageKind::RESPONSE);
      packer.pack(messageId);
      packer.pack(NetCommon::StatusCode::OK);
      methodIter->second(this, reqMessage.args, respondBuffer);

      logI("RPC: [%" PRIu64 "] >> remote\n", messageId);
    } catch (msgpack::type_error& e) {
      logE("MsgPack type error when handling request: %s\n", e.what());

      respondBuffer.clear();
      msgpack::pack(respondBuffer, NetCommon::Response{
        NetCommon::MessageKind::RESPONSE,
        messageId,
        NetCommon::StatusCode::METHOD_WRONG_ARGS,
        msgpack::object(msgpack::type::nil_t())
      });
    }
  } else {
    // Method does not exist
    logE("Remote tried to call non-existent method %s\n", reqMessage.methodName.c_str());

    respondBuffer.clear();
    msgpack::pack(respondBuffer, NetCommon::Response{
      NetCommon::MessageKind::RESPONSE,
      messageId,
      NetCommon::StatusCode::METHOD_NOT_FOUND,
      msgpack::object(msgpack::type::nil_t())
    });
  }
}

std::vector<uint8_t> NetShared::recvGetFile() {
  return gui->runOnGuiThread<std::vector<uint8_t>>([&]() {
    SafeWriter* writer = gui->getEngine()->saveFur();

    std::vector<uint8_t> data(writer->size());
    memcpy(data.data(), writer->getFinalBuf(), writer->size());

    writer->finish();
    delete writer;

    return data;
  }).get();
}

msgpack::type::nil_t NetShared::recvDoAction(const UndoAction& action) {
  return gui->runOnGuiThread<msgpack::type::nil_t>([&]() {
    gui->doRedoAction(action);

    return msgpack::type::nil_t();
  }).get();
}
