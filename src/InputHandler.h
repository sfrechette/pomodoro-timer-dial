/**
 * Input Handler Module
 * Handles encoder, button, and touch input
 */

#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <Arduino.h>
#include <M5Dial.h>
#include "config.h"
#include "types.h"

class InputHandler {
public:
    // Constructor
    InputHandler();
    
    // Initialize input handler
    void init();
    
    // Main input processing function (call from loop)
    void processInput(TimerState& currentState, 
                     PomodoroSettings& settings,
                     uint8_t& settingsMenuIndex,
                     bool& settingsEditing,
                     uint32_t& timerRemaining,
                     uint32_t& timerDuration,
                     bool& needsRedraw,
                     void (*startTimerCallback)(uint32_t),
                     void (*pauseTimerCallback)(),
                     void (*resumeTimerCallback)(),
                     void (*resetTimerCallback)());
    
private:
    // Input state tracking
    long lastEncoderPos;
    uint32_t buttonPressTime;
    bool buttonPressed;
    bool longPressHandled;
    
    // Performance optimization - encoder debouncing
    uint32_t lastEncoderChangeTime;
    static constexpr uint32_t ENCODER_DEBOUNCE_MS = 10;  // Balanced for smoothness and responsiveness
    static constexpr int32_t ENCODER_THRESHOLD = 1;       // Minimum encoder delta to process
    
    // Internal handlers
    void handleEncoderInput(TimerState& currentState,
                           PomodoroSettings& settings,
                           uint8_t& settingsMenuIndex,
                           bool& settingsEditing,
                           uint32_t& timerRemaining,
                           uint32_t& timerDuration,
                           bool& needsRedraw);
    
    void handleButtonInput(TimerState& currentState,
                          PomodoroSettings& settings,
                          uint8_t& settingsMenuIndex,
                          bool& settingsEditing,
                          bool& needsRedraw,
                          void (*startTimerCallback)(uint32_t),
                          void (*pauseTimerCallback)(),
                          void (*resumeTimerCallback)(),
                          void (*resetTimerCallback)());
    
    void handleTouchInput(TimerState& currentState,
                         uint8_t& settingsMenuIndex,
                         bool& settingsEditing,
                         bool& needsRedraw);
    
    void handleButtonPress(TimerState& currentState,
                          PomodoroSettings& settings,
                          uint8_t& settingsMenuIndex,
                          bool& settingsEditing,
                          bool& needsRedraw,
                          void (*startTimerCallback)(uint32_t),
                          void (*pauseTimerCallback)(),
                          void (*resumeTimerCallback)(),
                          void (*resetTimerCallback)());
};

#endif // INPUT_HANDLER_H

