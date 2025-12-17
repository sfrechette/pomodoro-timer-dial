#include <Arduino.h>
#include <M5Dial.h>
#include <math.h>
#include <string.h>
#include <SPIFFS.h>

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

// Global Variables
TimerState currentState = STATE_IDLE;
PomodoroSettings settings = {
    .workDuration = 25 * 60,           // 25 minutes
    .shortBreakDuration = 5 * 60,      // 5 minutes
    .longBreakDuration = 25 * 60,      // 25 minutes
    .pomodorosUntilLongBreak = 4
};

uint32_t timerStartTime = 0;
uint32_t timerRemaining = 0;
uint32_t timerDuration = 0;
uint32_t lastPomodoroDuration = 0; // Store the last pomodoro duration for auto-restart
TimerState stateBeforePause = STATE_IDLE; // Store the state before pausing
uint8_t completedPomodoros = 0;
bool timerCompleted = false; // Flag to track if timer has completed (to ensure 00:00 is shown)
uint32_t timerCompletionTime = 0; // Timestamp when timer reached 0
uint8_t beepState = 0; // 0 = waiting, 1-9 = beeping sequence (global to allow reset)
uint32_t lastBeepTime = 0; // Last beep timestamp (global to allow reset)
uint8_t settingsMenuIndex = 0;
bool settingsEditing = false;
long lastEncoderPos = 0;
uint32_t buttonPressTime = 0;
bool buttonPressed = false;
bool longPressHandled = false; // Track if long press was already handled
bool needsRedraw = true;
uint32_t lastDisplayedSeconds = 0;
TimerState lastDisplayedState = STATE_SETTINGS; // Initialize to different state to force first draw
float lastDisplayedProgress = -1.0;

// Display dimensions
const int16_t SCREEN_WIDTH = 240;
const int16_t SCREEN_HEIGHT = 240;
const int16_t CENTER_X = SCREEN_WIDTH / 2;
const int16_t CENTER_Y = SCREEN_HEIGHT / 2;
const int16_t CIRCLE_RADIUS = 90;
const int16_t CIRCLE_THICKNESS = 8;

// Feature flag: Set to true to show the white circle, false to hide it
const bool SHOW_WHITE_CIRCLE = false;

// Color definitions
const uint16_t COLOR_WORK = TFT_RED;
const uint16_t COLOR_BREAK = TFT_GREEN;
const uint16_t COLOR_LONG_BREAK = TFT_CYAN;
const uint16_t COLOR_BG = TFT_BLACK;
const uint16_t COLOR_TEXT = TFT_WHITE;
const uint16_t COLOR_PROGRESS_BG = 0x2104; // Dark gray
const uint16_t COLOR_WORK_BG = TFT_RED;    // Red background for pomodoro
const uint16_t COLOR_SHORT_BREAK_BG = TFT_DARKGREEN; // Dark green background for short break
const uint16_t COLOR_LONG_BREAK_BG = TFT_ORANGE; // Orange background for long break

// Function prototypes
void drawCircularProgress(float progress, uint16_t color);
void drawTimerDisplay(uint32_t seconds, uint16_t color);
void drawStatusText(const char* text, uint16_t color);
void drawPomodoroCounter();
void drawTomatoIcon();
void drawSettingsMenu();
void updateTimer();
void handleTimerCompletion();
void handleEncoderInput();
void handleButtonPress();
void startTimer(uint32_t duration);
void pauseTimer();
void resumeTimer();
void resetTimer();
void completeSession();
String formatTime(uint32_t seconds);
uint16_t getStateColor();
uint16_t getStateBackgroundColor();
uint16_t getShortBreakDuration();
uint16_t getLongBreakDuration();

void setup() {
    Serial.begin(115200);
    delay(1000); // Wait for serial to initialize
    Serial.println("\n\n╔═══════════════════════════════════════════╗");
    Serial.println("║   POMODORO TIMER STARTING UP              ║");
    Serial.println("╚═══════════════════════════════════════════╝\n");
    
    auto cfg = M5.config();
    M5Dial.begin(cfg, true, true);
    
    // Initialize SPIFFS for image loading
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
    } else {
        Serial.println("SPIFFS Mounted Successfully");
        // List files in SPIFFS for debugging
        File root = SPIFFS.open("/");
        File file = root.openNextFile();
        Serial.println("Files in SPIFFS:");
        while(file){
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
            file = root.openNextFile();
        }
    }
    
    M5Dial.Display.setBrightness(100);
    M5Dial.Display.setRotation(0);
    M5Dial.Display.fillScreen(COLOR_WORK_BG); // Start with red background
    
    // Initialize encoder position
    lastEncoderPos = M5Dial.Encoder.read();
    
    // Draw initial screen
    needsRedraw = true;
    resetTimer();
}

void loop() {
    M5Dial.update();
    
    // Handle encoder input
    handleEncoderInput();
    
    // Handle button press - simplified to two actions only
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
                    resetTimer();
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
                handleButtonPress();
            }
        }
    }
    
    // Handle touch input for settings star icon at bottom
    auto touch = M5Dial.Touch.getDetail();
    if (touch.wasPressed()) {
        int16_t touchX = touch.x;
        int16_t touchY = touch.y;
        
        // Check if touch is in the gear icon area (bottom center)
        // Gear is at bottom: x = CENTER_X-15 to CENTER_X+15, y = SCREEN_HEIGHT-35 to SCREEN_HEIGHT-5
        if (touchX >= CENTER_X - 15 && touchX <= CENTER_X + 15 &&
            touchY >= SCREEN_HEIGHT - 35 && touchY <= SCREEN_HEIGHT - 5) {
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
    
    // Update timer if running
    if (currentState == STATE_RUNNING || currentState == STATE_SHORT_BREAK || currentState == STATE_LONG_BREAK) {
        updateTimer();
        handleTimerCompletion(); // Handle beep and switch after 00:00 is shown
    }
    
    // Check if we need to redraw (only when something changes)
    bool shouldRedraw = needsRedraw;
    if (currentState == STATE_RUNNING || currentState == STATE_SHORT_BREAK || currentState == STATE_LONG_BREAK) {
        // Update every second for timer, or immediately if state changed
        if (timerRemaining != lastDisplayedSeconds || currentState != lastDisplayedState) {
            shouldRedraw = true;
        }
    } else if (currentState == STATE_IDLE) {
        // In idle, redraw if state changed or if timer duration changed (from dial adjustment)
        if (currentState != lastDisplayedState || timerRemaining != lastDisplayedSeconds) {
            shouldRedraw = true;
        }
    } else if (currentState != lastDisplayedState) {
        shouldRedraw = true;
    }
    
    // Always redraw status text when state changes
    if (currentState != lastDisplayedState) {
        shouldRedraw = true;
    }
    
    // Redraw display when needed
    if (shouldRedraw) {
        switch (currentState) {
            case STATE_IDLE:
            case STATE_RUNNING:
            case STATE_PAUSED:
            case STATE_SHORT_BREAK:
            case STATE_LONG_BREAK:
                drawTimerDisplay(timerRemaining, getStateColor());
                drawStatusText(
                    currentState == STATE_IDLE ? "Ready" :
                    currentState == STATE_PAUSED ? "Paused" :
                    currentState == STATE_RUNNING ? "Focusing" :
                    currentState == STATE_SHORT_BREAK ? "Short Break" :
                    "Long Break",
                    getStateColor()
                );
                drawPomodoroCounter();
                drawTomatoIcon();
                break;
            case STATE_SETTINGS:
                drawSettingsMenu();
                break;
        }
        lastDisplayedSeconds = timerRemaining;
        lastDisplayedState = currentState;
        needsRedraw = false;
    }
    
    delay(10);
}

void drawCircularProgress(float progress, uint16_t color) {
    // Draw a static full white circle ring (no progress updates)
    uint16_t bgColor = getStateBackgroundColor();
    int16_t outerRadius = CIRCLE_RADIUS + CIRCLE_THICKNESS/2;
    int16_t innerRadius = CIRCLE_RADIUS - CIRCLE_THICKNESS/2;
    
    // Draw a full solid white ring by filling the outer circle and clearing the inner circle
    // First, draw filled outer circle
    M5Dial.Display.fillCircle(CENTER_X, CENTER_Y, outerRadius, color);
    // Then, clear the inner circle to create the ring
    M5Dial.Display.fillCircle(CENTER_X, CENTER_Y, innerRadius, bgColor);
}

void drawTimerDisplay(uint32_t seconds, uint16_t color) {
    // Calculate progress
    float progress = timerDuration > 0 ? 1.0 - ((float)timerRemaining / (float)timerDuration) : 0.0;
    
    // Get background color based on state
    uint16_t bgColor = getStateBackgroundColor();
    
    // Redraw everything if state changed, or on first draw
    bool fullRedraw = (lastDisplayedState != currentState) || (lastDisplayedProgress < 0);
    
    if (fullRedraw) {
        // Full screen clear with state background color
        M5Dial.Display.fillScreen(bgColor);
        // Draw white circle only if enabled
        if (SHOW_WHITE_CIRCLE) {
            drawCircularProgress(progress, COLOR_TEXT); // Draw static white circle
        }
        // Draw tomato icon after screen clear
        drawTomatoIcon();
        lastDisplayedProgress = progress;
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
    if (currentState == STATE_IDLE) {
        statusText = "Ready";
    } else if (currentState == STATE_PAUSED) {
        statusText = "Paused";
    } else if (currentState == STATE_RUNNING) {
        statusText = "Focusing";
    } else if (currentState == STATE_SHORT_BREAK) {
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

void drawStatusText(const char* text, uint16_t color) {
    // Status text is now drawn inside the circle in drawTimerDisplay
    // This function draws the instructions and settings gear at the bottom
    
    // Get background color based on state
    uint16_t bgColor = getStateBackgroundColor();
    
    // Draw instructions at bottom (moved higher to avoid gear icon)
    int16_t instructionY = SCREEN_HEIGHT - 48;
    M5Dial.Display.fillRect(0, instructionY - 10, SCREEN_WIDTH, 20, bgColor);
    M5Dial.Display.setTextColor(COLOR_TEXT);
    M5Dial.Display.setTextSize(1);
    const char* instruction = "";
    if (currentState == STATE_IDLE) {
        instruction = "Press: Start | Hold: Reset";
    } else if (currentState == STATE_PAUSED) {
        instruction = "Press: Resume | Hold: Reset";
    } else {
        instruction = "Press: Pause | Hold: Reset";
    }
    M5Dial.Display.drawString(instruction, CENTER_X, instructionY);
    
    // Draw settings gear icon at bottom center (only when not in settings)
    // Simple approach: always draw when state changed from what was displayed last frame
    if (currentState != STATE_SETTINGS && lastDisplayedState != currentState) {
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

void drawCurvedText(const char* text, int16_t centerX, int16_t centerY, int16_t radius, float startAngle, uint16_t color) {
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

void drawPomodoroCounter() {
    // Get background color based on state
    uint16_t bgColor = getStateBackgroundColor();
    
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

void drawTomatoIcon() {
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
        uint16_t bgColor = getStateBackgroundColor();
        M5Dial.Display.fillRect(iconX, iconYPos, iconSize, iconSize, TFT_RED);
    }
}

void drawSettingsMenu() {
    // Clear screen if we just entered settings (transitioning from another state)
    if (lastDisplayedState != STATE_SETTINGS) {
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
        if (i == settingsMenuIndex) {
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

void updateTimer() {
    if (timerStartTime == 0) return; // Timer not started
    
    uint32_t currentTime = millis();
    uint32_t elapsed = (currentTime - timerStartTime) / 1000;
    
    // Calculate remaining time
    if (elapsed >= timerDuration) {
        // Timer has fully completed - ensure it shows 00:00
        timerRemaining = 0;
        
        // CRITICAL: Set timerCompletionTime when timer completes
        if (timerCompletionTime == 0) {
            timerCompleted = true;
            timerCompletionTime = millis(); // Record when we reached 00:00
            needsRedraw = true; // Force display update to show 00:00
            Serial.println("╔════════════════════════════════════════╗");
            Serial.print("║ TIMER COMPLETED! State: ");
            Serial.print(currentState);
            Serial.println("               ║");
            Serial.print("║ timerCompletionTime set to: ");
            Serial.print(timerCompletionTime);
            Serial.println("    ║");
            Serial.print("║ beepState: ");
            Serial.print(beepState);
            Serial.println("                           ║");
            Serial.println("╚════════════════════════════════════════╝");
        }
    } else {
        // Update remaining time - ensure it counts down to 0
        timerRemaining = timerDuration - elapsed;
        timerCompleted = false;
    }
}

void handleTimerCompletion() {
    // Check if timer has completed and we need to beep
    if (timerCompletionTime == 0) return; // No completion to handle
    
    // Only handle completion for timer states
    if (currentState != STATE_RUNNING && currentState != STATE_SHORT_BREAK && currentState != STATE_LONG_BREAK) {
        return;
    }
    
    uint32_t elapsedSinceCompletion = millis() - timerCompletionTime;
    
    Serial.print("→ handleTimerCompletion: elapsed=");
    Serial.print(elapsedSinceCompletion);
    Serial.print("ms, beepState=");
    Serial.print(beepState);
    Serial.print(", currentState=");
    Serial.println(currentState);
    
    // Wait 1 second to ensure 00:00 is displayed, then beep
    if (elapsedSinceCompletion >= 1000 && beepState == 0) {
        Serial.println("╔═══════════════════════════════════════════╗");
        Serial.println("║  🔊 STARTING BUZZER SEQUENCE              ║");
        Serial.print("║  State: ");
        Serial.print(currentState);
        Serial.println("                              ║");
        Serial.println("╚═══════════════════════════════════════════╝");
        
        // Set flag to prevent re-entry
        beepState = 1;
        
        // CRITICAL: Ensure speaker is ready
        M5Dial.Speaker.end(); // Stop any previous tone
        delay(50);
        M5Dial.update();
        
        Serial.println("Playing beeps with speaker resets...");
        
        // Play 5-beep pattern with explicit speaker management
        for (int i = 0; i < 4; i++) {
            Serial.print("Beep ");
            Serial.println(i + 1);
            M5Dial.Speaker.tone(3000, 250);
            uint32_t startBeep = millis();
            while (millis() - startBeep < 250) {
                M5Dial.update();
                delay(1);
            }
            M5Dial.Speaker.end(); // Explicitly stop
            uint32_t startPause = millis();
            while (millis() - startPause < 300) {
                M5Dial.update();
                delay(1);
            }
        }
        
        // Final longer beep
        Serial.println("Final beep");
        M5Dial.Speaker.tone(3000, 400);
        uint32_t startFinalBeep = millis();
        while (millis() - startFinalBeep < 400) {
            M5Dial.update();
            delay(1);
        }
        M5Dial.Speaker.end(); // Stop speaker
        Serial.println("All beeps complete");
        
        Serial.println("╔═══════════════════════════════════════════╗");
        Serial.println("║  ✓ BUZZER COMPLETE - SWITCHING STATE     ║");
        Serial.println("╚═══════════════════════════════════════════╝");
        
        // Reset completion flag
        timerCompletionTime = 0;
        beepState = 0; // Reset immediately before state switch
        
        Serial.print("Before completeSession: timerCompletionTime=");
        Serial.print(timerCompletionTime);
        Serial.print(", beepState=");
        Serial.println(beepState);
        
        // Switch to next state
        completeSession();
        
        Serial.print("After completeSession: timerCompletionTime=");
        Serial.print(timerCompletionTime);
        Serial.print(", beepState=");
        Serial.print(beepState);
        Serial.print(", currentState=");
        Serial.println(currentState);
        Serial.println("═══════════════════════════════════════════\n");
    }
}

void handleEncoderInput() {
    long currentPos = M5Dial.Encoder.read();
    long delta = currentPos - lastEncoderPos;
    
    if (delta == 0) return;
    
    lastEncoderPos = currentPos;
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

void handleButtonPress() {
    needsRedraw = true; // Mark that we need to redraw
    
    switch (currentState) {
        case STATE_IDLE:
            // Start work timer
            startTimer(settings.workDuration);
            break;
            
        case STATE_RUNNING:
        case STATE_SHORT_BREAK:
        case STATE_LONG_BREAK:
            // Pause timer
            pauseTimer();
            break;
            
        case STATE_PAUSED:
            // Resume timer or reset if held
            resumeTimer();
            break;
            
        case STATE_SETTINGS:
            if (settingsMenuIndex == 4) {
                // Back to main screen - force full screen clear to prevent overlap
                uint16_t bgColor = COLOR_WORK_BG; // Red background for idle
                M5Dial.Display.fillScreen(bgColor);
                currentState = STATE_IDLE;
                resetTimer();
                needsRedraw = true;
                Serial.println("Exiting Settings -> Idle");
            } else if (settingsMenuIndex >= 0 && settingsMenuIndex <= 3) {
                // Allow editing all settings: Work Duration, Short Break, Long Break, and Pomodoros/Long
                settingsEditing = !settingsEditing;
            }
            break;
    }
}

void startTimer(uint32_t duration) {
    Serial.println("\n▶▶▶ startTimer() CALLED ◀◀◀");
    Serial.print("  Duration: ");
    Serial.print(duration);
    Serial.print("s, State: ");
    Serial.println(currentState);
    
    timerDuration = duration;
    timerRemaining = duration;
    timerStartTime = millis();
    timerCompleted = false;
    timerCompletionTime = 0; // ALWAYS reset this
    beepState = 0; // ALWAYS reset this
    lastBeepTime = 0;
    
    Serial.print("  Reset flags: timerCompletionTime=");
    Serial.print(timerCompletionTime);
    Serial.print(", beepState=");
    Serial.println(beepState);
    
    if (currentState == STATE_IDLE || currentState == STATE_PAUSED) {
        currentState = STATE_RUNNING;
        lastPomodoroDuration = duration;
        Serial.println("  Changed state to RUNNING");
    } else {
        Serial.print("  Keeping state as: ");
        Serial.println(currentState);
    }
    Serial.println("▶▶▶ startTimer() DONE ◀◀◀\n");
}

void pauseTimer() {
    if (currentState == STATE_RUNNING || currentState == STATE_SHORT_BREAK || currentState == STATE_LONG_BREAK) {
        // Store the current state before pausing
        stateBeforePause = currentState;
        currentState = STATE_PAUSED;
        // Don't update timerStartTime, so we can resume from where we paused
    }
}

void resumeTimer() {
    if (currentState == STATE_PAUSED) {
        // Recalculate start time based on remaining time
        timerStartTime = millis() - ((timerDuration - timerRemaining) * 1000);
        
        // Restore the state that was active before pausing
        currentState = stateBeforePause;
    }
}

void resetTimer() {
    timerRemaining = settings.workDuration;
    timerDuration = settings.workDuration;
    timerStartTime = 0;
    currentState = STATE_IDLE;
}

void completeSession() {
    // Note: Beep sound is now played in handleTimerCompletion() before calling this function
    // This ensures the sequence: Show 00:00 -> Beep -> Switch states
    
    Serial.print("completeSession called: currentState=");
    Serial.println(currentState);
    
    if (currentState == STATE_RUNNING) {
        completedPomodoros++;
        Serial.print("Pomodoro completed! Total: ");
        Serial.println(completedPomodoros);
        
        // Check if it's time for a long break
        if (completedPomodoros % settings.pomodorosUntilLongBreak == 0) {
            Serial.println("Starting LONG BREAK");
            currentState = STATE_LONG_BREAK;
            startTimer(getLongBreakDuration()); // Long break = pomodoro time
        } else {
            Serial.println("Starting SHORT BREAK");
            currentState = STATE_SHORT_BREAK;
            startTimer(getShortBreakDuration()); // Short break = pomodoro time / 5
        }
    } else if (currentState == STATE_SHORT_BREAK) {
        Serial.println("Short break completed, starting new POMODORO");
        // Short break completed, automatically start new focusing session with same duration
        // Use the last pomodoro duration, or fallback to settings if not available
        uint32_t durationToUse = (lastPomodoroDuration > 0) ? lastPomodoroDuration : settings.workDuration;
        
        // Set state to RUNNING BEFORE calling startTimer so it doesn't change it
        currentState = STATE_RUNNING;
        lastPomodoroDuration = durationToUse; // Store before startTimer
        
        // Use startTimer() for consistency - it resets all flags properly
        startTimer(durationToUse);
        
        Serial.print("New pomodoro started with duration: ");
        Serial.println(durationToUse);
    } else {
        Serial.println("Long break completed, returning to IDLE");
        // Long break completed, return to idle
        currentState = STATE_IDLE;
        resetTimer();
    }
    
    needsRedraw = true; // Ensure display updates after state change
}

String formatTime(uint32_t seconds) {
    uint32_t minutes = seconds / 60;
    uint32_t secs = seconds % 60;
    
    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%02lu:%02lu", minutes, secs);
    return String(buffer);
}

uint16_t getStateColor() {
    switch (currentState) {
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

uint16_t getStateBackgroundColor() {
    switch (currentState) {
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

// Helper functions to get break durations from settings
uint16_t getShortBreakDuration() {
    return settings.shortBreakDuration;
}

uint16_t getLongBreakDuration() {
    return settings.longBreakDuration;
}
