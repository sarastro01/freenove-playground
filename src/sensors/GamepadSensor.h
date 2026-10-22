/**
 * @file GamepadSensor.h
 * @brief Gamepad input sensor class - Button and axis input handling
 * @author Refactoring Agent
 * @date 2026-10-22
 *
 * Encapsulates Adafruit Gamepad library functionality for unified button/axis tracking.
 * Provides real-time button press detection, analog stick position, and event counting.
 *
 * **Parameters** (8 total):
 *   0: X Position    - Analog stick X axis (-512 to +511, int)
 *   1: Y Position    - Analog stick Y axis (-512 to +511, int)
 *   2: Buttons       - Button state bitmask (32-bit)
 *   3: Event Count   - Number of events since last update (uint8_t)
 *   4: Last Event    - Last event type (0=none, 1=button, 2=stick) (uint8_t)
 *   5: Has Event     - True if new event detected (bool)
 *   6: Raw Buttons   - Raw button state (32-bit, debugging)
 *   7: Event Status  - Event type text (string)
 *
 * **Button Mapping**:
 *   - Button A:  0x001 (Pressed)
 *   - Button B:  0x002
 *   - Button X:  0x004
 *   - Button Y:  0x008
 *   - L1:        0x010
 *   - L2:        0x020
 *   - R1:        0x040
 *   - R2:        0x080
 *   - L3:        0x100 (Stick press)
 *   - R3:        0x200
 *   - Start:     0x400
 *   - Select:    0x800
 *
 * **Behaviors**:
 *   - Change detection: Flags change when buttons or analog sticks move
 *   - Event counting: Increments counter for each new button press
 *   - Availability: Always available if device initialized successfully
 *   - Error handling: Recoverable I2C errors (retries automatically)
 *   - Reset: Manual reset via clearEvents() method
 *
 * **Example Usage**:
 * ```cpp
 * GamepadSensor gamepad;
 * gamepad.initialize();
 * gamepad.update();
 * String x = gamepad.getValue(0);      // "-256"
 * String buttons = gamepad.getValue(2); // "0x012" (A + B pressed)
 * ```
 *
 * @see BaseSensor - Abstract interface
 * @see SensorRegistry - Sensor management
 */

#ifndef GAMEPADSENSOR_H
#define GAMEPADSENSOR_H

#include "BaseSensor.h"
#include <Adafruit_Gamepad.h>

/**
 * @class GamepadSensor
 * @brief Encapsulates Adafruit Gamepad for unified button/analog tracking
 *
 * Tracks:
 * - Analog stick position (X, Y axes: -512 to +511)
 * - Button press/release state (12 buttons)
 * - Event counting (increments for each new press)
 * - Change detection (for display refresh optimization)
 *
 * Thread-safe: No (single-threaded Arduino model)
 * Blocking: No (update() is fast I2C read, typically < 5ms)
 */
class GamepadSensor : public BaseSensor {
private:
  /// @brief Cached gamepad state
  struct {
    int x;                 ///< X-axis position (-512 to +511)
    int y;                 ///< Y-axis position (-512 to +511)
    uint32_t buttons;      ///< Button state bitmask
    uint8_t eventCount;    ///< Events since boot
    uint8_t lastEvent;     ///< Event type (0=none, 1=button, 2=stick)
    bool hasEvent;         ///< New event detected
  } values{};

  /// @brief Previous values for change detection
  struct {
    int x;
    int y;
    uint32_t buttons;
    uint8_t eventCount;
  } lastValues{};

  /// @brief Adafruit Gamepad device instance
  Adafruit_Gamepad gamepad{};

  /// @brief Device initialization status
  bool initialized{false};

public:
  /// @brief Constructor - initializes default values
  GamepadSensor() : BaseSensor() {}

  /// @brief Destructor
  ~GamepadSensor() = default;

  // ========================================================================
  // BaseSensor Interface Implementation (10 pure virtual methods)
  // ========================================================================

  /**
   * @brief Initialize gamepad and test I2C communication
   * @return true if device detected and initialized, false otherwise
   *
   * Called once during setup(). Sets availability flag based on device detection.
   * Device address: 0x77 (standard Adafruit Gamepad)
   */
  bool initialize() override;

  /**
   * @brief Read gamepad state and update cached values
   * @return true if read successful, false on error
   *
   * Called repeatedly from main loop (typically every 50-200ms).
   * Reads button and analog stick state via I2C.
   * Non-blocking: Returns immediately after I2C read.
   */
  bool update() override;

  /**
   * @brief Get formatted parameter value by index
   * @param index Parameter index (0-7)
   * @return Formatted value string, or empty string if invalid index
   *
   * Returns parameter formatted as string:
   *   0: X position     "-256"
   *   1: Y position     "128"
   *   2: Button mask    "0x0012"
   *   3: Event count    "42"
   *   4: Last event     "1" (0=none, 1=button, 2=stick)
   *   5: Has event      "true"
   *   6: Raw buttons    "0x0012"
   *   7: Event status   "button"
   */
  String getValue(uint8_t index) const override;

  /**
   * @brief Get large display format: value, label, unit, color
   * @param index Parameter index (0-7)
   * @return Formatted parameter with metadata for large display
   *
   * Used by large display mode to show single parameter with label and unit.
   * Example: {value: "-256", label: "X Position", unit: "pos", color: 0x00FFFF}
   */
  LargeDisplayValue getLargeDisplayValue(uint8_t index) const override;

  /**
   * @brief Get number of parameters this sensor provides
   * @return Always 8 (X, Y, buttons, event count, last event, has event, raw, status)
   */
  uint8_t getParameterCount() const override { return 8; }

  /**
   * @brief Get human-readable sensor name
   * @return "Gamepad"
   */
  const char *getSensorName() const override { return "Gamepad"; }

  /**
   * @brief Check if sensor is available and responding
   * @return true if device is initialized and communicating, false otherwise
   */
  bool isAvailable() const override { return initialized; }

  /**
   * @brief Check if any value changed since last update
   * @return true if buttons, X, or Y changed, false if all values same
   *
   * Used by display layer to optimize redraws.
   * Returns true if any button state changed or analog sticks moved significantly.
   */
  bool hasValueChanged() const override;

  /**
   * @brief Get debug status string
   * @return Status formatted as "OK", "ERROR: ...", etc.
   *
   * Example: "OK: x=-256, y=128, btns=0x0012, events=42"
   */
  String getStatus() const override;

  // ========================================================================
  // GamepadSensor-Specific Methods
  // ========================================================================

  /**
   * @brief Clear event counters and "has event" flag
   * @note Called manually to reset event tracking after processing
   */
  void clearEvents();

  /**
   * @brief Get current X position
   * @return X axis value (-512 to +511)
   */
  int getX() const { return values.x; }

  /**
   * @brief Get current Y position
   * @return Y axis value (-512 to +511)
   */
  int getY() const { return values.y; }

  /**
   * @brief Get current button state
   * @return Button bitmask (bit 0=A, bit 1=B, etc.)
   */
  uint32_t getButtons() const { return values.buttons; }

  /**
   * @brief Check if specific button is pressed
   * @param button Button code (0x001 for A, 0x002 for B, etc.)
   * @return true if button is currently pressed, false otherwise
   */
  bool isButtonPressed(uint32_t button) const { return (values.buttons & button) != 0; }

  /**
   * @brief Get event count since boot
   * @return Total number of button/stick events
   */
  uint8_t getEventCount() const { return values.eventCount; }

  /**
   * @brief Get event type
   * @return 0 = no event, 1 = button, 2 = analog stick
   */
  uint8_t getLastEvent() const { return values.lastEvent; }

  /**
   * @brief Check if new event detected in last update
   * @return true if event occurred since last clearEvents(), false otherwise
   */
  bool hasNewEvent() const { return values.hasEvent; }

  /**
   * @brief Get button name for button code
   * @param button Button code (0x001-0x800)
   * @return Button name ("A", "B", "X", "Y", "L1", "L2", "R1", "R2", "L3", "R3", "Start", "Select", "Unknown")
   */
  static const char *getButtonName(uint32_t button);

  /**
   * @brief Format button bitmask as hex string
   * @param buttons Button bitmask
   * @return Formatted hex string "0x0012"
   */
  static String formatButtonMask(uint32_t buttons);
};

#endif // GAMEPADSENSOR_H
