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
  void UndoStack::push(UndoStep&& cmd, size_t maxUndoSteps) {
    commands.resize(currentPoint);
    commands.push_back(std::forward<UndoStep>(cmd));

    currentPoint++;

    while (commands.size() > maxUndoSteps) {
      commands.pop_front();
      currentPoint--;
    }
  }

  std::optional<UndoStep*> UndoStack::undoCommand() {
    // Check if we can undo any more
    if (currentPoint == 0) return std::nullopt;
    currentPoint--;

    return &commands[currentPoint];
  }

  std::optional<UndoStep*> UndoStack::redoCommand() {
    // Check if we can redo any more
    if (currentPoint >= commands.size()) return std::nullopt;
    currentPoint++;

    return &commands[currentPoint - 1];
  }

  void UndoStack::clear() {
    commands.clear();
    currentPoint = 0;
  }

  bool CommandAddOrder::exec(FurnaceGUI* gui, Origin origin) {
    bool success;

    if (data.depth == CloneDepth::SHALLOW) {
      success = gui->getEngine()->addOrder(data.duplicateFrom, data.where);
    } else {
      if (data.duplicateFrom.has_value()) {
        success = gui->getEngine()->deepCloneOrder(*data.duplicateFrom, data.where);
      } else {
        logE("Trying to deep clone order without any `duplicateFrom`");
        return false;
      }
    }

    if (origin == Origin::LOCAL && success) gui->getEngine()->setOrder(data.where);

    return success;
  }

  void CommandAddOrder::revert(FurnaceGUI* gui, Origin origin) {
    gui->getEngine()->deleteOrder(data.where);
  }

  bool CommandDeleteOrder::exec(FurnaceGUI* gui, Origin origin) {
    // Store order data for reverting
    for (int channel = 0; channel < DIV_MAX_CHANS; channel++) {
      revertData.orderData[channel] = gui->getEngine()->song.orders.ord[channel][data.which];
    }

    return gui->getEngine()->deleteOrder(data.which);
  }

  void CommandDeleteOrder::revert(FurnaceGUI* gui, Origin origin) {
    // Re-add the order
    gui->getEngine()->addOrder(std::nullopt, data.which);

    // Re-add the order data
    for (int channel = 0; channel < DIV_MAX_CHANS; channel++) {
      gui->getEngine()->song.orders.ord[channel][data.which] = revertData.orderData[channel];
    }
  }

  bool CommandSwapOrders::exec(FurnaceGUI* gui, Origin origin) {
    if (gui->getEngine()->swapOrders(data.a, data.b)) {
      if (origin == Origin::LOCAL) {
        // Change the current order if the cursor was on an order we just swapped
        if (gui->getEngine()->getOrder() == data.a) {
          gui->getEngine()->setOrder(data.b);
        } else if (gui->getEngine()->getOrder() == data.b) {
          gui->getEngine()->setOrder(data.a);
        }
      }
    }

    return true;
  }

  void CommandSwapOrders::revert(FurnaceGUI* gui, Origin origin) {
    // Reverting has the exact same behaviour as execing for swapping orders
    exec(gui, origin);
  }

  bool CommandSetOrders::exec(FurnaceGUI* gui, Origin origin) {
    revertData.oldPatterns.clear();

    bool didModify = false;

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
        return didModify;
      }

      int oldPattern = gui->getEngine()->song.orders.ord[newPattern.channel][newPattern.order];

      if (oldPattern != newPattern.pattern) {
        revertData.oldPatterns.push_back(OrderPattern {
          newPattern.order,
          newPattern.channel,
          oldPattern,
        });
        gui->getEngine()->song.orders.ord[newPattern.channel][newPattern.order] = newPattern.pattern;

        didModify = true;
      }
    }

    if (didModify) {
      gui->getEngine()->walkSong(gui->loopOrder, gui->loopRow, gui->loopEnd);
    }

    return didModify;
  }

  void CommandSetOrders::revert(FurnaceGUI* gui, Origin origin) {
    for (const OrderPattern& oldPattern : revertData.oldPatterns) {
      gui->getEngine()->song.orders.ord[oldPattern.channel][oldPattern.order] = oldPattern.pattern;
    }

    gui->getEngine()->walkSong(gui->loopOrder, gui->loopRow, gui->loopEnd);
  }

  bool CommandSetPatternData::exec(FurnaceGUI* gui, Origin origin) {
    revertData.oldPatternData.clear();

    bool didModify = false;

    for (const PatternDataEdit& edit : data.newPatternData) {
      if (
        edit.channel < 0 ||
        edit.channel >= DIV_MAX_CHANS ||
        edit.patternIndex < 0 ||
        edit.patternIndex >= DIV_MAX_PATTERNS ||
        edit.row < 0 ||
        edit.row >= DIV_PATTERN_MAX_ROWS ||
        edit.type < 0 ||
        edit.type >= DIV_PATTERN_MAX_TYPES
      ) {
        logE("CommandSetPatternData got out-of-bounds data");
        return didModify;
      }

      DivPattern* pattern = gui
        ->getEngine()
        ->song
        .pat[edit.channel]
        .getPattern(edit.patternIndex, true);

      short oldValue = pattern->data[edit.row][edit.type];

      if (oldValue != edit.newValue) {
        // Store the current value before we mutate it, so we can `revert()` it
        revertData.oldPatternData.push_back(PatternDataEdit {
            edit.channel,
            edit.patternIndex,
            edit.row,
            edit.type,
            oldValue,
        });

        pattern->data[edit.row][edit.type] = edit.newValue;

        didModify = true;
      }
    }

    return didModify;
  }

  void CommandSetPatternData::revert(FurnaceGUI* gui, Origin origin) {
    for (const PatternDataEdit& edit : revertData.oldPatternData) {
      DivPattern* pattern = gui
        ->getEngine()
        ->song
        .pat[edit.channel]
        .getPattern(edit.patternIndex, true);

      pattern->data[edit.row][edit.type] = edit.newValue;
    }

    gui->getEngine()->walkSong(gui->loopOrder, gui->loopRow, gui->loopEnd);
  }

  bool CommandUpdateInstrument::exec(FurnaceGUI* gui, Origin origin) {
    if (data.instrumentIndex >= gui->getEngine()->song.ins.size()) return false;

    DivInstrument* instrument = gui->getEngine()->song.ins[data.instrumentIndex];

    // TODO: Generate revertData
    StructUpdate::update(*instrument, data.partial);
    gui->getEngine()->notifyInsChange(data.instrumentIndex);

    return true;
  }

  void CommandUpdateInstrument::revert(FurnaceGUI* gui, Origin origin) {
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
        case Kind::PATTERN_SET_DATA: {
          std::unique_ptr<CommandSetPatternData> cmd = std::make_unique<CommandSetPatternData>();
          obj.convert(cmd);
          return cmd;
        }
        case Kind::UPDATE_INSTRUMENT: {
          std::unique_ptr<CommandUpdateInstrument> cmd = std::make_unique<CommandUpdateInstrument>();
          obj.convert(cmd);
          return cmd;
        }
        default:
          return std::nullopt;
      }
    } catch (msgpack::type_error& e) {
      logE("Error deserializing command: %s\n", e.what());
      return std::nullopt;
    }
  }
#endif
}
