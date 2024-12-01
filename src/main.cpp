#include <Arduino.h>
#include <TFT_eSPI.h>   // Bibliothek für das Display des Wio Terminals
#include "DHT.h"        // Bibliothek für Grove Temperature & Humidity Sensor
#include <rpcWiFi.h>    // Bibliothek für WLAN-Verbindung
#include <RTClib.h>

#define MOISTURE_PIN A2 // Pin A1 für den Feuchtigkeitssensor am rechten Grove-Anschluss
#define DHT_PIN 0       // Grove-Analoganschluss für den DHT-Sensor (Standard ist D0 oder A0)
#define DHT_TYPE DHT11  // Typ des DHT-Sensors, falls Sie DHT11 oder DHT22 verwenden

DHT dht(DHT_PIN, DHT_TYPE); // DHT-Sensor-Objekt erstellen
TFT_eSPI tft = TFT_eSPI();  // Display-Objekt erstellen
RTC_DS3231 rtc;             // RTC-Objekt erstellen

int lastMoistureValue = -1; // Speichern des vorherigen Feuchtigkeitswerts
float lastTemperature = -1000; // Speichern des vorherigen Temperaturwerts
float lastHumidity = -1000; // Speichern des vorherigen Feuchtigkeitswerts

const char* ssid = ""; // Ihre WLAN-SSID
const char* password = ""; // Ihr WLAN-Passwort

void setup() {
    Serial.begin(115200);         // Serial Monitor starten
    pinMode(MOISTURE_PIN, INPUT); // Feuchtigkeitssensor als Eingang konfigurieren

    // Display initialisieren
    tft.begin();
    tft.setRotation(3);           // Querformat
    tft.fillScreen(TFT_BLACK);    // Hintergrundfarbe auf Schwarz setzen
    Serial.println("Feuchtigkeitssensor- und DHT-Sensor-Test gestartet");

    // DHT-Sensor initialisieren
    dht.begin();

    // RTC
    if (!rtc.begin()) {
        Serial.println("RTC nicht gefunden!");
        while (1);
    }

    if (rtc.lostPower()) {
        Serial.println("RTC hat die Stromversorgung verloren, Zeit wird eingestellt!");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Zeit einstellen
    }

    // WiFi verbinden
    tft.setCursor(10, 10);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.println("Verbinde mit WLAN...");
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        tft.print(".");
    }

    tft.fillScreen(TFT_BLACK);  // Bildschirm sofort nach Verbindung leeren
    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("WLAN verbunden!");

    delay(2000);  // Zeit für die Anzeige der Erfolgsnachricht
    tft.fillScreen(TFT_BLACK);  // Bildschirm erneut leeren für Sensoranzeige
   // Statische Texte initialisieren
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
}

void updateValue(int moistureValue, float temperature, float humidity) {
    // Uhrzeit aktualisieren
    DateTime now = rtc.now();
    char timeBuffer[16];
    snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    tft.fillRect(200, 10, 100, 30, TFT_BLACK); // Löschen der alten Uhrzeit
    tft.setCursor(200, 10);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.println(timeBuffer);
    // Feuchtigkeit Pflanze anzeigen
    if (moistureValue != lastMoistureValue) {
        tft.fillRect(200, 50, 80, 30, TFT_BLACK); // Löschen des vorherigen Werts
        tft.setCursor(200, 50);
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(2);
        tft.print(moistureValue);
        lastMoistureValue = moistureValue;
    }

    // Temperatur anzeigen
    if (temperature != lastTemperature) {
        tft.fillRect(200, 90, 100, 30, TFT_BLACK); // Löschen des vorherigen Temperaturwerts
        tft.setCursor(200, 90);
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(2);
        tft.print(temperature);
        tft.print(" C");
        lastTemperature = temperature;
    }

    // Luftfeuchtigkeit anzeigen
      if (humidity != lastHumidity) {
        tft.fillRect(200, 130, 100, 30, TFT_BLACK); // Löschen des vorherigen Luftfeuchtigkeitswerts
        tft.setCursor(200, 130);
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(2);
        tft.print(humidity);
        tft.print(" %");
        lastHumidity = humidity;
    }
    // Statusanzeige aktualisieren
    String statusMessage;  // Status-Nachricht zusammensetzen
    tft.fillRect(10, 210, 300, 30, TFT_BLACK); // Löschen des vorherigen Status
    if (moistureValue <= 10) {
        tft.setCursor(20, 210);  // Eingerückt anzeigen
        tft.setTextColor(TFT_RED);
        tft.setTextSize(2);
        statusMessage = "Bitte giessen";
    } else if (moistureValue > 10 && moistureValue <= 300) {
        tft.setCursor(20, 210);
        tft.setTextColor(TFT_YELLOW);
        tft.setTextSize(2);
        statusMessage = "Bald giessen";
    } else if (moistureValue > 300) {
        tft.setCursor(20, 210);
        tft.setTextColor(TFT_GREEN);
        tft.setTextSize(2);
        statusMessage = "Alles gut";
    }
    tft.println(statusMessage);  // Status in einer Zeile ausgeben
}

void loop() {
    // Feuchtigkeitswert vom Sensor lesen
    int moistureValue = analogRead(MOISTURE_PIN);

    // Temperatur- und Feuchtigkeitswerte vom DHT-Sensor lesen
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    // Überprüfen, ob die Werte korrekt gelesen wurden
    if (!isnan(temperature) && !isnan(humidity)) {
        updateValue(moistureValue, temperature, humidity);
    } else {
        Serial.println("Fehler beim Lesen des DHT-Sensors!");
    }

    delay(5000); // Eine Sekunde warten, bevor der nächste Wert gelesen wird
}
