# Pomodoro Timer Dial

A modular, customizable and efficient Pomodoro timer implementation for the M5Stack Dial v1.1 (ESP32-S3).

![Hardware](https://img.shields.io/badge/Hardware-M5Stack-red)
![ESP32-S3](https://img.shields.io/badge/ESP32-S3-blue)
![Platform](https://img.shields.io/badge/Platform-PlatformIO-orange)
![License](https://img.shields.io/badge/License-MIT-green)

## Features

### Timer Functionality (defaults)

- **Customizable work sessions** (1-25 minutes via dial - default is 25 minutes)
- **Short breaks** after each pomodoro (default 5 minutes)
- **Long breaks** after 4 pomodoros (default 25 minutes - same as work session)
- **Short smart break calculation**: Automatically calculates breaks based on work duration (1/5 rule)
- **Audio alerts**: Buzzer sounds when timer completes

### User Interface

- **Rotary dial input**: Adjust timer duration smoothly
- **Button controls**:
  - Short press (<2s): Start / Pause / Resume
  - Long press (>2s): Reset to Ready state
- **Touch input**: Tap gear icon to access settings
- **Visual feedback**:
  - Color-coded states (Red=Work, Green=Short Break, Orange=Long Break)
  - Progress circle animation
  - Pomodoro counter with tomato icons
  - Status text display

### Settings Menu (customization) via the gear icon

- Work Duration (1-60 minutes)
- Short Break Duration (1-60 minutes)
- Long Break Duration (1-60 minutes)
- Pomodoros Until Long Break (1-10 sessions)

## Architecture Overview

The project is organized into modular components for maintainability and efficiency:

```
src/
├── main.cpp              # Main application orchestration
├── config.h              # Configuration constants and colors
├── types.h               # Common data types and enums
├── Display.h/.cpp        # Display rendering and UI management
├── InputHandler.h/.cpp   # User input processing (dial, button, touch)
└── TimerManager.h/.cpp   # Timer logic and state management
```

### Key Modules

**Display**: Handles all screen rendering, including timer display, progress circles, icons, and settings menu. Implements smart redrawing to minimize CPU usage.

**InputHandler**: Processes rotary encoder, button presses (short/long), and touch input. Manages debouncing and state-specific input handling.

**TimerManager**: Encapsulates timer logic, including start/pause/resume/reset, session completion, buzzer alerts, and automatic state transitions.

## Installation

### 1. Clone Repository

```bash
git clone https://github.com/sfrechette/pomodoro-timer-dial.git
cd pomodoro-timer-dial
```

### 2. Install Dependencies

```bash
# PlatformIO will automatically install required libraries:
# - M5Dial
# - M5Unified
# - M5GFX
# - SPIFFS
```

### 3. Upload Filesystem (Images)

```bash
pio run --target uploadfs
```

### 4. Upload Firmware

```bash
pio run --target upload
```

### 5. Monitor Serial Output (Optional)

```bash
pio device monitor
```

## Usage

### Basic Operation

1. **Ready State**: Device shows "Ready" with default 25:00 timer
2. **Adjust Time**: Rotate dial to change duration (1-25 minutes)
3. **Start Timer**: Short press button to begin countdown
4. **Pause**: Short press button while running to pause
5. **Resume**: Short press button while paused to continue
6. **Reset**: Long press button (>2s) to reset to Ready state

### Settings

1. **Open Settings**: Touch the gear icon at bottom of screen
2. **Navigate**: Rotate dial to highlight menu items
3. **Edit Value**: Short press to enter edit mode
4. **Adjust**: Rotate dial to change value
5. **Save**: Short press again to confirm and exit edit mode
6. **Exit Settings**: Select "Back" option

### Smart Breaks

When you adjust the work duration using the dial in Ready state:

- **Short Break** = Work Duration ÷ 5
- **Long Break** = Same as Work Duration

Example: Set 20 minutes work → 4 minutes short break, 20 minutes long break

Custom break durations set in Settings menu will be preserved until you adjust work duration with the dial.

## Configuration

Edit `src/config.h` to customize:

### Display Settings

```cpp
constexpr uint16_t SCREEN_WIDTH = 240;
constexpr uint16_t SCREEN_HEIGHT = 240;
constexpr uint8_t CIRCLE_RADIUS = 90;
constexpr uint8_t CIRCLE_THICKNESS = 12;
```

### Timer Defaults

```cpp
constexpr uint16_t DEFAULT_WORK_DURATION = 25 * 60;        // 25 minutes
constexpr uint16_t DEFAULT_SHORT_BREAK = 5 * 60;           // 5 minutes
constexpr uint16_t DEFAULT_LONG_BREAK = 25 * 60;           // 25 minutes
constexpr uint8_t DEFAULT_POMODOROS_UNTIL_LONG = 4;
```

### Colors

All colors are defined using RGB565 format in `Config::Colors` namespace.

## Project Structure

```
pomodoro-timer-dial/
├── data/                  # SPIFFS filesystem (images)
│   ├── pomodoro.png       # Tomato icon
│   └── gear.png           # Settings icon
├── src/                   # Source code
│   ├── main.cpp           # Main application
│   ├── config.h           # Configuration
│   ├── types.h            # Data types
│   ├── Display.h/.cpp     # Display module
│   ├── InputHandler.h/.cpp # Input module
│   └── TimerManager.h/.cpp # Timer module
├── platformio.ini         # PlatformIO configuration
└── README.md              # This file
```

## Development

### Building

```bash
pio run
```

### Uploading

```bash
pio run --target upload
```

### Cleaning Build Files

```bash
pio run --target clean
```

### Serial Debugging

The application outputs helpful debug information to serial monitor at 115200 baud:

```bash
pio device monitor --baud 115200
```

## Troubleshooting

### Gear Icon Shows White Box

Make sure you uploaded the filesystem before the firmware:

```bash
pio run --target uploadfs
pio run --target upload
```

### Buzzer Not Working

- Ensure speaker is not muted in M5Dial hardware
- Check that timer is completing properly (watch serial monitor)

### Display Issues

- Try adjusting brightness in setup (default: 100)
- Ensure SPIFFS mounted successfully (check serial output)

### Compilation Errors

- Clean build and try again: `pio run --target clean && pio run`
- Verify PlatformIO libraries are up to date

## Performance

The modularized architecture with performance optimizations provides:

- **Efficient rendering**: Frame-rate limited to 60 FPS, only redraws changed elements
- **Responsive input**: Debounced encoder with 5ms filtering, non-blocking button handling
- **Adaptive timing**: 40% CPU reduction during idle states
- **Low memory usage**: ~22KB RAM (6.8%), ~505KB Flash (15%)
- **Smooth animations**: Consistent frame timing with smart redrawing

### Performance Features

- Encoder debouncing (5ms) with threshold filtering
- Adaptive loop timing (10ms active, 20ms idle)
- Frame rate limiting (~60 FPS max)
- Optional performance monitoring (debug mode)

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- [M5Stack for the Dial v1.1](https://shop.m5stack.com/products/m5stack-dial-v1-1) hardware device platform
- [PlatformIO](https://platformio.org/) for the development environment

## Support

- **Issues**: [GitHub Issues](https://github.com/sfrechette/pomodoro-timer-dial/issues)

---
