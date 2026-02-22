// Copyright 2026 DeepMind Technologies Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "open_spiel/games/shogi/shogi_common.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/spiel_utils.h"

namespace open_spiel {
namespace shogi_common {
using shogi::kBoardSize;
using shogi::kNumSquares;
namespace {

int DiffToDestinationIndex(int diff) {
  int destination_index = diff + kBoardSize - 1;
  if (diff > 0) --destination_index;
  return destination_index;
}

int DestinationIndexToDiff(int destination_index) {
  int diff = destination_index - kBoardSize + 1;
  if (diff >= 0) ++diff;
  return diff;
}

template <typename KnightOffsets>
int OffsetToDestinationIndexImpl(const Offset& offset,
                                 const KnightOffsets& knight_offsets) {
  // Encodes chess queen moves + knight moves.
  int move_type = -1;
  int destination_index = -1;
  if (offset.x_offset == 0) {
    // vertical moves
    move_type = 0;
    destination_index = DiffToDestinationIndex(offset.y_offset);
  } else if (offset.y_offset == 0) {
    // horizontal moves
    move_type = 1;
    destination_index = DiffToDestinationIndex(offset.x_offset);
  } else if (offset.x_offset == offset.y_offset) {
    // left downward or right upward diagonal moves.
    move_type = 2;
    destination_index = DiffToDestinationIndex(offset.x_offset);
  } else if (offset.x_offset == -offset.y_offset) {
    // left upward or right downward diagonal moves.
    move_type = 3;
    destination_index = DiffToDestinationIndex(offset.x_offset);
  } else {
    // knight moves.
    move_type = 4;
    auto itr = std::find(knight_offsets.begin(), knight_offsets.end(), offset);
    if (itr != knight_offsets.end()) {
      destination_index = std::distance(knight_offsets.begin(), itr);
    } else {
      SpielFatalError(absl::StrCat("Unexpected offset (",
                                   static_cast<int>(offset.x_offset), ", ",
                                   static_cast<int>(offset.y_offset), ")"));
    }
  }

  return move_type * 2 * (kBoardSize - 1) + destination_index;
}

template <typename KnightOffsets>
Offset DestinationIndexToOffsetImpl(int destination_index,
                                    const KnightOffsets& knight_offsets) {
  int move_type = destination_index / (2 * (kBoardSize - 1));
  destination_index = destination_index % (2 * (kBoardSize - 1));
  int8_t diff = DestinationIndexToDiff(destination_index);

  if (move_type == 0) {
    return {0, diff};
  } else if (move_type == 1) {
    return {diff, 0};
  } else if (move_type == 2) {
    return {diff, diff};
  } else if (move_type == 3) {
    return {diff, static_cast<int8_t>(-diff)};
  } else if (move_type == 4) {
    SPIEL_CHECK_GE(destination_index, 0);
    SPIEL_CHECK_LT(destination_index, knight_offsets.size());
    return knight_offsets[destination_index];
  } else {
    SpielFatalError(absl::StrCat("Unexpected move type (", move_type, ")"));
  }
}

}  // namespace

int OffsetToDestinationIndex(const Offset& offset,
                             const std::array<Offset, 8>& knight_offsets) {
  return OffsetToDestinationIndexImpl(offset, knight_offsets);
}

int OffsetToDestinationIndex(const Offset& offset,
                             const std::array<Offset, 2>& knight_offsets) {
  return OffsetToDestinationIndexImpl(offset, knight_offsets);
}

Offset DestinationIndexToOffset(int destination_index,
                                const std::array<Offset, 8>& knight_offsets) {
  return DestinationIndexToOffsetImpl(destination_index, knight_offsets);
}

Offset DestinationIndexToOffset(int destination_index,
                                const std::array<Offset, 2>& knight_offsets) {
  return DestinationIndexToOffsetImpl(destination_index, knight_offsets);
}

std::pair<Square, int> DecodeNetworkTarget(int i,
                                           int num_actions_destinations) {
  int xy = i / num_actions_destinations;
  SPIEL_CHECK_GE(xy, 0);
  SPIEL_CHECK_LT(xy, kNumSquares);
  int8_t x = xy / kBoardSize;
  int8_t y = xy % kBoardSize;
  int destination_index = i % num_actions_destinations;
  SPIEL_CHECK_GE(destination_index, 0);
  SPIEL_CHECK_LT(destination_index, num_actions_destinations);
  return std::make_pair(Square{x, y}, destination_index);
}

int EncodeNetworkTarget(const Square& from_square, int destination_index,
                        int num_actions_destinations) {
  return (from_square.x * kBoardSize + from_square.y) *
             num_actions_destinations +
         destination_index;
}

}  // namespace shogi_common
}  // namespace open_spiel
