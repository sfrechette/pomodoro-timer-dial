/**
 * Display Module Implementation
 * All drawing functions moved here from main.cpp
 * UI/visuals remain exactly the same
 */

#include "Display.h"

Display::Display() {
    // Constructor - nothing to initialize
}

void Display::drawCircularProgress(float progress, uint16_t color, TimerState state) {
    // Draw a static full white circle ring (no progress updates)
    uint16_t bgColor = getStateBackgroundColor(state, state);
    int16_t outerRadius = CIRCLE_RADIUS + CIRCLE_THICKNESS/2;
    int16_t innerRadius = CIRCLE_RADIUS - CIRCLE_THICKNESS/2;
    
    // Draw a full solid white ring by filling the outer circle and clearing the inner circle
    // First, draw filled outer circle
    M5Dial.Display.fillCircle(CENTER_X, CENTER_Y, outerRadius, color);
    // Then, clear the inner circle to create the ring
    M5Dial.Display.fillCircle(CENTER_X, CENTER_Y, innerRadius, bgColor);
}

void Display::drawTimerDisplay(uint32_t seconds, uint16_t color, TimerState state,
                               uint32_t duration, uint32_t remaining,
                               TimerState lastState, float& lastProgress) {
    // Calculate progress
    float progress = duration > 0 ? 1.0 - ((float)remaining / (float)duration) : 0.0;
    
    // Get background color based on state
    uint16_t bgColor = getStateBackgroundColor(state, state);
    
    // Redraw everything if state changed, or on first draw
    bool fullRedraw = (lastState != state) || (lastProgress < 0);
    
    if (fullRedraw) {
        // Full screen clear with state background color
        M5Dial.Display.fillScreen(bgColor);
        // Draw white circle only if enabled
        if (SHOW_WHITE_CIRCLE) {
            drawCircularProgress(progress, COLOR_TEXT, state); // Draw static white circle
        }
        // Draw tomato icon after screen clear
        drawTomatoIcon(state);
        lastProgress = progress;
    }
    // Circle is static - no need to update it based on progress changes
    
    // Always redraw time text in white (it changes every second, or when adjusting)
    // Position timer in the exact center of the circle
    M5Dial.Display.setTextColor(COLOR_TEXT);
    M5Dial.Display.setTextDatum(middle_center);
    M5Dial.Display.setTextSize(5); // Bigger font size (was 4)
    // Clear area behind text first with state background - centered
    int16_t timerY = CENTER_Y; // Exact center of circle
    M5Dial.Display.fillRect(CENTER_X - 80, timerY - 25, 160, 45, bgColor); // Larger clear area for bigger text
    M5Dial.Display.drawString(formatTime(seconds).c_str(), CENTER_X, timerY);
    
    // Draw status text inside the circle, below the timer
    const char* statusText = "";
    if (state == STATE_IDLE) {
        statusText = "Ready";
    } else if (state == STATE_PAUSED) {
        statusText = "Paused";
    } else if (state == STATE_RUNNING) {
        statusText = "Focusing";
    } else if (state == STATE_SHORT_BREAK) {
        statusText = "Short Break";
    } else {
        statusText = "Long Break";
    }
    
    // Clear area for status text inside circle - positioned below centered timer
    M5Dial.Display.fillRect(CENTER_X - 60, CENTER_Y + 30, 120, 20, bgColor);
    M5Dial.Display.setTextColor(TFT_WHITE);
    M5Dial.Display.setTextDatum(middle_center);
    M5Dial.Display.setTextSize(2); // Bigger text size
    M5Dial.Display.drawString(statusText, CENTER_X, CENTER_Y + 40); // Positioned lower (was +35)
    
    // No moving indicator dot - static circle only
}

void Display::drawStatusText(const char* text, uint16_t color, TimerState state, TimerState lastState) {
    // Status text is now drawn inside the circle in drawTimerDisplay
    // This function draws the instructions and settings gear at the bottom
    
    // Get background color based on state
    uint16_t bgColor = getStateBackgroundColor(state, state);
    
    // Draw instructions at bottom (moved higher to avoid gear icon)
    int16_t instructionY = SCREEN_HEIGHT - 48;
    M5Dial.Display.fillRect(0, instructionY - 10, SCREEN_WIDTH, 20, bgColor);
    M5Dial.Display.setTextColor(COLOR_TEXT);
    M5Dial.Display.setTextSize(1);
    const char* instruction = "";
    if (state == STATE_IDLE) {
        instruction = "Press: Start | Hold: Reset";
    } else if (state == STATE_PAUSED) {
        instruction = "Press: Resume | Hold: Reset";
    } else {
        instruction = "Press: Pause | Hold: Reset";
    }
    M5Dial.Display.drawString(instruction, CENTER_X, instructionY);
    
    // Draw settings gear icon at bottom center (only when not in settings)
    // Simple approach: always draw when state changed from what was displayed last frame
    if (state != STATE_SETTINGS && lastState != state) {
        Serial.println(">>> Drawing gear icon (state changed)!");
        int16_t iconY = SCREEN_HEIGHT - 20;
        int16_t iconSize = 24;
        int16_t iconX = CENTER_X - iconSize/2;
        int16_t iconYPos = iconY - iconSize/2;
        
        // Clear area for icon
        M5Dial.Display.fillRect(CENTER_X - 15, iconY - 15, 30, 30, bgColor);
        
        // Load and draw gear icon from SPIFFS
        File file = SPIFFS.open("/gear.png", "r");
        if (file) {
            file.seek(0);
            bool result = M5Dial.Display.drawPng(&file, iconX, iconYPos);
            if (!result) {
                Serial.println("PNG draw failed - using fallback");
                M5Dial.Display.setTextColor(TFT_WHITE);
                M5Dial.Display.setTextDatum(middle_center);
                M5Dial.Display.setTextSize(2);
                M5Dial.Display.drawString("\xE2\x9A\x99", CENTER_X, iconY);
            } else {
                Serial.println("Gear PNG drawn successfully");
            }
            file.close();
        } else {
            Serial.println("Failed to open gear.png - using fallback");
            M5Dial.Display.setTextColor(TFT_WHITE);
            M5Dial.Display.setTextDatum(middle_center);
            M5Dial.Display.setTextSize(2);
            M5Dial.Display.drawString("\xE2\x9A\x99", CENTER_X, iconY);
        }
    }
}

void Display::drawCurvedText(const char* text, int16_t centerX, int16_t centerY, int16_t radius, float startAngle, uint16_t color) {
    // Draw text along a circular arc
    M5Dial.Display.setTextColor(color);
    M5Dial.Display.setTextSize(1);
    
    int len = strlen(text);
    float angleStep = (2.0 * PI) / len; // Distribute characters evenly around the arc
    
    for (int i = 0; i < len; i++) {
        float angle = startAngle + (i * angleStep);
        int16_t x = centerX + (int16_t)(radius * cos(angle));
        int16_t y = centerY - (int16_t)(radius * sin(angle));
        
        // Draw character at this position
        char ch[2] = {text[i], '\0'};
        M5Dial.Display.drawString(ch, x, y);
    }
}

void Display::drawPomodoroCounter(uint8_t completedPomodoros, TimerState state) {
    // Get background color based on state
    uint16_t bgColor = getStateBackgroundColor(state, state);
    
    // Clear area at the top (use state background)
    M5Dial.Display.fillRect(0, 0, SCREEN_WIDTH, 35, bgColor);
    
    // Draw pomodoro count text at the top center (slightly lower)
    char pomoText[25];
    snprintf(pomoText, sizeof(pomoText), "Pomodoros: %d", completedPomodoros);
    
    // Draw text at the top center - simple and visible
    M5Dial.Display.setTextColor(COLOR_TEXT);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.setTextDatum(middle_center);
    M5Dial.Display.drawString(pomoText, CENTER_X, 20); // Lowered from 15 to 20
}

void Display::drawTomatoIcon(TimerState state) {
    // Draw tomato icon between pomodoro counter (y=20) and timer (y=120)
    // Position it slightly higher: around y=60
    int16_t iconY = 60; // Slightly higher than middle (was 70)
    int16_t iconSize = 32; // PNG is 32x32
    int16_t iconX = CENTER_X - iconSize/2;
    int16_t iconYPos = iconY - iconSize/2;
    
    // Use M5GFX's drawPng to load PNG from SPIFFS with transparency support
    // Position centered horizontally
    File file = SPIFFS.open("/pomodoro.png", "r");
    if (file) {
        Serial.print("Opening pomodoro.png, size: ");
        Serial.println(file.size());
        Serial.print("Drawing at position: ");
        Serial.print(iconX);
        Serial.print(", ");
        Serial.println(iconYPos);
        // Reset file pointer to beginning
        file.seek(0);
        // M5GFX drawPng automatically handles transparency from PNG alpha channel
        bool result = M5Dial.Display.drawPng(&file, iconX, iconYPos);
        if (!result) {
            Serial.println("Failed to draw PNG");
            // Draw a red rectangle as fallback to verify position
            M5Dial.Display.fillRect(iconX, iconYPos, iconSize, iconSize, TFT_RED);
        } else {
            Serial.println("PNG drawn successfully");
        }
        file.close();
    } else {
        Serial.println("Failed to open /pomodoro.png from SPIFFS");
        // Draw a simple placeholder rectangle to verify position
        uint16_t bgColor = getStateBackgroundColor(state, state);
        M5Dial.Display.fillRect(iconX, iconYPos, iconSize, iconSize, TFT_RED);
    }
}

void Display::drawSettingsMenu(const PomodoroSettings& settings, uint8_t menuIndex,
                               bool editing, TimerState lastState) {
    // Clear screen if we just entered settings (transitioning from another state)
    if (lastState != STATE_SETTINGS) {
        M5Dial.Display.fillScreen(COLOR_BG);
        Serial.println("Clearing screen for Settings entry");
    }
    
    M5Dial.Display.setTextColor(COLOR_TEXT);
    M5Dial.Display.setTextDatum(top_center);
    M5Dial.Display.setTextSize(2);
    M5Dial.Display.drawString("Settings", CENTER_X, 10);
    
    M5Dial.Display.setTextSize(1);
    int16_t yPos = 50;
    
    // Clear the menu area before redrawing to remove old highlights
    M5Dial.Display.fillRect(0, 40, SCREEN_WIDTH, 140, COLOR_BG);
    
    const char* menuItems[] = {
        "Work Duration",
        "Short Break",
        "Long Break",
        "Pomodoros/Long",
        "Back"
    };
    
    for (uint8_t i = 0; i < 5; i++) {
        // Clear the specific line area first
        M5Dial.Display.fillRect(10, yPos - 2, SCREEN_WIDTH - 20, 18, COLOR_BG);
        
        // Draw highlight only for selected item
        if (i == menuIndex) {
            M5Dial.Display.fillRect(10, yPos - 2, SCREEN_WIDTH - 20, 18, COLOR_PROGRESS_BG);
            M5Dial.Display.setTextColor(COLOR_WORK);
        } else {
            M5Dial.Display.setTextColor(COLOR_TEXT);
        }
        
        char line[50];
        if (i == 0) {
            // Work Duration - editable
            snprintf(line, sizeof(line), "%s: %s", menuItems[i], formatTime(settings.workDuration).c_str());
        } else if (i == 1) {
            // Short Break - editable
            snprintf(line, sizeof(line), "%s: %s", menuItems[i], formatTime(settings.shortBreakDuration).c_str());
        } else if (i == 2) {
            // Long Break - editable
            snprintf(line, sizeof(line), "%s: %s", menuItems[i], formatTime(settings.longBreakDuration).c_str());
        } else if (i == 3) {
            // Pomodoros until long break - editable
            snprintf(line, sizeof(line), "%s: %d", menuItems[i], settings.pomodorosUntilLongBreak);
        } else {
            // Back
            snprintf(line, sizeof(line), "%s", menuItems[i]);
        }
        
        M5Dial.Display.drawString(line, CENTER_X, yPos);
        yPos += 25;
    }
    
    // Instructions (clear area first) - moved higher to be fully visible
    M5Dial.Display.fillRect(0, SCREEN_HEIGHT - 45, SCREEN_WIDTH, 45, COLOR_BG);
    M5Dial.Display.setTextColor(COLOR_TEXT);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("Dial: Navigate/Adjust", CENTER_X, SCREEN_HEIGHT - 35);
    M5Dial.Display.drawString("Press: Select/Edit", CENTER_X, SCREEN_HEIGHT - 20);
}

String Display::formatTime(uint32_t seconds) {
    uint32_t minutes = seconds / 60;
    uint32_t secs = seconds % 60;
    
    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%02lu:%02lu", minutes, secs);
    return String(buffer);
}

uint16_t Display::getStateColor(TimerState state) {
    switch (state) {
        case STATE_RUNNING:
            return COLOR_WORK;
        case STATE_SHORT_BREAK:
            return COLOR_BREAK;
        case STATE_LONG_BREAK:
            return COLOR_LONG_BREAK;
        case STATE_PAUSED:
            return 0x7BEF; // Yellow
        default:
            return COLOR_TEXT;
    }
}

uint16_t Display::getStateBackgroundColor(TimerState state, TimerState stateBeforePause) {
    switch (state) {
        case STATE_RUNNING:
            return COLOR_WORK_BG; // Red background
        case STATE_SHORT_BREAK:
            return COLOR_SHORT_BREAK_BG; // Green background
        case STATE_LONG_BREAK:
            return COLOR_LONG_BREAK_BG; // Orange background for long break
        case STATE_PAUSED:
            // When paused, keep the color of the state that was paused
            // Use stateBeforePause to determine the background color
            switch (stateBeforePause) {
                case STATE_RUNNING:
                    return COLOR_WORK_BG; // Red background
                case STATE_SHORT_BREAK:
                    return COLOR_SHORT_BREAK_BG; // Green background
                case STATE_LONG_BREAK:
                    return COLOR_LONG_BREAK_BG; // Orange background
                default:
                    return COLOR_BG; // Default to black
            }
        default:
            return COLOR_WORK_BG; // Red background for idle (default)
    }
}

