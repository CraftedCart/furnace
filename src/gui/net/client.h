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

#include "shared.h"
#include "../../ta-utils.h"
#include "../../ta-log.h"
#include <zmq.hpp>
#include <future>
#include <unordered_map>

// Forward declarations
class FurnaceGUI;
struct UndoAction;

class NetClient : public NetShared {
  private:
    /**
     * @brief Are we in the middle of downloading the .fur file from the server?
     */
    bool downloadingFile = false;

  public:
    NetClient(FurnaceGUI* gui);

    /**
     * @brief Start the client on another thread
     */
    void start(const String& address);

    bool isDownloadingFile() const;

    void sendDownloadFile();
    void sendExecCommand(const EditAction::Command& cmd);

  private:
    void runThread(const String& address);

    /**
     * @brief Invoke a method on the remote
     */
    template<typename... ArgTs>
    std::future<RpcResponse> rpcCall(const String& methodName, ArgTs... args) {
      assert(thread.has_value() && "Tried to do RPC call when net thread isn't running");
      assert(std::this_thread::get_id() == thread->get_id() && "RPC calls need to be done on the net thread");

      try {
        uint64_t requestId = lastRequestId++;
        logI("RPC: [%" PRIu64 "] remote << %s\n", requestId, methodName.c_str());

        // Serialize the request
        msgpack::zone zone;
        NetCommon::Request reqMessage = NetCommon::Request{
          NetCommon::MessageKind::REQUEST,
          requestId,
          methodName,
          msgpack::object(msgpack::type::tuple<ArgTs...>(args...), zone)
        };
        msgpack::sbuffer requestBuffer;
        msgpack::pack(requestBuffer, reqMessage);

        // Send a request to the server
        while (!socket.send(zmq::buffer(requestBuffer.data(), requestBuffer.size()), zmq::send_flags::dontwait).has_value()) {
          if (stopThread) {
            std::promise<RpcResponse> promise;
            promise.set_value(RpcResponse());
            return promise.get_future();
          }

          taskQueue.processTasks();
          std::this_thread::yield();
        }

        // Make a promise corresponding to this request
        std::promise<RpcResponse> promise;
        std::future<RpcResponse> future = promise.get_future();
        pendingRequests.insert({requestId, std::move(promise)});
        return future;

      } catch (zmq::error_t& e) {
        logE("ZMQ error: %s\n", e.what());

        std::promise<RpcResponse> promise;
        promise.set_value(RpcResponse());
        return promise.get_future();
      }
    }
};

#endif
