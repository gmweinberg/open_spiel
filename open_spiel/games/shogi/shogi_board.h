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

#ifndef OPEN_SPIEL_GAMES_SHOGI_SHOGI_BOARD_H_
#define OPEN_SPIEL_GAMES_SHOGI_SHOGI_BOARD_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ostream>
#include <string>
#include <utility>

#include "open_spiel/abseil-cpp/absl/types/optional.h"
#include "open_spiel/games/shogi/shogi_common.h"
#include "open_spiel/spiel_utils.h"

namespace open_spiel {
namespace shogi {

using shogi_common::kInvalidSquare;  // NOLINT
using shogi_common::Offset;
using shogi_common::Square;
using shogi_common::SquareToString;  // NOLINT

template <std::size_t... Dims>
using ZobristTableU64 = shogi_common::ZobristTable<uint64_t, Dims...>;

enum class Color : int8_t { kBlack = 0, kWhite = 1, kEmpty = 2 };

inline int ToInt(Color color) { return color == Color::kWhite ? 1 : 0; }

inline Color OppColor(Color color) {
  return color == Color::kWhite ? Color::kBlack : Color::kWhite;
}

std::string ColorToString(Color c);

inline std::ostream& operator<<(std::ostream& stream, Color c) {
  return stream << ColorToString(c);
}

enum class PieceType : int8_t {
  kEmpty = 0,
  kKing = 1,
	kLance = 2,
	kKnight = 3,
	KSilver = 4,
	kGold = 5,
	kPawn = 6,
	kBishop = 7,
	kRook = 8,
	kLanceP = 9,
	kKnightP = 10,
	kSilver = 11,
	kPawnP = 12,
	kBishopP = 13,
	kRookP = 14,
  kQueen = 15, // TODO get rid of this
};

static inline constexpr std::array<PieceType, 10> kPieceTypes = {
    {PieceType::kKing, PieceType::kLance, PieceType::kKnight,
		PieceType::kSilver, PieceType::kGold, PieceType::kPawn,
		PieceType::kBishop, PieceType::kRook,  PieceType::kLanceP,
		PieceType::kKnightP, PieceType::kPawnP, PieceType::kBishopP,
		PieceType::kRookP}};

PieceType PromotedType(PieceType type);
PieceType UnpromotedType(PieceType type);

// Tries to parse piece type from char ('K', 'Q', 'R', 'B', 'N', 'P').
// Case-insensitive.
absl::optional<PieceType> PieceTypeFromChar(char c);

// Converts piece type to one character strings - "K", "Q", "R", "B", "N", "P".
// p must be one of the enumerator values of PieceType.
std::string PieceTypeToString(PieceType p, bool uppercase = true);

struct Piece {
  bool operator==(const Piece& other) const {
    return type == other.type && color == other.color;
  }

  bool operator!=(const Piece& other) const { return !(*this == other); }

  std::string ToUnicode() const;
  std::string ToString() const;

  Color color;
  PieceType type;
};

static inline constexpr Piece kEmptyPiece =
    Piece{Color::kEmpty, PieceType::kEmpty};

inline std::ostream& operator<<(std::ostream& stream, const Piece& p) {
  return stream << p.ToString();
}

inline absl::optional<int8_t> ParseRank(char c) {
  if (c >= '1' && c <= '8') return c - '1';
  return absl::nullopt;
}

inline absl::optional<int8_t> ParseFile(char c) {
  if (c >= 'a' && c <= 'h') return c - 'a';
  return absl::nullopt;
}

// Maps y = [0, 7] to rank ["1", "8"].
inline std::string RankToString(int8_t rank) {
  return std::string(1, '1' + rank);
}

// Maps x = [0, 7] to file ["a", "h"].
inline std::string FileToString(int8_t file) {
  return std::string(1, 'a' + file);
}

// Offsets for all possible knight moves.
inline constexpr std::array<Offset, 8> kKnightOffsets = {
    {{-2, -1}, {-2, 1}, {2, -1}, {2, 1}}};

absl::optional<Square> SquareFromString(const std::string& s);


struct Move {
  Square from;
  Square to;
  Piece piece;
  bool promote = false;
  bool drop = false;

	Move() {};

  Move(const Square& from, const Square& to, const Piece& piece,
       bool promote = false,
       bool drop = false)
      : from(from),
        to(to),
        piece(piece),
        promote(promote),
        drop(drop) {}

  std::string ToString() const;
  bool IsDropMove() const {
    return drop;
  }

  // Converts to long algebraic notation, as required by the UCI protocol.
  std::string ToLAN() const;

  std::string ToSAN(const ShogiBoard& board) const;


  bool operator==(const Move& other) const {
    return from == other.from && to == other.to && piece == other.piece &&
           promote == other.promote &&
           drop == other.drop;
  }
};

inline std::ostream& operator<<(std::ostream& stream, const Move& m) {
  return stream << m.ToString();
}

bool IsMoveCharacter(char c);

std::pair<std::string, std::string> SplitAnnotations(const std::string& move);

inline const std::string kDefaultStandardFEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
inline const std::string kDefaultSmallFEN = "r1kr/pppp/PPPP/R1KR w - - 0 1";

using ObservationTable = std::array<bool, kNumSquares>;

// Specifies policy for pseudo legal moves generation.
enum PseudoLegalMoveSettings {
  // Standard legal moves (do not allow to move past enemy pieces).
  kAcknowledgeEnemyPieces,
  // Pseudo-legal moves, where a piece can move anywhere (according to the rules
  // for that piece), except if it was blocked from doing so by other player's
  // pieces. This is used in games, where the player may not know the position
  // of an enemy piece (like Kriegspiel or RBC) and it can try to move past the
  // enemy (for example a rook can try to move the other side of the board, even
  // if it is in fact blocked by an unseen opponent's pawn).
  kBreachEnemyPieces,
};

// Some chess variants (RBC) allow a "pass" action/move
inline constexpr open_spiel::Action kPassAction = 0;
// The linter won't let me have a line-continuation indent
inline const shogi::Move kPassMove = Move(
    Square{-1, -1}, Square{-1, -1}, Piece{Color::kEmpty, PieceType::kEmpty});

int PocketIndex(PieceType ptype);

PieceType PocketPieceType(int index);

class Pocket {
 public:
  // Iteration support
  static constexpr std::array<PieceType, 7> PieceTypes() {
    kBoardSize return {PieceType::kPawn, PieceType::kLance, PieceType::kKnight,
			      PieceType::kSilver, PieceType::kGold, PieceType::kBishop,
            PieceType::kRook};
  }

  // Modifiers
  void Increment(PieceType piece, int count);
  void Decrement(PieceType piece);

  // Accessor
  int Count(PieceType piece) const;

  // Move encoding
  static std::size_t Index(PieceType ptype);

 private:
  static constexpr std::size_t kNumPocketPieces = 7;
  // Internal storage: Pawn, Lance
  std::array<int, kNumPocketPieces> counts_{};
};

class ShogiBoard {
 public:
  // Constructs a chess board at the given position in Forsyth-Edwards Notation.
  // https://en.wikipedia.org/wiki/Forsyth%E2%80%93Edwards_Notation
	ShogiBoard();
  static absl::optional<ShogiBoard> BoardFromFEN(
      const std::string& fen
  );

  const Piece& at(Square sq) const { return board_[SquareToIndex_(sq)]; }

  void set_square(Square sq, Piece p);

  const std::array<Piece, kNumSquares>& pieces() const { return board_; }

  Color ToPlay() const { return to_play_; }
  void SetToPlay(Color c);

  int32_t Movenumber() const { return move_number_; }


  void AddToPocket(Color owner, PieceType piece);

  void RemoveFromPocket(Color owner, PieceType piece);

  // Find the location of any one piece of the given type, or kInvalidSquare.
  Square find(const Piece& piece) const;

  using MoveYieldFn = std::function<bool(const Move&)>;
  void GenerateLegalMoves(const MoveYieldFn& yield) const {
    GenerateLegalMoves(yield, to_play_);
  }
  void GenerateLegalMoves(const MoveYieldFn& yield, Color color) const;


  // counts of pocket pieces by type
  // Pawn, Knight, Bishop, Rook, Queen
  Pocket white_pocket_;  // counts of pocket pieces by type
  Pocket black_pocket_;

  bool HasLegalMoves() const {
    bool found = false;
    GenerateLegalMoves([&found](const Move&) {
      found = true;
      return false;  // We don't need any more moves.
    });
    return found;
  }

  bool IsMoveLegal(const Move& tested_move) const {
    bool found = false;
    GenerateLegalMoves([&found, &tested_move](const Move& found_move) {
      if (tested_move == found_move) {
        found = true;
        return false;  // We don't need any more moves.
      }
      return true;
    });
    return found;
  }

  // Parses a move in standard algebraic notation or long algebraic notation
  // (see below). Returns absl::nullopt on failure.
  absl::optional<Move> ParseMove(const std::string& move) const;

  // For both LAN and SAN we first check for a drop move with syntax like N@d4
  // All drop moves are shown with a drop syntax, so Nd4 always mean a knight
  // on the board moved.
  absl::optional<Move> ParseDropMove(const std::string& move) const;
  // Parses a move in standard algebraic notation as defined by FIDE.
  // https://en.wikipedia.org/wiki/Algebraic_notation_(chess).
  // Returns absl::nullopt on failure.

  // Parses a move in long algebraic notation.
  // Long algebraic notation is not standardized and there are many variants,
  // but the one we care about is of the form "e2e4" and "f7f8q". This is the
  // form used by chess engine text protocols that are of interest to us.
  // Returns absl::nullopt on failure.
  absl::optional<Move> ParseLANMove(const std::string& move) const;

  void ApplyMove(const Move& move);

  // Applies a pseudo-legal move and returns whether it's legal. This avoids
  // applying and copying the whole board once for legality testing, and once
  // for actually applying the move.
  bool TestApplyMove(const Move& move);

  bool InBoardArea(const Square& sq) const {
    return sq.x >= 0 && sq.x < kBoardSize && sq.y >= 0 && sq.y < kBoardSize;
  }

  bool IsEmpty(const Square& sq) const {
    const Piece& piece = board_[SquareToIndex_(sq)];
    return piece.type == PieceType::kEmpty;
  }

  bool IsEnemy(const Square& sq, Color our_color) const {
    const Piece& piece = board_[SquareToIndex_(sq)];
    return piece.type != PieceType::kEmpty && piece.color != our_color;
  }

  bool IsFriendly(const Square& sq, Color our_color) const {
    const Piece& piece = board_[SquareToIndex_(sq)];
    return piece.color == our_color;
  }

  bool IsEmptyOrEnemy(const Square& sq, Color our_color) const {
    const Piece& piece = board_[SquareToIndex_(sq)];
    return piece.color != our_color;
  }

  /* Whether the square is on the pawn starting rank for our_color. */
  bool IsPawnStartingRank(const Square& sq, Color our_color) const {
    return ((our_color == Color::kWhite && sq.y == 1) ||
            (our_color == Color::kBlack && sq.y == (kBoardSize - 2)));
  }

  bool IsPawnPromotionRank(const Square& sq) const {
    // No need to test for color here because a pawn can't be on the "wrong"
    // promotion rank.
    return sq.y == 0 || sq.y == (kBoardSize - 1);
  }

  /* Whether the sq is under attack by the opponent. */
  bool UnderAttack(const Square& sq, Color our_color) const;

  bool InCheck() const {
    return UnderAttack(find(Piece{to_play_, PieceType::kKing}), to_play_);
  }

  bool KingInCheckAllowed() const { return king_in_check_allowed_; }

  bool AllowPassMove() const { return allow_pass_move_; }

  uint64_t HashValue() const { return zobrist_hash_; }

  std::string DebugString(bool shredder_fen = false) const;

  // Constructs a string describing the chess board position in Forsyth-Edwards
  // Notation. https://en.wikipedia.org/wiki/Forsyth%E2%80%93Edwards_Notation
  // Modified to support promoted and pocket pieces.
  std::string ToFEN(bool shredder = false) const;

  bool IsBreachingMove(Move move) const;
  void BreachingMoveToCaptureMove(Move* move) const;

 private:
  size_t SquareToIndex_(Square sq) const { return sq.y * kBoardSize + sq.x; }

  /* Generate*Destinations functions call yield(sq) for every potential
   * destination generated.
   * Eg.
   * std::vector<Move> knight_moves;
   * board.GenerateKnightDestinations(Square{3, 3},
   *                                  [](const Square& sq) {
   *                                    Move move{Square{3, 3}, sq,
   *                                              Piece{kWhite, kKnight}};
   *                                    knight_moves.push_back(move);
   *                                  });
   */

  /* All the Generate*Destinations functions work in the same slightly strange
   * way -
   * They assume there's a piece of the type in question at sq, and generate
   * potential destinations. Potential destinations may include moves that
   * will leave the king exposed, and are therefore illegal.
   * This strange semantic is to support reusing these functions for checking
   * whether one side is in check, which would otherwise require an almost-
   * duplicate move generator.
   */

  template <typename YieldFn>
  void GenerateKingDestinations_(Square sq, Color color,
                                 const YieldFn& yield) const;

  template <typename YieldFn>
  void GenerateQueenDestinations_(Square sq, Color color,
                                  PseudoLegalMoveSettings settings,
                                  const YieldFn& yield) const;

  template <typename YieldFn>
  void GenerateRookDestinations_(Square sq, Color color,
                                 PseudoLegalMoveSettings settings,
                                 const YieldFn& yield) const;

  template <typename YieldFn>
  void GenerateBishopDestinations_(Square sq, Color color,
                                   PseudoLegalMoveSettings settings,
                                   const YieldFn& yield) const;

  template <typename YieldFn>
  void GenerateKnightDestinations_(Square sq, Color color,
                                   const YieldFn& yield) const;

  template <typename YieldFn>
  // Pawn moves without captures.
  void GeneratePawnDestinations_(Square sq, Color color,
                                 PseudoLegalMoveSettings settings,
                                 const YieldFn& yield) const;

  template <typename YieldFn>
  // Pawn diagonal capture destinations, with or without en passant.
  void GeneratePawnCaptureDestinations_(Square sq, Color color,
                                        PseudoLegalMoveSettings settings,
                                        bool include_ep,
                                        const YieldFn& yield) const;

  // Helper function.
  template <typename YieldFn>
  void GenerateRayDestinations_(Square sq, Color color,
                                PseudoLegalMoveSettings settings,
                                Offset offset_step, const YieldFn& yield) const;

  template <typename YieldFn>
  void GenerateDropDestinations_(Color player,
                                 const PseudoLegalMoveSettings& settings,
                                 const YieldFn& yield) const;

  void SetMovenumber(int move_number);

  bool king_in_check_allowed_;
  bool allow_pass_move_;

  std::array<Piece, kNumSquares> board_;
  Color to_play_;

  // This starts at 1, and increments after each black move (a "full move" in
  // chess is a "half move" by white followed by a "half move" by black).
  int32_t move_number_;

  uint64_t zobrist_hash_;
};

inline std::ostream& operator<<(std::ostream& stream,
                                const ShogiBoard& board) {
  return stream << board.DebugString();
}

inline std::ostream& operator<<(std::ostream& stream, const PieceType& pt) {
  return stream << PieceTypeToString(pt);
}

bool StuckPiece(Color player, PieceType ptype, int8_t y);

bool InPromoZone(Color player, int8_t y);

std::string DefaultFen();

}  // namespace shogi
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_SHOGI_SHOGI_BOARD_H_
