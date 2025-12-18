/**
 * Configuration Constants for Pomodoro Timer
 * All display dimensions, colors, and feature flags
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <M5Dial.h>

// ==================== DISPLAY DIMENSIONS ====================
const int16_t SCREEN_WIDTH = 240;
const int16_t SCREEN_HEIGHT = 240;
const int16_t CENTER_X = SCREEN_WIDTH / 2;
const int16_t CENTER_Y = SCREEN_HEIGHT / 2;
const int16_t CIRCLE_RADIUS = 90;
const int16_t CIRCLE_THICKNESS = 8;

// ==================== FEATURE FLAGS ====================
// Set to true to show the white circle, false to hide it
const bool SHOW_WHITE_CIRCLE = false;

// ==================== COLOR DEFINITIONS ====================
const uint16_t COLOR_WORK = TFT_RED;
const uint16_t COLOR_BREAK = TFT_GREEN;
const uint16_t COLOR_LONG_BREAK = TFT_CYAN;
const uint16_t COLOR_BG = TFT_BLACK;
const uint16_t COLOR_TEXT = TFT_WHITE;
const uint16_t COLOR_PROGRESS_BG = 0x2104; // Dark gray
const uint16_t COLOR_WORK_BG = TFT_RED;    // Red background for pomodoro
const uint16_t COLOR_SHORT_BREAK_BG = TFT_DARKGREEN; // Dark green background for short break
const uint16_t COLOR_LONG_BREAK_BG = TFT_ORANGE; // Orange background for long break

#endif // CONFIG_H

