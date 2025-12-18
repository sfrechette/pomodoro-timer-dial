/**
 * Common Type Definitions
 * Shared types for Pomodoro Timer
 */

#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

// Pomodoro Timer States
enum TimerState {
    STATE_IDLE,
    STATE_RUNNING,
    STATE_PAUSED,
    STATE_SHORT_BREAK,
    STATE_LONG_BREAK,
    STATE_SETTINGS
};

// Settings Structure
struct PomodoroSettings {
    uint16_t workDuration;           // Work duration in seconds (default: 25 min)
    uint16_t shortBreakDuration;     // Short break duration in seconds (default: 5 min)
    uint16_t longBreakDuration;      // Long break duration in seconds (default: 25 min)
    uint8_t pomodorosUntilLongBreak; // Number of pomodoros before long break (default: 4)
};

#endif // TYPES_H

