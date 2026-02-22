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
//

#include "open_spiel/games/shogi/shogi.h"

#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "open_spiel/abseil-cpp/absl/random/uniform_int_distribution.h"
#include "open_spiel/abseil-cpp/absl/strings/str_split.h"
#include "open_spiel/abseil-cpp/absl/types/optional.h"
#include "open_spiel/abseil-cpp/absl/types/span.h"
#include "open_spiel/games/shogi/shogi_board.h"
#include "open_spiel/spiel.h"
#include "open_spiel/spiel_globals.h"
#include "open_spiel/spiel_utils.h"
#include "open_spiel/tests/basic_tests.h"

namespace open_spiel {
namespace shogi {
namespace {

namespace testing = open_spiel::testing;

void CheckUndo(const char* fen, const char* move_san, const char* fen_after) {
}


void ApplyLANMove(const char* move_lan, ShogiState* state) {
}

void BasicShogiTests() {
  testing::LoadGameTest("shogi");
  testing::NoChanceOutcomesTest(*LoadGame("shogi"));
  testing::RandomSimTest(*LoadGame("shogi"), 10);
}


void TerminalReturnTests() {
}


void ObservationTensorTests() {
}

void MoveConversionTests() {
}

void SerializaitionTests() {
  auto game = LoadGame("shogi");

}



void HashTest() {
}

}  // namespace
}  // namespace shogi
}  // namespace open_spiel

int main(int argc, char** argv) {
}
