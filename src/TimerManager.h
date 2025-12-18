/**
 * Timer Manager Module
 * Handles all timer logic including the buzzer sequence
 */

#ifndef TIMER_MANAGER_H
#define TIMER_MANAGER_H

#include <Arduino.h>
#include <M5Dial.h>
#include "config.h"
#include "types.h"

class TimerManager {
public:
    // Constructor
    TimerManager();
    
    // Main timer update (call from loop)
    void update(TimerState& currentState,
                PomodoroSettings& settings,
                uint8_t& completedPomodoros,
                bool& needsRedraw);
    
    // Timer control functions
    void start(uint32_t duration, TimerState& currentState);
    void pause(TimerState& currentState);
    void resume(TimerState& currentState);
    void reset(TimerState& currentState, PomodoroSettings& settings);
    
    // Getters for display
    uint32_t getRemaining() const { return timerRemaining; }
    uint32_t getDuration() const { return timerDuration; }
    bool isCompleted() const { return timerCompleted; }
    
    // Setters for dial adjustments in idle state
    void setRemaining(uint32_t remaining) { timerRemaining = remaining; }
    void setDuration(uint32_t duration) { timerDuration = duration; }
    
private:
    // Timer state variables
    uint32_t timerStartTime;
    uint32_t timerRemaining;
    uint32_t timerDuration;
    uint32_t lastPomodoroDuration;
    TimerState stateBeforePause;
    bool timerCompleted;
    uint32_t timerCompletionTime;
    uint8_t beepState;
    uint32_t lastBeepTime;
    
    // Internal helper functions
    void updateTimer();
    void handleTimerCompletion(TimerState& currentState,
                               PomodoroSettings& settings,
                               uint8_t& completedPomodoros,
                               bool& needsRedraw);
    void completeSession(TimerState& currentState,
                        PomodoroSettings& settings,
                        uint8_t& completedPomodoros,
                        bool& needsRedraw);
    uint16_t getShortBreakDuration(PomodoroSettings& settings);
    uint16_t getLongBreakDuration(PomodoroSettings& settings);
};

#endif // TIMER_MANAGER_H

