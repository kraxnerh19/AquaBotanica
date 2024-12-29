//Libaries
#include "config.h"
#include <Arduino.h>
#include <TFT_eSPI.h>   // Bibliothek für das Display des Wio Terminals
#include "DHT.h"        // Bibliothek für Grove Temperature & Humidity Sensor
#include <rpcWiFi.h>    // Bibliothek für WLAN-Verbindung
#include <RTClib.h>
#include <SD.h>
#include <WiFiUdp.h>    // Bibliothek für UDP-Verbindung (NTP nutzt UDP)
#include <Wire.h>       // I2C-Bibliothek für VL53L0X
#include <Adafruit_VL53L0X.h>  // VL53L0X-Bibliothek für Entfernungsmessung
#include "lcd_backlight.hpp"

//Definitionen
#define MOISTURE_PIN A2     // Pin A2 für den Feuchtigkeitssensor am rechten Grove-Anschluss
#define RELAY_PIN D6       // Pin D1, entspricht dem A1-Anschluss des Terminals
#define DHT_PIN 0           // Grove-Analoganschluss für den DHT-Sensor (Standard ist D0 oder A0)
#define DHT_TYPE DHT11      // Typ des DHT-Sensors, falls Sie DHT11 oder DHT22 verwenden
#define SDCARD_SS_PIN       // CS-Pin für die SD-Karte, prüfen Sie Ihren Anschluss!
#define DIST_THRESHOLD 100  // Abstandsschwelle in mm für das Display (wenn unter diesem Wert wird das Display aktualisiert)
#define TFT_DARKORANGE  0xFCC0
#define TFT_DARKYELLOW  0xCD00

File dataFile;              // Dateiobjekt für das Speichern der Daten
DHT dht(DHT_PIN, DHT_TYPE); // DHT-Sensor-Objekt erstellen
TFT_eSPI tft = TFT_eSPI();  // Display-Objekt erstellen
RTC_DS3231 rtc;             // RTC-Objekt erstellen
Adafruit_VL53L0X lox = Adafruit_VL53L0X(); // Instanz für Distanz Sensor 
static LCDBackLight backLight;

// Konfiguration für NTP
const char* ntpServer = "pool.ntp.org"; // NTP-Server-Adresse
const int ntpPort = 123;                // NTP-Port (Standard ist 123)
WiFiUDP udp;                            // UDP-Instanz für NTP

// Offset für Zeitzone (in Sekunden)
const long gmtOffsetSec = 3600;
int daylightOffsetSec = 0; // Sommerzeit-Offset in Sekunden

int lastMoistureValue = -1; // Speichern des vorherigen Feuchtigkeitswerts
float lastTemperature = -1000; // Speichern des vorherigen Temperaturwerts
float lastHumidity = -1000; // Speichern des vorherigen Feuchtigkeitswerts

unsigned long previousTimeUpdate = 0; // Letzte Aktualisierung der Uhrzeit
unsigned long previousSensorUpdate = 0; // Letzte Aktualisierung der Sensorwerte
const unsigned long timeInterval = 1000; // Intervall für Zeitaktualisierung (1 Sekunde)
const unsigned long sensorInterval = 4000; // Intervall für Sensoraktualisierung (4 Sekunden)
const unsigned long displayTimeout = 15000; // Timeout für die Anzeige von Sensorwerten (15 Sekunden)
bool isDisplayingSensorValues = false;
unsigned long displayUpdateTime = 0;
bool firstMainScreen = false;
unsigned long previousFlowerUpdate = 0;

void connectToWiFi() {
    int maxRetries = 2;  // Maximale Anzahl an Versuchen
    int retryCount = 0;  // Zähler für Versuche

    tft.setCursor(10, 10);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.println("Verbinde mit WLAN...");
    Serial.println("Verbinde mit WLAN...");

    WiFi.begin(ssid, password);

    // Wiederhole Verbindung bis Maximum erreicht ist
    while (WiFi.status() != WL_CONNECTED && retryCount < maxRetries) {
        delay(500);
        Serial.print(".");
        tft.print(".");
        retryCount++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        tft.fillScreen(TFT_BLACK);  // Bildschirm sofort nach Verbindung leeren
        tft.setTextColor(TFT_GREEN);
        tft.setTextSize(2);
        tft.setCursor(10, 10);
        tft.println("WLAN verbunden!");
        Serial.println("\nWLAN verbunden!");
    } else {
        tft.fillScreen(TFT_BLACK);  // Bildschirm leeren
        tft.setTextColor(TFT_RED);
        tft.setTextSize(2);
        tft.setCursor(10, 10);
        tft.println("WLAN fehlgeschlagen!");
        Serial.println("\nWLAN fehlgeschlagen!");
    }

    delay(2000); 
}

void getNtpTime() {
    // Display Ausgabe
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(10, 10);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.println("NTP Zeit festlegen...");
    Serial.println("NTP Zeit festlegen...");

    // UDP starten
    udp.begin(ntpPort);
    Serial.println("NTP-Client gestartet...");

    // NTP-Anfrage vorbereiten
    byte packetBuffer[48]; // NTP-Paket hat eine feste Größe von 48 Bytes
    memset(packetBuffer, 0, 48);

    // NTP-Paket Header konfigurieren
    packetBuffer[0] = 0b11100011; // LI, Version, Mode
    packetBuffer[1] = 0;          // Stratum
    packetBuffer[2] = 6;          // Polling Interval
    packetBuffer[3] = 0xEC;       // Precision

    // NTP-Anfrage an den Server senden
    udp.beginPacket(ntpServer, ntpPort);
    udp.write(packetBuffer, 48);
    udp.endPacket();

    // Warten auf Antwort
    delay(1000);

    if (udp.parsePacket()) {
        Serial.println("Antwort vom NTP-Server erhalten!");

        // NTP-Antwort lesen
        udp.read(packetBuffer, 48);

        // Zeitstempel auslesen (Bytes 40–43 im Paket enthalten die Zeit in Sekunden seit 1900)
        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
        unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
        unsigned long secsSince1900 = (highWord << 16) | lowWord;

        // Epoch-Zeit berechnen (1970 ist der Startpunkt der Unix-Zeit)
        const unsigned long seventyYears = 2208988800UL;
        unsigned long epoch = secsSince1900 - seventyYears;

        // Berechnung für Sommerzeit oder Winterzeit
        DateTime now = rtc.now();  
        int year = now.year();
        DateTime lastSundayMarch(year, 3, 31);
        while (lastSundayMarch.dayOfTheWeek() != 0) {
            lastSundayMarch = lastSundayMarch - TimeSpan(1);
        }
        DateTime lastSundayOctober(year, 10, 31);
        while (lastSundayOctober.dayOfTheWeek() != 0) {
            lastSundayOctober = lastSundayOctober - TimeSpan(1);
        }
        if (now >= lastSundayMarch && now <= lastSundayOctober) {
            daylightOffsetSec = 3600;  // Sommerzeit-Offset
            Serial.println("Sommerzeit aktiv.");
        } else {
            daylightOffsetSec = 0;  // Kein Sommerzeit-Offset
            Serial.println("Winterzeit aktiv.");
        }
        
        // Zeitzonen-Offset anwenden
        epoch += gmtOffsetSec;
        if (daylightOffsetSec) {
            epoch += daylightOffsetSec;
        }

        // RTC synchronisieren
        rtc.adjust(DateTime(epoch));
        Serial.println("RTC erfolgreich synchronisiert!");
    } else {
        Serial.println("Keine Antwort vom NTP-Server erhalten!");
    }

    udp.stop(); // UDP beenden
}

// Funktion zum Zeichnen eines Kreisbogens
void drawArc(int x, int y, int r, int startAngle, int endAngle, uint16_t color, int thickness) {
    for (int i = startAngle; i <= endAngle; i++) {
    float angle = i * 3.14159 / 180.0;
    int x0 = x + (int)(cos(angle) * r);
    int y0 = y - (int)(sin(angle) * r);
    tft.drawPixel(x0, y0, color);

        for (int j = -thickness / 2; j < thickness / 2; j++) {
            tft.drawPixel(x0 + j, y0, color);
        }
    }
}

// Funktion zum Zeichnen der Sonnenblume
void drawSunflower(String mood, uint16_t faceColor) {
    tft.fillScreen(TFT_BLACK); // Bildschirm löschen

    int centerX = 160; // horizontal zentriert
    int centerY = 90;  
    int faceRadius = 50; // Radius des Gesichts

    for (int angle = 0; angle < 360; angle += 30) {
    float rad = angle * 3.14159 / 180.0;
    int xOffset = (int)(cos(rad) * 60);
    int yOffset = (int)(sin(rad) * 60);

    tft.fillCircle(centerX + xOffset,centerY + yOffset,20,TFT_YELLOW);
    }

    //Gesicht
    tft.fillCircle(centerX, centerY, faceRadius, faceColor);

    //Augen
    tft.fillCircle(centerX - 25, centerY - 12, 6, TFT_BLACK); // Linkes Auge
    tft.fillCircle(centerX + 25, centerY - 12, 6, TFT_BLACK); // Rechtes Auge

    //Mund
    int mouthY = centerY + 20;  
    int mouthRadius = 25;      

    if (mood == "happy") {
    // Lachender Mund (oben gewölbt)
    drawArc(centerX, mouthY, mouthRadius, 180, 360, TFT_BLACK, 4);
    } 
    else if (mood == "sad") {
    // Trauriger Mund (unten gewölbt)
    drawArc(centerX, mouthY + 10, mouthRadius, 0, 180, TFT_BLACK, 4);
    } 
    else {
    // Neutraler Mund (gerade Linie)
    tft.fillRect(centerX - 25, mouthY - 2, 50, 4, TFT_BLACK);
    }

    //Stiel
    int stemWidth = 12; 
    int stemX = centerX - stemWidth / 2;
    int stemY = centerY + faceRadius; 
    int stemHeight = 100; 

    tft.fillRect(stemX, stemY, stemWidth, stemHeight, TFT_GREEN);

    //Blätter
    int leafCenterY = stemY + stemHeight / 2 + 10; 
    int leafOffsetX = 40;   
    int leafRadiusX = 30;   
    int leafRadiusY = 15;   

    // Linkes Blatt
    tft.fillEllipse(centerX - leafOffsetX, leafCenterY, leafRadiusX, leafRadiusY, TFT_GREEN);
    // Rechtes Blatt
    tft.fillEllipse(centerX + leafOffsetX, leafCenterY, leafRadiusX, leafRadiusY, TFT_GREEN);
}


void updateTimeDisplay() {
    DateTime now = rtc.now();
    char timeBuffer[16];
    snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    tft.fillRect(200, 10, 100, 30, TFT_BLACK);
    tft.setCursor(200, 10);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.println(timeBuffer);
}

void controlRelayBasedOnMoisture(int moistureValue) {
    if (moistureValue <= 10) { 
        digitalWrite(RELAY_PIN, HIGH);  // Relais einschalten
    } else if (moistureValue > 10 && moistureValue <= 300) {
        digitalWrite(RELAY_PIN, LOW);  // Relais ausschalten
    } else {
        digitalWrite(RELAY_PIN, LOW);  // Relais ausschalten
    }
}

void updateSensorData(int moistureValue, float temperature, float humidity) {
    if (moistureValue != lastMoistureValue || firstMainScreen) {
        tft.fillRect(200, 50, 80, 30, TFT_BLACK);
        tft.setCursor(200, 50);
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(2);
        tft.print(moistureValue);
        lastMoistureValue = moistureValue;
    }

    if (temperature != lastTemperature || firstMainScreen) {
        tft.fillRect(200, 90, 100, 30, TFT_BLACK);
        tft.setCursor(200, 90);
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(2);
        tft.print(temperature);
        tft.print(" C");
        lastTemperature = temperature;
    }

    if (humidity != lastHumidity || firstMainScreen) {
        tft.fillRect(200, 130, 100, 30, TFT_BLACK);
        tft.setCursor(200, 130);
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(2);
        tft.print(humidity);
        tft.print(" %");
        lastHumidity = humidity;
    }

    tft.fillRect(10, 210, 300, 30, TFT_BLACK);
    String statusMessage;
    if (moistureValue <= 10) {
        statusMessage = "Bitte giessen";
        tft.setTextColor(TFT_RED);
    } else if (moistureValue > 10 && moistureValue <= 300) {
        statusMessage = "Bald giessen";
        tft.setTextColor(TFT_YELLOW);
    } else {
        statusMessage = "Alles gut";
        tft.setTextColor(TFT_GREEN);
    }
    tft.setCursor(20, 210);
    tft.setTextSize(2);
    tft.println(statusMessage);
    //Relai
    controlRelayBasedOnMoisture(moistureValue);

}

void logDataToCSV(int moistureValue, float temperature, float humidity) {
    dataFile = SD.open("sensors.csv", FILE_WRITE); // Datei im Anhängemodus öffnen
    if (dataFile) {
        DateTime now = rtc.now(); // Aktuelle Uhrzeit abrufen
        char timeBuffer[16];
        snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());

        // Datenzeile in die CSV schreiben
        dataFile.print(timeBuffer); // Zeit
        dataFile.print(",");
        dataFile.print(moistureValue); // Feuchtigkeit
        dataFile.print(",");
        dataFile.print(temperature); // Temperatur
        dataFile.print(",");
        dataFile.println(humidity); // Luftfeuchtigkeit

        dataFile.close(); // Datei schließen
        //Serial.println("Daten erfolgreich in CSV gespeichert!");
    } else {
        Serial.println("Fehler beim Öffnen der CSV-Datei!");
    }
}

// Hauptbildschirm mit Sensorwerten
void mainScreen() {
    //Update Screen
    backLight.setBrightness(100);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("Uhrzeit:");
    tft.setCursor(10, 50);
    tft.println("Pflanze F.:");
    tft.setCursor(10, 90);
    tft.println("Temperatur:");
    tft.setCursor(10, 130);
    tft.println("Luft F.:");
    tft.setCursor(10, 170);
    tft.println("Status:");

    //Update Sensordaten, Zeit, Relai
    int moistureValue = analogRead(MOISTURE_PIN);
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    updateSensorData(moistureValue, temperature, humidity);
    updateTimeDisplay();
    controlRelayBasedOnMoisture(moistureValue);
    isDisplayingSensorValues = true;
}

// Standby Screen
void showStandbyScreen() {
    tft.fillScreen(TFT_BLACK); 
    backLight.setBrightness(20);
    tft.setCursor(10, 10);
    drawSunflower("neutral", TFT_DARKYELLOW);
}

void setup() {
    Serial.begin(115200);         // Serial Monitor starten
    pinMode(MOISTURE_PIN, INPUT); // Feuchtigkeitssensor als Eingang konfigurieren
    pinMode(RELAY_PIN, OUTPUT);   // Relais-Pin als Ausgang konfigurieren
    digitalWrite(PIN_WIRE_SCL, LOW);  // Relais im Default als LOW setzen -> ausschalten

    // Display initialisieren
    tft.begin();
    tft.setRotation(3);           // Querformat
    tft.fillScreen(TFT_BLACK);    // Hintergrundfarbe auf Schwarz setzen
    Serial.println("Feuchtigkeitssensor- und DHT-Sensor-Test gestartet");

    // DHT-Sensor initialisieren
    dht.begin();

    // RTC initialisieren
    if (!rtc.begin()) {
        Serial.println("RTC nicht gefunden!");
        while (1);
    }

    if (!lox.begin()) {
        Serial.println("VL53L0X Sensor konnte nicht initialisiert werden.");
        while (1);  // Falls der Sensor nicht gefunden wird, bleibe hier
    } else {
        Serial.println("VL53L0X Sensor initialisiert.");
    }

    // RTC Zeit Konfigurieren
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Zeit einstellen
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)).unixtime() + 16); // 16 Sekunden hinzufügen (Falls nicht korrekt)

    backLight.initialize();

    // SD-Karte initialisieren
    if (!SD.begin(SDCARD_SS_PIN)) {
        Serial.println("SD-Karte konnte nicht initialisiert werden!");
        while (1);
    }
    Serial.println("SD-Karte erfolgreich initialisiert!");

    // CSV-Datei erstellen/öffnen und Kopfzeilen schreiben
    if (!SD.exists("sensors.csv")) {
        // Datei existiert nicht, neu erstellen und Header schreiben
        dataFile = SD.open("sensors.csv", FILE_WRITE);
        if (dataFile) {
            dataFile.println("Zeit,Feuchtigkeit_Pflanze,Temperatur,Luftfeuchtigkeit");
            dataFile.close();
            Serial.println("Kopfzeile in CSV-Datei geschrieben.");
        } else {
            Serial.println("Konnte CSV-Datei nicht erstellen!");
            while (1);
        }
    } else {
        Serial.println("CSV-Datei existiert bereits, kein Header geschrieben.");
    }

    connectToWiFi();
    getNtpTime();

    delay(2000);  // Zeit für die Anzeige der Erfolgsnachricht
    tft.fillScreen(TFT_BLACK);  // Bildschirm erneut leeren für Sensoranzeige
    mainScreen();
}

void loop() {
    unsigned long currentMillis = millis();
    uint16_t distance = lox.readRange();

    //Main-Screen und Standby-Screen Anzeige

     if (distance <= DIST_THRESHOLD) {
        if (!isDisplayingSensorValues) {
            displayUpdateTime = currentMillis;  // Timer starten, wenn der Abstand unter der Schwelle liegt
            isDisplayingSensorValues = true;
            firstMainScreen = true;
            mainScreen();
        }
    } else {
        if (isDisplayingSensorValues && currentMillis - displayUpdateTime >= displayTimeout) {
            showStandbyScreen(); 
            isDisplayingSensorValues = false;
            firstMainScreen = false;
        }
    }

    if (!isDisplayingSensorValues && currentMillis - previousSensorUpdate >= sensorInterval) {
        previousSensorUpdate = currentMillis;
        int moistureValue = analogRead(MOISTURE_PIN);
        controlRelayBasedOnMoisture(moistureValue);
    }

    // Uhrzeit jede Sekunde aktualisieren
    if (isDisplayingSensorValues && currentMillis - previousTimeUpdate >= timeInterval) {
        previousTimeUpdate = currentMillis;
        updateTimeDisplay();
    }

    // Sensorwerte alle 4 Sekunden aktualisieren
    if (isDisplayingSensorValues && currentMillis - previousSensorUpdate >= sensorInterval) {
        previousSensorUpdate = currentMillis;
        firstMainScreen = false;
        int moistureValue = analogRead(MOISTURE_PIN);
        float temperature = dht.readTemperature();
        float humidity = dht.readHumidity();

        if (!isnan(temperature) && !isnan(humidity)) {
            updateSensorData(moistureValue, temperature, humidity);
            controlRelayBasedOnMoisture(moistureValue);
        } else {
            Serial.println("Fehler beim Lesen eines Sensors!");
        }
    }

    //Standby Screen
    if (!isDisplayingSensorValues && currentMillis - previousFlowerUpdate >= sensorInterval) {
        previousFlowerUpdate = currentMillis;

        int moistureValue = analogRead(MOISTURE_PIN);
        uint16_t faceColor = TFT_DARKORANGE;

        if (moistureValue > 300) { // In Ordnung
            drawSunflower("happy", faceColor);
        } else if (moistureValue > 10) { // Bald gießen
            drawSunflower("neutral", faceColor);
        } else { // Sofort gießen
            drawSunflower("sad", faceColor);
        }
    }

    // Daten auf SD Karte schreiben alle 4 Sekunden
    if (currentMillis - previousSensorUpdate >= sensorInterval) {
        previousSensorUpdate = currentMillis;

        int moistureValue = analogRead(MOISTURE_PIN);
        float temperature = dht.readTemperature();
        float humidity = dht.readHumidity();

        if (!isnan(temperature) && !isnan(humidity)) {
            logDataToCSV(moistureValue, temperature, humidity);
        } else {
            Serial.println("Fehler beim Lesen eines Sensors!");
        }
    }
}