# Sensor Pages Dashboard

Gemeinsames PlatformIO-Projekt fuer das Freenove ESP32 Display, das mehrere I2C-Sensoren und den Rotary Encoder seesaw auf eigenen TFT-Seiten anzeigt.

## Seitensteuerung

- Gamepad QT Y: vorherige Seite
- Gamepad QT A: naechste Seite
- Seiten werden auch angezeigt, wenn der Sensor nicht angeschlossen ist
- Sensoren werden beim Start unabhaengig voneinander erkannt
- INA219-Akkuwerte werden mit 50 Hz erfasst und als integrierte Ladung in mAh angezeigt
- Rotary Encoder und Gamepad werden mit 50 Hz abgefragt
- Die ausgewaehlte Encoder-/Gamepad-Seite wird mit 50 Hz aktualisiert
- DS18B20 werden mit Adresse und Temperatur auf einer gemeinsamen Seite angezeigt

## Unterstuetzte Geraete

- BME280 (`0x77`)
- DS18B20 OneWire-Sensoren an GPIO23
- Gamepad QT (`0x50`)
- INA219 (`0x40`)
- Rotary Encoder seesaw (`0x36`)
- SCD30 (`0x61`)
- SI1145 (`0x60`)
- TSL2591 (`0x29`)
- VEML6070 (`0x38` / `0x39`)
- VEML6075 (`0x10`)
- Zusaetzliche Gamepad-Seite mit Joystick- und Tastenstatus

## Hardware

- SDA: GPIO 32
- SCL: GPIO 25
- I2C: 400 kHz
- OneWire: GPIO23 mit `INPUT_PULLUP`
- Upload/Monitor: `/dev/ttyUSB0`

## Build / Upload

```bash
cd /mnt/SSD_4TB_01/mnt/rslsync/folders/Privat02/Projects/2026/08/freenove-playground/sensor-pages-dashboard
~/.platformio/penv/bin/pio run
~/.platformio/penv/bin/pio run -t upload
~/.platformio/penv/bin/pio device monitor
```
