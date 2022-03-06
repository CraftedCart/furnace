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

#ifndef _GUI_EDIT_ACTION_H
#define _GUI_EDIT_ACTION_H

#include "net/serialize.h"
#include "../engine/orders.h"

// Forward declarations
class FurnaceGUI;

#ifdef HAVE_NETWORKING
  #define FURNACE_COMMAND_SIMPLE_IMPL(T, commandKind) \
    private: \
      struct PackedData { \
        Kind kind; \
        Data data; \
        \
        FURNACE_NET_STRUCT_SERIALIZABLE(kind, data); \
      }; \
      \
    private: \
      Data data; \
      \
    public: \
      T() = default; \
      T(Data data) : data(data) {} \
      virtual bool exec(FurnaceGUI* gui, Origin origin) override; \
      virtual void revert(FurnaceGUI* gui, Origin origin) override; \
      \
      virtual msgpack::object serialize(msgpack::zone& z) const override { \
        PackedData packedData = { commandKind, data }; \
        return msgpack::object(packedData, z); \
      } \
      \
      template<typename Packer> \
      void msgpack_pack(Packer& msgpack_pk) const { \
        PackedData packedData = { commandKind, data }; \
        packedData.msgpack_pack(msgpack_pk); \
      } \
      \
      void msgpack_unpack(msgpack::object const& msgpack_o) { \
        PackedData packedData; \
        packedData.msgpack_unpack(msgpack_o); \
        \
        assert(packedData.kind == commandKind); \
        data = packedData.data; \
      } \
      \
      template<typename MSGPACK_OBJECT> \
      void msgpack_object(MSGPACK_OBJECT* msgpack_o, msgpack::zone& msgpack_z) const { \
        PackedData packedData = { commandKind, data }; \
        packedData.msgpack_object(msgpack_o, msgpack_z); \
      }

  #define FURNACE_COMMAND_SIMPLE_IMPL_WITH_REVERT_DATA(T, commandKind) \
    private: \
      struct PackedData { \
        Kind kind; \
        Data data; \
        RevertData revertData; \
        \
        FURNACE_NET_STRUCT_SERIALIZABLE(kind, data, revertData); \
      }; \
      \
    private: \
      Data data; \
      RevertData revertData; \
      \
    public: \
      T() = default; \
      T(Data data) : data(data) {} \
      virtual bool exec(FurnaceGUI* gui, Origin origin) override; \
      virtual void revert(FurnaceGUI* gui, Origin origin) override; \
      \
      virtual msgpack::object serialize(msgpack::zone& z) const override { \
        PackedData packedData = { commandKind, data, revertData }; \
        return msgpack::object(packedData, z); \
      } \
      \
      template<typename Packer> \
      void msgpack_pack(Packer& msgpack_pk) const { \
        PackedData packedData = { commandKind, data, revertData }; \
        packedData.msgpack_pack(msgpack_pk); \
      } \
      \
      void msgpack_unpack(msgpack::object const& msgpack_o) { \
        PackedData packedData; \
        packedData.msgpack_unpack(msgpack_o); \
        \
        assert(packedData.kind == commandKind); \
        data = packedData.data; \
        revertData = packedData.revertData; \
      } \
      \
      template<typename MSGPACK_OBJECT> \
      void msgpack_object(MSGPACK_OBJECT* msgpack_o, msgpack::zone& msgpack_z) const { \
        PackedData packedData = { commandKind, data, revertData }; \
        packedData.msgpack_object(msgpack_o, msgpack_z); \
      }
#else
  #define FURNACE_COMMAND_SIMPLE_IMPL(kind) \
    private: \
      Data data; \
      \
    public: \
      T() = default; \
      T(Data data) : data(data) {} \
      virtual bool exec(FurnaceGUI* gui, Origin origin) override; \
      virtual void revert(FurnaceGUI* gui, Origin origin) override;

  #define FURNACE_COMMAND_SIMPLE_IMPL_WITH_REVERT_DATA(T, commandKind) \
    private: \
      Data data; \
      RevertData revertData; \
      \
    public: \
      T() = default; \
      T(Data data) : data(data) {} \
      virtual bool exec(FurnaceGUI* gui, Origin origin) override; \
      virtual void revert(FurnaceGUI* gui, Origin origin) override;
#endif

/**
 * Implements the `clone()` virtual member function to just copy `data`
 */
#define FURNACE_COMMAND_SIMPLE_CLONE(T) \
  public: \
    virtual std::unique_ptr<Command> clone() const override { \
      std::unique_ptr<T> cloned = std::make_unique<T>(); \
      cloned->data = data; \
      \
      return cloned; \
    } \
  private:

struct SelectionPoint {
  int xCoarse, xFine;
  int y;
  SelectionPoint():
    xCoarse(0), xFine(0), y(0) {}
};

namespace EditAction {
  /**
   * @brief Each command has its own entry in this enum
   */
  enum class Kind {
    ORDER_ADD = 0,
    ORDER_DELETE = 1,
    ORDER_SWAP = 2,
    ORDER_SET = 3,
    PATTERN_SET_DATA = 4,
  };

  /**
   * @brief Where a command originates from
   *
   * Commands may want to differentiate between local and remote origins if say, a local user doing commands should move
   * the cursor around, but remote users seeing the same command should not have their cursor jump around.
   */
  enum class Origin {
    LOCAL,
    REMOTE,
  };

  enum class CloneDepth {
    SHALLOW = 0,
    DEEP = 1,
  };

  struct OrderPattern {
    int order;
    int channel;
    int pattern;

    FURNACE_NET_STRUCT_SERIALIZABLE(order, channel, pattern);
  };

  /**
   * @brief A single edit that should be applied to a pattern
   *
   * Applying the edit is pretty much doing...
   * @code
   * pattern->data[row][type] = newValue;
   * @endcode
   */
  struct PatternDataEdit {
    int channel;
    unsigned char patternIndex;
    int row;
    int type;
    short newValue;

    FURNACE_NET_STRUCT_SERIALIZABLE(channel, patternIndex, row, type, newValue);
  };

  /**
   * @brief Base class for undoable/networkable commands
   */
  class Command {
    public:
      virtual ~Command() = default;

      /**
       * @brief Run the command
       *
       * Used for redoing as well as initially doing
       *
       * @return Whether the command made any changes (and should be recorded in the undo history)
       */
      [[nodiscard]]
      virtual bool exec(FurnaceGUI* gui, Origin origin) = 0;

      /**
       * @brief Undo the command
       */
      virtual void revert(FurnaceGUI* gui, Origin origin) = 0;

#ifdef HAVE_NETWORKING
      virtual msgpack::object serialize(msgpack::zone& z) const = 0;
#endif

      virtual std::unique_ptr<Command> clone() const = 0;
  };

  /**
   * @brief Cursor and order positioning for an undo step
   */
  struct UndoPosition {
    SelectionPoint cursor, selStart, selEnd;
    int order;
    bool nibble;
  };

  struct UndoStep {
    /**
     * @brief Call `cmd.revert()` to undo, and `cmd.exec()` to redo
     */
    std::unique_ptr<Command> cmd;

    /**
     * @brief Cursor/order positioning before the command was executed
     */
    UndoPosition positionPre;

    /**
     * @brief Cursor/order positioning after the command was executed
     */
    UndoPosition positionPost;
  };

  class UndoStack {
    private:
      std::deque<UndoStep> commands;
      size_t currentPoint = 0;

    public:
      UndoStack() = default;

      /**
       * @brief Push an item onto the undo stack
       *
       * @param step The undo step to push
       * @param maxUndoSteps If the new undo stack becomes larger than this value, old commands will be discarded
       */
      void push(UndoStep&& step, size_t maxUndoSteps);

      /**
       * @brief Get a step to undo
       *
       * The returned value is a non-owning pointer and can become invalidated when pushing new commands onto the stack.
       *
       * Usage:
       * @code
       * std::optional<EditAction::UndoStep*> step = undoStack.undoCommand();
       * if (step.has_value()) {
       *   (*step)->cmd->revert(gui, origin);
       *
       *   if (!engine->isPlaying()) {
       *     cursor = (*step)->positionPre.cursor;
       *     selStart = (*step)->positionPre.selStart;
       *     selEnd = (*step)->positionPre.selEnd;
       *     curNibble = (*step)->positionPre.nibble;
       *     updateScroll(cursor.y);
       *     engine->setOrder((*step)->positionPre.order);
       *   }
       * }
       * @endcode
       */
      [[nodiscard]]
      std::optional<UndoStep*> undoCommand();

      /**
       * @brief Get a step to redo
       *
       * The returned value is a non-owning pointer and can become invalidated when pushing new commands onto the stack.
       *
       * Usage:
       * @code
       * std::optional<EditAction::UndoStep*> step = undoStack.redoCommand();
       * if (step.has_value()) {
       *   (*step)->cmd->exec(gui, origin);
       *
       *   if (!engine->isPlaying()) {
       *     cursor = (*step)->positionPost.cursor;
       *     selStart = (*step)->positionPost.selStart;
       *     selEnd = (*step)->positionPost.selEnd;
       *     curNibble = (*step)->positionPost.nibble;
       *     updateScroll(cursor.y);
       *     engine->setOrder((*step)->positionPost.order);
       *   }
       * }
       * @endcode
       */
      [[nodiscard]]
      std::optional<UndoStep*> redoCommand();

      /**
       * @brief Clear the undo/redo history
       */
      void clear();
  };

  /**
   * @brief Add or duplicate an order
   */
  class CommandAddOrder : public Command {
    public:
      struct Data {
        std::optional<int> duplicateFrom;
        int where;
        CloneDepth depth;

        FURNACE_NET_STRUCT_SERIALIZABLE(duplicateFrom, where, depth);
      };

    private:
      FURNACE_COMMAND_SIMPLE_IMPL(CommandAddOrder, Kind::ORDER_ADD);
      FURNACE_COMMAND_SIMPLE_CLONE(CommandAddOrder);
  };

  /**
   * @brief Delete an order
   */
  class CommandDeleteOrder : public Command {
    public:
      struct Data {
        int which;

        FURNACE_NET_STRUCT_SERIALIZABLE(which);
      };

      struct RevertData {
        unsigned char orderData[DIV_MAX_CHANS];

        FURNACE_NET_STRUCT_SERIALIZABLE(orderData);
      };

    private:
      FURNACE_COMMAND_SIMPLE_IMPL_WITH_REVERT_DATA(CommandDeleteOrder, Kind::ORDER_DELETE);
      FURNACE_COMMAND_SIMPLE_CLONE(CommandDeleteOrder);
  };

  /**
   * @brief Swap two orders
   *
   * Used for shifting orders up/down
   */
  class CommandSwapOrders : public Command {
    public:
      struct Data {
        int a;
        int b;

        FURNACE_NET_STRUCT_SERIALIZABLE(a, b);
      };

    private:
      FURNACE_COMMAND_SIMPLE_IMPL(CommandSwapOrders, Kind::ORDER_SWAP);
      FURNACE_COMMAND_SIMPLE_CLONE(CommandSwapOrders);
  };

  /**
   * @brief Set order patterns
   */
  class CommandSetOrders : public Command {
    public:
      struct Data {
        std::vector<OrderPattern> newPatterns;

        FURNACE_NET_STRUCT_SERIALIZABLE(newPatterns);
      };

      struct RevertData {
        std::vector<OrderPattern> oldPatterns;

        FURNACE_NET_STRUCT_SERIALIZABLE(oldPatterns);
      };

    private:
      FURNACE_COMMAND_SIMPLE_IMPL_WITH_REVERT_DATA(CommandSetOrders, Kind::ORDER_SET);
      FURNACE_COMMAND_SIMPLE_CLONE(CommandSetOrders);
  };

  /**
   * @brief Set some data in a pattern
   */
  class CommandSetPatternData : public Command {
    public:
      struct Data {
        std::vector<PatternDataEdit> newPatternData;

        FURNACE_NET_STRUCT_SERIALIZABLE(newPatternData);
      };

      struct RevertData {
        std::vector<PatternDataEdit> oldPatternData;

        FURNACE_NET_STRUCT_SERIALIZABLE(oldPatternData);
      };

    private:
      FURNACE_COMMAND_SIMPLE_IMPL_WITH_REVERT_DATA(CommandSetPatternData, Kind::PATTERN_SET_DATA);
      FURNACE_COMMAND_SIMPLE_CLONE(CommandSetPatternData);
  };

#if HAVE_NETWORKING
  std::optional<std::unique_ptr<Command>> deserializeCommand(const msgpack::object& obj);
#endif
}

FURNACE_NET_ENUM_SERIALIZABLE(EditAction::Kind);
FURNACE_NET_ENUM_SERIALIZABLE(EditAction::CloneDepth);

#endif
