/**
 * Input Handler Implementation
 * All input handling code moved here from main.cpp
 * Behavior remains exactly the same
 */

#include "InputHandler.h"

InputHandler::InputHandler() 
    : lastEncoderPos(0),
      buttonPressTime(0),
      buttonPressed(false),
      longPressHandled(false),
      lastEncoderChangeTime(0) {
}

void InputHandler::init() {
    // Initialize encoder position
    lastEncoderPos = M5Dial.Encoder.read();
}

void InputHandler::processInput(TimerState& currentState,
                                PomodoroSettings& settings,
                                uint8_t& settingsMenuIndex,
                                bool& settingsEditing,
                                uint32_t& timerRemaining,
                                uint32_t& timerDuration,
                                bool& needsRedraw,
                                void (*startTimerCallback)(uint32_t),
                                void (*pauseTimerCallback)(),
                                void (*resumeTimerCallback)(),
                                void (*resetTimerCallback)()) {
    // Handle encoder input
    handleEncoderInput(currentState, settings, settingsMenuIndex, settingsEditing,
                      timerRemaining, timerDuration, needsRedraw);
    
    // Handle button input with callbacks
    handleButtonInput(currentState, settings, settingsMenuIndex, settingsEditing, needsRedraw,
                     startTimerCallback, pauseTimerCallback, resumeTimerCallback, resetTimerCallback);
    
    // Handle touch input
    handleTouchInput(currentState, settingsMenuIndex, settingsEditing, needsRedraw);
}

void InputHandler::handleEncoderInput(TimerState& currentState,
                                     PomodoroSettings& settings,
                                     uint8_t& settingsMenuIndex,
                                     bool& settingsEditing,
                                     uint32_t& timerRemaining,
                                     uint32_t& timerDuration,
                                     bool& needsRedraw) {
    long currentPos = M5Dial.Encoder.read();
    long delta = currentPos - lastEncoderPos;
    
    // Performance optimization: Ignore small changes and debounce
    if (abs(delta) < ENCODER_THRESHOLD) return;
    
    uint32_t now = millis();
    if (now - lastEncoderChangeTime < ENCODER_DEBOUNCE_MS) return;
    
    lastEncoderPos = currentPos;
    lastEncoderChangeTime = now;
    needsRedraw = true; // Mark that we need to redraw
    
    if (currentState == STATE_SETTINGS) {
        if (settingsEditing) {
            // Adjust current setting value
            if (settingsMenuIndex == 0) {
                // Work Duration (adjust by 60 seconds)
                int32_t newVal = settings.workDuration + (delta * 60);
                if (newVal < 60) newVal = 60;      // Minimum 1 minute
                if (newVal > 3600) newVal = 3600;  // Maximum 60 minutes
                settings.workDuration = newVal;
            } else if (settingsMenuIndex == 1) {
                // Short Break Duration (adjust by 60 seconds)
                int32_t newVal = settings.shortBreakDuration + (delta * 60);
                if (newVal < 60) newVal = 60;      // Minimum 1 minute
                if (newVal > 1800) newVal = 1800;  // Maximum 30 minutes
                settings.shortBreakDuration = newVal;
            } else if (settingsMenuIndex == 2) {
                // Long Break Duration (adjust by 60 seconds)
                int32_t newVal = settings.longBreakDuration + (delta * 60);
                if (newVal < 60) newVal = 60;      // Minimum 1 minute
                if (newVal > 3600) newVal = 3600;  // Maximum 60 minutes
                settings.longBreakDuration = newVal;
            } else if (settingsMenuIndex == 3) {
                // Pomodoros until long break (1-10 range)
                int16_t newVal = settings.pomodorosUntilLongBreak + delta;
                if (newVal < 1) newVal = 1;
                if (newVal > 10) newVal = 10;
                settings.pomodorosUntilLongBreak = newVal;
            }
        } else {
            // Navigate menu
            if (delta > 0) {
                settingsMenuIndex = (settingsMenuIndex + 1) % 5;
            } else {
                settingsMenuIndex = (settingsMenuIndex + 4) % 5;
            }
        }
    } else if (currentState == STATE_IDLE) {
        // In idle state, encoder adjusts pomodoro time (1-25 minutes)
        uint16_t currentMinutes = settings.workDuration / 60;
        int16_t newMinutes = currentMinutes + delta;
        
        // Clamp between 1 and 25 minutes
        if (newMinutes < 1) newMinutes = 1;
        if (newMinutes > 25) newMinutes = 25;
        
        // Only update if value actually changed
        if (newMinutes != currentMinutes) {
            settings.workDuration = newMinutes * 60;
            timerRemaining = settings.workDuration;
            timerDuration = settings.workDuration;
            
            // When dial is used, automatically calculate breaks using 1/5 rule
            settings.shortBreakDuration = settings.workDuration / 5;
            settings.longBreakDuration = settings.workDuration;
            
            // Play click sound when adjusting time
            M5Dial.Speaker.tone(800, 30); // Short click sound (800 Hz, 30ms)
        }
    }
}

void InputHandler::handleButtonInput(TimerState& currentState,
                                    PomodoroSettings& settings,
                                    uint8_t& settingsMenuIndex,
                                    bool& settingsEditing,
                                    bool& needsRedraw,
                                    void (*startTimerCallback)(uint32_t),
                                    void (*pauseTimerCallback)(),
                                    void (*resumeTimerCallback)(),
                                    void (*resetTimerCallback)()) {
    if (M5Dial.BtnA.isPressed()) {
        if (!buttonPressed) {
            buttonPressed = true;
            longPressHandled = false;
            buttonPressTime = millis();
        } else {
            uint32_t pressDuration = millis() - buttonPressTime;
            
            // Check for long press (2+ seconds) = Reset to ready
            if (pressDuration > 2000 && !longPressHandled) {
                longPressHandled = true;
                if (currentState == STATE_SETTINGS) {
                    // In settings, long press does nothing
                    Serial.println("In Settings - use Back to exit");
                } else if (currentState == STATE_RUNNING || currentState == STATE_PAUSED || 
                           currentState == STATE_SHORT_BREAK || currentState == STATE_LONG_BREAK) {
                    // Long press when timer is active = reset to ready state
                    if (resetTimerCallback) {
                        resetTimerCallback();
                    }
                    needsRedraw = true;
                    Serial.println("Reset to Ready (2s+ press)");
                } else if (currentState == STATE_IDLE) {
                    // Long press when already idle = do nothing
                    Serial.println("Already in Ready state");
                }
            }
        }
    } else {
        if (buttonPressed) {
            buttonPressed = false;
            uint32_t pressDuration = millis() - buttonPressTime;
            // Only handle short press if long press was NOT handled
            if (pressDuration < 2000 && !longPressHandled) {
                // Short press (< 2 seconds) = normal actions
                handleButtonPress(currentState, settings, settingsMenuIndex, settingsEditing,
                                needsRedraw, startTimerCallback, pauseTimerCallback, 
                                resumeTimerCallback, resetTimerCallback);
            }
        }
    }
}

void InputHandler::handleTouchInput(TimerState& currentState,
                                   uint8_t& settingsMenuIndex,
                                   bool& settingsEditing,
                                   bool& needsRedraw) {
    auto touch = M5Dial.Touch.getDetail();
    if (touch.wasPressed()) {
        int16_t touchX = touch.x;
        int16_t touchY = touch.y;
        
        // Check if touch is in the gear icon area (bottom center)
        // Increased touch area for better responsiveness: 40x40 pixel area
        // Gear is at bottom: x = CENTER_X-20 to CENTER_X+20, y = SCREEN_HEIGHT-45 to SCREEN_HEIGHT
        if (touchX >= CENTER_X - 20 && touchX <= CENTER_X + 20 &&
            touchY >= SCREEN_HEIGHT - 45 && touchY <= SCREEN_HEIGHT) {
            // Touch on gear icon = open settings
            if (currentState != STATE_SETTINGS) {
                currentState = STATE_SETTINGS;
                settingsMenuIndex = 0;
                settingsEditing = false;
                needsRedraw = true;
                Serial.println("Opening Settings (gear icon touched)");
            }
        }
    }
}

void InputHandler::handleButtonPress(TimerState& currentState,
                                    PomodoroSettings& settings,
                                    uint8_t& settingsMenuIndex,
                                    bool& settingsEditing,
                                    bool& needsRedraw,
                                    void (*startTimerCallback)(uint32_t),
                                    void (*pauseTimerCallback)(),
                                    void (*resumeTimerCallback)(),
                                    void (*resetTimerCallback)()) {
    needsRedraw = true; // Mark that we need to redraw
    
    switch (currentState) {
        case STATE_IDLE:
            // Start work timer
            if (startTimerCallback) {
                startTimerCallback(settings.workDuration);
            }
            break;
            
        case STATE_RUNNING:
        case STATE_SHORT_BREAK:
        case STATE_LONG_BREAK:
            // Pause timer
            if (pauseTimerCallback) {
                pauseTimerCallback();
            }
            break;
            
        case STATE_PAUSED:
            // Resume timer
            if (resumeTimerCallback) {
                resumeTimerCallback();
            }
            break;
            
        case STATE_SETTINGS:
            if (settingsMenuIndex == 4) {
                // Back to main screen - force full screen clear to prevent overlap
                uint16_t bgColor = COLOR_WORK_BG; // Red background for idle
                M5Dial.Display.fillScreen(bgColor);
                currentState = STATE_IDLE;
                if (resetTimerCallback) {
                    resetTimerCallback();
                }
                needsRedraw = true;
                Serial.println("Exiting Settings -> Idle");
            } else if (settingsMenuIndex >= 0 && settingsMenuIndex <= 3) {
                // Allow editing all settings: Work Duration, Short Break, Long Break, and Pomodoros/Long
                settingsEditing = !settingsEditing;
            }
            break;
    }
}
