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

// Concrete sensor implementations (to be added in Phase 2-3)
// #include "Ina219Sensor.h"
// #include "Bme280Sensor.h"
// #include "Scd30Sensor.h"
// #include "EncoderSensor.h"
// #include "GamepadSensor.h"
// #include "Veml6075Sensor.h"
// #include "Veml6070Sensor.h"
// #include "Tsl2591Sensor.h"
// #include "Si1145Sensor.h"
// #include "Ds2484Sensor.h"

#endif  // SENSORS_H
