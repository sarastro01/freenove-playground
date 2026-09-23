# Feature-Spezifikation: INA219 Batterie-Monitor Upgrade

**Status:** Spezifikation (noch nicht implementiert)  
**Datum:** 2026-09-23  
**Betroffen:** INA219 Sensor-Seite (Page 2)

---

## 1. Betroffene Anforderungen

**Existierende Requirements:**
- INA219-Sensor ist auf I2C-Adresse 0x40 verbunden
- Sensor wird alle 20ms abgefragt (50Hz) in `readIna219At50Hz()`
- Display wird alle 500ms aktualisiert (`SENSOR_REFRESH_MS`)
- Charge-Werte werden akkumuliert basierend auf Strom und Zeit
- Systemstart initialisiert Messungen mit `initialized = true`

**Neue Requirements:**
- Laufzeit seit Systemstart anzeigen (HH:MM:SS)
- Ladestrom mit "Charging"/"Discharging" Status-Label
- Neugeordnetes UI-Layout mit optimierter Platznutzung
- Nach Systemstart: INA219-Seite wird als erste Seite angezeigt (statt Page 0)
- Nach Systemstart: INA219-Seite wird als erste Seite angezeigt (statt Page 0)

---

## 2. Betroffene Domain-Entities

### `Ina219Values` Struct
**Aktuell:**
```cpp
struct Ina219Values { 
  float shunt;           // Shunt-Spannung (mV)
  float bus;             // Bus-Spannung / Vdd (V)
  float load;            // Last-Spannung = Batteriespannung (V)
  float current;         // Strom (mA, positiv = Laden)
  float power;           // Leistung (mW)
  float chargeMah;       // Ladung seit Start (mAh)
  bool initialized;      // Messung gestartet?
  uint32_t lastSampleUs; // Zeit des letzten Samples (µs)
};
```

**Erforderliche Erweiterung:**
```
+ uint32_t startTimeUs   // Systemstartzeit (µs) - für Laufzeit-Berechnung
```

---

## 3. Geschäftsregeln

### Laufzeit-Berechnung
- **Regel:** Laufzeit = aktuelle Zeit - `startTimeUs`
- **Format:** HH:MM:SS (Stunden:Minuten:Sekunden)
- **Genauigkeit:** Millisekunden, aber nur Sekunden angezeigt
- **Overflow-Handling:** ESP32 `micros()` läuft zuverlässig über längere Zeiträume

### Strom-Richtung
- **Charging:** `current > 0` (Akku wird geladen) → Label "Charging"
- **Discharging:** `current <= 0` (Akku wird entladen) → Label "Discharging"
- **Grenzfall:** Bei `current ≈ 0` → "Discharging" (konservativ)

### Batterie-Parameter
- **Nennspannung:** 3.7V (Lithium-Ionen)
- **Kapazität:** 2500 mAh
- **Messgenauigkeit:** 3 Dezimalstellen (wie aktuell)

---

## 4. Datenänderungen

### Neue Variablen
| Variable | Typ | Zweck | Initialisierung |
|----------|-----|-------|-----------------|
| `ina219Values.startTimeUs` | uint32_t | Startzeit der Messung | Beim ersten `readIna219At50Hz()` |

### Berechnungen
- **Laufzeit (µs):** `nowUs - startTimeUs`
- **Laufzeit (s):** `(nowUs - startTimeUs) / 1000000`
- **Laufzeit (HH:MM:SS):** `sprintf("%02d:%02d:%02d", seconds/3600, (seconds%3600)/60, seconds%60)`

---

## 5. UI-Änderungen

### Aktueller Layout (mit Bugs)
```
Zeile 56:  Bus voltage:     X.XXX V      [Überschrieben durch Charge]
Zeile 80:  Shunt voltage:   X.XXX mV
Zeile 104: Load voltage:    X.XXX V
Zeile 128: Current:         X.XXX mA
Zeile 152: Power:           X.XXX mW     [BUG: wird überschrieben]
Zeile 152: Charge:          X.XXX mAh    [BUG: überschreibt Power]
```

### Neues Layout (mit Decisions)
```
Zeile 56:  Laufzeit:        HH:MM:SS
Zeile 80:  Batterie:        X.XXX V
Zeile 104: Versorgung:      X.XXX V
Zeile 128: Strom:           ±X.X mA [Charging|Discharging]
Zeile 152: Ladung:          X.XXX mAh
Zeile 176: Shunt-Spannung:  X.XX mV
Zeile 200: Power:           XXXX mW
```

### Farb-Schema
- **Laufzeit:** TFT_CYAN (Überwachungs-Information)
- **Batterie-Spannung:** TFT_GREEN (kritischer Wert)
- **Versorgungsspannung:** TFT_GREEN (kritischer Wert)
- **Strom:** TFT_ORANGE (mit Status-Label)
- **Ladung:** TFT_CYAN (Historischer Wert — bright, sunlight-readable)
- **Shunt:** TFT_GREEN (Nebenparameter)

---

## 6. API-Änderungen

### `readIna219At50Hz()` Funktion - Neue Logik
```cpp
void readIna219At50Hz()
{
  if (!available.ina219) return;

  const uint32_t nowUs = micros();
  
  // Startzeit beim ersten Aufruf speichern
  if (!ina219Values.initialized) {
    ina219Values.startTimeUs = nowUs;
    ina219Values.initialized = true;
  }
  
  // Bestehende Logik: Charge-Akkumulation
  const uint32_t elapsedUs = nowUs - ina219Values.lastSampleUs;
  if (elapsedUs < 200000) {
    ina219Values.chargeMah += ina219Values.current * (float)elapsedUs / 3600000000.0f;
  }
  
  ina219Values.lastSampleUs = nowUs;
  
  // Sensor-Werte lesen (unverändert)
  ina219Values.shunt = -ina219.getShuntVoltage_mV();
  ina219Values.bus = ina219.getBusVoltage_V();
  ina219Values.current = -ina219.getCurrent_mA();
  ina219Values.power = ina219.getPower_mW();
  ina219Values.load = ina219Values.bus - ina219Values.shunt / 1000.0f;
}
```

### Neue Hilfsfunktion: `formatRuntime()`
```cpp
// Formatiert Laufzeit als HH:MM:SS
String formatRuntime(uint32_t elapsedUs) {
  uint32_t seconds = elapsedUs / 1000000;
  uint16_t hours = seconds / 3600;
  uint16_t minutes = (seconds % 3600) / 60;
  uint16_t secs = seconds % 60;
  
  char buffer[12];
  sprintf(buffer, "%02d:%02d:%02d", hours, minutes, secs);
  return String(buffer);
}
```

### `pageIna219()` Funktion - Neuer Code
```cpp
void pageIna219()
{
  if (!available.ina219) return unavailable("INA219", "0x40", "I2C address:");
  
  // Laufzeit berechnen
  const uint32_t runtimeUs = micros() - ina219Values.startTimeUs;
  String runtimeStr = formatRuntime(runtimeUs);
  
  // Strom-Status
  const char *statusLabel = ina219Values.current > 0 ? "Charging" : "Discharging";
  
  // UI-Zeilen
  row(56, "Laufzeit:", runtimeStr, TFT_CYAN);
  row(80, "Batterie:", String(ina219Values.load, 3) + " V", TFT_GREEN);
  row(104, "Versorgung:", String(ina219Values.bus, 3) + " V", TFT_GREEN);
  row(128, "Strom:", String(ina219Values.current, 1) + " mA [" + statusLabel + "]", TFT_ORANGE);
  row(152, "Ladung:", String(ina219Values.chargeMah, 3) + " mAh", TFT_CYAN);
  row(176, "Shunt-Spannung:", String(ina219Values.shunt, 2) + " mV", TFT_GREEN);
  row(200, "Power:", String((uint16_t)ina219Values.power) + " mW", TFT_ORANGE);
  
  footer("Y/A: Seite wechseln | INA219");
}
```

---

## 7. Fehlerbehandlung

### Sensor nicht verfügbar
- Verhalten bleibt unverändert: `unavailable()` wird aufgerufen
- Laufzeit-Berechnung wird nicht durchgeführt

### Mikrosekunden-Overflow
- **Problem:** `uint32_t` Mikrosekunden können nach längeren Zeiten überläufig
- **Lösung:** Relative Berechnung (nicht vom Nullpunkt)
- **Funktioniert zuverlässig** für die typische Betriebsdauer von IoT-Geräten

### Ungültiger Strom-Wert
- Normalerweise: -9999 bis +9999 mA (Sensor-Spezifikation)
- **Handling:** Wird wie erhalten angezeigt (keine spezielle Behandlung nötig)

---

## 8. Tests

### Unit-Tests (konzeptuell)
1. **Laufzeit-Formatierung:**
   - `formatRuntime(0)` → "00:00:00"
   - `formatRuntime(3661000000)` → "01:01:01" (1h 1m 1s)
   - `formatRuntime(86399000000)` → "23:59:59" (max 24h)

2. **Strom-Richtung:**
   - `current > 0` → "Charging"
   - `current <= 0` → "Discharging"

### Integrationstests (manuell)
1. **Laufzeit erhöht sich kontinuierlich** nach Systemstart
2. **Strom-Status wechselt** bei Polaritätswechsel
3. **Charge-Wert** akkumuliert korrekt (mit bekanntem Strom)
4. **Display aktualisiert** alle 500ms ohne Flimmern
5. **Alle 6 Zeilen** sind sichtbar ohne Überlappungen

### Manuelles Testing
1. Systemstart beobachten → Laufzeit sollte bei 00:00:00 starten
2. Nach 1 Minute → Laufzeit sollte 00:01:00 zeigen
3. Ladegerät anschließen → Status sollte "Charging" zeigen
4. Ladegerät abziehen → Status sollte "Discharging" zeigen
5. Alle Werte sollten sichtbar sein und regelmäßig aktualisiert werden

---

## 9. Dokumentation

### Code-Kommentare
- **Neu:** Kommentar zu Laufzeit-Berechnung in `readIna219At50Hz()`
- **Neu:** Kommentar zur Strom-Richtungs-Logik in `pageIna219()`
- **Neu:** Kommentar für `formatRuntime()` Funktion

### README Updates
- **Optional:** Batterie-Parameter dokumentieren (3.7V, 2500mAh)
- **Optional:** Sensor-Funktionalität beschreiben

---

## 10. Sicherheit & Autorisierung

- **Keine neuen Sicherheitsaspekte:** Reine Datenanzeigeänderung
- **Keine Authentifizierung erforderlich**
- **Keine Datenspeicherung außer in RAM**

---

## 11. Implementierungsprioritäten

| Priorität | Aufgabe | Abhängigkeiten |
|-----------|---------|-----------------|
| 1 | `Ina219Values` Struct um `startTimeUs` erweitern | Keine |
| 2 | `readIna219At50Hz()` mit Startzeit-Speicherung aktualisieren | Priorität 1 |
| 3 | `formatRuntime()` Hilfsfunktion erstellen | Keine |
| 4 | `pageIna219()` Layout komplett neuordnen | Priorität 2, 3 |
| 5 | Display-Test durchführen | Priorität 4 |

---

## 12. Annahmen & Entscheidungen (FINALISIERT)

### Getroffene Entscheidungen
1. **Laufzeit-Overflow:** µs overflow ist aktuell nicht relevant (Standard-Betriebsdauer)
2. **Strom-Formatierung:** Immer mit Vorzeichen: `String(ina219Values.current, 3)` zeigt ±X.XXX
3. **Status-Label Platzierung:** Auf gleicher Zeile wie Strom-Wert: "±X.XXX mA [Charging|Discharging]"
4. **Power-Zeile:** Wird als 7. Zeile (Zeile 200) beibehalten
5. **Farb-Schema:** Grün für kritische Batterie-Werte, Orange für Strom/Power
6. **Strom-Status:** Bei `current == 0` wird "Discharging" angezeigt (konservativ)

### Annahmen
- ESP32 `micros()` läuft zuverlässig über längere Zeiträume
- Display hat Platz für 7 Zeilen Daten (320x480 Auflösung)
- Benutzer versteht "Charging" und "Discharging" Labels
- Vorzeichen in der Strom-Anzeige ist aussagekräftig (Benutzer kann positive/negative Werte deuten)

---

## 13. Offene Fragen (RESOLVED)

✅ **Alle Fragen geklärt:**

1. **Laufzeit-Overflow-Handling:** 
   - **RESOLVED:** µs overflow ist aktuell nicht relevant

2. **Strom-Formatierung:** 
   - **RESOLVED:** Immer mit Vorzeichen anzeigen (±X.XXX mA)

3. **Status-Label Platzierung:** 
   - **RESOLVED:** Auf gleicher Zeile wie Strom-Wert

4. **Power-Zeile:** 
   - **RESOLVED:** Wird als letzte Zeile (Zeile 200) beibehalten

---

## 14. Implementierungs-Checklist

- [ ] `Ina219Values` struct um `startTimeUs` erweitern
- [ ] `readIna219At50Hz()` aktualisieren (Startzeit speichern)
- [ ] `formatRuntime()` Funktion implementieren
- [ ] `pageIna219()` komplett neuschreiben
- [ ] Code kompilieren und auf Fehler prüfen
- [ ] Display-Test durchführen (alle 6 Zeilen sichtbar?)
- [ ] Laufzeit-Test durchführen (erhöht sich kontinuierlich?)
- [ ] Strom-Richtungs-Test durchführen (Charging/Discharging wechselt?)
- [ ] Dokumentation aktualisieren (optional)

---

**Nächster Schritt:** Klärung der offenen Fragen vor Implementierung
