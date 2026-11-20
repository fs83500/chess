#pragma once
// =============================================================================
//  Portable chess engine core for the automated chess board.
//  No hardware dependency, no dynamic allocation -> safe on ESP32.
//  Written in plain C++17 so the same code is unit-tested on the host and
//  compiled for the ESP32.
// =============================================================================

#include <cstdint>
#include <vector>

namespace chess {

// Piece encoding: sign = colour (positive white, negative black).
// 1=pawn, 2=knight, 3=bishop, 4=rook, 5=queen, 6=king, 0=empty.
enum PieceType : int8_t {
    EMPTY = 0, PAWN = 1, KNIGHT = 2, BISHOP = 3,
    ROOK = 4, QUEEN = 5, KING = 6
};

// A move on the board.
struct Move {
    uint8_t from;       // 0..63 (a1=0, h1=7, ..., h8=63)
    uint8_t to;         // 0..63
    uint8_t promotion;  // piece type to promote to, or 0
    bool     captured;  // a piece is captured by this move
    bool     enPassant; // pawn captures en passant
    bool     castle;    // castling move
};

// Difficulty levels for the bot.
enum Difficulty : uint8_t {
    EASY   = 0,  // depth 1, very random
    MEDIUM = 1,  // depth 2, mildly random
    HARD   = 2,  // depth 3, deterministic
    EXPERT = 3,  // depth 4, deterministic + better eval
};

// Result of a search / status helpers.
struct SearchResult {
    Move  bestMove;
    int   score;     // in centipawns, from white's perspective
};

class Board {
public:
    Board();

    // Reset to the standard starting position.
    void reset();

    // Returns true if it is white's turn to move.
    bool whiteToMove() const { return side_ == 1; }

    // Generate all legal moves for the side to move.
    void generateLegalMoves(std::vector<Move>& out);

    // True if the side to move has no legal move (checkmate or stalemate).
    bool isGameOver();

    // True if the side to move is in check (const-friendly variant).
    bool inCheck() const;

    // Make a move (assumed legal). Returns true if the move was applied.
    bool makeMove(const Move& m);

    // Undo the last applied move. Returns false if there was none.
    bool unmakeMove();

    // Parse a move from algebraic-like coordinates, e.g. "e2e4", "g1f3",
    // "e7e8q". Returns true and fills `out` if valid & legal.
    bool parseMove(const char* s, Move& out);

    // Square content helper: signed piece value or 0.
    int pieceAt(int sq) const { return squares_[sq]; }

    // Square index from file/rank: file 0='a'..7='h', rank 0='1'..7='8'.
    static int square(int file, int rank) { return rank * 8 + file; }

    // Convert a square index to a human readable string ("e4").
    static void squareToString(int sq, char out[3]);

    // Side to move: +1 white, -1 black.
    int side() const { return side_; }

    // Fullmove/halfmove counters (informational).
    int fullMoveNumber() const { return fullMove_; }

private:
    int8_t  squares_[64];       // signed piece values
    int8_t  side_;              // +1 white, -1 black
    bool    whiteCastleK_;      // white can castle kingside
    bool    whiteCastleQ_;
    bool    blackCastleK_;
    bool    blackCastleQ_;
    int8_t  enPassant_;         // en-passant target square or -1
    int     halfMove_;
    int     fullMove_;

    struct State {
        Move  move;
        int8_t captured;        // captured piece (signed) for restore
        int8_t enPassant;
        bool  wk, wq, bk, bq;
        int   halfMove;
    };
    std::vector<State> history_;

    // Pseudo-legal generation (may leave king in check).
    void generatePseudo(std::vector<Move>& out) const;
    void genPawn(int sq, std::vector<Move>& out) const;
    void genPiece(int sq, std::vector<Move>& out) const;
    void genSliding(int sq, const int* deltas, int nDelta,
                    std::vector<Move>& out) const;

    bool attackedBy(int sq, int bySide) const;   // is `sq` attacked by `bySide`
    bool squareAttacked(int sq, int bySide) const;
    int  kingSquare(int side) const;

    bool legal(const Move& m);  // check that own king not in check after
};

// -----------------------------------------------------------------------------
//  Search / AI
// -----------------------------------------------------------------------------
class Engine {
public:
    Engine();

    // Choose a move for the side to move, given a difficulty.
    SearchResult chooseMove(Board& b, Difficulty d);

private:
    int  evaluate(const Board& b) const;          // centipawns, white POV
    int  search(Board& b, int depth, int alpha, int beta, int ply);
    int  quiescence(Board& b, int alpha, int beta, int ply);

    Difficulty difficulty_;
};

} // namespace chess
