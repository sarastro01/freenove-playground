/**
 * @file EncoderSensor.h
 * @brief Rotary encoder sensor class - Position, button, and LED control
 * @author Refactoring Agent
 * @date 2026-10-22
 *
 * Encapsulates Adafruit Seesaw rotary encoder with integrated NeoPixel LED.
 * Provides position tracking, button detection, and LED color/brightness control.
 *
 * **Parameters** (7 total):
 *   0: Position    - Cumulative encoder position (int64_t, ticks)
 *   1: Delta       - Position change since last update (int64_t)
 *   2: Pressed     - Button currently pressed (bool)
 *   3: Presses     - Total button presses since boot (uint32_t)
 *   4: Brightness  - LED brightness 0-255 (uint8_t)
 *   5: Color       - Color name (string: "blue", "green", "red", "purple")
 *   6: Color Index - Color index 0-3 (uint8_t)
 *
 * **Button Behavior**:
 *   - Single press: Sets "pressed" flag, increments press counter
 *   - Hold: Continuous pressed state
 *   - Release: Clears pressed flag
 *
 * **LED Control**:
 *   - 4 color presets: blue (0x0060FF), green (0x00FF20), red (0xFF3000), purple (0xB400FF)
 *   - Brightness: 0-255 (0=off, 255=full)
 *   - Can be updated manually via setLedColor() and setLedBrightness()
 *
 * **Encoder Features**:
 *   - Position tracking: Increments/decrements with rotation
 *   - Delta calculation: Difference from previous position
 *   - Debouncing: Handled by Adafruit Seesaw library
 *   - Error recovery: Automatically retries on I2C error
 *
 * **Example Usage**:
 * ```cpp
 * EncoderSensor encoder;
 * encoder.initialize();
 * encoder.setLedColor(2);           // Set to red
 * encoder.setLedBrightness(200);    // 80% brightness
 * encoder.update();
 * String pos = encoder.getValue(0); // "15" (position)
 * String delta = encoder.getValue(1); // "1" (moved 1 tick)
 * ```
 *
 * @see BaseSensor - Abstract interface
 * @see SensorRegistry - Sensor management
 */

#ifndef ENCODERSENSOR_H
#define ENCODERSENSOR_H

#include "BaseSensor.h"
#include <Adafruit_seesaw.h>

/**
 * @class EncoderSensor
 * @brief Encapsulates Adafruit Seesaw rotary encoder with NeoPixel LED
 *
 * Tracks:
 * - Encoder position (ticks, int64_t)
 * - Button press/release state
 * - Total presses since boot
 * - LED color (4 presets) and brightness (0-255)
 * - Change detection (for display refresh)
 *
 * Thread-safe: No (single-threaded Arduino model)
 * Blocking: No (I2C communication < 10ms)
 */
class EncoderSensor : public BaseSensor {
private:
  /// @brief Cached encoder state
  struct {
    int64_t position;           ///< Current position (ticks)
    int64_t delta;              ///< Change since last update
    bool pressed;               ///< Button currently pressed
    uint32_t presses;           ///< Total presses since boot
    uint8_t brightness;         ///< LED brightness (0-255)
    uint8_t colorIndex;         ///< Color selection (0-3)
    const char *colorName;      ///< Color name ("blue", "green", etc.)
  } values{};

  /// @brief Previous values for change detection
  struct {
    int64_t position;
    bool pressed;
    uint32_t presses;
    uint8_t brightness;
    uint8_t colorIndex;
  } lastValues{};

  /// @brief Adafruit Seesaw device instance
  Adafruit_seesaw seesaw{};

  /// @brief Device initialization status
  bool initialized{false};

  /// @brief Previous button state for edge detection
  bool lastPressed{false};

  /// @brief Color presets (RGB565 format, but can be used as RGB888 with conversion)
  static const uint32_t COLOR_PRESETS[4];
  static const char *COLOR_NAMES[4];

public:
  /// @brief Constructor - initializes default values
  EncoderSensor() : BaseSensor() {
    values.colorIndex = 0;
    values.colorName = COLOR_NAMES[0];
    values.brightness = 64;
  }

  /// @brief Destructor
  ~EncoderSensor() = default;

  // ========================================================================
  // BaseSensor Interface Implementation (10 pure virtual methods)
  // ========================================================================

  /**
   * @brief Initialize encoder and LED, test I2C communication
   * @return true if device detected and initialized, false otherwise
   *
   * Called once during setup(). Sets availability flag based on device detection.
   * Device address: 0x36 (standard Adafruit Seesaw)
   * Initializes encoder module and NeoPixel LED.
   */
  bool initialize() override;

  /**
   * @brief Read encoder position, button state, and update cached values
   * @return true if read successful, false on error
   *
   * Called repeatedly from main loop (typically every 50-200ms).
   * Reads encoder position and button via I2C.
   * Updates LED color/brightness if changed.
   * Non-blocking: Returns immediately after I2C read.
   */
  bool update() override;

  /**
   * @brief Get formatted parameter value by index
   * @param index Parameter index (0-6)
   * @return Formatted value string, or empty string if invalid index
   *
   * Returns parameter formatted as string:
   *   0: Position      "15"
   *   1: Delta         "1"
   *   2: Pressed       "true"
   *   3: Presses       "42"
   *   4: Brightness    "200"
   *   5: Color         "red"
   *   6: Color Index   "2"
   */
  String getValue(uint8_t index) const override;

  /**
   * @brief Get large display format: value, label, unit, color
   * @param index Parameter index (0-6)
   * @return Formatted parameter with metadata for large display
   *
   * Used by large display mode to show single parameter with label and unit.
   * Example: {value: "15", label: "Position", unit: "ticks", color: 0x00FFFF}
   */
  LargeDisplayValue getLargeDisplayValue(uint8_t index) const override;

  /**
   * @brief Get number of parameters this sensor provides
   * @return Always 7 (Position, Delta, Pressed, Presses, Brightness, Color, ColorIndex)
   */
  uint8_t getParameterCount() const override { return 7; }

  /**
   * @brief Get human-readable sensor name
   * @return "Encoder"
   */
  const char *getSensorName() const override { return "Encoder"; }

  /**
   * @brief Check if sensor is available and responding
   * @return true if device is initialized and communicating, false otherwise
   */
  bool isAvailable() const override { return initialized; }

  /**
   * @brief Check if any value changed since last update
   * @return true if position, button, or LED settings changed, false if all same
   *
   * Used by display layer to optimize redraws.
   * Returns true if position changed or button was pressed/released.
   */
  bool hasValueChanged() const override;

  /**
   * @brief Get debug status string
   * @return Status formatted as "OK", "ERROR: ...", etc.
   *
   * Example: "OK: pos=15, delta=1, pressed=false, presses=42, brightness=200, color=red"
   */
  String getStatus() const override;

  // ========================================================================
  // EncoderSensor-Specific Methods
  // ========================================================================

  /**
   * @brief Get current encoder position (ticks)
   * @return Absolute position from start
   */
  int64_t getPosition() const { return values.position; }

  /**
   * @brief Get position change since last update
   * @return Delta (positive = clockwise, negative = counter-clockwise)
   */
  int64_t getDelta() const { return values.delta; }

  /**
   * @brief Check if button is currently pressed
   * @return true if pressed, false if released
   */
  bool isPressed() const { return values.pressed; }

  /**
   * @brief Get total button presses since boot
   * @return Press counter (incremented on press edge)
   */
  uint32_t getPressCount() const { return values.presses; }

  /**
   * @brief Get LED brightness
   * @return Brightness 0-255 (0=off, 255=full)
   */
  uint8_t getLedBrightness() const { return values.brightness; }

  /**
   * @brief Get LED color index
   * @return Index 0-3 (0=blue, 1=green, 2=red, 3=purple)
   */
  uint8_t getLedColorIndex() const { return values.colorIndex; }

  /**
   * @brief Get LED color name
   * @return Color name ("blue", "green", "red", or "purple")
   */
  const char *getLedColorName() const { return values.colorName; }

  /**
   * @brief Set LED color by index
   * @param colorIndex Index 0-3 (0=blue, 1=green, 2=red, 3=purple)
   * @return true if set successfully, false if index out of range
   *
   * Updates LED immediately and caches for display.
   */
  bool setLedColor(uint8_t colorIndex);

  /**
   * @brief Set LED brightness
   * @param brightness Brightness 0-255 (0=off, 255=full)
   * @return true if set successfully
   *
   * Updates LED immediately and caches for display.
   */
  bool setLedBrightness(uint8_t brightness);

  /**
   * @brief Update LED color and brightness in one call
   * @param colorIndex Color index 0-3
   * @param brightness Brightness 0-255
   * @return true if set successfully
   */
  bool setLedColorAndBrightness(uint8_t colorIndex, uint8_t brightness);

  /**
   * @brief Get RGB color value for color index
   * @param colorIndex Index 0-3
   * @return RGB color (uint32_t in 0xRRGGBB format)
   */
  static uint32_t getColorForIndex(uint8_t colorIndex);

  /**
   * @brief Get color name for color index
   * @param colorIndex Index 0-3
   * @return Color name or "unknown"
   */
  static const char *getColorNameForIndex(uint8_t colorIndex);
};

#endif // ENCODERSENSOR_H
