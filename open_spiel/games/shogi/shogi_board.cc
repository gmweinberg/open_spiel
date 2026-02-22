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

#include "open_spiel/games/shogi/shogi_board.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "open_spiel/abseil-cpp/absl/strings/ascii.h"
#include "open_spiel/abseil-cpp/absl/strings/match.h"
#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/abseil-cpp/absl/strings/str_split.h"
#include "open_spiel/abseil-cpp/absl/strings/string_view.h"
#include "open_spiel/abseil-cpp/absl/types/optional.h"
#include "open_spiel/games/shogi/shogi_common.h"
#include "open_spiel/spiel_utils.h"

namespace open_spiel {
namespace shogi {
namespace {

inline PieceType PromotedType(PieceType type) {
  switch (type) {
    case PieceType::kKnight:
      return PieceType::kKnightP;
    case PieceType::kBishop:
      return PieceType::kBishopP;
    case PieceType::kRook:
      return PieceType::kRookP;
    case PieceType::kQueen:
      return PieceType::kQueenP;
    default:
      SpielFatalError("Invalid piece type for promotion");
      return PieceType::kKnightP;  // Unreachable, but silences compiler
  }
}

}  // namespace

bool IsMoveCharacter(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9');
}

std::pair<std::string, std::string> SplitAnnotations(const std::string& move) {
  for (int i = 0; i < move.size(); ++i) {
    if (!IsMoveCharacter(move[i])) {
      return {move.substr(0, i), std::string(absl::ClippedSubstr(move, i))};
    }
  }
  return {move, ""};
}

std::string ColorToString(Color c) {
  switch (c) {
    case Color::kBlack:
      return "black";
    case Color::kWhite:
      return "white";
    case Color::kEmpty:
      return "empty";
    default:
      SpielFatalError(absl::StrCat("Unknown color: ", c));
      return "This will never return.";
  }
}

absl::optional<PieceType> PieceTypeFromChar(char c) {
  switch (toupper(c)) {
    case 'P':
      return PieceType::kPawn;
    case 'N':
      return PieceType::kKnight;
    case 'B':
      return PieceType::kBishop;
    case 'R':
      return PieceType::kRook;
    case 'Q':
      return PieceType::kQueen;
    case 'K':
      return PieceType::kKing;
    case 'H':
      return PieceType::kKnightP;
    case 'A':
      return PieceType::kBishopP;
    case 'C':
      return PieceType::kRookP;
    case 'E':
      return PieceType::kQueenP;
    default:
      std::cerr << "Invalid piece type: " << c << std::endl;
      return absl::nullopt;
  }
}

std::string PieceTypeToString(PieceType p, bool uppercase) {
  switch (p) {
    case PieceType::kEmpty:
      return " ";
    case PieceType::kPawn:
      return uppercase ? "P" : "p";
    case PieceType::kKnight:
      return uppercase ? "N" : "n";
    case PieceType::kBishop:
      return uppercase ? "B" : "b";
    case PieceType::kRook:
      return uppercase ? "R" : "r";
    case PieceType::kQueen:
      return uppercase ? "Q" : "q";
    case PieceType::kKing:
      return uppercase ? "K" : "k";
    case PieceType::kQueenP:
      return uppercase ? "E" : "e";
    case PieceType::kRookP:
      return uppercase ? "C" : "c";
    case PieceType::kBishopP:
      return uppercase ? "A" : "a";
    case PieceType::kKnightP:
      return uppercase ? "H" : "h";
    default:
      SpielFatalError(std::string("Unknown piece (ptts): ") +
                      std::to_string(static_cast<int>(p)));
      return "This will never return.";
  }
}

std::string Piece::ToString() const {
  std::string base = PieceTypeToString(type);
  return color == Color::kWhite ? absl::AsciiStrToUpper(base)
                                : absl::AsciiStrToLower(base);
}

absl::optional<Square> SquareFromString(const std::string& s) {
  if (s.size() != 2) return kInvalidSquare;

  auto file = ParseFile(s[0]);
  auto rank = ParseRank(s[1]);
  if (file && rank) return Square{*file, *rank};
  return absl::nullopt;
}

bool IsLongDiagonal(const shogi::Square& from_sq,
                    const shogi::Square& to_sq) {
  if (from_sq == to_sq) {
    return false;
  }
  int half_kBoardSize = kBoardSize / 2;
  if ((to_sq.y < half_kBoardSize && to_sq.x < half_kBoardSize) ||
      (to_sq.y >= half_kBoardSize && to_sq.x >= half_kBoardSize)) {
    return from_sq.y - to_sq.y == from_sq.x - to_sq.x;
  } else {
    return from_sq.y - to_sq.y == to_sq.x - from_sq.x;
  }
}

std::string Move::ToString() const {
  std::string extra;

  if (promote) {
		// TODO append a + or something
  }
  return absl::StrCat(piece.ToString(), " ", SquareToString(from), " to ",
                      SquareToString(to), extra);
}

std::string Move::ToLAN() const {
  if (IsDropMove()) {
    std::string move_text;
    PieceType from_type = Pocket::DropPieceType(from.y);
    move_text += PieceTypeToString(from_type);
    move_text += '@';
    absl::StrAppend(&move_text, SquareToString(to));
    return move_text;
  }
	std::string promotion;
	if (promote) {
		//TODO
	}
	return absl::StrCat(SquareToString(from), SquareToString(to), promotion);
}

std::string Move::ToSAN(const ShogiBoard& board) const {
  std::string move_text;
  if (IsDropMove()) {
    PieceType from_type = Pocket::DropPieceType(from.y);
    move_text += PieceTypeToString(from_type);
    move_text += '@';
    absl::StrAppend(&move_text, SquareToString(to));
    return move_text;
  }
  PieceType piece_type = board.at(from).type;
	switch (piece_type) {
		case PieceType::kKing:
		case PieceType::kQueen:
		case PieceType::kRook:
		case PieceType::kBishop:
		case PieceType::kKnight:
		case PieceType::kQueenP:
		case PieceType::kRookP:
		case PieceType::kBishopP:
		case PieceType::kKnightP:
			move_text += PieceTypeToString(piece_type);
			break;
		case PieceType::kPawn:
			// No piece type required.
			break;
		case PieceType::kEmpty:
			std::cerr << "Move doesn't have a piece type" << move_text << std::endl;

    // Now we generate all moves from this position, and see if our file and
    // rank are unique.
    bool file_unique = true;
    bool rank_unique = true;
    bool disambiguation_required = false;

    board.GenerateLegalMoves([&](const Move& move) -> bool {
      if (move.IsDropMove()) {
        return true;
      }
      if (move.piece.type != piece.type) {
        return true;  // Continue generating moves.
      }
      if (move.to != to) {
        return true;
      }
      if (move.from == from) {
        // This is either us, or a promotion to a different type. We don't count
        // them as ambiguous in either case.
        return true;
      }
      disambiguation_required = true;
      if (move.from.x == from.x) {
        file_unique = false;
      } else if (move.from.y == from.y) {
        rank_unique = false;
      }
      return true;
    });

    bool file_required = false;
    bool rank_required = false;

    if (piece_type == PieceType::kPawn && from.x != to.x) {
      // Pawn captures always need file, and they will never require rank dis-
      // ambiguation.
      file_required = true;
    } else if (disambiguation_required) {
      if (file_unique) {
        // This includes when both will disambiguate, in which case we have to
        // use file. [FIDE Laws of Chess (2018): C.10.3].
        file_required = true;
      } else if (rank_unique) {
        rank_required = true;
      } else {
        // We have neither unique file nor unique rank. This is only possible
        // with 3 or more pieces of the same type.
        file_required = true;
        rank_required = true;
      }
    }

    if (file_required) {
      absl::StrAppend(&move_text, FileToString(from.x));
    }

    if (rank_required) {
      absl::StrAppend(&move_text, RankToString(from.y));
    }

    // We have a capture if either 1) the destination square has a piece, or
    // 2) we are making a diagonal pawn move (which can also be an en-passant
    // capture, where the destination square would not have a piece).
    auto piece_at_to_square = board.at(to);
    if ((piece_at_to_square.type != PieceType::kEmpty) ||
        (piece_type == PieceType::kPawn && from.x != to.x)) {
      absl::StrAppend(&move_text, "x");
    }

    // Destination square is always fully encoded.
    absl::StrAppend(&move_text, SquareToString(to));

  }

  // Figure out if this is a check / checkmating move or not.
  if (!board.KingInCheckAllowed()) {
    auto board_copy = board;
    board_copy.ApplyMove(*this);
    if (board_copy.InCheck()) {
      bool has_escape = false;
      board_copy.GenerateLegalMoves([&](const Move&) -> bool {
        has_escape = true;
        return false;  // No need to keep generating moves.
      });

      if (has_escape) {
        // Check.
        absl::StrAppend(&move_text, "+");
      } else {
        // Checkmate.
        absl::StrAppend(&move_text, "#");
      }
    }
  }

  return move_text;
}

ShogiBoard::ShogiBoard() {
  board_.fill(kEmptyPiece);
}

/*static*/ absl::optional<ShogiBoard> ShogiBoard::BoardFromFEN(
    const std::string& fen) {
  std::string fen_copy = fen;
  std::string pocket_section;

  auto lb = fen_copy.find('[');
  if (lb != std::string::npos) {
    auto rb = fen_copy.find(']', lb);
    if (rb == std::string::npos) {
      std::cerr << "Malformed pocket section in FEN: " << fen << std::endl;
      return absl::nullopt;
    }
    pocket_section = fen_copy.substr(lb + 1, rb - lb - 1);
    fen_copy.erase(lb, rb - lb + 1);
    fen_copy = absl::StripAsciiWhitespace(fen_copy);
  }
  /* An FEN string includes a board position, side to play
   * and full move number. In that order.
   *
   * Eg. start position is:
   * rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
   *
   * Board is described from rank 8 to rank 1, and files from a to h. Empty
   * squares are encoded as number of consecutive empty squares.
   *
   * Many FEN strings don't have the last two fields.
   */
  ShogiBoard board;

  std::vector<std::string> fen_parts = absl::StrSplit(fen_copy, ' ');

  if (fen_parts.size() != 6 && fen_parts.size() != 4) {
    std::cerr << "Invalid FEN: " << fen << std::endl;
    return absl::nullopt;
  }

  std::string& piece_configuration = fen_parts[0];
  std::string& side_to_move = fen_parts[1];

  // These are defaults if the FEN string doesn't have these fields.
  std::string fifty_clock = "0";
  std::string move_number = "1";

  if (fen_parts.size() == 6) {
    fifty_clock = fen_parts[4];
    move_number = fen_parts[5];
  }

  std::vector<std::string> piece_config_by_rank =
      absl::StrSplit(piece_configuration, '/');

  for (int8_t current_y = kBoardSize - 1; current_y >= 0; --current_y) {
    std::string& rank = piece_config_by_rank[kBoardSize - current_y - 1];
    int8_t current_x = 0;
    for (char c : rank) {
      if (current_x >= kBoardSize) {
        std::cerr << "Too many things on FEN rank: " << rank << std::endl;
        return absl::nullopt;
      }

      if (c >= '1' && c <= '8') {
        current_x += c - '0';
      } else {
        auto piece_type = PieceTypeFromChar(c);
        if (!piece_type) {
          std::cerr << "Invalid piece type in FEN: " << c << std::endl;
          return absl::nullopt;
        }

        Color color = isupper(c) ? Color::kWhite : Color::kBlack;
        board.set_square(Square{current_x, current_y},
                         Piece{color, *piece_type});

        ++current_x;
      }
    }
  }

  if (side_to_move == "b") {
    board.SetToPlay(Color::kBlack);
  } else if (side_to_move == "w") {
    board.SetToPlay(Color::kWhite);
  } else {
    std::cerr << "Invalid side to move in FEN: " << side_to_move << std::endl;
    return absl::nullopt;
  }


  board.SetMovenumber(std::stoi(move_number));
  // ----- Parse pockets -----
  if (!pocket_section.empty()) {
    for (char pc : pocket_section) {
      bool white = std::isupper(pc);
      char uc = std::toupper(pc);
      absl::optional<PieceType> opt = PieceTypeFromChar(uc);
      PieceType pptype = *opt;
      if (!opt) {
        std::cerr << "Invalid pocket char in FEN: " << pc << std::endl;
        return absl::nullopt;
      }
      if (white) {
        board.AddToPocket(Color::kWhite, pptype);
      } else {
        board.AddToPocket(Color::kBlack, pptype);
      }
    }
  }

  return board;
}

Square ShogiBoard::find(const Piece& piece) const {
  for (int8_t y = 0; y < kBoardSize; ++y) {
    for (int8_t x = 0; x < kBoardSize; ++x) {
      Square sq{x, y};
      if (at(sq) == piece) {
        return sq;
      }
    }
  }

  return kInvalidSquare;
}

void ShogiBoard::GenerateLegalMoves(const MoveYieldFn& yield,
                                         Color color) const {
  // We do not need to filter moves that would result for King to move / stay
  // in check, so we can yield all pseudo legal moves
  if (king_in_check_allowed_) {
    GeneratePseudoLegalMoves(yield, color);
  } else {
    auto king_square = find(Piece{color, PieceType::kKing});

    GeneratePseudoLegalMoves(
        [this, &king_square, &yield, color](const Move& move) {
          // See if the move is legal by applying, checking whether the king is
          // under attack, and undoing the move.
          auto board_copy = *this;
          board_copy.ApplyMove(move);

          auto ks = king_square;
          if (!(move.IsDropMove()) && at(move.from).type == PieceType::kKing) {
            ks = move.to;
          }
          if (board_copy.UnderAttack(ks, color)) {
            return true;
          } else {
            return yield(move);
          }
        },
        color);
  }
}

void ShogiBoard::GeneratePseudoLegalMoves(
    const MoveYieldFn& yield, Color color,
    PseudoLegalMoveSettings settings) const {
  bool generating = true;

#define YIELD(move)     \
  if (!yield(move)) {   \
    generating = false; \
  }

  if (allow_pass_move_) YIELD(kPassMove);

  GenerateDropDestinations_(color, settings, yield);

  for (int8_t y = 0; y < kBoardSize && generating; ++y) {
    for (int8_t x = 0; x < kBoardSize && generating; ++x) {
      Square sq{x, y};
      auto& piece = at(sq);
      if (piece.type != PieceType::kEmpty && piece.color == color) {
        switch (piece.type) {
          case PieceType::kKing:
            GenerateKingDestinations_(
                sq, color,
                [&yield, &piece, &sq, &generating](const Square& to) {
                  YIELD(Move(sq, to, piece));
                });
            break;
          case PieceType::kQueen:
          case PieceType::kQueenP:
            GenerateQueenDestinations_(
                sq, color, settings,
                [&yield, &sq, &piece, &generating](const Square& to) {
                  YIELD(Move(sq, to, piece));
                });
            break;
          case PieceType::kRook:
          case PieceType::kRookP:
            GenerateRookDestinations_(
                sq, color, settings,
                [&yield, &sq, &piece, &generating](const Square& to) {
                  YIELD(Move(sq, to, piece));
                });
            break;
          case PieceType::kBishop:
          case PieceType::kBishopP:
            GenerateBishopDestinations_(
                sq, color, settings,
                [&yield, &sq, &piece, &generating](const Square& to) {
                  YIELD(Move(sq, to, piece));
                });
            break;
          case PieceType::kKnight:
          case PieceType::kKnightP:
            GenerateKnightDestinations_(
                sq, color,
                [&yield, &sq, &piece, &generating](const Square& to) {
                  YIELD(Move(sq, to, piece));
                });
            break;
          default:
            std::cerr << "Unknown piece type: " << static_cast<int>(piece.type)
                      << std::endl;
        }
      }
    }
  }

#undef YIELD
}

void ShogiBoard::GenerateLegalPawnCaptures(const MoveYieldFn& yield,
                                                Color color) const {
}


template <typename YieldFn>
void ShogiBoard::GenerateDropDestinations_(
    Color player, const PseudoLegalMoveSettings& settings,
    const YieldFn& yield) const {
  // Get the pocket for the player
  Pocket pocket = (player == Color::kWhite ? white_pocket_ : black_pocket_);

  // Loop over drop-capable piece types
  for (PieceType ptype : Pocket::PieceTypes()) {
    if (pocket.Count(ptype) == 0) continue;

    for (int8_t y = 0; y < kBoardSize; ++y) {
      for (int8_t x = 0; x < kBoardSize; ++x) {
        Square sq{x, y};

        // Only drop on empty squares
        if (at(sq) != kEmptyPiece) continue;

        // Pawn drop restriction
        if (ptype == PieceType::kPawn && (y == 0 || y == kBoardSize - 1))
          continue;

        // Build the Move
        Move m;
        m.from = Square{
            static_cast<int8_t>(kBoardSize),           // sentinel X
            static_cast<int8_t>(pocket.Index(ptype))};  // piece index in Y
        m.to = sq;

        // Output the move
        yield(m);
      }
    }
  }
}


absl::optional<Move> ShogiBoard::ParseMove(const std::string& move) const {
  // First see if they are in the long form -
  // "anan" (eg. "e2e4") or "anana" (eg. "f7f8q")
  // SAN moves will never have this form because an SAN move that starts with
  // a lowercase letter must be a pawn move, and pawn moves will never require
  // rank disambiguation (meaning the second character will never be a number).
  auto lan_move = ParseLANMove(move);
  if (lan_move) {
    return lan_move;
  }

  return absl::nullopt;
}


absl::optional<Move> ShogiBoard::ParseDropMove(
    const std::string& move) const {
  if (move.empty()) {
    return absl::nullopt;
  }
  if (move.size() == 4 && move[1] == '@') {
    char pc = move[0];
    char file = move[2];
    char rank = move[3];

    // Validate square
    if (file < 'a' || file >= ('a' + kBoardSize) || rank < '1' ||
        rank >= ('1' + kBoardSize)) {
      return absl::nullopt;
    }

    // Parse piece type
    absl::optional<PieceType> opt = PieceTypeFromChar(pc);
    if (!opt) return absl::nullopt;

    PieceType ptype = *opt;

    // Disallow illegal drops
    if (ptype == PieceType::kKing) return absl::nullopt;

    auto to = SquareFromString(move.substr(2, 2));
    if (!to) return absl::nullopt;

    // Construct drop move
    Move drop;
    drop.from = Square{static_cast<int8_t>(kBoardSize),
                       static_cast<int8_t>(Pocket::Index(ptype))};
    drop.to = *to;
    drop.piece = Piece{to_play_, ptype};
    return drop;
  }

  return absl::nullopt;
}

absl::optional<Move> ShogiBoard::ParseLANMove(const std::string& move) const {
  if (move.empty()) {
    return absl::nullopt;
  }
  auto drop_move = ParseDropMove(move);
  if (drop_move) {
    return drop_move;
  }

  // Long algebraic notation moves (of the variant we care about) are in one of
  // two forms -
  // "anan" (eg. "e2e4") or "anana" (eg. "f7f8q")
  if (move.size() == 4 || move.size() == 5) {
    if (move[0] < 'a' || move[0] >= ('a' + kBoardSize) || move[1] < '1' ||
        move[1] >= ('1' + kBoardSize) || move[2] < 'a' ||
        move[2] >= ('a' + kBoardSize) || move[3] < '1' ||
        move[3] >= ('1' + kBoardSize)) {
      return absl::nullopt;
    }

    if (move.size() == 5 && move[4] != 'q' && move[4] != 'r' &&
        move[4] != 'b' && move[4] != 'n') {
      return absl::nullopt;
    }

    auto from = SquareFromString(move.substr(0, 2));
    auto to = SquareFromString(std::string(absl::ClippedSubstr(move, 2, 2)));
    if (from && to) {
      absl::optional<PieceType> promotion_type;
      if (move.size() == 5) {
        promotion_type = PieceTypeFromChar(move[4]);
        if (!promotion_type) {
          std::cerr << "Invalid promotion type" << std::endl;
          return absl::nullopt;
        }
      }

      // Other regular moves.
      std::vector<Move> candidates;
			//TODO fix this
			/*
      GenerateLegalMoves(
          [&to, &from, &promotion_type, &candidates](const Move& move) {
            if (move.from == *from && move.to == *to &&
                (!promotion_type || (move.promotion_type == *promotion_type))) {
              candidates.push_back(move);
            }
            return true;
          });
			*/
      //TODO fix this
      if (candidates.empty()) {
        std::cerr << "Illegal move - " << move << " on " << "Tuesday"
                  << std::endl;
        return Move();
      } else if (candidates.size() > 1) {
        std::cerr << "Multiple matches (is promotion type missing?) - " << move
                  << std::endl;
        return Move();
      }

      return candidates[0];
    }
  } else {
    return absl::nullopt;
  }
  SpielFatalError("All conditionals failed; this is a bug.");
}

void ShogiBoard::ApplyMove(const Move& move) {
  // Skip applying a move if it's a pass.
  if (move == kPassMove) {
    if (to_play_ == Color::kBlack) ++move_number_;
    SetToPlay(OppColor(to_play_));
    return;
  }

  // We remove the moving piece from the original
  // square or pocket and put it on the destination square, overwriting whatever was
  // there before. If we capture, put the captured piece in the pocket.
  //
  Piece moving_piece;
  Piece destination_piece = at(move.to);

  if (move.IsDropMove()) {
    PieceType from_type = Pocket::DropPieceType(move.from.y);
    moving_piece = Piece{to_play_, from_type};
    RemoveFromPocket(to_play_, from_type);
  } else {
    moving_piece = at(move.from);
    set_square(move.from, kEmptyPiece);
  }

  set_square(move.to, moving_piece);
  // Increment pockets for capture.
  if (destination_piece != kEmptyPiece) {
    PieceType dpt = destination_piece.type;
    if (dpt == PieceType::kKing) {
      std::cerr << "King capture from" << SquareToString(move.from)
                << std::endl;
      SpielFatalError("King capture detected.");
    }
    AddToPocket(to_play_, dpt);
  }


  // Special cases that require adjustment -

  // 2. En-passant

  // 3. Promotions
	// TODO fix this


  if (to_play_ == Color::kBlack) {
    ++move_number_;
  }

  SetToPlay(OppColor(to_play_));
}

bool ShogiBoard::TestApplyMove(const Move& move) {
  Color color = to_play_;
  ApplyMove(move);
  return !UnderAttack(find(Piece{color, PieceType::kKing}), color);
}

bool ShogiBoard::UnderAttack(const Square& sq, Color our_color) const {
  SPIEL_CHECK_NE(sq, kInvalidSquare);

  bool under_attack = false;
  Color opponent_color = OppColor(our_color);

  // We do this by pretending we are a piece of different types, and seeing if
  // we can attack opponent pieces. Eg. if we pretend we are a knight, and can
  // attack an opponent knight, that means the knight can also attack us.

  // King moves (this is possible because we use this function for checking
  // whether we are moving into check, and we can be trying to move the king
  // into a square attacked by opponent king).
  GenerateKingDestinations_(
      sq, our_color, [this, &under_attack, &opponent_color](const Square& to) {
        if (at(to) == Piece{opponent_color, PieceType::kKing}) {
          under_attack = true;
        }
      });
  if (under_attack) {
    return true;
  }

  // Rook moves (for rooks and queens)
  GenerateRookDestinations_(
      sq, our_color, PseudoLegalMoveSettings::kAcknowledgeEnemyPieces,
      [this, &under_attack, &opponent_color](const Square& to) {
        if ((at(to) == Piece{opponent_color, PieceType::kRook}) ||
            (at(to) == Piece{opponent_color, PieceType::kQueen}) ||
            (at(to) == Piece{opponent_color, PieceType::kRookP}) ||
            (at(to) == Piece{opponent_color, PieceType::kQueenP})) {
          under_attack = true;
        }
      });
  if (under_attack) {
    return true;
  }

  // Bishop moves (for bishops and queens)
  GenerateBishopDestinations_(
      sq, our_color, PseudoLegalMoveSettings::kAcknowledgeEnemyPieces,
      [this, &under_attack, &opponent_color](const Square& to) {
        if ((at(to) == Piece{opponent_color, PieceType::kBishop}) ||
            (at(to) == Piece{opponent_color, PieceType::kBishopP}) ||
            (at(to) == Piece{opponent_color, PieceType::kQueenP}) ||
            (at(to) == Piece{opponent_color, PieceType::kQueen})) {
          under_attack = true;
        }
      });
  if (under_attack) {
    return true;
  }

  // Knight moves
  GenerateKnightDestinations_(
      sq, our_color, [this, &under_attack, &opponent_color](const Square& to) {
        if ((at(to) == Piece{opponent_color, PieceType::kKnight}) ||
            (at(to) == Piece{opponent_color, PieceType::kKnightP})) {
          under_attack = true;
        }
      });
  if (under_attack) {
    return true;
  }

  if (under_attack) {
    return true;
  }

  return false;
}

std::string ShogiBoard::DebugString(bool shredder_fen) const {
  std::string s;
  s = absl::StrCat("FEN: ", ToFEN(shredder_fen), "\n");
  absl::StrAppend(&s, "\n  ---------------------------------\n");
  for (int8_t y = kBoardSize - 1; y >= 0; --y) {
    // Rank label.
    absl::StrAppend(&s, RankToString(y), " ");

    // Pieces on the rank.
    for (int8_t x = 0; x < kBoardSize; ++x) {
      Square sq{x, y};
      absl::StrAppend(&s, "| ", at(sq).ToString(), " ");
    }
    absl::StrAppend(&s, "|\n");
    absl::StrAppend(&s, "  ---------------------------------\n");
  }

  // File labels.
  absl::StrAppend(&s, "    ");
  for (int8_t x = 0; x < kBoardSize; ++x) {
    absl::StrAppend(&s, FileToString(x), "   ");
  }
  absl::StrAppend(&s, "\n");

  absl::StrAppend(&s, "To play: ", to_play_ == Color::kWhite ? "W" : "B", "\n");
  absl::StrAppend(&s, "Move number: ", move_number_, "\n\n");

  absl::StrAppend(&s, "\n");

  return s;
}

// King moves.
template <typename YieldFn>
void ShogiBoard::GenerateKingDestinations_(Square sq, Color color,
                                                const YieldFn& yield) const {
  static const std::array<Offset, 8> kOffsets = {
      {{1, 0}, {1, 1}, {1, -1}, {0, 1}, {0, -1}, {-1, 1}, {-1, 0}, {-1, -1}}};

  for (const auto& offset : kOffsets) {
    Square dest = sq + offset;
    if (InBoardArea(dest) && IsEmptyOrEnemy(dest, color)) {
      yield(dest);
    }
  }
}

template <typename YieldFn>
void ShogiBoard::GenerateQueenDestinations_(
    Square sq, Color color, PseudoLegalMoveSettings settings,
    const YieldFn& yield) const {
  GenerateRookDestinations_(sq, color, settings, yield);
  GenerateBishopDestinations_(sq, color, settings, yield);
}

template <typename YieldFn>
void ShogiBoard::GenerateRookDestinations_(
    Square sq, Color color, PseudoLegalMoveSettings settings,
    const YieldFn& yield) const {
  GenerateRayDestinations_(sq, color, settings, {1, 0}, yield);
  GenerateRayDestinations_(sq, color, settings, {-1, 0}, yield);
  GenerateRayDestinations_(sq, color, settings, {0, 1}, yield);
  GenerateRayDestinations_(sq, color, settings, {0, -1}, yield);
}

template <typename YieldFn>
void ShogiBoard::GenerateBishopDestinations_(
    Square sq, Color color, PseudoLegalMoveSettings settings,
    const YieldFn& yield) const {
  GenerateRayDestinations_(sq, color, settings, {1, 1}, yield);
  GenerateRayDestinations_(sq, color, settings, {-1, 1}, yield);
  GenerateRayDestinations_(sq, color, settings, {1, -1}, yield);
  GenerateRayDestinations_(sq, color, settings, {-1, -1}, yield);
}

template <typename YieldFn>
void ShogiBoard::GenerateKnightDestinations_(Square sq, Color color,
                                                  const YieldFn& yield) const {
  for (const auto& offset : kKnightOffsets) {
    Square dest = sq + offset;
    if (InBoardArea(dest) && IsEmptyOrEnemy(dest, color)) {
      yield(dest);
    }
  }
}

// Pawn moves without captures.
template <typename YieldFn>
void ShogiBoard::GeneratePawnDestinations_(
    Square sq, Color color, PseudoLegalMoveSettings settings,
    const YieldFn& yield) const {
  int8_t y_direction = color == Color::kWhite ? 1 : -1;
  Square dest = sq + Offset{0, y_direction};
  if (InBoardArea(dest) &&
      (IsEmpty(dest) ||
       (IsEnemy(dest, color) &&
        settings == PseudoLegalMoveSettings::kBreachEnemyPieces))) {
    yield(dest);

    // Test for double move. Only defined on standard board
    if (kBoardSize == 8 && IsPawnStartingRank(sq, color)) {
      dest = sq + Offset{0, static_cast<int8_t>(2 * y_direction)};
      if (IsEmpty(dest) ||
          (IsEnemy(dest, color) &&
           settings == PseudoLegalMoveSettings::kBreachEnemyPieces)) {
        yield(dest);
      }
    }
  }
}

template <typename YieldFn>
void ShogiBoard::GenerateRayDestinations_(Square sq, Color color,
                                               PseudoLegalMoveSettings settings,
                                               Offset offset_step,
                                               const YieldFn& yield) const {
  for (Square dest = sq + offset_step; InBoardArea(dest); dest += offset_step) {
    if (IsEmpty(dest)) {
      yield(dest);
    } else if (IsEnemy(dest, color)) {
      yield(dest);
      if (settings == PseudoLegalMoveSettings::kAcknowledgeEnemyPieces) {
        break;
      }
    } else {
      // We have a friendly piece.
      break;
    }
  }
}


std::string ShogiBoard::ToFEN(bool shredder) const {
  std::string fen;

  // ----- 1. Board -----
  for (int8_t rank = kBoardSize - 1; rank >= 0; --rank) {
    int num_empty = 0;

    for (int8_t file = 0; file < kBoardSize; ++file) {
      auto piece = at(Square{file, rank});

      if (piece == kEmptyPiece) {
        ++num_empty;
      } else {
        if (num_empty > 0) {
          absl::StrAppend(&fen, num_empty);
          num_empty = 0;
        }
        absl::StrAppend(&fen, piece.ToString());
      }
    }

    if (num_empty > 0) absl::StrAppend(&fen, num_empty);
    if (rank > 0) fen.push_back('/');
  }

  // ----- 1.5 🙂 Crazyhouse pockets -----
  std::string pockets;

  // white pockets: Pawn, Knight, Bishop, Rook, Queen indices 0..4
  for (Color color : {Color::kWhite, Color::kBlack}) {
    const Pocket& pocket =
        (color == Color::kWhite) ? white_pocket_ : black_pocket_;
    for (PieceType ptype : Pocket::PieceTypes()) {
      Piece pocket_piece{color, ptype};
      char c = pocket_piece.ToString()[0];
      pockets.append(pocket.Count(ptype), c);
    }
  }
  if (!pockets.empty()) {
    absl::StrAppend(&fen, "[", pockets, "]");
  }

  // ----- 2. Side to move -----
  absl::StrAppend(&fen, " ", to_play_ == Color::kWhite ? "w" : "b");

  // ----- 6. Move number -----
  absl::StrAppend(&fen, " ", move_number_);

  return fen;
}

// For purposes of the hash
// we  will saturate the pocket piece count at 16,
// although the actual piece count can go beyond that.
static constexpr int kMaxPocketHashCount = 16;
static const ZobristTableU64<2, 5, kMaxPocketHashCount + 1> kPocketZobrist(
    /*seed=*/2825712);

inline int HashCount(int n) { return std::min(n, kMaxPocketHashCount); }

void ShogiBoard::AddToPocket(Color owner, PieceType piece) {
  Pocket& pocket = owner == Color::kWhite ? white_pocket_ : black_pocket_;

	int old = pocket.Count(piece);
	int new_ = old + 1;

	int old_hash = HashCount(old);
	int new_hash = HashCount(new_);

	zobrist_hash_ ^=
			kPocketZobrist[ToInt(owner)][pocket.Index(piece)][old_hash];
	zobrist_hash_ ^=
			kPocketZobrist[ToInt(owner)][pocket.Index(piece)][new_hash];

	pocket.Increment(piece, 1);
}

void ShogiBoard::RemoveFromPocket(Color owner, PieceType piece) {
  Pocket& pocket = owner == Color::kWhite ? white_pocket_ : black_pocket_;
  int old = pocket.Count(piece);
  SPIEL_CHECK_GT(old, 0);

  int new_ = old - 1;

  int old_hash = HashCount(old);
  int new_hash = HashCount(new_);

  if (old_hash != new_hash) {
    zobrist_hash_ ^=
        kPocketZobrist[ToInt(owner)][pocket.Index(piece)][old_hash];
    zobrist_hash_ ^=
        kPocketZobrist[ToInt(owner)][pocket.Index(piece)][new_hash];
  }

  pocket.Decrement(piece);
}

void ShogiBoard::set_square(Square sq, Piece piece) {
	// TODO that 11 must change
  static const ZobristTableU64<kNumSquares, 3, 11> kZobristValues(
      /*seed=*/2765481);

  // First, remove the current piece from the hash.
  auto position = SquareToIndex_(sq);
  auto current_piece = at(sq);
  zobrist_hash_ ^=
      kZobristValues[position][static_cast<int>(current_piece.color)]
                    [static_cast<int>(current_piece.type)];

  // Then add the new piece
  zobrist_hash_ ^= kZobristValues[position][static_cast<int>(piece.color)]
                                 [static_cast<int>(piece.type)];

  board_[position] = piece;
}

void ShogiBoard::SetToPlay(Color c) {
  static const ZobristTableU64<2> kZobristValues(/*seed=*/284628);

  // Remove old color and add new to play.
  zobrist_hash_ ^= kZobristValues[ToInt(to_play_)];
  zobrist_hash_ ^= kZobristValues[ToInt(c)];
  to_play_ = c;
}


void ShogiBoard::SetMovenumber(int move_number) {
  move_number_ = move_number;
}

std::string DefaultFen() {
    return shogi::kDefaultStandardFEN;
}

void Pocket::Increment(PieceType piece, int count) {
  const std::size_t i = Index(piece);
  counts_[i] += count;
}

void Pocket::Decrement(PieceType piece) {
  const std::size_t i = Index(piece);
  SPIEL_CHECK_GT(counts_[i], 0);
  --counts_[i];
}

int Pocket::Count(PieceType piece) const { return counts_[Index(piece)]; }

// A captured promoted piece reverts to being a pawn
std::size_t Pocket::Index(PieceType ptype) {
  switch (ptype) {
    case PieceType::kPawn:
    case PieceType::kKnightP:
    case PieceType::kBishopP:
    case PieceType::kRookP:
    case PieceType::kQueenP:
      return 0;
    case PieceType::kKnight:
      return 1;
    case PieceType::kBishop:
      return 2;
    case PieceType::kRook:
      return 3;
    case PieceType::kQueen:
      return 4;
    default: {
      SpielFatalError(absl::StrCat("Invalid PieceType for Pocket: ",
                                   static_cast<int>(ptype)));
    }
      return 0;  // never happens
  }
}

PieceType Pocket::DropPieceType(int y) {
  switch (y) {
    case 0:
      return PieceType::kPawn;
    case 1:
      return PieceType::kKnight;
    case 2:
      return PieceType::kBishop;
    case 3:
      return PieceType::kRook;
    case 4:
      return PieceType::kQueen;
    default:
      return PieceType::kEmpty;
  }
}

}  // namespace shogi
}  // namespace open_spiel
