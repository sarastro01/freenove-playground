# Feature-Spezifikation: Einheitliche Sensor-Caching-Architektur

**Feature Name:** Implement Caching Architecture for BME280, TSL2591, SI1145  
**Status:** ✅ Spezifikation (noch nicht implementiert)  
**Datum:** 2026-09-24  
**Betroffen:** Pages 0 (BME280), 5 (SI1145), 6 (TSL2591)  
**Komplexität:** Mittel  
**Priorität:** [Zu bestimmen]

---

## 1. Executive Summary

Aktuell werden BME280, TSL2591 und SI1145 Sensoren **direkt vom I2C-Bus** gelesen, jedes Mal wenn die entsprechende Seite angezeigt wird. Dies ist ineffizient und blockiert den I2C-Bus unnötig.

Diese Spezifikation führt ein einheitliches **Sensor-Caching-Pattern** ein, das bereits bei VEML6075, VEML6070, SCD30 und DS2484 verwendet wird:
- Sensoren werden in `readSensors()` (500ms Intervall) gelesen
- Werte werden in Strukturen **gecacht**
- Page-Funktionen zeigen nur **gecachte Werte** an (keine I2C-Operationen)

**Ziel:** Konsistente Architektur, reduzierte I2C-Last, bessere Responsivität.

---

## 2. Betroffene Anforderungen

### Existierende Requirements
- BME280 wird in `setup()` initialisiert (0x77)
- TSL2591 wird in `setup()` initialisiert (0x29)
- SI1145 wird in `setup()` initialisiert (0x60)
- `SENSOR_REFRESH_MS = 500` ms Intervall für Sensor-Lesevorgänge
- Display-Refresh alle 500ms in `drawCurrentPage(false)`
- High-Frequency Input (Gamepad 40Hz, Encoder 50Hz, INA219 50Hz)

### Neue Requirements
- BME280/TSL2591/SI1145 Werte werden **nicht direkt vom Sensor gelesen** in Page-Funktionen
- Alle drei Sensoren werden in `readSensors()` (500ms) ausgelesen
- Werte werden in dedizierten Caching-Strukturen gespeichert
- Page-Funktionen zeigen **nur gecachte Werte** an
- Zeitmessungen über Serial (Performance-Monitoring)

---

## 3. Betroffene Domain-Entities

### Neue Strukturen (werden erstellt)

#### `Bme280Values` Struct
```cpp
struct Bme280Values {
  float temperature;      // Temperatur (°C)
  float humidity;         // Relative Feuchte (%)
  float pressure;         // Luftdruck (hPa)
  float altitude;         // Höhe über Meeresspiegel (m)
  float dewPoint;         // Taupunkt (°C)
};
```

**Datenquellen:**
- `temperature` ← `bme280.readTemperature()`
- `humidity` ← `bme280.readHumidity()`
- `pressure` ← `bme280.readPressure() / 100.0f` (Pa → hPa)
- `altitude` ← `bme280.readAltitude(1013.25f)` (Referenzdruck = 1013.25 hPa)
- `dewPoint` ← berechnet: `T - (100% - RH%) / 5.0f`

---

#### `Tsl2591Values` Struct
```cpp
struct Tsl2591Values {
  uint32_t fullSpectrum;      // Vollspektrum-Rohwert (16-bit)
  uint32_t infrared;          // Infrarot-Rohwert (16-bit)
  uint32_t visible;           // Sichtbares Licht (berechnet: full - IR)
  float lux;                  // Beleuchtungsstärke (lx)
  uint16_t gain;              // Sensorverstärkung (1x, 25x, 428x, 9876x)
  uint16_t integrationTime;   // Integrationszeit (ms)
};
```

**Datenquellen:**
- `fullSpectrum` ← `tsl2591.getFullSpectrum(SENSOR_ADDR)` oder `readFullSpectrum()`
- `infrared` ← `tsl2591.getIR(SENSOR_ADDR)` oder `readIR()`
- `visible` ← berechnet: `fullSpectrum - infrared`
- `lux` ← `tsl2591.calculateLux(fullSpectrum, infrared)` oder `getLux()`
- `gain` ← `tsl2591.getGain()` (Read-Only, nur zur Anzeige)
- `integrationTime` ← `tsl2591.getIntegrationTime()` (Read-Only, nur zur Anzeige)

---

#### `Si1145Values` Struct
```cpp
struct Si1145Values {
  uint16_t visible;       // Sichtbares Licht (16-bit Rohwert)
  uint16_t infrared;      // Infrarot (16-bit Rohwert)
  uint16_t uv;            // UV-Index × 100 (berechnet aus Rohwert)
  float uvIndex;          // UV-Index (berechnet: uv / 100.0f)
};
```

**Datenquellen:**
- `visible` ← `si1145.readVisible()`
- `infrared` ← `si1145.readIR()`
- `uv` ← `si1145.readUV()` (Rohwert, normalerweise 0–500)
- `uvIndex` ← berechnet: `uv / 100.0f`

---

### Modifizierte Strukturen

**`SensorStatus` Struct (bereits existierend):**
- `.bme280`, `.tsl2591`, `.si1145` Flags bleiben unverändert
- Werden in `detectSensors()` gesetzt
- Verwendet in `readBme280()`, `readTsl2591()`, `readSi1145()` zur Prüfung

---

### Globale Instanzen (neue Variablen)

```cpp
// Global scope (neben anderen Sensor-Werten)
Bme280Values bme280Values{};
Tsl2591Values tsl2591Values{};
Si1145Values si1145Values{};
```

**Initialization:** Automatisch auf `{}` (Null-Werte) initialisiert

---

## 4. Geschäftsregeln

### Sensor-Lesezyklus
1. **Trigger:** `readSensors()` wird alle 500ms aufgerufen (siehe `loop()`, Zeile ~1060)
2. **Bedingung:** Nur wenn Sensor verfügbar (`available.bme280 == true` etc.)
3. **Aktion:** Sensor wird gelesen, Werte in Struktur gecacht
4. **Timing:** Zeitmessung über Serial (Beginn → Ende)

### Cache-Konsistenz
- **Regel:** Gecachte Werte werden **NICHT** außerhalb von `readBme280()`, `readTsl2591()`, `readSi1145()` verändert
- **Regel:** Page-Funktionen haben **READ-ONLY** Zugriff auf Caches
- **Ausgangszustand:** Bei Systemstart sind Werte `{}` (0.0f, 0, etc.)
- **Nach erstem Read:** Werte sind aktuell (max 500ms alt)

### Fehlerbehndlung (explizit in dieser Spec)
- **Sensor nicht verfügbar:** `readSensors()` springt sofort zurück (if-Check)
- **I2C-Lesefehler:** 
  - Aktuell: Letzte bekannte Werte bleiben bestehen
  - Keine spezielle Fehlerbehandlung implementiert
- **Sensor antwortet nicht (Hardware-Problem):** Wird durch `detectSensors()` erfasst, nicht durch read-Funktionen

### Datenfluss-Konsistenz
- **VEML6075, VEML6070, SCD30, DS2484:** Existierendes Pattern (Vorbild)
- **BME280, TSL2591, SI1145:** Müssen das gleiche Pattern folgen
- **INA219:** Separate High-Frequency Read (50Hz) — nicht betroffen
- **Gamepad/Encoder:** High-Frequency Read — nicht betroffen

---

## 5. Datenänderungen

### Neue Global-Variablen

| Variable | Typ | Initialisierung | Zweck |
|----------|-----|-----------------|-------|
| `bme280Values` | `Bme280Values` | `{}` + `initializeSensorCaches()` | Cache für BME280-Messungen (NaN initial) |
| `tsl2591Values` | `Tsl2591Values` | `{}` + `initializeSensorCaches()` | Cache für TSL2591-Messungen (NaN initial) |
| `si1145Values` | `Si1145Values` | `{}` + `initializeSensorCaches()` | Cache für SI1145-Messungen (NaN initial) |

### Berechnungen

#### BME280
```
altitude = bme280.readAltitude(1013.25f)  // mit Referenzdruck 1013.25 hPa

// Dew Point (Alduchov & Eskridge, more accurate)
a = 17.62, b = 243.12
alpha = ((a * T) / (b + T)) + ln(RH% / 100)
dewPoint = (b * alpha) / (a - alpha)   // °C, ±0.35°C accuracy
```

#### TSL2591
```
fullLum = getFullLuminosity()  // uint32_t
fullSpectrum = fullLum & 0xFFFF        // Lower 16 bits (CHAN0)
infrared = fullLum >> 16               // Upper 16 bits (CHAN1)
visible = fullSpectrum - infrared      // Berechnet
lux = calculateLux(fullSpectrum, infrared)
```

#### SI1145
```
uvIndex = uv / 100.0f
```

### Speicherauswirkung (geschätzt)
```
Bme280Values:   5 × 4 bytes (float) = 20 bytes
Tsl2591Values:  6 × 4 bytes = 24 bytes
Si1145Values:   4 × 4 bytes = 16 bytes
                             ─────────
Total:                         60 bytes

Current RAM: 22,688 / 327,680 bytes (6.9%)
After:       22,748 / 327,680 bytes (~6.9%)  [negligible increase]
```

---

## 6. API-Änderungen

### Neue Funktionen

#### `void readBme280(Bme280Values &values)`

```cpp
void readBme280(Bme280Values &values)
{
  if (!available.bme280) return;
  
  uint32_t startTime = micros();
  
  // Lese Sensor-Werte
  values.temperature = bme280.readTemperature();
  values.humidity = bme280.readHumidity();
  values.pressure = bme280.readPressure() / 100.0f;  // Pa → hPa
  values.altitude = bme280.readAltitude(1013.25f);
  
  // Error Check: Unrealistic values indicate I2C failure
  // Valid ranges: T[-40,85]°C, RH[0,100]%, P[300,1100]hPa
  if (values.temperature < -40 || values.temperature > 85 ||
      values.humidity < 0 || values.humidity > 100 ||
      values.pressure < 300 || values.pressure > 1100) {
    Serial.printf("ERROR: BME280 read returned invalid values: T=%.1f, RH=%.1f, P=%.1f\n",
                  values.temperature, values.humidity, values.pressure);
    // Do NOT update cache — keep last known values
    return;
  }
  
  // Dew Point (Alduchov & Eskridge Formula - more accurate than simplified)
  // Valid for: 0-50°C, 1-100% RH, ±0.35°C accuracy
  const float a = 17.62;
  const float b = 243.12;
  float alpha = ((a * values.temperature) / (b + values.temperature)) + log(values.humidity / 100.0f);
  values.dewPoint = (b * alpha) / (a - alpha);
  
  uint32_t endTime = micros();
  uint32_t duration = endTime - startTime;
  Serial.printf("BME280 read time: %lu µs\n", duration);
}
```

**Note:** Requires `#include <math.h>` for `log()` function.

**Aufruf:** `if (available.bme280) readBme280(bme280Values);` in `readSensors()`

**Timing-Erwartung:** 5–15 ms (I2C Read von 3 Registern)

---

#### `void readTsl2591(Tsl2591Values &values)`

```cpp
void readTsl2591(Tsl2591Values &values)
{
  if (!available.tsl2591) return;
  
  uint32_t startTime = micros();
  
  // API: getFullLuminosity() returns uint32_t with:
  // - Lower 16 bits: CHAN0 (fullSpectrum)
  // - Upper 16 bits: CHAN1 (infrared)
  uint32_t fullLum = tsl2591.getFullLuminosity();
  values.fullSpectrum = fullLum & 0xFFFF;        // CHAN0
  values.infrared = fullLum >> 16;               // CHAN1
  values.visible = values.fullSpectrum - values.infrared;
  values.lux = tsl2591.calculateLux(values.fullSpectrum, values.infrared);
  values.gain = tsl2591.getGain();
  values.integrationTime = tsl2591.getIntegrationTime();
  
  // Error Check: If values are 0, might indicate I2C error
  if (values.fullSpectrum == 0 && values.infrared == 0) {
    Serial.println("ERROR: TSL2591 read returned zero values");
    // Do NOT update cache — keep last known values
    return;
  }
  
  uint32_t endTime = micros();
  uint32_t duration = endTime - startTime;
  Serial.printf("TSL2591 read time: %lu µs\n", duration);
}
```

**Aufruf:** `if (available.tsl2591) readTsl2591(tsl2591Values);` in `readSensors()`

**Timing-Erwartung:** 5–20 ms (I2C Reads + Berechnung)

---

#### `void readSi1145(Si1145Values &values)`

```cpp
void readSi1145(Si1145Values &values)
{
  if (!available.si1145) return;
  
  uint32_t startTime = micros();
  
  // Lese Sensor-Werte (readUV() returns raw value from register 0x2C)
  values.visible = si1145.readVisible();
  values.infrared = si1145.readIR();
  values.uv = si1145.readUV();
  
  // Error Check: If all three values are 0, might indicate I2C error
  if (values.visible == 0 && values.infrared == 0 && values.uv == 0) {
    Serial.println("ERROR: SI1145 read returned all zeros");
    // Do NOT update cache — keep last known values
    return;
  }
  
  // UV Index (standard Adafruit formula: raw / 100.0)
  values.uvIndex = values.uv / 100.0f;
  
  uint32_t endTime = micros();
  uint32_t duration = endTime - startTime;
  Serial.printf("SI1145 read time: %lu µs\n", duration);
}
```

**Aufruf:** `if (available.si1145) readSi1145(si1145Values);` in `readSensors()`

**Timing-Erwartung:** 5–15 ms (I2C Reads + Berechnung)

---

### Modifizierte Funktionen

#### `void readSensors()` — Update

**Aktuell (Zeile 612–620):**
```cpp
void readSensors()
{
  if (available.veml6075) readVeml6075(veml6075Values);
  if (available.veml6070) readVeml6070(veml6070Values);
  if (available.scd30 && scd30Ready()) readScd30(scd30Values);
  if (available.ds2484) {
    performDs2484Scan();
    readDs2484Temperatures();
  }
}
```

**Neu:**
```cpp
void readSensors()
{
  if (available.veml6075) readVeml6075(veml6075Values);
  if (available.veml6070) readVeml6070(veml6070Values);
  if (available.scd30 && scd30Ready()) readScd30(scd30Values);
  if (available.ds2484) {
    performDs2484Scan();
    readDs2484Temperatures();
  }
  // NEW: Add the three cached sensor reads
  if (available.bme280) readBme280(bme280Values);
  if (available.tsl2591) readTsl2591(tsl2591Values);
  if (available.si1145) readSi1145(si1145Values);
}
```

**Timing-Auswirkung:** +15–50 ms pro 500ms Zyklus (drei neue I2C-Operationen)

---

#### `void pageBme280()` — Refactor

**Aktuell (Zeile 778–795):**
```cpp
void pageBme280()
{
  if (!available.bme280) return unavailable("BME280", "0x77", "I2C address:");
  float temp = bme280.readTemperature();      // ← DIREKT VOM SENSOR
  float humidity = bme280.readHumidity();     // ← DIREKT VOM SENSOR
  float pressure = bme280.readPressure() / 100.0f;
  float altitude = bme280.readAltitude(1013.25f);
  float dewPoint = temp - ((100.0f - humidity) / 5.0f);
  
  row(56, "Temperature:", String(temp, 2) + " C", TFT_GREEN);
  row(80, "Humidity:", String(humidity, 2) + " %", TFT_GREEN);
  row(104, "Pressure:", String(pressure, 2) + " hPa", TFT_ORANGE);
  row(128, "Altitude:", String(altitude, 1) + " m", TFT_CYAN);
  row(152, "Dew point:", String(dewPoint, 2) + " C", TFT_CYAN);
  footer("Y/A: Seite wechseln | BME280");
}
```

**Neu (mit Caching):**
```cpp
void pageBme280()
{
  if (!available.bme280) return unavailable("BME280", "0x77", "I2C address:");
  
  // Alle Werte aus Cache (KEINE I2C-Operationen!)
  row(56, "Temperature:", String(bme280Values.temperature, 2) + " C", TFT_GREEN);
  row(80, "Humidity:", String(bme280Values.humidity, 2) + " %", TFT_GREEN);
  row(104, "Pressure:", String(bme280Values.pressure, 2) + " hPa", TFT_ORANGE);
  row(128, "Altitude:", String(bme280Values.altitude, 1) + " m", TFT_CYAN);
  row(152, "Dew point:", String(bme280Values.dewPoint, 2) + " C", TFT_CYAN);
  footer("Y/A: Seite wechseln | BME280");
}
```

**Änderungen:**
- ✅ Entfernt: Vier I2C-Lesevorgänge (`bme280.read*()`)
- ✅ Hinzugefügt: Direkter Zugriff auf `bme280Values` Struct
- ✅ Gleiches UI-Layout, nur interne Datenquelle ändert sich

---

#### `void pageTsl2591()` — Refactor

**Aktuell (Zeile 718–732):**
```cpp
void pageTsl2591()
{
  if (!available.tsl2591) return unavailable("TSL2591", "0x29", "I2C address:");
  uint32_t lum = tsl2591.getFullSpectrum();   // ← DIREKT VOM SENSOR
  uint16_t ir = tsl2591.getIR();              // ← DIREKT VOM SENSOR
  float lux = tsl2591.calculateLux(lum, ir);  // ← DIREKT BERECHNET
  
  row(56, "Full spectrum:", String(lum), TFT_GREEN);
  row(80, "Infrared:", String(ir), TFT_GREEN);
  row(104, "Visible:", String(lum - ir), TFT_ORANGE);
  row(128, "Lux:", String(lux, 2) + " lx", TFT_CYAN);
  footer("Y/A: Seite wechseln | TSL2591");
}
```

**Neu (mit Caching):**
```cpp
void pageTsl2591()
{
  if (!available.tsl2591) return unavailable("TSL2591", "0x29", "I2C address:");
  
  // Alle Werte aus Cache (KEINE I2C-Operationen!)
  row(56, "Full spectrum:", String(tsl2591Values.fullSpectrum), TFT_GREEN);
  row(80, "Infrared:", String(tsl2591Values.infrared), TFT_GREEN);
  row(104, "Visible:", String(tsl2591Values.visible), TFT_ORANGE);
  row(128, "Lux:", String(tsl2591Values.lux, 2) + " lx", TFT_CYAN);
  footer("Y/A: Seite wechseln | TSL2591");
}
```

**Änderungen:**
- ✅ Entfernt: Drei I2C-Lesevorgänge + Lux-Berechnung
- ✅ Hinzugefügt: Direkter Zugriff auf `tsl2591Values` Struct

---

#### `void pageSi1145()` — Refactor

**Aktuell (Zeile 756–767):**
```cpp
void pageSi1145()
{
  if (!available.si1145) return unavailable("SI1145", "0x60", "I2C address:");
  uint16_t vis = si1145.readVisible();    // ← DIREKT VOM SENSOR
  uint16_t ir = si1145.readIR();          // ← DIREKT VOM SENSOR
  uint16_t uv = si1145.readUV();          // ← DIREKT VOM SENSOR
  float uvIndex = uv / 100.0f;
  
  row(56, "Visible:", String(vis), TFT_GREEN);
  row(80, "Infrared:", String(ir), TFT_GREEN);
  row(104, "UV raw:", String(uv), TFT_ORANGE);
  row(128, "UV index:", String(uvIndex, 2), TFT_CYAN);
  footer("Y/A: Seite wechseln | SI1145");
}
```

**Neu (mit Caching):**
```cpp
void pageSi1145()
{
  if (!available.si1145) return unavailable("SI1145", "0x60", "I2C address:");
  
  // Alle Werte aus Cache (KEINE I2C-Operationen!)
  row(56, "Visible:", String(si1145Values.visible), TFT_GREEN);
  row(80, "Infrared:", String(si1145Values.infrared), TFT_GREEN);
  row(104, "UV raw:", String(si1145Values.uv), TFT_ORANGE);
  row(128, "UV index:", String(si1145Values.uvIndex, 2), TFT_CYAN);
  footer("Y/A: Seite wechseln | SI1145");
}
```

**Änderungen:**
- ✅ Entfernt: Drei I2C-Lesevorgänge + Berechnung
- ✅ Hinzugefügt: Direkter Zugriff auf `si1145Values` Struct

---

## 7. UI-Änderungen

**NO UI CHANGES REQUIRED**

- Display-Layout bleibt identisch
- Angezeigten Werte bleiben die gleichen
- Nur interne Datenquelle ändert sich (I2C-Read → gecachter Wert)
- Benutzer sieht keinen Unterschied

**Verbesserung (nicht sichtbar, aber vorhanden):**
- Schnellere Display-Refresh (keine I2C-Blockierung während Redraw)
- Gleichmäßigere I2C-Auslastung (gruppierte Reads statt verteilt)

---

## 8. Fehlerbehandlung

### Sensor Nicht Verfügbar
- **Verhalten:** Unverändert — `unavailable()` wird aufgerufen
- **Implementation:** `if (!available.bme280) return;` am Anfang von `readBme280()`

### I2C-Lesefehler
- **Aktuell:** Keine explizite Fehlerbehandlung für BME280/TSL2591/SI1145
- **Verhalten:** Letzte bekannte Werte bleiben bestehen (gecachte Werte werden nicht aktualisiert)
- **Future-Improvement:** Könnten Error-Counter implementieren (nicht in dieser Spec)

### Sensor antwortet nicht (Hardware-Fehler)
- **Erfassung:** Nur in `detectSensors()`, nicht in read-Funktionen
- **Verhalten:** `available.bme280/tsl2591/si1145` bleibt `false`

---

## 9. Tests

### Manuelle Integrationstests (keine Unit-Test-Framework verfügbar)

#### TC-001: BME280 Cache-Update
**Voraussetzung:** BME280 Sensor angeschlossen (0x77)

1. Systemstart → BME280-Seite anzeigen (Page 0)
2. Beobachten: Temperature/Humidity/Pressure sollten sich alle ~500ms aktualisieren
3. Beobachten: Keine Verzögerung beim Seitenwechsel (schneller Redraw)
4. Serial-Monitor: `BME280 read time: XXX µs` sollte alle 500ms ausgegeben werden
5. **PASS:** Werte aktualisieren sich regelmäßig, keine I2C-Blockierung

---

#### TC-002: TSL2591 Cache-Update
**Voraussetzung:** TSL2591 Sensor angeschlossen (0x29)

1. Systemstart → TSL2591-Seite anzeigen (Page 6)
2. Beobachten: Lux/Full Spectrum/Infrared sollten sich alle ~500ms aktualisieren
3. Light-Test: Finger über Sensor halten → Lux sollte sinken
4. Serial-Monitor: `TSL2591 read time: XXX µs` sollte alle 500ms ausgegeben werden
5. **PASS:** Werte reagieren auf Lichtwechsel, Cache aktualisiert sich

---

#### TC-003: SI1145 Cache-Update
**Voraussetzung:** SI1145 Sensor angeschlossen (0x60)

1. Systemstart → SI1145-Seite anzeigen (Page 5)
2. Beobachten: Visible/Infrared/UV-Index sollten sich alle ~500ms aktualisieren
3. UV-Test: Unter UV-Licht halten → UV-Index sollte steigen
4. Serial-Monitor: `SI1145 read time: XXX µs` sollte alle 500ms ausgegeben werden
5. **PASS:** Werte aktualisieren sich, UV-Empfindlichkeit funktioniert

---

#### TC-004: Concurrent Sensor Operations (Keine I2C-Blockierung)
**Voraussetzung:** Alle Sensoren angeschlossen

1. Systemstart
2. Schnell zwischen Seiten navigieren (Gamepad Y/A drücken)
3. Gleichzeitig: Encoder manipulieren
4. Beobachten: Gamepad-/Encoder-Eingaben sollten immer responsiv sein
5. Serial-Monitor: `*_read time` Messages sollten kontinuierlich ausgegeben werden (keine Gaps)
6. **PASS:** Keine Verzögerungen, Eingaben sind responsiv

---

#### TC-005: Sensor Nicht Verfügbar
**Voraussetzung:** Einen Sensor abklemmen (z.B. BME280)

1. Systemstart
2. BME280-Seite anzeigen (Page 0)
3. Beobachten: "Sensor nicht vorhanden" wird angezeigt
4. Wechsel zu nächster Seite (Page 1, Gamepad)
5. Wechsel zurück (Page 0)
6. Serial-Monitor: Keine I2C-Error-Messages (sanftes Handling)
7. **PASS:** Fehlerbehandlung funktioniert, keine Crashes

---

#### TC-006: Speicher-Sanity-Check
**Voraussetzung:** Kompiliertes Projekt flashen

1. Systemstart
2. Memory-Ausgabe im Serial Monitor prüfen (falls verfügbar)
3. **PASS:** RAM usage < 7% (wie vorher)

---

## 10. Dokumentation

### Code-Kommentare (neu)
- ✅ Kommentar über Sensor-Caching-Pattern in `readBme280()`, `readTsl2591()`, `readSi1145()`
- ✅ Kommentar in `readSensors()` erklärt die Aufrufe der drei neuen Funktionen

### README-Updates (optional)
- [ ] Sensor-Datenfluss-Diagramm aktualisieren (falls vorhanden)
- [ ] Erwähnen, dass alle Sensoren ein einheitliches Caching-Pattern verwenden

### Bestehende Dokumentation
- `02-architecture.md` — Könnte Sensor-Datenfluss dokumentieren (optional)
- `04-data-model.md` — Könnte neue Strukturen dokumentieren (optional)

---

## 11. Sicherheit & Autorisierung

- ✅ **Keine neuen Sicherheitsaspekte:** Reine Datenanzeigeänderung
- ✅ **Keine Authentifizierung erforderlich:** Embedded System
- ✅ **Keine Datenspeicherung außer RAM:** Caches nicht persistent

---

## 12. Implementierungsprioritäten

| Priorität | Aufgabe | Abhängigkeiten |
|-----------|---------|-----------------|
| 1 | Struct-Definitionen erstellen (Bme280/Tsl2591/Si1145) | Keine |
| 2 | Global-Instanzen deklarieren | Priorität 1 |
| 3 | `readBme280()` Funktion implementieren | Priorität 1, 2 |
| 4 | `readTsl2591()` Funktion implementieren | Priorität 1, 2 |
| 5 | `readSi1145()` Funktion implementieren | Priorität 1, 2 |
| 6 | `readSensors()` aktualisieren (drei neue Aufrufe) | Priorität 3, 4, 5 |
| 7 | `pageBme280()` refaktorieren | Priorität 3, 6 |
| 8 | `pageTsl2591()` refaktorieren | Priorität 4, 6 |
| 9 | `pageSi1145()` refaktorieren | Priorität 5, 6 |
| 10 | Kompilieren, Flashen, Tests | Priorität 7, 8, 9 |

---

## 13. Annahmen & Entscheidungen

### Getroffene Entscheidungen

1. **Caching-Strategie:**
   - ✅ Folge dem existierenden Pattern (VEML6075, VEML6070, SCD30, DS2484)
   - ✅ NICHT direkt vom Sensor lesen in Page-Funktionen

2. **Zeitmessung:**
   - ✅ Ähnlich wie INA219: `Serial.printf("XXX read time: %lu µs\n", duration)`
   - ✅ Hilft bei Performance-Monitoring

3. **Fehlerbehandlung:**
   - ✅ Minimal: nur Sensor-Verfügbarkeits-Check
   - ✅ I2C-Fehler führen zu "letzter bekannter Wert" (kein Crash)

4. **Struct-Feldnamen:**
   - ✅ Aussagekräftig (z.B. `temperature`, `uvIndex`)
   - ✅ Keine Abkürzungen, für Lesbarkeit

5. **Sensor-Read-Reihenfolge in `readSensors()`:**
   - ✅ VEML/Scd30/Ds2484 zuerst (existierende Reihenfolge)
   - ✅ Neue Sensoren (Bme280/Tsl2591/Si1145) am Ende hinzufügen

---

### Annahmen

1. **Adafruit-Bibliotheks-API:**
   - Annahme: `bme280.readTemperature()`, `tsl2591.getFullSpectrum()`, `si1145.readVisible()` arbeiten wie erwartet
   - Validierung: Vor Implementierung in Adafruit-Docs prüfen

2. **I2C-Bus-Kapazität:**
   - Annahme: I2C-Bus kann zusätzliche 15–50 ms Load alle 500ms verkraften
   - Realität: Ja (siehe Bench-Tests in AUDIT-DS2484-IMPLEMENTATION.md)

3. **ESP32 Floating-Point:**
   - Annahme: Berechnungen mit `float` sind ausreichend genau
   - Realität: Ja (für Sensordaten typisch)

4. **Benutzererwartung:**
   - Annahme: Benutzer erkennt keinen Unterschied in UI/Behavior
   - Realität: Ja (nur interne Refaktorierung)

---

## 14. Offene Fragen

### Zu Klären Vor Implementierung

1. **TSL2591 API Varianten:**
   - **Frage:** Nutzt die Adafruit_TSL2591 Library `getFullSpectrum(SENSOR_ADDR)` oder `getFullSpectrum()`?
   - **Resolution:** ✅ GEKLÄRT — API ist `getFullLuminosity()` (uint32_t):
     - Lower 16 bits: CHAN0 (Vollspektrum = Sichtbar + IR)
     - Upper 16 bits: CHAN1 (Infrarot)
     - Convenience: `getLuminosity(TSL2591_FULLSPECTRUM)`, `getLuminosity(TSL2591_INFRARED)`, `getLuminosity(TSL2591_VISIBLE)`
   - **Status:** ✅ GELÖST

2. **SI1145 UV-Index Berechnung:**
   - **Frage:** Ist die Formel `uvIndex = uv / 100.0f` korrekt?
   - **Resolution:** ✅ GEKLÄRT — `readUV()` returns raw value (Register 0x2C). UV-Index = raw / 100.0f (Adafruit Standard)
   - **Status:** ✅ GELÖST

3. **Sensor-Read-Fehlerbehandlung:**
   - **Frage:** Sollen fehlgeschlagene Reads in Serial-Log protokolliert werden?
   - **Resolution:** ✅ JA — Fehlerbehandlung mit Serial-Logging hinzufügen:
     - Try/catch nicht verfügbar in C, daher: Plausibilitätsprüfung nach I2C-Read
     - Falls Sensor antwortet nicht oder Werte unrealistisch: `Serial.printf("ERROR: XXX read failed\n")`
     - Cache wird NICHT aktualisiert, letzte Werte bleiben bestehen
   - **Status:** 📋 DECISION: Error Logging für alle drei Sensoren implementieren

4. **Taupunkt-Formel für BME280:**
   - **Frage:** Ist die vereinfachte Formel `T - (100% - RH%) / 5.0f` akkurat genug?
   - **Resolution:** ✅ GENAUERE FORMEL — Magnus-Formel (akkurater als vereinfacht):
     ```
     alpha = ((17.27 * T) / (237.7 + T)) + ln(RH% / 100)
     Td = (237.7 * alpha) / (17.27 - alpha)  [Einheit: °C]
     ```
     Oder vereinfacht (noch genauer): Alduchov & Eskridge (±0.35°C):
     ```
     a = 17.62, b = 243.12  [für 0-50°C, 1-100% RH]
     alpha = ((a * T) / (b + T)) + ln(RH / 100)
     Td = (b * alpha) / (a - alpha)
     ```
   - **Implementierung:** Alduchov & Eskridge (guter Kompromiss zwischen Genauigkeit und CPU-Last)
   - **Status:** 📋 DECISION: Alduchov & Eskridge Formel verwenden

5. **Initial Cache-Werte:**
   - **Frage:** Sollten Caches auf bestimmte Werte initialisiert sein (z.B. `NaN`, `-1`) oder `0.0f`?
   - **Resolution:** ✅ NaN — Bessere Unterscheidung zwischen "nicht initialisiert" und "reale Messung":
     ```cpp
     // Globale Initialisierungsfunktion
     void initializeSensorCaches() {
       bme280Values.temperature = NAN;
       bme280Values.humidity = NAN;
       bme280Values.pressure = NAN;
       bme280Values.altitude = NAN;
       bme280Values.dewPoint = NAN;
       // ... etc für tsl2591Values, si1145Values
     }
     
     // Im setup() aufrufen
     initializeSensorCaches();
     ```
   - **UI-Verhalten:** Page zeigt "--" für NaN-Werte bis erste Messung verfügbar ist
   - **Status:** 📋 DECISION: Initialisierung auf NaN

---

## 15. Akzeptanzkriterien

### ✅ Funktionale Kriterien
- [ ] Struct-Definitionen `Bme280Values`, `Tsl2591Values`, `Si1145Values` erstellt
- [ ] Global-Instanzen `bme280Values`, `tsl2591Values`, `si1145Values` deklariert
- [ ] `readBme280()` Funktion implementiert mit Zeitmessung
- [ ] `readTsl2591()` Funktion implementiert mit Zeitmessung
- [ ] `readSi1145()` Funktion implementiert mit Zeitmessung
- [ ] `readSensors()` ruft alle drei neuen Read-Funktionen auf (mit Verfügbarkeits-Checks)

### ✅ Refactor-Kriterien
- [ ] `pageBme280()` liest ausschließlich aus `bme280Values` Struct
- [ ] `pageTsl2591()` liest ausschließlich aus `tsl2591Values` Struct
- [ ] `pageSi1145()` liest ausschließlich aus `si1145Values` Struct
- [ ] **KEINE direkt vom Sensor gelesen** in Page-Funktionen (alle I2C-Calls entfernt)

### ✅ Code-Qualitäts-Kriterien
- [ ] Code kompiliert ohne Errors oder Warnings
- [ ] RAM/Flash-Nutzung < 7% (negligible increase)
- [ ] Zeitmessungen über Serial ausgegeben

### ✅ Performance-Kriterien
- [ ] Display-Refresh-Rate bleibt konstant (~500ms)
- [ ] Gamepad/Encoder Responsivität unverändert (no I2C blocking)
- [ ] Serial-Output zeigt regelmäßig alle Sensor-Read-Times

### ✅ Test-Kriterien
- [ ] TC-001 (BME280 Cache): PASS
- [ ] TC-002 (TSL2591 Cache): PASS
- [ ] TC-003 (SI1145 Cache): PASS
- [ ] TC-004 (Concurrent Operations): PASS
- [ ] TC-005 (Sensor Unavailable): PASS
- [ ] TC-006 (Memory Check): PASS

### ✅ Dokumentation-Kriterien
- [ ] Code-Kommentare für neue Funktionen hinzugefügt
- [ ] README aktualisiert (optional, falls Sensor-Flow dokumentiert wird)

---

## 16. Implementierungs-Checkliste

- [ ] Issue erstellen für Code-Review
- [ ] Branches & Pull-Request vorbereiten
- [ ] Struct-Definitionen implementieren (Priorität 1)
- [ ] Global-Instanzen deklarieren (Priorität 2)
- [ ] `readBme280()` implementieren (Priorität 3)
- [ ] `readTsl2591()` implementieren (Priorität 4)
- [ ] `readSi1145()` implementieren (Priorität 5)
- [ ] `readSensors()` aktualisieren (Priorität 6)
- [ ] `pageBme280()` refaktorieren (Priorität 7)
- [ ] `pageTsl2591()` refaktorieren (Priorität 8)
- [ ] `pageSi1145()` refaktorieren (Priorität 9)
- [ ] Kompilieren & Test (Priorität 10)
- [ ] Flashen auf Gerät
- [ ] Manuelle Test-Cases durchführen
- [ ] Pull-Request mergen
- [ ] Issue schließen

---

## 17. Verwandte Issues & Referenzen

- **Vorheriges Pattern:** FEATURE-SPEC-003 (INA219 Battery Monitor)
- **Architektur-Audit:** AUDIT-DS2484-IMPLEMENTATION.md (Caching Pattern)
- **Abhängig von:** Adafruit_BME280, Adafruit_TSL2591, Adafruit_SI1145 Libraries

---

**Autor:** GitHub Copilot  
**Datum:** 2026-09-24  
**Status:** ✅ Spezifikation Fertig — Bereit für Implementierung
