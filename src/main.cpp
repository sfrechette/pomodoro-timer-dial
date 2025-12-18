#include <Arduino.h>
#include <M5Dial.h>
#include <math.h>
#include <string.h>
#include <SPIFFS.h>
#include "config.h"
#include "types.h"
#include "Display.h"
#include "InputHandler.h"
#include "TimerManager.h"


// Global Variables
TimerState currentState = STATE_IDLE;
PomodoroSettings settings = {
    .workDuration = 25 * 60,           // 25 minutes
    .shortBreakDuration = 5 * 60,      // 5 minutes
    .longBreakDuration = 25 * 60,      // 25 minutes
    .pomodorosUntilLongBreak = 4
};

uint8_t completedPomodoros = 0;
uint8_t settingsMenuIndex = 0;
bool settingsEditing = false;
bool needsRedraw = true;
uint32_t lastDisplayedSeconds = 0;
TimerState lastDisplayedState = STATE_SETTINGS; // Initialize to different state to force first draw
float lastDisplayedProgress = -1.0;

// Module instances
Display display;
InputHandler inputHandler;
TimerManager timerManager;

// Function prototypes (callback wrappers for InputHandler)
void startTimer(uint32_t duration);
void pauseTimer();
void resumeTimer();
void resetTimer();

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
    
    // Initialize input handler
    inputHandler.init();
    
    // Draw initial screen
    needsRedraw = true;
    resetTimer();
}

void loop() {
    M5Dial.update();
    
    // Performance monitoring (optional debug mode)
    #if ENABLE_PERFORMANCE_MONITOR
    static uint32_t loopCount = 0;
    static uint32_t lastPerfReport = 0;
    static uint32_t redrawCount = 0;
    static uint32_t skippedFrames = 0;
    loopCount++;
    #endif
    
    // Get current timer values for input handler
    uint32_t timerRemaining = timerManager.getRemaining();
    uint32_t timerDuration = timerManager.getDuration();
    
    // Store old state to detect reset
    TimerState oldState = currentState;
    
    // Handle all input (encoder, button, touch) through InputHandler
    inputHandler.processInput(currentState, settings, settingsMenuIndex, settingsEditing,
                              timerRemaining, timerDuration, needsRedraw,
                              startTimer, pauseTimer, resumeTimer, resetTimer);
    
    // Only sync back if we're in IDLE state (encoder adjustments)
    // If reset happened (state changed to IDLE), re-read values instead
    if (currentState == STATE_IDLE && oldState == STATE_IDLE) {
        // Dial was adjusted in idle - sync changes back
        timerManager.setRemaining(timerRemaining);
        timerManager.setDuration(timerDuration);
    }
    
    // Update timer logic (including buzzer)
    timerManager.update(currentState, settings, completedPomodoros, needsRedraw);
    
    // Get current timer values for display
    uint32_t currentRemaining = timerManager.getRemaining();
    uint32_t currentDuration = timerManager.getDuration();
    
    // Check if we need to redraw (only when something changes)
    bool shouldRedraw = needsRedraw;
    if (currentState == STATE_RUNNING || currentState == STATE_SHORT_BREAK || currentState == STATE_LONG_BREAK) {
        // Update every second for timer, or immediately if state changed
        if (currentRemaining != lastDisplayedSeconds || currentState != lastDisplayedState) {
            shouldRedraw = true;
        }
    } else if (currentState == STATE_IDLE) {
        // In idle, redraw if state changed or if timer duration changed (from dial adjustment)
        if (currentState != lastDisplayedState || currentRemaining != lastDisplayedSeconds) {
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
    // Performance optimization: Frame rate limiting
    static uint32_t lastRedrawTime = 0;
    uint32_t redrawNow = millis();
    bool canRedraw = (redrawNow - lastRedrawTime >= MIN_REDRAW_INTERVAL_MS);
    
    if (shouldRedraw && canRedraw) {
        lastRedrawTime = redrawNow;
        #if ENABLE_PERFORMANCE_MONITOR
        redrawCount++;
        #endif
        switch (currentState) {
            case STATE_IDLE:
            case STATE_RUNNING:
            case STATE_PAUSED:
            case STATE_SHORT_BREAK:
            case STATE_LONG_BREAK:
                display.drawTimerDisplay(currentRemaining, display.getStateColor(currentState),
                                        currentState, currentDuration, currentRemaining,
                                        lastDisplayedState, lastDisplayedProgress);
                display.drawStatusText(
                    currentState == STATE_IDLE ? "Ready" :
                    currentState == STATE_PAUSED ? "Paused" :
                    currentState == STATE_RUNNING ? "Focusing" :
                    currentState == STATE_SHORT_BREAK ? "Short Break" :
                    "Long Break",
                    display.getStateColor(currentState),
                    currentState, lastDisplayedState
                );
                display.drawPomodoroCounter(completedPomodoros, currentState);
                display.drawTomatoIcon(currentState);
                break;
            case STATE_SETTINGS:
                display.drawSettingsMenu(settings, settingsMenuIndex, settingsEditing, lastDisplayedState);
                break;
        }
        lastDisplayedSeconds = currentRemaining;
        lastDisplayedState = currentState;
        needsRedraw = false;
    } else if (shouldRedraw && !canRedraw) {
        #if ENABLE_PERFORMANCE_MONITOR
        skippedFrames++;
        #endif
    }
    
    // Performance monitoring: Periodic reporting
    #if ENABLE_PERFORMANCE_MONITOR
    uint32_t now = millis();
    if (now - lastPerfReport >= PERF_REPORT_INTERVAL_MS) {
        float fps = (float)loopCount / (float)(now - lastPerfReport) * 1000.0f;
        float redrawFps = (float)redrawCount / (float)(now - lastPerfReport) * 1000.0f;
        Serial.println("\n═══ PERFORMANCE STATS ═══");
        Serial.print("Loop FPS: "); Serial.println(fps, 1);
        Serial.print("Redraw FPS: "); Serial.println(redrawFps, 1);
        Serial.print("Skipped Frames: "); Serial.println(skippedFrames);
        Serial.print("Free Heap: "); Serial.print(ESP.getFreeHeap()); Serial.println(" bytes");
        Serial.println("═══════════════════════════\n");
        
        loopCount = 0;
        redrawCount = 0;
        skippedFrames = 0;
        lastPerfReport = now;
    }
    #endif
    
    // Balanced loop delay for responsiveness and efficiency
    delay(10);
}

// Callback wrappers for InputHandler to call TimerManager
void startTimer(uint32_t duration) {
    timerManager.start(duration, currentState);
}

void pauseTimer() {
    timerManager.pause(currentState);
}

void resumeTimer() {
    timerManager.resume(currentState);
}

void resetTimer() {
    timerManager.reset(currentState, settings);
}
