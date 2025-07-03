#include <ESP8266WiFi.h>
// #include <ESP8266HTTPClient.h>
// #include <WiFiUdp.h>
// #include <NTPClient.h>
#include <Adafruit_NeoPixel.h>
// #include <ArduinoJson.h>

// ---PINS---
// LED Ring Parameter
#define LED_PIN D8
#define BUTTON_RED_PIN D1    //  Starts measurement only
#define BUTTON_BLACK_PIN D2  // Starts measurement + LED ring
#define STRIPSIZE 12

#define GASSENSOR_PIN A0
#define TEMP_PIN D3
#define LED_AIR_PIN D6
#define LED_TEMP_PIN D7

// Parameter
const float ADC_VOLT = 3.3;  // Board‑Versorgungsspannung
const int ADC_MAX = 1023;    // 10‑Bit Auflösung

const float AirQ_OK = 1.00;  // < 1.0 V = gute Luft

const float TEMP_MIN_OK = 16.0;  // Untergrenze in Celsius
const float TEMP_MAX_OK = 20.0;  // Obergrenze in Celsius


Adafruit_NeoPixel strip = Adafruit_NeoPixel(STRIPSIZE, LED_PIN, NEO_GRB + NEO_KHZ800);
int HELLIGKEIT = 20;

bool ringActive = false;
int LAUFZEIT = 120;    //2 Minuten als Test
int ABKLINGZEIT = 20;  //Alle 20 sec anpassen

// WLAN-Zugangsdaten
const char* ssid = "DEIN_SSID";               // HIER WLAN-SSID EINTRAGEN
const char* password = "DEIN_WIFI_PASSWORT";  // HIER  WLAN-PASSWORT EINTRAGEN


// OpenWeatherMap API
const char* owm_api_key = "DEIN_API_KEY";  // HIER  OPENWEATHERMAP API-SCHLÜSSEL EINTRAGEN


// Standort (Stadtname)
const char* cityName = "Oldenburg,de";  // HIER STADTNAME EINTRAGEN (z.B. "Berlin,de")


// --NTP Client für Timer--
// WiFiUDP ntpUDP;
// const long utcOffsetInSeconds = 3600;         // MEZ
// NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

// Timer-Variablen
unsigned long timerStartMillis = 0;
bool timerRunning = false;


// Timer-Funktionen
/**
 * Startet den 1-Stunden-Timer.
 */
void startTimer() {
  timerStartMillis = millis();
  timerRunning = true;

  long sollZeitMillis = millis() + (LAUFZEIT * 1000);
  int ROT = 255;
  int GRUEN = 147;
  int BLAU = 41;
  colorFill(ROT, GRUEN, BLAU);

  // Warte, bis Sollzeit erreicht
  while (millis() < sollZeitMillis) {
    delay(1000);
    startSensors();
  }

  // Abklingzeit
  double i = (float)ABKLINGZEIT / HELLIGKEIT;
  int j = 1;
  unsigned long abklingEnde = millis() + (ABKLINGZEIT * 1000);
  while (millis() < abklingEnde) {
    delay(1000);
    startSensors();
    int newBrightness = max(0, HELLIGKEIT - static_cast<int>(i * j));
    strip.setBrightness(newBrightness);
    strip.show();
    j++;
  }

  strip.setBrightness(0);
  strip.show();
  timerRunning = false;
}

void colorFill(int r, int g, int b) {
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
    strip.show();
  }
}

bool isTimerActive() {
  if (!timerRunning) return false;
  return (millis() - timerStartMillis) < 3600000UL;  // 1h = 3600000 ms
}

/**
 * Gibt die verbleibende Zeit in Sekunden zurück.
 * @return Restsekunden oder 0.
 */
unsigned long getTimerRemaining() {
  if (!timerRunning) return 0;
  unsigned long elapsed = millis() - timerStartMillis;
  if (elapsed < 3600000UL) return (3600000UL - elapsed) / 1000;
  timerRunning = false;
  return 0;
}
// Sensoren
float readVoltage(int pin) {
  int raw = analogRead(pin);        // wert zwischen 0-4095
  return raw * ADC_VOLT / ADC_MAX;  // in Volt
}

float readTemp(int pin) {
  int raw = analogRead(pin);
  float voltage = raw * ADC_VOLT / ADC_MAX;
  float temperatureC = voltage / 0.01;  // 10mV pro Celsius
  return temperatureC;
}



void startSensors() {
  // Luftqualität
  float air_volt = readVoltage(GASSENSOR_PIN);

  if (air_volt < AirQ_OK) {
    digitalWrite(LED_AIR_PIN, HIGH);
  } else {
    digitalWrite(LED_AIR_PIN, LOW);
  }

  // Temperatur
  float temp = readTemp(TEMP_PIN);

  // get Optimal Temperature intervall
  float maxTemp = TEMP_MAX_OK;
  float minTemp = TEMP_MIN_OK;

  // switch Status LED
  if (temp >= minTemp && temp <= maxTemp) {
    digitalWrite(LED_TEMP_PIN, HIGH);
  } else {
    digitalWrite(LED_TEMP_PIN, LOW);
  }

  Serial.begin(115200);
  // then inside startSensors():
  Serial.print(" Air voltage:  ");
  Serial.println(air_volt);
  Serial.print(" Temp: ");
  Serial.print(temp);
}

// ---------- SETUP ----------
void setup() {
  // NeoPixel
  pinMode(BUTTON_RED_PIN, INPUT_PULLUP);    // Initialize Red Button
  pinMode(BUTTON_BLACK_PIN, INPUT_PULLUP);  // Initialize Black Button

  // LED Ring setup
  strip.begin();
  strip.setBrightness(HELLIGKEIT);
  strip.show();  // Initialize all pixels to 'off'

  // Sensoren und Status LED
  pinMode(LED_AIR_PIN, OUTPUT);
  pinMode(LED_TEMP_PIN, OUTPUT);


  digitalWrite(LED_AIR_PIN, HIGH);
  digitalWrite(LED_TEMP_PIN, HIGH);
}

unsigned long abklingStartMillis = 0;
bool abklingRunning = false;

// ---------- LOOP ----------
void loop() {

  //senoren lesen werte während der timer läuft

    if (timerRunning) {
      delay(800);
      startSensors();
      unsigned long elapsed = millis() - timerStartMillis;

      if(!ringActive) {
        if (timerRunning) {
        delay(800);
        startSensors();
        unsigned long elapsed = millis() - timerStartMillis;

        if (!abklingRunning) {
          if (elapsed >= LAUFZEIT * 1000) {
            // Start Abklingen
            abklingStartMillis = millis();
            abklingRunning = true;
          }
        } else {
          // Abklingzeit läuft
          unsigned long abklingElapsed = millis() - abklingStartMillis;

          if (abklingElapsed >= ABKLINGZEIT * 1000) {
            // Abklingzeit vorbei, Timer beenden
            timerRunning = false;
            abklingRunning = false;

            // Status-LEDs ausschalten
            digitalWrite(LED_AIR_PIN, HIGH);
            digitalWrite(LED_TEMP_PIN, HIGH);
          }
        }
      }
      } else {
    
      if (!abklingRunning) {
        if (elapsed < LAUFZEIT * 1000) {
          // LED Ring an, volle Helligkeit
          colorFill(255, 147, 41);
          strip.setBrightness(HELLIGKEIT);
          strip.show();
        } else {
          // Start Abklingen
          abklingStartMillis = millis();
          abklingRunning = true;
        }
      } else {
        // Abklingzeit läuft
        unsigned long abklingElapsed = millis() - abklingStartMillis;

        if (abklingElapsed < ABKLINGZEIT * 1000) {
          // Helligkeit reduzieren
          float fraction = 1.0 - (float)abklingElapsed / (ABKLINGZEIT * 1000);
          int newBrightness = (int)(HELLIGKEIT * fraction);
          strip.setBrightness(newBrightness);
          strip.show();
        } else {
          // Abklingzeit vorbei, LEDs ausschalten
          strip.setBrightness(0);
          strip.clear();
          strip.show();
          timerRunning = false;
          abklingRunning = false;
          // Status LEDs ausschalten
          digitalWrite(LED_AIR_PIN, HIGH);
          digitalWrite(LED_TEMP_PIN, HIGH);
          unsigned long abklingStartMillis = 0;
        }
      }
    }
  }


  if (!digitalRead(BUTTON_BLACK_PIN) && !timerRunning) {
    timerStartMillis = millis();
    ringActive = true;
    timerRunning = true;
    startSensors();
    
    // startTimer();
  }

  if (!digitalRead(BUTTON_RED_PIN) && !timerRunning) {
    timerStartMillis = millis();
    timerRunning = true;
    ringActive = false;
    startSensors();
  }
}
