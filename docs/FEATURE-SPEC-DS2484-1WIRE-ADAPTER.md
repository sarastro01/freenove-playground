# Feature-Spezifikation: DS2484 I2C-zu-1-Wire Adapter mit Temperatursensoren

**Status:** Spezifikation (Ready for Implementation)  
**Datum:** 2026-09-23  
**Hardware:** Adafruit DS2484 I2C to 1-Wire Bus Adapter Breakout  
**Betroffen:** Neue Sensor-Seite (Page 9), Sensor-Verwaltung, I2C-Bus

---

## 1. Betroffene Anforderungen

### Existierende Requirements (Ausgangslage)
- Sensor-Dashboard mit 9 Seiten für verschiedene Sensoren (Seiten 0-8)
- Alle Sensoren werden über I2C erkannt und initialisiert
- Sensor-Daten werden alle 500ms aktualisiert (`SENSOR_REFRESH_MS`)
- Sensor-Verfügbarkeit wird in `SensorStatus` Struct getracked
- Display-Navigation durch Gamepad-Tasten (Y = vorherig, A = nächst)
- Konsistente UI-Layout mit `row()` Hilfsfunktion
- OneWire und DallasTemperature Libraries sind bereits als Abhängigkeiten vorhanden

### Neue Requirements (durch diese Feature)
- **DS2484 I2C-zu-1-Wire Adapter Detection:** Hardware wird beim Bootup auf I2C-Adresse 0x18 erkannt
- **1-Wire-Bus Scan:** Automatische Erkennung aller Temperatursensoren auf dem 1-Wire-Bus über DS2484
- **Multi-Sensor-Typ Support:** Unterstützung aller 1-Wire-Temperatursensor-Typen (DS18B20, DS1822, DS1820, etc.)
- **Maximal 3 Sensoren:** Hardware-Limit des Adafruit Breakout Boards
- **Eindeutige Identifikation:** Anzeige der vollständigen 64-Bit ROM-ID (8 Bytes HEX) pro Sensor
- **Temperaturmessung:** Auslesen und Anzeige der Temperatur mit 12-Bit Auflösung (Standard)
- **Status-Monitoring:** Anzeige des DS2484-Status (Connected, Error, Sensor Count)
- **Neue Sensor-Seite:** Page 9 mit allen bis zu 3 Sensoren auf einer Seite
- **PAGE_COUNT erhöhen:** Von 9 auf 10 Seiten (Zirkular-Navigation aktualisiert)
- **Polling-Zeiten:** Bus-Scan alle 60 Sekunden, Temperatur-Messung alle 3 Sekunden
- **Error Display:** Fehlerhafte/Offline-Sensoren zeigen "Disconnected" Status

---

## 2. Betroffene Domain-Entities

### Neue Structs

#### `Ds18b20Sensor` - Einzelner Temperatursensor
```cpp
struct Ds18b20Sensor {
  uint8_t address[8];           // 64-Bit ROM-ID (FamilyCode + SerialNumber + CRC)
  uint8_t familyCode;           // 0x28 (DS18B20), 0x22 (DS1822), 0x10 (DS1820), etc.
  float temperature;            // Letzte gemessene Temperatur (°C)
  uint32_t lastReadMs;          // Zeitstempel der letzten Messung (ms)
  bool available;               // Sensor erkannt und antwortet
  uint8_t errorCount;           // Fehlerzähler (für CRC-Fehler)
};
```

#### `Ds2484Values` - Hauptstruktur für DS2484 I2C Adapter
```cpp
struct Ds2484Values {
  bool masterPresent;           // DS2484 erkannt und funktionsfähig
  uint8_t sensorCount;          // Anzahl erkannter Temperatursensoren (max 3)
  Ds18b20Sensor sensors[3];     // Array von maximal 3 Sensoren
  uint32_t lastScanMs;          // Zeitstempel des letzten Bus-Scans (ms)
  uint32_t lastConversionMs;    // Zeitstempel der letzten Temperaturmessung (ms)
};
```

### Betroffene Structs

#### `SensorStatus` - Erweiterung
```cpp
struct SensorStatus {
  // ... existing sensors ...
  bool ds2484;                  // DS2484 I2C Adapter erkannt
};
```

### Globale Variablen - Neue Definitionen

```cpp
// DS2484 I2C Adapter Initialisierung
Ds2484Values ds2484Values{};
OneWire oneWire(SDA_PIN, SCL_PIN);         // 1-Wire über I2C via DS2484
DallasTemperature sensors(&oneWire);       // DallasTemperature API
```

---

## 3. Geschäftsregeln

### DS2484 I2C Adapter Recognition
- **I2C-Adresse:** 0x18 (fest, keine Alternativen)
- **Hardware:** Adafruit DS2484 I2C to 1-Wire Bus Adapter Breakout
- **Erkennung:** Beim Bootup `Wire.beginTransmission(0x18)` durchführen
- **Status:** Nur wenn erfolgreich initialisiert → `available.ds2484 = true`
- **Kommunikation:** Vollständig über I2C (kein GPIO für 1-Wire erforderlich)
- **Pull-up-Widerstände:** Nicht erforderlich (bereits auf Breakout-Board vorhanden)

### 1-Wire-Bus Scanning
- **Scan-Timing:** Periodischer Scan alle 60 Sekunden nach neuen/verlorenen Sensoren
- **Unterstützte Sensor-Typen:** Alle 1-Wire-Temperatursensor-Kompatiblen:
  - 0x28: DS18B20 (Standard)
  - 0x22: DS1822
  - 0x10: DS1820
  - Weitere kompatible Typen (werden alle erkannt)
- **ROM-ID Format:** 8 Bytes: [FamilyCode(1 Byte) | SerialNumber(6 Bytes) | CRC(1 Byte)]
- **Maximale Sensoren:** 3 (Hardware-Limit des Adafruit Breakout Boards)
- **Sensor-Erkennung:** Als "verfügbar" markieren nach erfolgreichem ROM-ID Scan

### Temperaturmessung
- **Auflösung:** Standard 12-Bit (0,0625°C Genauigkeit, ~750ms Konvertierungszeit)
- **Messintervall:** Alle 3 Sekunden (asynchrone Konvertierung über DS2484 I2C-Interface)
- **Timeout:** Max 1000ms für Temperaturabfrage pro Sensor
- **Wertebereich:** -55°C bis +125°C (typisch für 1-Wire Temperatursensoren)
- **Display-Precision:** 2 Dezimalstellen für Temperaturanzeige (z.B. 23.45°C)
- **Konvertierungs-Mode:** Asynchron (requestTemperatures, später Abfrage)

### Fehlerbehandlung
- **CRC-Fehler:** Fehler-Counter pro Sensor inkrementieren
- **Sensor-Offline-Markierung:** Nach 3 konsekutiven Fehlern (CRC oder Timeout) → Status "Disconnected"
- **Bus-Fehler:** Wenn 1-Wire-Bus über DS2484 fehlschlägt → DS2484 Status zeigt "Error"
- **Sensor-Disconnect:** Sensor nicht mehr im Scan gefunden (nach 60s Scan) → Status "Disconnected"
- **Recovery:** Fehler-Counter wird auf 0 zurückgesetzt nach erfolgreicher Messung

### Display-Logik
- **Sensoren ≤ 3:** Alle Sensoren auf einer Seite anzeigen (kein Paging nötig)
- **ROM-ID Format:** Immer vollständige 8-Byte HEX-Darstellung (z.B. "28:AB:CD:EF:01:02:03:04")
- **Verfügbare vs. Offline:** Online-Sensoren zeigen Temperaturwert, Offline-Sensoren zeigen "Disconnected"

---

## 4. Datenänderungen

### Neue #define Konstanten
| Konstante | Wert | Beschreibung |
|-----------|------|-------------|
| `DS2484_ADDR` | 0x18 | I2C-Adresse des DS2484 Adapters (fest) |
| `MAX_DS2484_SENSORS` | 3 | Maximale Anzahl Sensoren pro DS2484 |
| `DS2484_SCAN_INTERVAL_MS` | 60000 | Bus-Scan Intervall (60 Sekunden) |
| `DS2484_TEMP_INTERVAL_MS` | 3000 | Temperatur-Messung Intervall (3 Sekunden) |
| `PAGE_COUNT` | 10 | Erhöht von 9 auf 10 (neue DS2484-Seite) |

### Neue globale Variablen
```cpp
// Im globalen Scope (nach existierenden Sensor-Variablen)
Ds2484Values ds2484Values{};
OneWire oneWire(SDA_PIN, SCL_PIN);
DallasTemperature sensors(&oneWire);
uint32_t nextDs2484ScanMs = 0;
uint32_t nextDs2484TempMs = 0;
```

### Speicherverbrauch-Estimation
```
Ds2484Values struct:              ~60 Bytes (3 Sensoren × 20 Bytes + Header)
OneWire library overhead:         ~100 Bytes
DallasTemperature library:        ~200 Bytes
Gesamt neu:                       ~360 Bytes
Verfügbar auf ESP32:              ~320 KB RAM
Prozentsatz:                      0.11%  ✓ Akzeptabel
```

---

## 5. API-Änderungen

### Neue Funktionen

#### `initDs2484()` - Initialization
```cpp
void initDs2484()
{
  // DS2484 I2C Erkennung
  Wire.beginTransmission(DS2484_ADDR);
  available.ds2484 = (Wire.endTransmission() == 0);
  
  if (!available.ds2484) return;
  
  // OneWire/DallasTemperature Initialization
  sensors.begin();
  
  // Initiales Bus-Scan durchführen
  performDs2484Scan();
}
```

#### `performDs2484Scan()` - 1-Wire Bus Discovery
```cpp
void performDs2484Scan()
{
  if (!available.ds2484) return;
  
  // DeviceCount auslesen
  uint8_t newCount = sensors.getDeviceCount();
  ds2484Values.sensorCount = min(newCount, (uint8_t)MAX_DS2484_SENSORS);
  
  // Für jeden Sensor: ROM-ID lesen und speichern
  for (uint8_t i = 0; i < ds2484Values.sensorCount; i++) {
    uint8_t address[8];
    if (sensors.getAddress(address, i)) {
      memcpy(ds2484Values.sensors[i].address, address, 8);
      ds2484Values.sensors[i].familyCode = address[0];
      ds2484Values.sensors[i].available = true;
      ds2484Values.sensors[i].errorCount = 0;
    }
  }
  
  ds2484Values.lastScanMs = millis();
}
```

#### `readDs2484Temperatures()` - Temperaturmessung
```cpp
void readDs2484Temperatures()
{
  if (!available.ds2484) return;
  
  // Temperatur-Konvertierung anfordern
  sensors.requestTemperatures();
  
  // Für jeden Sensor: Temperatur lesen
  for (uint8_t i = 0; i < ds2484Values.sensorCount; i++) {
    if (!ds2484Values.sensors[i].available) continue;
    
    float tempC = sensors.getTempC((uint8_t *)ds2484Values.sensors[i].address);
    
    if (tempC == DEVICE_DISCONNECTED_C) {
      ds2484Values.sensors[i].errorCount++;
      if (ds2484Values.sensors[i].errorCount >= 3) {
        ds2484Values.sensors[i].available = false;
      }
    } else {
      ds2484Values.sensors[i].temperature = tempC;
      ds2484Values.sensors[i].errorCount = 0;
      ds2484Values.sensors[i].lastReadMs = millis();
    }
  }
  
  ds2484Values.lastConversionMs = millis();
}
```

#### `formatDs2484RomId()` - ROM-ID Formatierung
```cpp
String formatDs2484RomId(const uint8_t address[8])
{
  // Vollständige 8-Byte ROM-ID als HEX
  // Format: "28:AB:CD:EF:01:02:03:04"
  char buffer[24];
  sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
    address[0], address[1], address[2], address[3],
    address[4], address[5], address[6], address[7]);
  return String(buffer);
}
```

#### `pageDs2484()` - Display-Funktion
Siehe Abschnitt 6: UI-Änderungen

### Betroffene Funktionen

#### `setup()` - Änderungen
```cpp
void setup()
{
  // ... existing code ...
  
  // Neue Zeile hinzufügen (nach anderen Sensor-Initialierungen):
  initDs2484();
  
  // ... rest of setup ...
}
```

#### `loop()` - Änderungen
```cpp
void loop()
{
  uint32_t now = millis();
  
  // ... existing code ...
  
  // Periodischer 1-Wire Bus-Scan (alle 60 Sekunden)
  if (available.ds2484 && (int32_t)(now - nextDs2484ScanMs) >= 0) {
    performDs2484Scan();
    nextDs2484ScanMs = now + DS2484_SCAN_INTERVAL_MS;
  }
  
  // Temperatur-Messung (alle 3 Sekunden)
  if (available.ds2484 && (int32_t)(now - nextDs2484TempMs) >= 0) {
    readDs2484Temperatures();
    nextDs2484TempMs = now + DS2484_TEMP_INTERVAL_MS;
  }
  
  // ... rest of loop ...
}
```

#### `drawCurrentPage()` - Änderungen
```cpp
// Im titles[] array:
const char *titles[] = {
  "BME280 (0x77)",
  "Gamepad QT (0x50)",
  "INA219 (0x40)",
  "Rotary Encoder (0x36)",
  "SCD30 (0x61)",
  "SI1145 (0x60)",
  "TSL2591 (0x29)",
  "VEML6070 (0x38/0x39)",
  "VEML6075 (0x10)",
  "DS2484 1-Wire (0x18)"  // Neue Seite
};

// Im switch statement:
switch (currentPage) {
  // ... existing cases ...
  case 9: pageDs2484(); break;  // Neue Page 9
}
```

### Library Dependencies - Status
- ✅ OneWire @ ^2.3.8 - bereits in platformio.ini vorhanden
- ✅ DallasTemperature @ ^4.0.4 - bereits in platformio.ini vorhanden
- ✓ Keine zusätzlichen Abhängigkeiten erforderlich (DS2484 wird über OneWire/DallasTemperature abstrahiert)

---

## 6. UI-Änderungen

### Layout für Page 9 (DS2484 1-Wire Adapter)
```
Zeile 56:  DS2484 Status:   OK - 3 Sensoren    [TFT_GREEN]

Zeile 80:  Sensor 1 ID:     28:AB:CD:EF:01:02:03:04
Zeile 96:  Temperatur:      24.56 °C           [TFT_CYAN]

Zeile 120: Sensor 2 ID:     28:AB:CD:EF:01:02:03:05
Zeile 136: Temperatur:      25.12 °C           [TFT_CYAN]

Zeile 160: Sensor 3 ID:     28:AB:CD:EF:01:02:03:06
Zeile 176: Temperatur:      23.87 °C           [TFT_CYAN]

Zeile 294: Footer: "Y/A: Seite wechseln | DS2484"
```

### Layout für Offline-Sensoren
```
Zeile 80:  Sensor 1 ID:     28:AB:CD:EF:01:02:03:04
Zeile 96:  Status:          Disconnected       [TFT_RED]
```

### Layout für fehlenden DS2484
```
(wie unavailable() Standard-Screen)
DS2484               I2C address:
Sensor nicht vorhanden
Y: vorherige Seite
A: naechste Seite
─────────────────────────────────
Keine Daten verfuegbar
```

### Farb-Schema
- **DS2484 Status (OK):** TFT_GREEN
- **DS2484 Status (Error):** TFT_RED
- **Sensor ID (ROM-ID):** TFT_YELLOW (für leichte Identifizierbarkeit)
- **Temperaturwert (Online):** TFT_CYAN
- **Status "Disconnected":** TFT_RED

### Text-Formatierung
```
ROM-ID: "28:AB:CD:EF:01:02:03:04"  (vollständige 8-Byte Adresse)
Temperatur: "24.56 °C" (2 Dezimalstellen)
Status: "Disconnected" oder Temperaturwert
```

### Display-Implementierung (pageDs2484)
```cpp
void pageDs2484()
{
  if (!available.ds2484) return unavailable("DS2484", "0x18", "I2C address:");
  
  // Status-Zeile
  String statusText = "OK - " + String(ds2484Values.sensorCount) + " Sensors";
  if (ds2484Values.sensorCount == 0) {
    statusText = "OK - No Sensors Found";
  }
  row(56, "DS2484 Status:", statusText, TFT_GREEN);
  
  // Sensoren anzeigen (max 3)
  for (uint8_t i = 0; i < ds2484Values.sensorCount; i++) {
    int baseY = 80 + (i * 40);
    
    // ROM-ID anzeigen
    String romIdStr = formatDs2484RomId(ds2484Values.sensors[i].address);
    row(baseY, "Sensor " + String(i+1) + " ID:", romIdStr, TFT_YELLOW);
    
    // Temperatur oder Status anzeigen
    if (ds2484Values.sensors[i].available) {
      row(baseY + 16, "Temp:", String(ds2484Values.sensors[i].temperature, 2) + " C", TFT_CYAN);
    } else {
      row(baseY + 16, "Status:", "Disconnected", TFT_RED);
    }
  }
  
  footer("Y/A: Seite wechseln | DS2484");
}
```

---

## 7. Fehlerbehandlung

### Fehlerszenarien & Recovery

| Szenario | Detektiert durch | Handling | UI-Feedback |
|----------|------------------|----------|------------|
| DS2484 nicht vorhanden | `Wire.beginTransmission(0x18)` fehlgeschlagen | `available.ds2484 = false` | "Sensor nicht vorhanden" (unavailable-Screen) |
| Sensor CRC-Fehler | `getTempC()` returns DEVICE_DISCONNECTED_C | Fehler-Counter +1 | Weiterhin letzter Wert angezeigt |
| Nach 3 CRC-Fehler | errorCount >= 3 | `sensor.available = false` | "Disconnected" Status angezeigt |
| Bus-Reset Fehler | Scan Fehler beim `getDeviceCount()` | Retry beim nächsten Scan (60s) | Sensoranzahl bleibt 0 oder stale |
| Sensor-Disconnect | Sensor bei Scan nicht gefunden | Nach 60s Scan → Sensor als offline markieren | ROM-ID bleibt, Status "Disconnected" |
| Konvertierungs-Timeout | `getTempC()` Timeout > 1000ms | Error-Counter +1 | Letzter bekannter Wert verwendet |

### Recovery-Strategien
```cpp
// Fehler-Counter Reset nach erfolgreicher Messung
if (tempC != DEVICE_DISCONNECTED_C) {
  sensor.errorCount = 0;
  sensor.available = true;  // Re-enable wenn wieder antwortet
}

// Nach Sensor-Disconnect (nicht im Scan gefunden)
// wird Sensor bei nächster Erkennung automatisch reaktiviert
```

---

## 8. Tests

### Unit-Tests (konzeptuell)

#### Test 1: ROM-ID Formatierung
```cpp
TEST(Ds2484, FormatRomId) {
  uint8_t addr[8] = {0x28, 0xAB, 0xCD, 0xEF, 0x01, 0x02, 0x03, 0x04};
  String result = formatDs2484RomId(addr);
  ASSERT_EQ(result, "28:AB:CD:EF:01:02:03:04");
}
```

#### Test 2: Error Counter Logik
```cpp
TEST(Ds2484, ErrorCounting) {
  Ds18b20Sensor sensor;
  sensor.available = true;
  sensor.errorCount = 0;
  
  // Sensor mit 2 Fehlern sollte noch verfügbar sein
  sensor.errorCount = 2;
  ASSERT_TRUE(sensor.available);
  
  // Nach 3. Fehler sollte offline sein (Markierung in readDs2484Temperatures)
  sensor.errorCount = 3;
  // (In echter Funktion wird available = false gesetzt)
  
  // Nach erfolgreicher Messung sollte Error-Counter zurückgesetzt werden
  sensor.errorCount = 0;
  ASSERT_TRUE(sensor.available);
}
```

### Integrationstests (manuell auf Hardware)

#### Test 1: DS2484 Erkennung & Sensor-Discovery
1. DS2484 Breakout anschließen (I2C Bus: SDA=GPIO32, SCL=GPIO25)
2. 1-3 Temperatursensoren anschließen (1-Wire-Pin auf DS2484)
3. Board hochfahren
4. Navigation zu Page 9 (A-Taste drücken bis Page 9)
5. **Erwartet:** 
   - "DS2484 Status: OK - X Sensors"
   - ROM-IDs der Sensoren angezeigt
   - Temperaturwerte sichtbar

#### Test 2: Temperaturmessung & -Update
1. Page 9 angezeigt mit 3 Sensoren
2. Warte 5 Sekunden (3s Polling-Interval)
3. **Erwartet:** Temperaturwerte werden angezeigt
4. Sensor mit Finger wärmen
5. Warte 3 Sekunden
6. **Erwartet:** Temperaturwert erhöht sich (z.B. von 23.45°C auf 24.20°C)

#### Test 3: Sensor Disconnect Handling
1. Page 9 anzeigen mit 3 Sensoren (alle online)
2. Einen Sensor abziehen/Verbindung trennen
3. Warten auf nächsten Scan (max 60 Sekunden)
4. **Erwartet:** Nach wenigen Sekunden "Disconnected" für diesen Sensor
5. Sensor wieder verbinden
6. Warten auf nächsten Scan (60s)
7. **Erwartet:** Sensor wird wieder erkannt und zeigt Temperatur

#### Test 4: PAGE_COUNT Navigation
1. Navigation mit Gamepad durchführen (Y = vorherig, A = nächst)
2. Von Page 8 (VEML6075) → A drücken → sollte Page 9 (DS2484) anzeigen
3. Von Page 9 (DS2484) → A drücken → sollte Page 0 (BME280) anzeigen (Wrap-around)
4. Von Page 0 → Y drücken → sollte Page 9 (DS2484) anzeigen (Rückwärts wrap-around)
5. **Erwartet:** Zirkular-Navigation funktioniert einwandfrei mit 10 Seiten

#### Test 5: Multi-Sensor Display
1. 3 Sensoren anschließen
2. Page 9 anzeigen
3. **Erwartet:** Alle 3 Sensoren mit ROM-IDs und Temperaturen auf einer Seite sichtbar
4. Layout sollte nicht überlappen, alle Werte lesbar sein

#### Test 6: Error Recovery
1. 1-Wire Verbindung temporär unterbrechen (z.B. Kabel kurzzeitig abziehen)
2. Beobachte Display: sollte "Disconnected" zeigen
3. Kabel wieder verbinden
4. Warte auf Sensor-Recovery (nächster Scan oder Temperatur-Read)
5. **Erwartet:** Sensor wird wieder als online erkannt und zeigt Wert

#### Test 7: Multi-Sensor-Typ Support
1. 2x DS18B20 (Family Code 0x28) anschließen
2. 1x DS1822 (Family Code 0x22) anschließen (falls vorhanden)
3. Page 9 angezeigt
4. **Erwartet:** Alle 3 verschiedenen Sensortypen werden erkannt und angezeigt
5. ROM-IDs zeigen korrekte Family-Codes

#### Test 8: Polling-Timing Validierung
1. Serial Monitor starten
2. Mit Debugging: Log Bus-Scan Timing (sollte 60s Intervall sein)
3. Log Temperatur-Messung Timing (sollte 3s Intervall sein)
4. **Erwartet:** Timing stimmt mit definierten Konstanten überein

#### Test 9: RAM-Stabilität (Long-Running Test)
1. Board mehrere Stunden laufen lassen
2. Periodisch Page 9 aufrufen
3. **Erwartet:**
   - Keine Memory-Leaks erkennbar
   - Keine Crashes
   - Werte bleiben konsistent
   - Fehler-Counter funktionieren korrekt

#### Test 10: DS2484 Disconnection
1. DS2484 Breakout abziehen (physikalisch von I2C Bus trennen)
2. Page 9 navigieren
3. **Erwartet:** Anzeige von "Sensor nicht vorhanden" (unavailable screen)
4. DS2484 wieder verbinden
5. Reset durchführen
6. **Erwartet:** Sensor wird wieder erkannt

---

## 9. Dokumentation

### Files zu erstellen/aktualisieren

| Datei | Änderung | Priorität |
|-------|----------|-----------|
| `src/main.cpp` | Neue Funktionen, Structs, Variablen, Integration in setup/loop | HIGH |
| `docs/FEATURE-SPEC-DS2484-1WIRE-ADAPTER.md` | Diese Spezifikation (neu) | HIGH |
| `docs/01-requirements.md` | DS2484 Feature dokumentieren | MEDIUM |
| `docs/04-data-model.md` | Ds2484Values + Ds18b20Sensor Structs dokumentieren | MEDIUM |
| `docs/05-api.md` | Neue API-Funktionen dokumentieren | MEDIUM |
| `README.md` | DS2484 zu Sensor-Liste hinzufügen | LOW |

### Code-Dokumentation (im Quellcode)
```cpp
// ========== DS2484 I2C-to-1-Wire Adapter ==========
// Hardware: Adafruit DS2484 I2C to 1-Wire Bus Adapter
// I2C Address: 0x18 (fixed)
// Supports: DS18B20, DS1822, DS1820, and other 1-Wire temperature sensors
// Max Sensors: 3 per adapter
// Bus Scan Interval: 60 seconds
// Temperature Polling: 3 seconds
// 12-Bit resolution (standard)
```

---

## 10. Sicherheit & Autorisierung

**Keine neuen Sicherheits-Anforderungen für diese Feature.**

- Datenaustausch erfolgt über Standard I2C/1-Wire (nicht über Internet)
- Keine Authentifizierung erforderlich (lokal embedded Device)
- Keine sensitiven Daten werden verarbeitet
- Hardware-Level Security wie bestehende Sensoren

---

## 11. Performance & Ressourcen

### Timing-Analyse
```
Scan-Operationen (alle 60s):
  - getDeviceCount():                    ~50ms
  - ROM-ID Lesen (3 Sensoren):          ~100ms
  - Gesamt Scan-Zeit:                   ~150ms

Temperatur-Operationen (alle 3s):
  - requestTemperatures():               ~750ms (12-Bit Standard)
  - getTempC() x3:                      ~100ms
  - Gesamt Konvertierung:               ~850ms

CPU-Auslastung:
  - Scan: ~150ms / 60s = 0.25%
  - Temp: ~850ms / 3s = 28% (aber asynchron, nicht blockierend)
  - Display: ~20ms (TFT-Refresh auf Page 9)
  - Gesamt: <5% durchschnittlich
```

### RAM-Verbrauch
```
Ds2484Values (3 Sensoren):              ~60 Bytes
OneWire library overhead:               ~100 Bytes
DallasTemperature library overhead:     ~200 Bytes
─────────────────────────────────────────────────
Gesamt neu:                             ~360 Bytes
Verfügbar auf ESP32:                    ~320 KB
Prozentsatz:                            0.11% ✓ Akzeptabel
```

### I2C Bus Load
- DS2484 Initialization: ~10ms einmalig
- ROM-ID Scan: ~100ms alle 60 Sekunden (0.17% Bus-Last)
- Temperatur-Abfrage: ~100ms alle 3 Sekunden (3% Bus-Last)
- Standard I2C Frequenz: 100kHz (ausreichend)

---

## 12. Akzeptanz-Kriterien

✓ DS2484 wird beim Bootup erkannt (oder "Sensor nicht vorhanden" angezeigt)  
✓ Alle angeschlossenen 1-Wire Sensoren werden gescannt und angezeigt (max 3)  
✓ ROM-ID (vollständige 8-Byte Adresse) wird für jeden Sensor angezeigt  
✓ Temperaturwerte werden korrekt gelesen mit 2 Dezimalstellen angezeigt  
✓ Alle Sensor-Typen (DS18B20, DS1822, DS1820) werden unterstützt  
✓ Seite 9 ist navigierbar (Y/A Tasten funktionieren)  
✓ Sensor-Disconnect wird erkannt und "Disconnected" Status angezeigt  
✓ Nach 3 Fehler-Counter Inkremente wird Sensor als offline gekennzeichnet  
✓ Fehler-Recovery funktioniert (errorCount zurückgesetzt nach erfolgreichem Read)  
✓ Bus-Scan alle 60 Sekunden durchgeführt  
✓ Temperatur-Messung alle 3 Sekunden durchgeführt  
✓ PAGE_COUNT = 10, Zirkular-Navigation funktioniert  
✓ Keine Memory-Leaks bei längerem Betrieb  
✓ Display aktualisiert sich ohne Flimmern  
✓ Alle 10 Seiten sind zirkular navigierbar  
✓ Fehlerbehandlung ist robust (keine Crashes)  

---

## 13. Implementierungs-Reihenfolge (Empfehlung)

1. **Phase 1:** Konstanten und Structs definieren (Ds2484Values, Ds18b20Sensor, #defines)
2. **Phase 2:** `initDs2484()` implementieren (Hardware-Erkennung auf 0x18)
3. **Phase 3:** `performDs2484Scan()` implementieren (OneWire/Dallas Bus-Scan)
4. **Phase 4:** `readDs2484Temperatures()` implementieren (Temperaturmessung)
5. **Phase 5:** `formatDs2484RomId()` Hilfsfunktion
6. **Phase 6:** `pageDs2484()` Display-Funktion
7. **Phase 7:** Integration in `setup()` (initDs2484Aufrufen)
8. **Phase 8:** Integration in `loop()` (Scan + Temp Polling)
9. **Phase 9:** Integration in `drawCurrentPage()` (Seite 9 hinzufügen, PAGE_COUNT erhöhen)
10. **Phase 10:** Tests & Debugging auf Hardware
11. **Phase 11:** Dokumentation aktualisieren

---

## 14. Open Questions

**Alle kritischen Fragen wurden mit User-Entscheidungen geklärt. Keine ausstehenden Open Questions.**

Entschiedene Punkte:
- ✓ DS2484 I2C-Adresse: 0x18 (fest)
- ✓ 1-Wire-Pin: nicht nötig (DS2484 ist I2C Adapter)
- ✓ Pullup-Widerstände: nicht nötig (auf Breakout Board)
- ✓ Max Sensoren: 3
- ✓ Sensor-Typ Support: Multi-type (alle 1-Wire)
- ✓ Auflösung: Standard 12-Bit
- ✓ ROM-ID Format: Alle 8 Bytes
- ✓ Multi-Sensor-Display: Nur für ≤3 (keine Sub-Pages nötig)
- ✓ Bus-Scan: 60 Sekunden
- ✓ Temperatur-Polling: 3 Sekunden
- ✓ Fehler-Anzeige: Status "Disconnected"
- ✓ Test-Hardware: Vorhanden

---

## 15. Version & Change Log

| Version | Datum | Änderung |
|---------|-------|----------|
| 2.0 | 2026-09-23 | Aktualisierung basierend auf User-Entscheidungen: DS2484 I2C Adapter, 3-Sensor Limit, 60s/3s Polling, Multi-Type Support |
| 1.0 | 2026-09-23 | Initiale Spezifikation (mit Open Questions) |

---

## Anhang A: Hardware-Anschlüsse

**Adafruit DS2484 I2C to 1-Wire Bus Adapter Breakout:**
- VCC → 3.3V (ESP32)
- GND → GND (ESP32)
- SDA → GPIO32 (ESP32 I2C)
- SCL → GPIO25 (ESP32 I2C)
- DQ → 1-Wire Sensoren (Data Line)

**1-Wire Temperatursensoren (DS18B20/DS1822/DS1820):**
- VDD → 3.3V (über DQ Pin oder separate)
- GND → GND
- DQ → DS2484 DQ Pin

---

