// =============================================================================
//  Automated chess board - ESP32 firmware entry point.
//
//  How a game works:
//    1. At boot the gantry homes, a new game starts, the human (White) moves.
//    2. The human enters a move on the Serial console as "e2e4" (coordinate
//       notation; promotion e.g. "e7e8q"). In a fully built board this would
//       instead be detected automatically from reed switches; here we accept
//       it from Serial so the system is testable without the full hardware.
//    3. The bot (ESP32 "mini IA") thinks at the selected difficulty and replies.
//    4. The bot's move is executed physically by the magnet gantry, and any
//       captured piece is parked off the board.
//
//  Difficulty is cycled by the on-board button (prints the current level) or by
//  sending "level 0..3" over Serial.
// =============================================================================

#include <Arduino.h>
#include "chess/chess.h"
#include "hardware/motion.h"

using namespace chess;

static Board     board;
static Engine    engine;
static hardware::Motion motion;

static Difficulty difficulty = MEDIUM;
static bool gameOver = false;

static const char* diffName(Difficulty d) {
    switch (d) {
        case EASY:   return "EASY";
        case MEDIUM: return "MEDIUM";
        case HARD:   return "HARD";
        case EXPERT: return "EXPERT";
        default:     return "?";
    }
}

static int fileOf(int sq) { return sq & 7; }
static int rankOf(int sq) { return sq >> 3; }

// Execute a move on the physical board: move the piece, and park any capture.
static void executePhysically(const Move& m) {
    int ff = fileOf(m.from), fr = rankOf(m.from);
    int tf = fileOf(m.to),   tr = rankOf(m.to);

    if (m.castle) {
        // Move the king...
        motion.movePiece(ff, fr, tf, tr);
        // ...and the rook to its castled square.
        if (m.to == Board::square(6, 0))      motion.movePiece(7, 0, 5, 0);
        else if (m.to == Board::square(2, 0)) motion.movePiece(0, 0, 3, 0);
        else if (m.to == Board::square(6, 7)) motion.movePiece(7, 7, 5, 7);
        else if (m.to == Board::square(2, 7)) motion.movePiece(0, 7, 3, 7);
        return;
    }

    if (m.enPassant) {
        // The captured pawn is on the same file as `to`, same rank as `from`.
        motion.movePiece(ff, fr, tf, tr);
        int capFile = fileOf(m.to);
        int capRank = rankOf(m.from);
        motion.parkPiece(capFile, capRank, capFile);
        return;
    }

    if (m.captured) {
        // Park the captured piece first, then bring the mover in.
        motion.parkPiece(tf, tr, tf);
    }
    motion.movePiece(ff, fr, tf, tr);
}

static void printBoard() {
    for (int r = 7; r >= 0; r--) {
        char line[40] = {0};
        int p = 0;
        line[p++] = '1' + r; line[p++] = ' ';
        for (int f = 0; f < 8; f++) {
            int v = board.pieceAt(r * 8 + f);
            if (v == 0) line[p++] = '.';
            else if (v > 0) line[p++] = "PNBRQK"[v - 1];
            else            line[p++] = "pnbrqk"[-v - 1];
            line[p++] = ' ';
        }
        line[p] = 0;
        Serial.println(line);
    }
    Serial.println("  a b c d e f g h");
}

static void announceMove(const char* who, const Move& m) {
    char a[3], b[3];
    Board::squareToString(m.from, a);
    Board::squareToString(m.to, b);
    char buf[32];
    if (m.promotion) {
        char pc = m.promotion == QUEEN ? 'q' : m.promotion == ROOK ? 'r'
                : m.promotion == BISHOP ? 'b' : 'n';
        snprintf(buf, sizeof(buf), "%s%s%c", a, b, pc);
    } else {
        snprintf(buf, sizeof(buf), "%s%s", a, b);
    }
    Serial.printf("%s plays %s\n", who, buf);
}

static void botMove() {
    if (gameOver) return;
    SearchResult r = engine.chooseMove(board, difficulty);
    if (r.bestMove.from == 0 && r.bestMove.to == 0) {
        // No move: game over.
        gameOver = true;
        Serial.println(board.inCheck() ? "Bot is checkmated." : "Stalemate.");
        return;
    }
    announceMove("Bot", r.bestMove);
    board.makeMove(r.bestMove);
    executePhysically(r.bestMove);

    printBoard();

    if (board.isGameOver()) {
        gameOver = true;
        Serial.println(board.inCheck() ? "Checkmate. You win!" : "Stalemate.");
    }
}

static void handleLine(const String& line) {
    String s = line;
    s.trim();
    if (s.length() == 0) return;

    if (s.startsWith("level")) {
        int lvl = s.substring(6).toInt();
        if (lvl >= 0 && lvl <= 3) {
            difficulty = (Difficulty)lvl;
            Serial.printf("Difficulty set to %s\n", diffName(difficulty));
        } else {
            Serial.println("Usage: level 0..3");
        }
        return;
    }
    if (s == "new") {
        board.reset();
        gameOver = false;
        Serial.println("New game. You are White. Your move.");
        printBoard();
        return;
    }
    if (s == "show") { printBoard(); return; }

    if (gameOver) {
        Serial.println("Game over. Type 'new' to start again.");
        return;
    }

    Move m;
    if (!board.parseMove(s.c_str(), m)) {
        Serial.println("Illegal move. Try again (e.g. e2e4).");
        return;
    }
    announceMove("You", m);
    board.makeMove(m);
    printBoard();

    if (board.isGameOver()) {
        gameOver = true;
        Serial.println(board.inCheck() ? "Checkmate. Bot wins." : "Stalemate.");
        return;
    }
    // Bot replies.
    botMove();
}

// Debounced button press -> cycle difficulty.
static void checkButton() {
    static uint8_t btnState = LOW;
    static uint32_t lastDebounce = 0;
    uint8_t reading = digitalRead(hardware::BUTTON_PIN);
    if (reading != btnState) {
        lastDebounce = millis();
        btnState = reading;
    }
    if (reading == HIGH && (millis() - lastDebounce) > 50 && btnState == HIGH) {
        // Hold-off so one press = one step.
        static uint32_t lastPress = 0;
        if (millis() - lastPress > 300) {
            difficulty = (Difficulty)((difficulty + 1) % 4);
            Serial.printf("Difficulty: %s\n", diffName(difficulty));
            lastPress = millis();
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== Automated Chess Board (ESP32) ===");
    motion.begin();
    motion.home();
    board.reset();
    Serial.printf("Difficulty: %s  (button cycles, or 'level 0..3')\n", diffName(difficulty));
    Serial.println("You are White. Enter moves like e2e4. 'new' / 'show' / 'level N'.");
    printBoard();
}

void loop() {
    checkButton();
    if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        handleLine(line);
    }
}

// When compiled for the host (no Arduino), provide a minimal main so the
// firmware can at least be type-checked. The real logic is exercised on the
// ESP32; the engine itself is covered by host_tests.cpp.
#ifdef UNIT_TEST
int main() { setup(); /* loop() not auto-run under test */ return 0; }
#endif
