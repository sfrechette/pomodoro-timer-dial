/**
 * Timer Manager Implementation
 * All timer logic moved here from main.cpp
 * EXACT same behavior - nothing changed!
 */

#include "TimerManager.h"

TimerManager::TimerManager()
    : timerStartTime(0),
      timerRemaining(0),
      timerDuration(0),
      lastPomodoroDuration(0),
      stateBeforePause(STATE_IDLE),
      timerCompleted(false),
      timerCompletionTime(0),
      beepState(0),
      lastBeepTime(0) {
}

void TimerManager::update(TimerState& currentState,
                         PomodoroSettings& settings,
                         uint8_t& completedPomodoros,
                         bool& needsRedraw) {
    // Update timer if running
    if (currentState == STATE_RUNNING || currentState == STATE_SHORT_BREAK || currentState == STATE_LONG_BREAK) {
        updateTimer();
        handleTimerCompletion(currentState, settings, completedPomodoros, needsRedraw);
    }
}

void TimerManager::updateTimer() {
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
            Serial.println("╔════════════════════════════════════════╗");
            Serial.print("║ TIMER COMPLETED! State: ");
            Serial.print("               ║");
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

void TimerManager::handleTimerCompletion(TimerState& currentState,
                                        PomodoroSettings& settings,
                                        uint8_t& completedPomodoros,
                                        bool& needsRedraw) {
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
        completeSession(currentState, settings, completedPomodoros, needsRedraw);
        
        Serial.print("After completeSession: timerCompletionTime=");
        Serial.print(timerCompletionTime);
        Serial.print(", beepState=");
        Serial.print(beepState);
        Serial.print(", currentState=");
        Serial.println(currentState);
        Serial.println("═══════════════════════════════════════════\n");
    }
}

void TimerManager::start(uint32_t duration, TimerState& currentState) {
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

void TimerManager::pause(TimerState& currentState) {
    if (currentState == STATE_RUNNING || currentState == STATE_SHORT_BREAK || currentState == STATE_LONG_BREAK) {
        // Store the current state before pausing
        stateBeforePause = currentState;
        currentState = STATE_PAUSED;
        // Don't update timerStartTime, so we can resume from where we paused
    }
}

void TimerManager::resume(TimerState& currentState) {
    if (currentState == STATE_PAUSED) {
        // Recalculate start time based on remaining time
        timerStartTime = millis() - ((timerDuration - timerRemaining) * 1000);
        
        // Restore the state that was active before pausing
        currentState = stateBeforePause;
    }
}

void TimerManager::reset(TimerState& currentState, PomodoroSettings& settings) {
    timerRemaining = settings.workDuration;
    timerDuration = settings.workDuration;
    timerStartTime = 0;
    timerCompleted = false;
    timerCompletionTime = 0;
    beepState = 0;
    lastBeepTime = 0;
    currentState = STATE_IDLE;
}

void TimerManager::completeSession(TimerState& currentState,
                                   PomodoroSettings& settings,
                                   uint8_t& completedPomodoros,
                                   bool& needsRedraw) {
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
            start(getLongBreakDuration(settings), currentState); // Long break = pomodoro time
        } else {
            Serial.println("Starting SHORT BREAK");
            currentState = STATE_SHORT_BREAK;
            start(getShortBreakDuration(settings), currentState); // Short break = pomodoro time / 5
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
        start(durationToUse, currentState);
        
        Serial.print("New pomodoro started with duration: ");
        Serial.println(durationToUse);
    } else {
        Serial.println("Long break completed, returning to IDLE");
        // Long break completed, return to idle
        currentState = STATE_IDLE;
        reset(currentState, settings);
    }
    
    needsRedraw = true; // Ensure display updates after state change
}

// Helper functions to get break durations from settings
uint16_t TimerManager::getShortBreakDuration(PomodoroSettings& settings) {
    return settings.shortBreakDuration;
}

uint16_t TimerManager::getLongBreakDuration(PomodoroSettings& settings) {
    return settings.longBreakDuration;
}

