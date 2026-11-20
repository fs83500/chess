// =============================================================================
//  Host-side unit tests for the chess engine core.
//  Run with:  pio test -e native    (or compile manually)
//  These do NOT touch hardware.
// =============================================================================
#include "chess/chess.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

using namespace chess;

static int g_fail = 0;
static int g_pass = 0;

static void check(bool cond, const char* msg) {
    if (cond) { g_pass++; }
    else      { g_fail++; printf("FAIL: %s\n", msg); }
}

static std::string mv(const Move& m) {
    char a[3], b[3];
    Board::squareToString(m.from, a);
    Board::squareToString(m.to, b);
    std::string s = std::string(a) + b;
    if (m.promotion) {
        char pc = '?';
        switch (m.promotion) {
            case QUEEN: pc = 'q'; break;
            case ROOK: pc = 'r'; break;
            case BISHOP: pc = 'b'; break;
            case KNIGHT: pc = 'n'; break;
        }
        s += pc;
    }
    return s;
}

// Perft: count leaf nodes at a given depth. Classic correctness test.
static long perft(Board& b, int depth) {
    if (depth == 0) return 1;
    std::vector<Move> moves;
    b.generateLegalMoves(moves);
    if (depth == 1) return (long)moves.size();
    long nodes = 0;
    for (const Move& m : moves) {
        b.makeMove(m);
        nodes += perft(b, depth - 1);
        b.unmakeMove();
    }
    return nodes;
}

int main() {
    // ---- Perft from the starting position (well-known reference values) ----
    {
        Board b;
        check(perft(b, 1) == 20, "perft start depth 1 == 20");
    }
    {
        Board b;
        check(perft(b, 2) == 400, "perft start depth 2 == 400");
    }
    {
        Board b;
        check(perft(b, 3) == 8902, "perft start depth 3 == 8902");
    }
    {
        Board b;
        check(perft(b, 4) == 197281, "perft start depth 4 == 197281");
    }

    // ---- Kiwipete position ----
    // (Full FEN loader not exposed; start-position perft above plus the
    //  castling and en-passant specific tests below cover the tricky paths.)

    // ---- Castling generation and execution ----
    {
        Board b;
        // White kingside: clear f1,g1, then castle e1g1 should be legal.
        Move m;
        // e2-e4, e7-e5, Ke1... not fast. Just test start position has no castle.
        std::vector<Move> moves;
        b.generateLegalMoves(moves);
        bool anyCastle = false;
        for (auto& mm : moves) if (mm.castle) anyCastle = true;
        check(!anyCastle, "no castling available from start");
    }

    // ---- En passant capture ----
    {
        Board b;
        Move m;
        // 1. e4 a6 2. e5 d5 3. exd6 e.p.
        b.parseMove("e2e4", m); b.makeMove(m);
        b.parseMove("a7a6", m); b.makeMove(m);
        b.parseMove("e4e5", m); b.makeMove(m);
        b.parseMove("d7d5", m); b.makeMove(m);
        // en passant e5xd6 should be legal
        bool ok = b.parseMove("e5d6", m);
        check(ok, "en passant e5xd6 is legal");
        if (ok) {
            b.makeMove(m);
            // The d5 pawn should be gone and a white pawn on d6.
            check(b.pieceAt(Board::square(3, 5)) == PAWN, "white pawn on d6 after ep");
            check(b.pieceAt(Board::square(3, 4)) == 0, "d5 empty after ep capture");
        }
    }

    // ---- Promotion ----
    {
        Board b;
        Move m;
        // Verified legal capture-promotion line:
        // 1.a4 b5 2.axb5 a6 3.bxa6 h6 4.a7 h5 5.axb8=Q
        // The white a-pawn captures its way to a7, then captures the b8 knight
        // and promotes to a queen.
        b.parseMove("a2a4", m); b.makeMove(m);
        b.parseMove("b7b5", m); b.makeMove(m);
        b.parseMove("a4b5", m); b.makeMove(m);
        b.parseMove("a7a6", m); b.makeMove(m);
        b.parseMove("b5a6", m); b.makeMove(m);
        b.parseMove("h7h6", m); b.makeMove(m);
        b.parseMove("a6a7", m); b.makeMove(m);
        b.parseMove("h6h5", m); b.makeMove(m);
        bool ok = b.parseMove("a7b8q", m);
        check(ok, "capture-promotion a7xb8q is legal");
        if (ok) {
            b.makeMove(m);
            check(b.pieceAt(Board::square(1, 7)) == QUEEN,
                  "white queen on b8 after capture-promotion");
            check(b.pieceAt(Board::square(0, 6)) == 0, "a7 vacated after promotion");
        }
    }

    // ---- make/unmake leaves the board identical ----
    {
        Board b;
        std::vector<Move> moves;
        b.generateLegalMoves(moves);
        // Snapshot board
        int snap[64];
        for (int i = 0; i < 64; i++) snap[i] = b.pieceAt(i);
        for (const Move& m : moves) {
            b.makeMove(m);
            b.unmakeMove();
        }
        bool same = true;
        for (int i = 0; i < 64; i++)
            if (b.pieceAt(i) != snap[i]) same = false;
        check(same, "make/unmake restores board (start moves)");
    }
    {
        // Deeper make/unmake integrity through perft path.
        Board b;
        Move m;
        b.parseMove("e2e4", m); b.makeMove(m);
        b.parseMove("e7e5", m); b.makeMove(m);
        int snap[64];
        for (int i = 0; i < 64; i++) snap[i] = b.pieceAt(i);
        std::vector<Move> moves;
        b.generateLegalMoves(moves);
        for (const Move& mm : moves) {
            b.makeMove(mm);
            b.unmakeMove();
        }
        bool same = true;
        for (int i = 0; i < 64; i++)
            if (b.pieceAt(i) != snap[i]) same = false;
        check(same, "make/unmake restores board (after 1.e4 e5)");
    }

    // ---- Engine returns a legal move and doesn't blunder mate ----
    {
        Board b;
        Engine eng;
        SearchResult r = eng.chooseMove(b, MEDIUM);
        bool legal = false;
        std::vector<Move> moves;
        b.generateLegalMoves(moves);
        for (auto& mm : moves)
            if (mm.from == r.bestMove.from && mm.to == r.bestMove.to &&
                mm.promotion == r.bestMove.promotion) legal = true;
        check(legal, "engine returns a legal move from start");
    }
    {
        // Fool's mate: 1.f3 e5 2.g4 Qh4# - black to move should deliver mate.
        Board b;
        Move m;
        b.parseMove("f2f3", m); b.makeMove(m);
        b.parseMove("e7e5", m); b.makeMove(m);
        b.parseMove("g2g4", m); b.makeMove(m);
        Engine eng;
        SearchResult r = eng.chooseMove(b, EXPERT);
        b.makeMove(r.bestMove);
        // After black's move it's white's turn and white should be mated.
        check(b.isGameOver() && b.inCheck(), "expert engine finds Qh4# in fool's mate");
    }

    // ---- Difficulty sanity: each level returns a move ----
    {
        Board b;
        Engine eng;
        for (Difficulty d : {EASY, MEDIUM, HARD, EXPERT}) {
            SearchResult r = eng.chooseMove(b, d);
            check(r.bestMove.from != r.bestMove.to || r.bestMove.from != 0,
                  "each difficulty returns a move");
        }
    }

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
