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

class NetClient {
  private:
    /** Non-owning pointer */
    FurnaceGUI* gui;

    /** Must be created on client thread */
    zmq::socket_t socket;

    /**
     * @brief Task queue run on the client thread
     */
    TaskQueue taskQueue;

    std::optional<std::thread> thread;

    /**
     * @brief Should the client thread be stopped (set to `true` on destruction)
     */
    bool stopThread;

  public:
    NetClient(FurnaceGUI* gui);
    ~NetClient();

    /**
     * @brief Start the client on another thread
     */
    void start(const String& address);

    void downloadFileAsync();

  private:
    void runThread(const String& address);

    /**
     * @brief Invoke a method on the server
     *
     * @return
     * - `R` if the request succeeds
     * - `std::nullopt` if there's an error (deserialization error or network error)
     */
    template<typename R, typename... ArgTs>
    std::optional<R> rpcCall(const String& methodName, ArgTs... args) {
      assert(thread.has_value() && "Tried to do RPC call when client thread isn't running");
      assert(std::this_thread::get_id() == thread->get_id() && "RPC calls need to be done on the client thread");

      logI("RPC: server << %s\n", methodName.c_str());

      try {
        // Serialize the request
        msgpack::zone zone;
        NetCommon::Request reqMessage = NetCommon::Request(
          methodName,
          msgpack::object(msgpack::type::tuple<ArgTs...>(args...), zone)
        );
        msgpack::sbuffer requestBuffer;
        msgpack::pack(requestBuffer, reqMessage);

        // Send a request to the server
        while (!socket.send(zmq::buffer(requestBuffer.data(), requestBuffer.size()), zmq::send_flags::dontwait).has_value()) {
          if (stopThread) return std::nullopt;
          std::this_thread::yield();
        }

        // Wait for a reply from the server
        zmq::message_t reply{};
        while (!socket.recv(reply, zmq::recv_flags::dontwait).has_value()) {
          if (stopThread) return std::nullopt;
          std::this_thread::yield();
        }

        // Try deserialize the response
        try {
          msgpack::object_handle respObjectHandle = msgpack::unpack(reply.data<char>(), reply.size());
          msgpack::object respObject = respObjectHandle.get();
          NetCommon::Response respMessage;
          respObject.convert(respMessage);

          NetCommon::StatusCode statusCode = respMessage.get<0>();
          if (statusCode == NetCommon::StatusCode::OK) {
            // The server handled our message fine! Try convert to the return type now.
            R out;
            respMessage.get<1>().convert(out);

            return out;
          } else {
            logE("Client got error from server: E%u %s\n", statusCode, NetCommon::statusToString(statusCode));
            return std::nullopt;
          }
        } catch (msgpack::type_error& e) {
          logE("MsgPack type error in client: %s\n", e.what());
          return std::nullopt;
        }
      } catch (zmq::error_t& e) {
        logE("ZMQ error in client: %s\n", e.what());
        return std::nullopt;
      }
    }
};

#endif
