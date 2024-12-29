# AquaBotanica

Dieses Projekt nutzt das Wio Terminal, um Umgebungsdaten wie Temperatur, Luftfeuchtigkeit und Bodenfeuchtigkeit zu messen und anzuzeigen.

## Aufbau Wio Terminal
   
### **1. Bodenfeuchtigkeitssensor**

| **Sensor-Pin** | **Funktion**         | **Wio Terminal-Anschluss** |
|----------------|----------------------|----------------------------|
| **VCC**        | Versorgungsspannung  | **Pin 2 (5V)**             |
| **GND**        | Masse                | **Pin 6 (GND)**            |
| **SIG**     | Analogsignal         | **Pin 16 (A2)**            |

### **2. DHT-Sensor**

Schließe den Sensor an den analogen Grove-Port (rechter Port A0) des Wio Terminals an.

### **3. RTC Sensor**

| **RTC-Pin** | **Funktion**         | **Wio Terminal-Anschluss** |
|-------------|----------------------|----------------------------|
| **VCC**     | Versorgungsspannung  | **Pin 1 (3.3V)**           |
| **GND**     | Masse                | **Pin 9 (GND)**            |
| **SDA**     | I²C-Datenleitung     | **Pin 3 (SDA)**            |
| **SCL**     | I²C-Taktleitung      | **Pin 5 (SCL)**            |

### **4. Relai**

| **RTC-Pin** | **Funktion**         | **Wio Terminal-Anschluss** |
|-------------|----------------------|----------------------------|
| **VCC**     | Versorgungsspannung  | **Pin 17 (3.3V)**          |
| **GND**     | Masse                | **Pin 34 (GND)**           |
| **SIG**     | Digitalsignal        | **Pin 33 (D6)**            |
---

## Vorbereitung

1. **Vergewissere dich, dass PlatformIO installiert ist.**
   - Installiere PlatformIO in [Visual Studio Code](https://platformio.org/install/ide?install=vscode).

2. **Schließe den Wio Terminal mit einem USB-Kabel an deinen Computer an.**

3. **Clone das Repository:**
   ```bash
   git clone https://github.com/kraxnerh19/AquaBotanica.git

4. **Wlan Daten ändern:**

   Öffne die Datei `main.cpp` im Projektverzeichnis.

   Ändere die WLAN-Logindaten in den folgenden Zeilen, um deine SSID und dein Passwort einzutragen:

   ```cpp
   const char* ssid = "Dein_WLAN_Name";         // WLAN-Name (SSID)
   const char* password = "Dein_WLAN_Passwort"; // WLAN-Passwort

  ---

## Projekt aufs WIO Terminal hochladen
1. **Upload-Modus aktivieren:**

   Schalte das Terminal über den Schieberegler rechts ein und drücke diesen 2-mal nach hinten, die blaue LEd sollte pulsieren.
3. **Build Projekt:**

   Zunächst wird das Projekt auf Fehler überprüft
   ```cpp
   pio run
5. **Upload Projekt:**

   Lade das Projekt aufs WIO Terminal hoch, um es zu testen.
   ```cpp
   pio run --target upload
   
   
