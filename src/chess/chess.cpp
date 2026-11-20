// =============================================================================
//  Portable chess engine core implementation.
//  See chess.h for the overview. This file is compiled both for the ESP32 and
//  for host-side unit tests.
// =============================================================================

#include "chess.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace chess {

// Piece values used for evaluation and search ordering, in centipawns.
static const int PIECE_VALUE[7] = {0, 100, 320, 330, 500, 900, 20000};

// Static piece-square tables (white perspective, a1=0..h8=63). These encourage
// sensible development and central control. Mirrored for black.
static const int PST_PAWN[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10,-20,-20, 10, 10,  5,
     5, -5,-10,  0,  0,-10, -5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5,  5, 10, 25, 25, 10,  5,  5,
    10, 10, 20, 30, 30, 20, 10, 10,
    50, 50, 50, 50, 50, 50, 50, 50,
     0,  0,  0,  0,  0,  0,  0,  0
};
static const int PST_KNIGHT[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};
static const int PST_BISHOP[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
};
static const int PST_ROOK[64] = {
      0,  0,  0,  5,  5,  0,  0,  0,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     5, 10, 10, 10, 10, 10, 10,  5,
      0,  0,  0,  0,  0,  0,  0,  0
};
static const int PST_QUEEN[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -10,  5,  5,  5,  5,  5,  0,-10,
      0,  0,  5,  5,  5,  5,  0, -5,
     -5,  0,  5,  5,  5,  5,  0, -5,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -10,  0,  0,  0,  0,  0, 0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};
static const int PST_KING_MID[64] = {
     20, 30, 10,  0,  0, 10, 30, 20,
     20, 20,  0,  0,  0,  0, 20, 20,
    -10,-20,-20,-20,-20,-20,-20,-10,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30
};

static const int* PST_TABLE[7] = {
    nullptr, PST_PAWN, PST_KNIGHT, PST_BISHOP, PST_ROOK, PST_QUEEN, PST_KING_MID
};

// Movement deltas expressed as (file, rank) steps.
struct Step { int8_t df, dr; };

static const Step KNIGHT_STEPS[8] = {
    {1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}
};
static const Step KING_STEPS[8] = {
    {1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1}
};
static const Step BISHOP_DIRS[4] = {{1,1},{1,-1},{-1,1},{-1,-1}};
static const Step ROOK_DIRS[4]   = {{1,0},{-1,0},{0,1},{0,-1}};

static inline int fileOf(int sq) { return sq & 7; }
static inline int rankOf(int sq) { return sq >> 3; }
static inline int onBoard(int f, int r) { return f >= 0 && f < 8 && r >= 0 && r < 8; }

static inline int8_t pieceType(int p) { return p < 0 ? (int8_t)(-p) : p; }
static inline int8_t pieceColour(int p) {
    if (p == 0) return 0;
    return p > 0 ? 1 : -1;
}

Board::Board() { reset(); }

void Board::reset() {
    static const int8_t back[8] = {ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK};
    for (int f = 0; f < 8; f++) {
        squares_[square(f, 0)] =  back[f];          // white back rank (rank 1)
        squares_[square(f, 1)] =  PAWN;             // white pawns
        squares_[square(f, 6)] = -PAWN;             // black pawns
        squares_[square(f, 7)] = -back[f];          // black back rank (rank 8)
        for (int r = 2; r < 6; r++) squares_[square(f, r)] = EMPTY;
    }
    side_ = 1;
    whiteCastleK_ = whiteCastleQ_ = true;
    blackCastleK_ = blackCastleQ_ = true;
    enPassant_ = -1;
    halfMove_ = 0;
    fullMove_ = 1;
    history_.clear();
}

int Board::kingSquare(int side) const {
    int target = side * KING;
    for (int sq = 0; sq < 64; sq++)
        if (squares_[sq] == target) return sq;
    return -1;
}

bool Board::attackedBy(int sq, int bySide) const {
    // Pawns: a square is attacked by an enemy pawn coming from the diagonal
    // behind it (relative to the attacker).
    int f = fileOf(sq), r = rankOf(sq);
    if (bySide == 1) {
        // white pawn attacks upward, so it sits one rank below
        if (r > 0) {
            if (f > 0 && squares_[sq - 9] == PAWN) return true;
            if (f < 7 && squares_[sq - 7] == PAWN) return true;
        }
    } else {
        if (r < 7) {
            if (f > 0 && squares_[sq + 7] == -PAWN) return true;
            if (f < 7 && squares_[sq + 9] == -PAWN) return true;
        }
    }
    // Knights
    for (const auto& s : KNIGHT_STEPS) {
        int nf = f + s.df, nr = r + s.dr;
        if (onBoard(nf, nr) && squares_[square(nf, nr)] == bySide * KNIGHT)
            return true;
    }
    // King
    for (const auto& s : KING_STEPS) {
        int nf = f + s.df, nr = r + s.dr;
        if (onBoard(nf, nr) && squares_[square(nf, nr)] == bySide * KING)
            return true;
    }
    // Bishops/queen diagonals
    for (const auto& d : BISHOP_DIRS) {
        int nf = f + d.df, nr = r + d.dr;
        while (onBoard(nf, nr)) {
            int p = squares_[square(nf, nr)];
            if (p != 0) {
                if (pieceColour(p) == bySide &&
                    (pieceType(p) == BISHOP || pieceType(p) == QUEEN))
                    return true;
                break;
            }
            nf += d.df; nr += d.dr;
        }
    }
    // Rooks/queen straight
    for (const auto& d : ROOK_DIRS) {
        int nf = f + d.df, nr = r + d.dr;
        while (onBoard(nf, nr)) {
            int p = squares_[square(nf, nr)];
            if (p != 0) {
                if (pieceColour(p) == bySide &&
                    (pieceType(p) == ROOK || pieceType(p) == QUEEN))
                    return true;
                break;
            }
            nf += d.df; nr += d.dr;
        }
    }
    return false;
}

bool Board::squareAttacked(int sq, int bySide) const {
    return attackedBy(sq, bySide);
}

bool Board::inCheck() const {
    int ks = kingSquare(side_);
    if (ks < 0) return false;
    return attackedBy(ks, -side_);
}

void Board::genSliding(int sq, const int* deltas, int nDelta,
                       std::vector<Move>& out) const {
    int f = fileOf(sq), r = rankOf(sq);
    int me = side_;
    for (int d = 0; d < nDelta; d++) {
        int df = deltas[2*d], dr = deltas[2*d+1];
        int nf = f + df, nr = r + dr;
        while (onBoard(nf, nr)) {
            int t = square(nf, nr);
            int p = squares_[t];
            if (p == 0) {
                Move m{}; m.from = sq; m.to = t; out.push_back(m);
            } else {
                if (pieceColour(p) != me) {
                    Move m{}; m.from = sq; m.to = t; m.captured = true; out.push_back(m);
                }
                break;
            }
            nf += df; nr += dr;
        }
    }
}

void Board::genPawn(int sq, std::vector<Move>& out) const {
    int f = fileOf(sq), r = rankOf(sq);
    int me = side_;
    int dir = me;                    // white +1, black -1
    int startRank = me == 1 ? 1 : 6;
    int promoRank = me == 1 ? 7 : 0;

    // Single/double push
    int nr = r + dir;
    if (onBoard(f, nr) && squares_[square(f, nr)] == 0) {
        if (nr == promoRank) {
            for (int promo : {QUEEN, ROOK, BISHOP, KNIGHT}) {
                Move m{}; m.from = sq; m.to = square(f, nr); m.promotion = promo;
                out.push_back(m);
            }
        } else {
            Move m{}; m.from = sq; m.to = square(f, nr); out.push_back(m);
            if (r == startRank) {
                int nr2 = r + 2 * dir;
                if (squares_[square(f, nr2)] == 0) {
                    Move m2{}; m2.from = sq; m2.to = square(f, nr2); out.push_back(m2);
                }
            }
        }
    }
    // Captures
    for (int df : {-1, 1}) {
        int nf = f + df;
        if (!onBoard(nf, nr)) continue;
        int t = square(nf, nr);
        int p = squares_[t];
        if (p != 0 && pieceColour(p) != me) {
            if (nr == promoRank) {
                for (int promo : {QUEEN, ROOK, BISHOP, KNIGHT}) {
                    Move m{}; m.from = sq; m.to = t; m.promotion = promo;
                    m.captured = true; out.push_back(m);
                }
            } else {
                Move m{}; m.from = sq; m.to = t; m.captured = true; out.push_back(m);
            }
        } else if (t == enPassant_ && enPassant_ >= 0) {
            Move m{}; m.from = sq; m.to = t; m.captured = true; m.enPassant = true;
            out.push_back(m);
        }
    }
}

void Board::genPiece(int sq, std::vector<Move>& out) const {
    int p = squares_[sq];
    int t = pieceType(p);
    int f = fileOf(sq), r = rankOf(sq);
    int me = side_;

    if (t == KNIGHT) {
        for (const auto& s : KNIGHT_STEPS) {
            int nf = f + s.df, nr = r + s.dr;
            if (!onBoard(nf, nr)) continue;
            int tt = square(nf, nr);
            int pp = squares_[tt];
            if (pp == 0) {
                Move m{}; m.from = sq; m.to = tt; out.push_back(m);
            } else if (pieceColour(pp) != me) {
                Move m{}; m.from = sq; m.to = tt; m.captured = true; out.push_back(m);
            }
        }
    } else if (t == KING) {
        for (const auto& s : KING_STEPS) {
            int nf = f + s.df, nr = r + s.dr;
            if (!onBoard(nf, nr)) continue;
            int tt = square(nf, nr);
            int pp = squares_[tt];
            if (pp == 0) {
                Move m{}; m.from = sq; m.to = tt; out.push_back(m);
            } else if (pieceColour(pp) != me) {
                Move m{}; m.from = sq; m.to = tt; m.captured = true; out.push_back(m);
            }
        }
        // Castling
        if (me == 1 && squares_[square(4, 0)] == KING) {
            if (whiteCastleK_ && squares_[square(5, 0)] == 0 &&
                squares_[square(6, 0)] == 0 && squares_[square(7, 0)] == ROOK &&
                !attackedBy(square(4, 0), -1) && !attackedBy(square(5, 0), -1) &&
                !attackedBy(square(6, 0), -1)) {
                Move m{}; m.from = square(4, 0); m.to = square(6, 0); m.castle = true;
                out.push_back(m);
            }
            if (whiteCastleQ_ && squares_[square(3, 0)] == 0 &&
                squares_[square(2, 0)] == 0 && squares_[square(1, 0)] == 0 &&
                squares_[square(0, 0)] == ROOK &&
                !attackedBy(square(4, 0), -1) && !attackedBy(square(3, 0), -1) &&
                !attackedBy(square(2, 0), -1)) {
                Move m{}; m.from = square(4, 0); m.to = square(2, 0); m.castle = true;
                out.push_back(m);
            }
        } else if (me == -1 && squares_[square(4, 7)] == -KING) {
            if (blackCastleK_ && squares_[square(5, 7)] == 0 &&
                squares_[square(6, 7)] == 0 && squares_[square(7, 7)] == -ROOK &&
                !attackedBy(square(4, 7), 1) && !attackedBy(square(5, 7), 1) &&
                !attackedBy(square(6, 7), 1)) {
                Move m{}; m.from = square(4, 7); m.to = square(6, 7); m.castle = true;
                out.push_back(m);
            }
            if (blackCastleQ_ && squares_[square(3, 7)] == 0 &&
                squares_[square(2, 7)] == 0 && squares_[square(1, 7)] == 0 &&
                squares_[square(0, 7)] == -ROOK &&
                !attackedBy(square(4, 7), 1) && !attackedBy(square(3, 7), 1) &&
                !attackedBy(square(2, 7), 1)) {
                Move m{}; m.from = square(4, 7); m.to = square(2, 7); m.castle = true;
                out.push_back(m);
            }
        }
    } else if (t == BISHOP) {
        static const int d[] = {1,1, 1,-1, -1,1, -1,-1};
        genSliding(sq, d, 4, out);
    } else if (t == ROOK) {
        static const int d[] = {1,0, -1,0, 0,1, 0,-1};
        genSliding(sq, d, 4, out);
    } else if (t ==  QUEEN) {
        static const int d[] = {1,1, 1,-1, -1,1, -1,-1, 1,0, -1,0, 0,1, 0,-1};
        genSliding(sq, d, 8, out);
    }
}

void Board::generatePseudo(std::vector<Move>& out) const {
    out.clear();
    int me = side_;
    for (int sq = 0; sq < 64; sq++) {
        int p = squares_[sq];
        if (p == 0 || pieceColour(p) != me) continue;
        if (pieceType(p) == PAWN) genPawn(sq, out);
        else genPiece(sq, out);
    }
}

bool Board::legal(const Move& m) {
    // After makeMove the side has flipped to the opponent; the mover is the
    // previous side to move. A move is legal only if it does NOT leave the
    // mover's own king in check. (Checking `inCheck()` here would be wrong,
    // because that tests the opponent's king -- i.e. "did I give check".)
    int mover = side_;
    makeMove(m);
    int ks = kingSquare(mover);
    bool ok = (ks >= 0) && !attackedBy(ks, -mover);
    unmakeMove();
    return ok;
}

void Board::generateLegalMoves(std::vector<Move>& out) {
    std::vector<Move> pseudo;
    generatePseudo(pseudo);
    out.clear();
    for (const Move& m : pseudo) {
        if (legal(m)) out.push_back(m);
    }
}

bool Board::makeMove(const Move& m) {
    if (m.from >= 64 || m.to >= 64) return false;
    int p = squares_[m.from];
    if (p == 0 || pieceColour(p) != side_) return false;

    State st{};
    st.move = m;
    st.captured = squares_[m.to];
    st.enPassant = enPassant_;
    st.wk = whiteCastleK_; st.wq = whiteCastleQ_;
    st.bk = blackCastleK_; st.bq = blackCastleQ_;
    st.halfMove = halfMove_;

    int t = pieceType(p);
    bool isPawn = (t == PAWN);
    bool isCapture = (squares_[m.to] != 0) || m.enPassant;

    // Move the piece.
    squares_[m.to] = p;
    squares_[m.from] = EMPTY;

    // En-passant capture removes the pawn behind the target square. Record the
    // captured piece BEFORE clearing it, so unmake can restore it correctly.
    if (m.enPassant) {
        int capSq = side_ == 1 ? m.to - 8 : m.to + 8;
        st.captured = squares_[capSq];
        squares_[capSq] = EMPTY;
    }

    // Save state only now that captured/en-passant info is final.
    history_.push_back(st);

    // Promotion.
    if (m.promotion) {
        squares_[m.to] = side_ * (int8_t)m.promotion;
    }

    // Castling: move the rook too.
    if (m.castle) {
        if (m.to == square(6, 0)) {            // white kingside
            squares_[square(5, 0)] = squares_[square(7, 0)];
            squares_[square(7, 0)] = EMPTY;
        } else if (m.to == square(2, 0)) {     // white queenside
            squares_[square(3, 0)] = squares_[square(0, 0)];
            squares_[square(0, 0)] = EMPTY;
        } else if (m.to == square(6, 7)) {     // black kingside
            squares_[square(5, 7)] = squares_[square(7, 7)];
            squares_[square(7, 7)] = EMPTY;
        } else if (m.to == square(2, 7)) {     // black queenside
            squares_[square(3, 7)] = squares_[square(0, 7)];
            squares_[square(0, 7)] = EMPTY;
        }
    }

    // Update castling rights.
    if (t == KING) {
        if (side_ == 1) { whiteCastleK_ = whiteCastleQ_ = false; }
        else { blackCastleK_ = blackCastleQ_ = false; }
    }
    // Rook moves from original squares remove rights.
    if (m.from == square(0, 0) || m.to == square(0, 0)) whiteCastleQ_ = false;
    if (m.from == square(7, 0) || m.to == square(7, 0)) whiteCastleK_ = false;
    if (m.from == square(0, 7) || m.to == square(0, 7)) blackCastleQ_ = false;
    if (m.from == square(7, 7) || m.to == square(7, 7)) blackCastleK_ = false;

    // En-passant target for next move.
    enPassant_ = -1;
    if (isPawn && abs((int)m.to - (int)m.from) == 16) {
        enPassant_ = side_ == 1 ? m.from + 8 : m.from - 8;
    }

    // Halfmove clock.
    halfMove_ = (isPawn || isCapture) ? 0 : halfMove_ + 1;

    if (side_ == -1) fullMove_++;
    side_ = -side_;

    return true;
}

bool Board::unmakeMove() {
    if (history_.empty()) return false;
    State st = history_.back();
    history_.pop_back();

    Move m = st.move;
    side_ = -side_;                          // switch back to mover
    if (side_ == -1) fullMove_--;

    int p = squares_[m.to];
    // Undo promotion
    if (m.promotion) p = side_ * PAWN;
    squares_[m.from] = p;
    squares_[m.to] = EMPTY;

    if (m.enPassant) {
        int capSq = side_ == 1 ? m.to - 8 : m.to + 8;
        squares_[capSq] = st.captured;
        squares_[m.to] = EMPTY;
    } else {
        squares_[m.to] = st.captured;
    }

    if (m.castle) {
        if (m.to == square(6, 0)) {
            squares_[square(7, 0)] = squares_[square(5, 0)];
            squares_[square(5, 0)] = EMPTY;
        } else if (m.to == square(2, 0)) {
            squares_[square(0, 0)] = squares_[square(3, 0)];
            squares_[square(3, 0)] = EMPTY;
        } else if (m.to == square(6, 7)) {
            squares_[square(7, 7)] = squares_[square(5, 7)];
            squares_[square(5, 7)] = EMPTY;
        } else if (m.to == square(2, 7)) {
            squares_[square(0, 7)] = squares_[square(3, 7)];
            squares_[square(3, 7)] = EMPTY;
        }
    }

    enPassant_ = st.enPassant;
    whiteCastleK_ = st.wk; whiteCastleQ_ = st.wq;
    blackCastleK_ = st.bk; blackCastleQ_ = st.bq;
    halfMove_ = st.halfMove;
    return true;
}

bool Board::isGameOver() {
    std::vector<Move> moves;
    generateLegalMoves(moves);
    return moves.empty();
}

void Board::squareToString(int sq, char out[3]) {
    out[0] = 'a' + fileOf(sq);
    out[1] = '1' + rankOf(sq);
    out[2] = 0;
}

bool Board::parseMove(const char* s, Move& out) {
    if (!s || strlen(s) < 4) return false;
    int f1 = s[0] - 'a', r1 = s[1] - '1';
    int f2 = s[2] - 'a', r2 = s[3] - '1';
    if (f1 < 0 || f1>7 || r1 < 0 || r1>7 || f2 < 0 || f2>7 || r2 < 0 || r2>7)
        return false;
    Move m{};
    m.from = square(f1, r1);
    m.to   = square(f2, r2);
    m.promotion = 0;
    if (strlen(s) >= 5) {
        switch (s[4]) {
            case 'q': case 'Q': m.promotion = QUEEN; break;
            case 'r': case 'R': m.promotion = ROOK; break;
            case 'b': case 'B': m.promotion = BISHOP; break;
            case 'n': case 'N': m.promotion = KNIGHT; break;
        }
    }
    std::vector<Move> legalMoves;
    generateLegalMoves(legalMoves);
    for (const Move& lm : legalMoves) {
        if (lm.from == m.from && lm.to == m.to &&
            lm.promotion == m.promotion) {
            out = lm;
            return true;
        }
    }
    return false;
}

// =============================================================================
//  Engine
// =============================================================================
Engine::Engine() : difficulty_(MEDIUM) {}

int Engine::evaluate(const Board& b) const {
    int score = 0;
    for (int sq = 0; sq < 64; sq++) {
        int p = b.pieceAt(sq);
        if (p == 0) continue;
        int t = pieceType(p);
        int sign = p > 0 ? 1 : -1;
        // White uses the table directly; black uses the mirrored square.
        int pstSq = sign == 1 ? sq : (sq ^ 56);  // mirror vertically
        int pst = PST_TABLE[t] ? PST_TABLE[t][pstSq] : 0;
        score += sign * (PIECE_VALUE[t] + pst);
    }
    return score;
}

// MVV-LVA ordering helper for capture sorting.
static int moveScoreForOrder(const Board& b, const Move& m) {
    int s = 0;
    if (m.captured) {
        int victim = pieceType(b.pieceAt(m.enPassant ? (m.to ^ 0) : m.to));
        // For en passant the captured pawn sits elsewhere; approximate.
        if (m.enPassant) victim = PAWN;
        int attacker = pieceType(b.pieceAt(m.from));
        s += 10000 + victim * 10 - attacker;
    }
    if (m.promotion) s += 8000 + PIECE_VALUE[m.promotion];
    return s;
}

int Engine::search(Board& b, int depth, int alpha, int beta, int ply) {
    if (depth <= 0) {
        return quiescence(b, alpha, beta, ply);
    }

    std::vector<Move> moves;
    b.generateLegalMoves(moves);

    if (moves.empty()) {
        if (b.inCheck()) return -100000 + ply;   // checkmate (bad for mover)
        return 0;                                // stalemate
    }

    // Order moves: captures and promotions first.
    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b2) {
        return moveScoreForOrder(b, a) > moveScoreForOrder(b, b2);
    });

    int best = -1000000;
    for (const Move& m : moves) {
        b.makeMove(m);
        int score = -search(b, depth - 1, -beta, -alpha, ply + 1);
        b.unmakeMove();
        if (score > best) best = score;
        if (best > alpha) alpha = best;
        if (alpha >= beta) break;                // beta cutoff
    }
    return best;
}

int Engine::quiescence(Board& b, int alpha, int beta, int ply) {
    int stand = evaluate(b);
    if (stand >= beta) return beta;
    if (stand > alpha) alpha = stand;
    if (ply > 16) return stand;                  // safety depth cap

    std::vector<Move> moves;
    b.generateLegalMoves(moves);
    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b2) {
        return moveScoreForOrder(b, a) > moveScoreForOrder(b, b2);
    });

    for (const Move& m : moves) {
        if (!m.captured && !m.promotion) continue;   // only tactical moves
        b.makeMove(m);
        int score = -quiescence(b, -beta, -alpha, ply + 1);
        b.unmakeMove();
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

SearchResult Engine::chooseMove(Board& b, Difficulty d) {
    difficulty_ = d;
    SearchResult result{};
    int depth;
    int randomness = 0;

    switch (d) {
        case EASY:   depth = 1; randomness = 60; break;
        case MEDIUM: depth = 2; randomness = 25; break;
        case HARD:   depth = 3; randomness = 0;  break;
        case EXPERT: depth = 4; randomness = 0;  break;
        default:     depth = 2; randomness = 25; break;
    }

    std::vector<Move> moves;
    b.generateLegalMoves(moves);
    if (moves.empty()) return result;

    // Score every root move.
    struct Scored { Move m; int s; };
    std::vector<Scored> scored;
    scored.reserve(moves.size());
    for (const Move& m : moves) {
        b.makeMove(m);
        int s = -search(b, depth - 1, -1000000, 1000000, 1);
        b.unmakeMove();
        scored.push_back({m, s});
    }

    // Sort best-first.
    std::sort(scored.begin(), scored.end(),
              [](const Scored& a, const Scored& b) { return a.s > b.s; });

    // Add randomness for easier levels: pick among near-best moves.
    if (randomness > 0) {
        int best = scored[0].s;
        std::vector<Scored> candidates;
        for (const auto& sc : scored)
            if (sc.s >= best - randomness) candidates.push_back(sc);
        // Weighted: prefer better moves but allow weaker ones.
        std::vector<int> weights;
        int total = 0;
        for (const auto& sc : candidates) {
            // Map score to a non-negative weight (better moves heavier).
            int w = 1 + (sc.s - (best - randomness));
            if (w < 1) w = 1;
            weights.push_back(w);
            total += w;
        }
        int r = rand() % total;
        int acc = 0;
        for (size_t i = 0; i < candidates.size(); i++) {
            acc += weights[i];
            if (r < acc) {
                result.bestMove = candidates[i].m;
                result.score = candidates[i].s;
                return result;
            }
        }
    }

    result.bestMove = scored[0].m;
    result.score = scored[0].s;
    return result;
}

} // namespace chess
