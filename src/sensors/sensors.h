/**
 * @file sensors.h
 * @brief Master include file for sensor classes
 * 
 * Include this file in main.cpp to get access to all sensor infrastructure:
 *   #include "sensors/sensors.h"
 * 
 * This includes:
 *   - BaseSensor abstract base class
 *   - SensorRegistry for centralized management
 *   - All concrete sensor implementations (once created)
 */

#ifndef SENSORS_H
#define SENSORS_H

// Core infrastructure
#include "BaseSensor.h"
#include "SensorRegistry.h"

// Concrete sensor implementations (added in Phase 2-3)
#include "Ina219Sensor.h"      // Phase 2: Current/Power monitor
#include "GamepadSensor.h"     // Phase 3A: Gamepad input
#include "EncoderSensor.h"     // Phase 3A: Rotary encoder input
#include "Bme280Sensor.h"      // Phase 3B: Temperature/Pressure/Humidity
#include "Si1145Sensor.h"      // Phase 3B: Light sensor (visible/IR/UV)
#include "Tsl2591Sensor.h"     // Phase 3B: Digital light sensor
#include "Veml6075Sensor.h"    // Phase 3C: UVA/UVB sensor
#include "Veml6070Sensor.h"    // Phase 3C: UV sensor
#include "Scd30Sensor.h"       // Phase 3C: CO2 + ASC
#include "Ds2484Sensor.h"      // Phase 3C: 1-Wire temperature

#endif  // SENSORS_H
