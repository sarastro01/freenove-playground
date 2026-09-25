# DETAILED PARAMETER CONSISTENCY AUDIT

## 1. BME280 Climate Sensor

### Large Display Helper (Lines ~461-488)
Labels: "Temperature", "Pressure", "Humidity", "Dew Point", "Altitude", "Air Quality", "Temp Trend"
Units: "C", "hPa", "%", "C", "m", "AQI", "C/min"
Switch cases: 0-6 (7 parameters)

### Sensor Page (Lines ~1921-1928)
rowTriple(56, "Temperature:", ..., "C", ...)
rowTriple(80, "Pressure:", ..., "hPa", ...)
rowTriple(104, "Humidity:", ..., "%", ...)
rowTriple(128, "Dew Point:", ..., "C", ...)
rowTriple(152, "Altitude:", ..., "m", ...)
rowTriple(176, "Air Quality:", ..., "AQI", ...)
rowTriple(200, "Temp Trend:", ..., "C/min", ...)

✅ CONSISTENT: 7 parameters, same order, same units

---

## 2. Gamepad QT Controller

### Large Display Helper (Lines ~492-521)
Labels: "Joystick X", "Joystick Y", "Button X", "Button Y", "Button A", "Button B", "Select/Start", "Button Mask"
Units: "ADC", "ADC", "State", "State", "State", "State", "State", "Hex"
Switch cases: 0-7 (8 parameters)

### Sensor Page (Lines ~1957-1976)
rowTriple(56, "Joystick X:", ..., "ADC", ...) ✅
rowTriple(80, "Joystick Y:", ..., "ADC", ...) ✅
rowTriple(104, "Button X:", ..., "State", ...) ✅ FIXED
rowTriple(128, "Button Y:", ..., "State", ...) ✅ FIXED
rowTriple(152, "Button A:", ..., "State", ...) ✅ FIXED
rowTriple(176, "Button B:", ..., "State", ...) ✅ FIXED
rowTriple(200, "Select / Start:", ..., "State", ...) ✅ FIXED
rowTriple(224, "Button Mask:", ..., "Hex", ...) ✅ FIXED
(NO "Last event" row anymore - REMOVED)

✅ CONSISTENT: 8 parameters, same order, same units

---

## 3. INA219 Power Monitor

### Large Display Helper (Lines ~428-456)
Labels: "Runtime", "Battery", "Supply", "Current", "Charge", "Shunt Voltage", "Power"
Units: "", "V", "V", "mA", "mAh", "mV", "mW"
Switch cases: 0-6 (7 parameters)

### Sensor Page (Lines ~1795-1804)
rowTriple(56, "Runtime:", ..., "", ...) ✅
rowTriple(80, "Battery:", ..., "V", ...) ✅
rowTriple(104, "Supply:", ..., "V", ...) ✅
rowTriple(128, "Current:", ..., "mA", ...) ✅
rowTriple(152, "Charge:", ..., "mAh", ...) ✅
rowTriple(176, "Shunt Voltage:", ..., "mV", ...) ✅
rowTriple(200, "Power:", ..., "mW", ...) ✅

✅ CONSISTENT: 7 parameters, same order, same units

---

## 4. Rotary Encoder Input

### Large Display Helper (Lines ~523-552)
Labels: "Position", "Delta", "Button", "Presses", "LED Color", "LED Brightness", "Seesaw Temp"
Units: "Steps", "Steps", "State", "Count", "Name", "/255", "C"
Switch cases: 0-6 (7 parameters)

### Sensor Page (Lines ~1860-1868)
rowTriple(56, "Position:", ..., "Steps", ...) ✅ FIXED
rowTriple(80, "Delta:", ..., "Steps", ...) ✅ FIXED
rowTriple(104, "Button:", ..., "State", ...) ✅ FIXED
rowTriple(128, "Presses:", ..., "Count", ...) ✅ FIXED
rowTriple(152, "LED Color:", ..., "Name", ...) ✅ FIXED
rowTriple(176, "LED Brightness:", ..., "/255", ...) ✅ FIXED
rowTriple(200, "Seesaw Temp:", ..., "C", ...) ✅ FIXED

✅ CONSISTENT: 7 parameters, same order, same units

---

## 5. SCD30 CO2 Sensor

### Large Display Helper (Lines ~554-576)
Labels: "CO2 Level", "Temperature", "Humidity"
Units: "ppm", "C", "%"
Switch cases: 0-2 (3 parameters)

### Sensor Page (Lines ~1905-1910)
rowTriple(56, "CO2 Level:", ..., "ppm", ...)
rowTriple(80, "Temperature:", ..., "C", ...)
rowTriple(104, "Humidity:", ..., "%", ...)

✅ CONSISTENT: 3 parameters, same order, same units

---

## 6. SI1145 UV/IR Sensor

### Large Display Helper (Lines ~577-608)
Labels: "Visible Light", "Infrared", "UV Index", "UV Raw"
Units: "ADC", "ADC", "Index", "ADC"
Switch cases: 0-3 (4 parameters)

### Sensor Page (Lines ~1890-1896)
rowTriple(56, "Visible Light:", ..., "ADC", ...)
rowTriple(80, "Infrared:", ..., "ADC", ...)
rowTriple(104, "UV Index:", ..., "Index", ...)
rowTriple(128, "UV Raw:", ..., "ADC", ...)

✅ CONSISTENT: 4 parameters, same order, same units

---

## 7. TSL2591 Light Sensor

### Large Display Helper (Lines ~610-638)
Labels: "Full Spectrum", "Infrared", "Visible", "Lux"
Units: "counts", "counts", "counts", "lux"
Switch cases: 0-3 (4 parameters)

### Sensor Page (Lines ~1835-1841)
rowTriple(56, "Full Spectrum:", ..., "counts", ...)
rowTriple(80, "Infrared:", ..., "counts", ...)
rowTriple(104, "Visible:", ..., "counts", ...)
rowTriple(128, "Lux:", ..., "lux", ...)

✅ CONSISTENT: 4 parameters, same order, same units

---

## 8. VEML6070 UV Sensor

### Large Display Helper (Lines ~640-660)
Labels: "UV Raw", "UV Level", "LSB/MSB"
Units: "ADC", "Level", "Hex"
Switch cases: 0-2 (3 parameters)

### Sensor Page (Lines ~1875-1880)
rowTriple(56, "UV Raw:", ..., "ADC", ...)
rowTriple(80, "UV Level:", ..., "Level", ...)
rowTriple(104, "LSB/MSB:", ..., "Hex", ...)

✅ CONSISTENT: 3 parameters, same order, same units

---

## 9. VEML6075 UV Sensor

### Large Display Helper (Lines ~662-682)
Labels: "UVA Raw", "UVB Raw", "UVA Compensated", "UVB Compensated", "UV Index", "Compensation"
Units: "ADC", "ADC", "Calc", "Calc", "Index", "Factors"
Switch cases: 0-5 (6 parameters)

### Sensor Page (Lines ~1820-1827)
rowTriple(56, "UVA Raw:", ..., "ADC", ...)
rowTriple(80, "UVB Raw:", ..., "ADC", ...)
rowTriple(104, "UVA Compensated:", ..., "Calc", ...)
rowTriple(128, "UVB Compensated:", ..., "Calc", ...)
rowTriple(152, "UV Index:", ..., "Index", ...)
rowTriple(176, "Compensation:", ..., "Factors", ...)

✅ CONSISTENT: 6 parameters, same order, same units

---

## 10. DS2484 1-Wire Adapter

### Large Display Helper (Lines ~684-723)
Special: Dynamic number of parameters (3 per sensor, max 3 sensors = 9)
Switch cases: 0-8 (up to 9 parameters, 3 per sensor)

### Sensor Page (Lines ~1990-2027)
Displays status line + up to 3 sensors with ROM-ID and temperature
Dynamic structure matching Large Display

✅ CONSISTENT: Dynamic per sensor, same structure

---

## Summary of Fixes Applied

1. ✅ Gamepad: Fixed units (added "State" for buttons, "Hex" for mask)
2. ✅ Gamepad: Removed "Last event" row (not in Large Display)
3. ✅ Encoder: Fixed units (Steps, State, Count, Name, /255, C)
4. ✅ Encoder: Fixed "LED Brightness" format (removed " / 255" from value)
5. ✅ Display Artifacts: Added screen clear on exit from Large Display Mode

---

## Verified Consistent Parameters

All 10 sensors now have:
- ✅ Identical parameter count
- ✅ Identical parameter order
- ✅ Identical parameter names (labels)
- ✅ Identical units
- ✅ Same values in both display modes

getPageValueCount() values:
- BME280: 7
- Gamepad: 8 (UPDATED)
- INA219: 7
- Encoder: 7 (UPDATED)
- SCD30: 3
- SI1145: 4
- TSL2591: 4
- VEML6070: 3
- VEML6075: 6
- DS2484: Dynamic (3 × sensor count)
