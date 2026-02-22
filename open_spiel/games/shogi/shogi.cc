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

#include "open_spiel/games/shogi/shogi.h"

#include <sys/types.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "open_spiel/abseil-cpp/absl/algorithm/container.h"
#include "open_spiel/abseil-cpp/absl/strings/match.h"
#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/abseil-cpp/absl/strings/str_join.h"
#include "open_spiel/abseil-cpp/absl/strings/str_split.h"
#include "open_spiel/abseil-cpp/absl/types/optional.h"
#include "open_spiel/abseil-cpp/absl/types/span.h"
#include "open_spiel/game_parameters.h"
#include "open_spiel/games/chess/chess.h"
#include "open_spiel/games/shogi/shogi_board.h"
#include "open_spiel/games/shogi/shogi_common.h"
#include "open_spiel/observer.h"
#include "open_spiel/spiel.h"
#include "open_spiel/spiel_globals.h"
#include "open_spiel/spiel_utils.h"

namespace open_spiel {
namespace shogi {
namespace {

constexpr int kNumReversibleMovesToDraw = 100;
constexpr int kNumRepetitionsToDraw = 3;

// Facts about the game
const GameType kGameType {
    /*short_name=*/"shogi",
    /*long_name=*/"Shogi",
    GameType::Dynamics::kSequential,
    GameType::ChanceMode::kDeterministic,
    GameType::Information::kPerfectInformation,
    GameType::Utility::kZeroSum,
    GameType::RewardModel::kTerminal,
    /*max_num_players=*/2,
    /*min_num_players=*/2,
    /*provides_information_state_string=*/true,
    /*provides_information_state_tensor=*/false,
    /*provides_observation_string=*/true,
    /*provides_observation_tensor=*/true,
    /*parameter_specification=*/
    {{}}};

std::shared_ptr<const Game> Factory(const GameParameters& params) {
  return std::shared_ptr<const Game>(new ShogiGame(params));
}

REGISTER_SPIEL_GAME(kGameType, Factory);

RegisterSingleTensorObserver single_tensor(kGameType.short_name);

// Adds a plane to the information state vector corresponding to the presence
// and absence of the given piece type and colour at each square.
void AddPieceTypePlane(Color color, PieceType piece_type,
                       const ShogiBoard& board,
                       absl::Span<float>::iterator& value_it) {
  for (int8_t y = 0; y < kBoardSize; ++y) {
    for (int8_t x = 0; x < kBoardSize; ++x) {
      Piece piece_on_board = board.at(Square{x, y});
      *value_it++ =
          (piece_on_board.color == color && piece_on_board.type == piece_type
               ? 1.0
               : 0.0);
    }
  }
}

// Adds a uniform scalar plane scaled with min and max.
template <typename T>
void AddScalarPlane(T val, T min, T max,
                    absl::Span<float>::iterator& value_it) {
  double normalized_val = static_cast<double>(val - min) / (max - min);
  for (int i = 0; i < kNumSquares; ++i) *value_it++ = normalized_val;
}

// Adds a binary scalar plane.
void AddBinaryPlane(bool val, absl::Span<float>::iterator& value_it) {
  AddScalarPlane<int>(val ? 1 : 0, 0, 1, value_it);
}
}  // namespace

ShogiState::ShogiState(std::shared_ptr<const Game> game)
    : State(game) {
  const auto* g = ParentGame();
  auto maybe_board = ShogiBoard::BoardFromFEN(kDefaultStandardFEN);
  repetitions_[current_board_.HashValue()] = 1;
}

ShogiState::ShogiState(std::shared_ptr<const Game> game,
                                 const std::string& fen)
    : State(game) {
  auto* g = static_cast<const ShogiGame*>(game.get());
  specific_initial_fen_ = fen;
  auto maybe_board = ShogiBoard::BoardFromFEN(fen);
  SPIEL_CHECK_TRUE(maybe_board);
  start_board_ = *maybe_board;
  current_board_ = start_board_;
  repetitions_[current_board_.HashValue()] = 1;
}

Player ShogiState::CurrentPlayer() const {
  return IsTerminal() ? kTerminalPlayerId : ColorToPlayer(Board().ToPlay());
}


Action ShogiState::ParseMoveToAction(const std::string& move_str) const {
  absl::optional<Move> move = Board().ParseMove(move_str);
  if (!move.has_value()) {
    return kInvalidAction;
  }
  return MoveToAction(*move);
}

void ShogiState::DoApplyAction(Action action) {

  Move move = ActionToMove(action, Board());
  moves_history_.push_back(move);
  Board().ApplyMove(move);
  ++repetitions_[current_board_.HashValue()];
  cached_legal_actions_.reset();
}

void ShogiState::MaybeGenerateLegalActions() const {
  if (!cached_legal_actions_) {
    cached_legal_actions_ = std::vector<Action>();
    Board().GenerateLegalMoves([this](const Move& move) -> bool {
      cached_legal_actions_->push_back(MoveToAction(move));
      return true;
    });
    absl::c_sort(*cached_legal_actions_);
  }
}

std::vector<Action> ShogiState::LegalActions() const {
  MaybeGenerateLegalActions();
  if (IsTerminal()) return {};
  return *cached_legal_actions_;
}

int EncodeMove(const Square& from_square, int destination_index,
               int num_actions_destinations) {
  return (from_square.x * kBoardSize + from_square.y) *
             num_actions_destinations +
         destination_index;
}

int8_t ReflectRank(Color to_play, int8_t rank) {
  return to_play == Color::kBlack ? kBoardSize - 1 - rank : rank;
}

Color PlayerToColor(Player p) {
  SPIEL_CHECK_NE(p, kInvalidPlayer);
  return static_cast<Color>(p);
}

Action MoveToAction(const Move& move) {
  // handle drop moves first
  if (move.from.x == kBoardSize) {
    // piece_index is stored in m.from.y (0=Pawn ... 4=Queen)
    int piece_index = move.from.y;
    int to_index = move.to.y * kBoardSize + move.to.x;  // flatten 2D → 1D
    int num_squares = kBoardSize * kBoardSize;
    int action_int = kFirstDropAction + piece_index * num_squares + to_index;
    return static_cast<Action>(action_int);
  }

  // Special-case for pass move.
  if (move == kPassMove) return kPassAction;

  Color color = move.piece.color;
  // We rotate the move to be from player p's perspective.
  Move player_move(move);

  // Rotate move to be from player p's perspective.
  player_move.from.y = ReflectRank(color, kBoardSize, player_move.from.y);
  player_move.to.y = ReflectRank(color, kBoardSize, player_move.to.y);

  // For each starting square, we enumerate 73 actions:
  // - 9 possible underpromotions
  // - 56 queen moves
  // - 8 knight moves
  // In total, this results in 64 * 73 = 4672 indices.
  // This is the union of all possible moves, by reducing this to the number of
  // moves actually available from each starting square this could still be
  // reduced a little to 1816 indices.
  int starting_index =
      EncodeMove(player_move.from, 0, kBoardSize, kNumActionDestinations);
  int8_t x_diff = player_move.to.x - player_move.from.x;
  int8_t y_diff = player_move.to.y - player_move.from.y;
  Offset offset{x_diff, y_diff};
}

std::pair<Square, int> ActionToDestination(int action, int kBoardSize,
                                           int num_actions_destinations) {
  const int xy = action / num_actions_destinations;
  SPIEL_CHECK_GE(xy, 0);
  SPIEL_CHECK_LT(xy, kBoardSize * kBoardSize);
  const int8_t x = xy / kBoardSize;
  const int8_t y = xy % kBoardSize;
  const int destination_index = action % num_actions_destinations;
  SPIEL_CHECK_GE(destination_index, 0);
  SPIEL_CHECK_LT(destination_index, num_actions_destinations);
  return {Square{x, y}, destination_index};
}

Move ActionToMove(const Action& action, const ShogiBoard& board) {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, NumDistinctActions());

  // Check for drop actions first

  if (action >= kFirstDropAction) {
		// TODO fix this
  }

  // Some chess variants (e.g. RBC) allow pass moves.
  if (board.AllowPassMove() && action == kPassAction) {
    return kPassMove;
  }

  // The encoded action represents an action encoded from color's perspective.
  Color color = board.ToPlay();
  PieceType promotion_type = PieceType::kEmpty;

  auto [from_square, destination_index] =
      ActionToDestination(action, kBoardSize, kNumActionDestinations);
  SPIEL_CHECK_LT(destination_index, kNumActionDestinations);

  bool is_under_promotion = destination_index < kNumUnderPromotions;
  Offset offset;
  if (is_under_promotion) {
    int promotion_index = destination_index / 3;
    int direction_index = destination_index % 3;
    promotion_type = kUnderPromotionIndexToType[promotion_index];
    offset = kUnderPromotionDirectionToOffset[direction_index];
  } else {
    destination_index -= kNumUnderPromotions;
    offset = DestinationIndexToOffset(destination_index, kKnightOffsets);
  }
  Square to_square = from_square + offset;

  from_square.y = ReflectRank(color, kBoardSize, from_square.y);
  to_square.y = ReflectRank(color, kBoardSize, to_square.y);

  // This uses the current state to infer the piece type.
  Piece piece = {board.ToPlay(), board.at(from_square).type};

	 // TODO fix this
   Move move(from_square, to_square, piece);

  // Move move(from_square, to_square, piece, promotion_type, castle_dir);
  return move;
}

std::string ShogiState::ActionToString(Player player,
                                            Action action) const {
  Move move = ActionToMove(action, Board());
  return move.ToSAN(Board());
}

std::string ShogiState::DebugString() const {
  return current_board_.DebugString();
}

std::string ShogiState::ToString() const {
  return Board().ToFEN();
}

std::vector<double> ShogiState::Returns() const {
  auto maybe_final_returns = MaybeFinalReturns();
  if (maybe_final_returns) {
    return *maybe_final_returns;
  } else {
    return {0.0, 0.0};
  }
}

std::string ShogiState::InformationStateString(Player player) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, num_players_);
  return HistoryString();
}

std::string ShogiState::ObservationString(Player player) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, num_players_);
  return ToString();
}

void ShogiState::ObservationTensor(Player player,
                                        absl::Span<float> values) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, num_players_);

  auto value_it = values.begin();

  // Piece configuration.
  for (const auto& piece_type : kPieceTypes) {
    AddPieceTypePlane(Color::kWhite, piece_type, Board(), value_it);
    AddPieceTypePlane(Color::kBlack, piece_type, Board(), value_it);
  }

  AddPieceTypePlane(Color::kEmpty, PieceType::kEmpty, Board(), value_it);

  const auto entry = repetitions_.find(Board().HashValue());
  SPIEL_CHECK_FALSE(entry == repetitions_.end());
  int repetitions = entry->second;

  // Num repetitions for the current board.
  AddScalarPlane(repetitions, 1, 3, value_it);

  // Side to play.
  AddScalarPlane(ColorToPlayer(Board().ToPlay()), 0, 1, value_it);

  // Pocket pieces.
  // Order: Pawn, Knight, Bishop, Rook, Queen.
  // Maximum pocket count encoded in observation tensor.
  // Counts above this are saturated.
  // This does not affect gameplay.
  constexpr int kMaxPocketCount = 16;  // safe upper bound

  for (Color color : {Color::kWhite, Color::kBlack}) {
    const Pocket& pocket = (color == Color::kWhite) ? Board().white_pocket_
                                                    : Board().black_pocket_;
    for (PieceType ptype : Pocket::PieceTypes()) {
      int count = pocket.Count(ptype);
      count = std::min(count, kMaxPocketCount);
      AddScalarPlane(count, 0, kMaxPocketCount, value_it);
    }
  }
  SPIEL_CHECK_EQ(value_it, values.end());
}

std::unique_ptr<State> ShogiState::Clone() const {
  return std::unique_ptr<State>(new ShogiState(*this));
}

void ShogiState::UndoAction(Player player, Action action) {
  SPIEL_CHECK_GE(moves_history_.size(), 1);
  --repetitions_[current_board_.HashValue()];
  moves_history_.pop_back();
  history_.pop_back();
  --move_number_;
  current_board_ = start_board_;
  for (const Move& move : moves_history_) {
    current_board_.ApplyMove(move);
  }
}

bool ShogiState::IsRepetitionDraw() const {
  const auto entry = repetitions_.find(Board().HashValue());
  SPIEL_CHECK_FALSE(entry == repetitions_.end());
  return entry->second >= kNumRepetitionsToDraw;
}

int ShogiState::NumRepetitions(const ShogiState& state) const {
  uint64_t state_hash_value = state.Board().HashValue();
  const auto entry = repetitions_.find(state_hash_value);
  if (entry == repetitions_.end()) {
    return 0;
  } else {
    return entry->second;
  }
}

std::pair<std::string, std::vector<std::string>>
ShogiState::ExtractFenAndMaybeMoves() const {
  std::string initial_fen = start_board_.ToFEN();
  std::vector<std::string> move_lans;
  std::unique_ptr<State> state = ParentGame()->NewInitialState(initial_fen);
  ShogiBoard board = down_cast<const ShogiState&>(*state).Board();
  for (const Move& move : moves_history_) {
    move_lans.push_back(move.ToLAN());
    board.ApplyMove(move);
  }
  return std::make_pair(initial_fen, move_lans);
}

absl::optional<std::vector<double>> ShogiState::MaybeFinalReturns() const {

  if (IsRepetitionDraw()) {
    return std::vector<double>{DrawUtility(), DrawUtility()};
  }
  // Compute and cache the legal actions.
  MaybeGenerateLegalActions();
  SPIEL_CHECK_TRUE(cached_legal_actions_);
  bool have_legal_moves = !cached_legal_actions_->empty();

  // If we don't have legal moves we are either stalemated or checkmated,
  // depending on whether we are in check or not.
  if (!have_legal_moves) {
    if (!Board().InCheck()) {
      return std::vector<double>{DrawUtility(), DrawUtility()};
    } else {
      std::vector<double> returns(NumPlayers());
      auto next_to_play = ColorToPlayer(Board().ToPlay());
      returns[next_to_play] = LossUtility();
      returns[OtherPlayer(next_to_play)] = WinUtility();
      return returns;
    }
  }


  return absl::nullopt;
}

std::string ShogiState::Serialize() const {
  std::string state_str = "";
  // If the specific_initial_fen is empty, the deserializer will use the
  // default NewInitialState(). Otherwise, the deserializer will specify
  // the specific initial fen by calling NewInitialState(string).
  absl::StrAppend(&state_str, "FEN: ", specific_initial_fen_, "\n");
  std::vector<Action> history = History();
  absl::StrAppend(&state_str, absl::StrJoin(history, "\n"), "\n");
  return state_str;
}

std::string ShogiState::StartFEN() const {
  return start_board_.ToFEN();
}

std::unique_ptr<State> ShogiGame::DeserializeState(
    const std::string& str) const {
  const std::string prefix("FEN: ");
  if (!absl::StartsWith(str, prefix)) {
    // Backward compatibility.
    return Game::DeserializeState(str);
  }
  std::vector<std::string> lines = absl::StrSplit(str, '\n');
  int line_num = 0;
  std::string fen = lines[line_num].substr(prefix.length());
  std::unique_ptr<State> state = nullptr;
  if (fen.empty()) {
    state = NewInitialState();
  } else {
    state = NewInitialState(fen);
  }
  line_num += 1;
  for (int i = line_num; i < lines.size(); ++i) {
    if (lines[i].empty()) {
      break;
    }
    Action action = static_cast<Action>(std::stol(lines[i]));
    state->ApplyAction(action);
  }
  return state;
}

}  // namespace shogi
}  // namespace open_spiel
