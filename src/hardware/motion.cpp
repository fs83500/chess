// =============================================================================
//  Hardware motion layer implementation (ESP32 / Arduino).
//  See motion.h for the assumed physical design.
// =============================================================================
#include "motion.h"

namespace hardware {

void Motion::begin() {
    pinMode(X_STEP_PIN, OUTPUT);
    pinMode(X_DIR_PIN, OUTPUT);
    pinMode(Y_STEP_PIN, OUTPUT);
    pinMode(Y_DIR_PIN, OUTPUT);
    pinMode(X_ENABLE_PIN, OUTPUT);
    pinMode(Y_ENABLE_PIN, OUTPUT);
    pinMode(MAGNET_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLDOWN);

    // Disable steppers until we move (saves power & heat).
    digitalWrite(X_ENABLE_PIN, HIGH);
    digitalWrite(Y_ENABLE_PIN, HIGH);
    magnetOff();
}

void Motion::magnetOn()  { digitalWrite(MAGNET_PIN, HIGH); }
void Motion::magnetOff() { digitalWrite(MAGNET_PIN, LOW); }

void Motion::stepAxis(int stepPin, int dirPin, int steps) {
    // steps < 0 => reverse direction.
    digitalWrite(dirPin, steps >= 0 ? HIGH : LOW);
    int n = steps < 0 ? -steps : steps;
    for (int i = 0; i < n; i++) {
        digitalWrite(stepPin, HIGH);
        delayMicroseconds(STEP_DELAY_US);
        digitalWrite(stepPin, LOW);
        delayMicroseconds(STEP_DELAY_US);
    }
}

void Motion::driveTo(int file, int rank) {
    int dx = (file - curFile_) * STEPS_PER_SQUARE;
    int dy = (rank - curRank_) * STEPS_PER_SQUARE;

    // Enable drivers while moving.
    digitalWrite(X_ENABLE_PIN, LOW);
    digitalWrite(Y_ENABLE_PIN, LOW);

    // Move diagonally-ish by interleaving steps. For robustness we move one
    // axis fully then the other (simpler timing; pieces stay on the board).
    if (dx != 0) stepAxis(X_STEP_PIN, X_DIR_PIN, dx);
    if (dy != 0) stepAxis(Y_STEP_PIN, Y_DIR_PIN, dy);

    digitalWrite(X_ENABLE_PIN, HIGH);
    digitalWrite(Y_ENABLE_PIN, HIGH);

    curFile_ = file;
    curRank_ = rank;
}

void Motion::moveTo(int file, int rank) {
    magnetOff();
    driveTo(file, rank);
}

void Motion::movePiece(int fromFile, int fromRank, int toFile, int toRank) {
    // 1. Go above the piece (magnet off -> no drag).
    driveTo(fromFile, fromRank);
    // 2. Engage the magnet to grab the piece (it snaps to the board from below).
    magnetOn();
    delay(120);
    // 3. Drag the piece to the destination.
    driveTo(toFile, toRank);
    // 4. Release.
    delay(80);
    magnetOff();
    delay(80);
}

void Motion::parkPiece(int fromFile, int fromRank, int pocketIndex) {
    // Parking pockets sit along the h-file edge, beyond rank 8.
    driveTo(fromFile, fromRank);
    magnetOn();
    delay(120);
    // Drive off the board edge to a pocket (file 8 = "column 8" = pocket area).
    int pocketFile = 8;
    int pocketRank = pocketIndex % 8;
    driveTo(pocketFile, pocketRank);
    delay(80);
    magnetOff();
    delay(80);
    // Return to a safe on-board square.
    driveTo(7, 7);
}

void Motion::home() {
    // Soft home: assume the board is aligned so that driving all the way to the
    // a1 corner (negative direction) hits a mechanical stop. For a real build,
    // wire endstops and read them here instead.
    digitalWrite(X_ENABLE_PIN, LOW);
    digitalWrite(Y_ENABLE_PIN, LOW);
    digitalWrite(X_DIR_PIN, LOW);
    digitalWrite(Y_DIR_PIN, LOW);
    const int homeSteps = STEPS_PER_SQUARE * 9; // overshoot to be sure
    for (int i = 0; i < homeSteps; i++) {
        digitalWrite(X_STEP_PIN, HIGH); delayMicroseconds(STEP_DELAY_US);
        digitalWrite(X_STEP_PIN, LOW);  delayMicroseconds(STEP_DELAY_US);
        digitalWrite(Y_STEP_PIN, HIGH); delayMicroseconds(STEP_DELAY_US);
        digitalWrite(Y_STEP_PIN, LOW);  delayMicroseconds(STEP_DELAY_US);
    }
    digitalWrite(X_ENABLE_PIN, HIGH);
    digitalWrite(Y_ENABLE_PIN, HIGH);
    curFile_ = 0;
    curRank_ = 0;
}

} // namespace hardware
