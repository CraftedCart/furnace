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

#ifndef _ENGINE_PATTERN_H
#define _ENGINE_PATTERN_H

#include "safeReader.h"

/**
 * @brief The maximum number of rows a pattern can have
 */
#define DIV_PATTERN_MAX_ROWS 256

/**
 * @brief The maximum number of "types" a pattern can have
 *
 * "types" perhaps isn't the best name for this - see the documentation for `DivPattern::data` for more info on what it
 * is.
 */
#define DIV_PATTERN_MAX_TYPES 32

struct DivPattern {
  String name;

  /**
   * @brief Pattern data, including notes, instruments, volumes, effects
   *
   * `data` goes as follows: `data[ROW][TYPE]`
   *
   * TYPE is:
   * - `0`: note
   * - `1`: octave
   * - `2`: instrument
   * - `3`: volume
   * - `4-5+`: effect/effect value
   */
  short data[DIV_PATTERN_MAX_ROWS][DIV_PATTERN_MAX_TYPES];

  /**
   * @brief Copy this pattern to `dest`
   */
  void copyOn(DivPattern* dest);

  SafeReader* compile(int len=DIV_PATTERN_MAX_ROWS, int fxRows=1);
  DivPattern();
};

struct DivChannelData {
  unsigned char effectRows;
  DivPattern* data[128];
  DivPattern* getPattern(int index, bool create);
  void wipePatterns();
  DivChannelData();
};

#endif
