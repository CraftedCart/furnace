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
#include "../gui.h"

NetServer::NetServer(FurnaceGUI* gui, uint16_t port) :
  gui(gui),
  server(port) {

  server.bind("getFile", [&]() { return rpcGetFile(); });
}

void NetServer::start() {
  server.async_run();
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
