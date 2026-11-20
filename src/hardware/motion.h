#pragma once
// =============================================================================
//  Hardware abstraction for the automated chess board.
//
//  Physical design assumed by this layer:
//    * Each square has a reed switch to detect a piece (optional, simple
//      polling here) -- used to confirm moves and detect the human's move.
//    * Pieces are moved by an electromagnet travelling under a ferromagnetic
//      board. The magnet rides on a 2-axis (X/Y) gantry driven by two stepper
//      motors. Raising/lowering the magnet (or toggling its coil) lets us
//      "grab" and "release" a piece. This is the same principle as the
//      "Square Off" style boards.
//    * Captured pieces are dragged to a parking pocket on the side.
//
//  The pinout is configurable below. Everything here is non-blocking-friendly
//  but, for clarity, the motion primitives block until the move finishes.
// =============================================================================

#include <Arduino.h>

namespace hardware {

// ---- Pin assignments (edit to match your wiring) ---------------------------
// X-axis stepper (files a..h -> columns 0..7)
constexpr int X_STEP_PIN   = 14;
constexpr int X_DIR_PIN    = 27;
constexpr int X_ENABLE_PIN = 26;
// Y-axis stepper (ranks 1..8 -> rows 0..7)
constexpr int Y_STEP_PIN   = 25;
constexpr int Y_DIR_PIN    = 33;
constexpr int Y_ENABLE_PIN = 32;
// Electromagnet (grab/release a piece). Active HIGH.
constexpr int MAGNET_PIN   = 13;
// A push-button to cycle difficulty at boot / during play.
constexpr int BUTTON_PIN   = 34;  // input-only GPIO on ESP32

// ---- Motion tuning ----------------------------------------------------------
constexpr int   STEPS_PER_SQUARE = 200;   // full steps to move one square
constexpr int   STEP_DELAY_US    = 900;   // lower = faster, mind torque
constexpr float SQ_SIDE_MM       = 57.0f; // standard tournament square

class Motion {
public:
    void begin();

    // Move the magnet (empty) to a board square: file 0..7, rank 0..7.
    void moveTo(int file, int rank);

    // Grab the piece currently under the magnet, move to (file,rank), release.
    void movePiece(int fromFile, int fromRank, int toFile, int toRank);

    // Drag a captured piece off the board to a parking pocket.
    void parkPiece(int fromFile, int fromRank, int pocketIndex);

    // Home the gantry (call at boot). Uses endstop-free soft home: drives to
    // a known corner assuming the board was placed in a fixed position.
    void home();

    // Magnet control.
    void magnetOn();
    void magnetOff();

    // Where is the magnet now?
    int currentFile() const { return curFile_; }
    int currentRank() const { return curRank_; }

private:
    void stepAxis(int stepPin, int dirPin, int steps);
    void driveTo(int file, int rank);

    int curFile_ = 0;
    int curRank_ = 0;
};

} // namespace hardware
