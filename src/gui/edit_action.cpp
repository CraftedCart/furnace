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

#include "edit_action.h"
#include "gui.h"
#include "../ta-log.h"

namespace EditAction {
  void CommandAddOrder::exec(FurnaceGUI* gui, Origin origin) {
    bool success;

    if (data.depth == CloneDepth::SHALLOW) {
      success = gui->getEngine()->addOrder(data.duplicateFrom, data.where);
    } else {
      if (data.duplicateFrom.has_value()) {
        success = gui->getEngine()->deepCloneOrder(*data.duplicateFrom, data.where);
      } else {
        logE("Trying to deep clone order without any `duplicateFrom`");
        return;
      }
    }

    if (origin == Origin::LOCAL && success) gui->getEngine()->setOrder(data.where);
    if (success) gui->setModified();
  }

  void CommandAddOrder::revert(FurnaceGUI* gui, Origin origin) {
    if (gui->getEngine()->deleteOrder(data.where)) {
      gui->setModified();
    }
  }

  void CommandDeleteOrder::exec(FurnaceGUI* gui, Origin origin) {
    // TODO: Store order data for reverting

    if (gui->getEngine()->deleteOrder(data.which)) {
      gui->setModified();
    }
  }

  void CommandDeleteOrder::revert(FurnaceGUI* gui, Origin origin) {
    // TODO
  }

  void CommandSwapOrders::exec(FurnaceGUI* gui, Origin origin) {
    if (gui->getEngine()->swapOrders(data.a, data.b)) {
      gui->setModified();

      if (origin == Origin::LOCAL) {
        // Change the current order if the cursor was on an order we just swapped
        if (gui->getEngine()->getOrder() == data.a) {
          gui->getEngine()->setOrder(data.b);
        } else if (gui->getEngine()->getOrder() == data.b) {
          gui->getEngine()->setOrder(data.a);
        }
      }
    }
  }

  void CommandSwapOrders::revert(FurnaceGUI* gui, Origin origin) {
    // Reverting has the exact same behaviour as execing for swapping orders
    exec(gui, origin);
  }

  void CommandSetOrders::exec(FurnaceGUI* gui, Origin origin) {
    for (const OrderPattern& newPattern : data.newPatterns) {
      if (
        newPattern.order < 0 ||
        newPattern.order >= DIV_MAX_ORDERS ||
        newPattern.channel < 0 ||
        newPattern.channel >= DIV_MAX_CHANS ||
        newPattern.pattern < 0 ||
        newPattern.pattern >= DIV_MAX_PATTERNS
      ) {
        logE("CommandSetOrders got out-of-bounds data");
        return;
      }

      gui->getEngine()->song.orders.ord[newPattern.channel][newPattern.order] = newPattern.pattern;
      gui->getEngine()->walkSong(gui->loopOrder, gui->loopRow, gui->loopEnd);
    }
  }

  void CommandSetOrders::revert(FurnaceGUI* gui, Origin origin) {
    // TODO
  }

#if HAVE_NETWORKING
  struct UntypedPackedData {
    Kind kind;

    FURNACE_NET_STRUCT_SERIALIZABLE(kind);
  };

  std::optional<std::unique_ptr<Command>> deserializeCommand(const msgpack::object& obj) {
    try {
      UntypedPackedData untypedPackedData;
      obj.convert(untypedPackedData);

      switch (untypedPackedData.kind) {
        case Kind::ORDER_ADD: {
          std::unique_ptr<CommandAddOrder> cmd = std::make_unique<CommandAddOrder>();
          obj.convert(cmd);
          return cmd;
        }
        case Kind::ORDER_DELETE: {
          std::unique_ptr<CommandDeleteOrder> cmd = std::make_unique<CommandDeleteOrder>();
          obj.convert(cmd);
          return cmd;
        }
        case Kind::ORDER_SWAP: {
          std::unique_ptr<CommandSwapOrders> cmd = std::make_unique<CommandSwapOrders>();
          obj.convert(cmd);
          return cmd;
        }
        case Kind::ORDER_SET: {
          std::unique_ptr<CommandSetOrders> cmd = std::make_unique<CommandSetOrders>();
          obj.convert(cmd);
          return cmd;
        }
        default:
          return std::nullopt;
      }
    } catch (msgpack::type_error& e) {
      return std::nullopt;
    }
  }
#endif
}
