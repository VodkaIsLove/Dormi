#include <ESP8266WiFi.h>
#include <OpenWeather.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// ---PINS---
// LED Ring Parameter
#define LED_PIN D5 // Make sure this PIN is right!
#define BUTTON_RED_PIN D2 // Make sure this PIN is right!
#define BUTTON_BLACK_PIN D1 // Make sure this PIN is right!
#define STRIPSIZE 12

#define GASSENSOR_PIN D7
#define TEMP1_PIN D3
#define TEMP2_PIN D4
#define LED_AIR_PIN D6
#define LED_TEMP_PIN D8

// Parameter 
const float ADC_VOLT = 3.3;   // Board‑Versorgungsspannung
const int   ADC_MAX = 4095;  // 12‑Bit Auflösung

const float AirQ_OK = 1.00; // < 1.0 V = gute Luft

const float TEMP_MIN_OK = 16.0; // Untergrenze in Celsius
const float TEMP_MAX_OK = 20.0; // Obergrenze in Celsius



Adafruit_NeoPixel strip = Adafruit_NeoPixel(STRIPSIZE, LED_PIN, NEO_GRB + NEO_KHZ800);
int HELLIGKEIT = 20;



// WLAN-Zugangsdaten
const char* ssid     = "DEIN_SSID"; // HIER WLAN-SSID EINTRAGEN
const char* password = "DEIN_WIFI_PASSWORT"; // HIER  WLAN-PASSWORT EINTRAGEN


// OpenWeatherMap API
#define OWM_API_KEY "DEIN_API_KEY" // HIER  OPENWEATHERMAP API-SCHLÜSSEL EINTRAGEN


// Standort (Stadtname)
const char* cityName = "Oldenburg,de"; // HIER STADTNAME EINTRAGEN (z.B. "Berlin,de")

// OpenWeather Client
OpenWeatherClient owm(OWM_API_KEY);

// NTP Client für Timer
WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 3600;         // MEZ
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

// Timer-Variablen
unsigned long timerStartEpoch = 0;
bool timerRunning = false;


// Wetter-Funktion
/**
 * Liest die aktuelle Temperatur (°C) der eingestellten Stadt.
 * @return Temperatur in °C oder NAN bei Fehler.
 */
float getCurrentTemperature() {
  WeatherData data = owm.getCurrentWeather();
  if (data.isValid()) {
    return data.temperature;
  }
  return NAN;
}


// Timer-Funktionen
/**
 * Startet den 1-Stunden-Timer.
 */
void startTimer() {
  timeClient.update();
  timerStartEpoch = timeClient.getEpochTime();
  timerRunning = true;
}

/**
 * Prüft, ob der Timer noch läuft.
 * @return true, wenn weniger als 3600s seit Start vergangen sind.
 */
bool isTimerActive() {
  if (!timerRunning) return false;
  timeClient.update();
  return (timeClient.getEpochTime() - timerStartEpoch) < 3600;
}

/**
 * Gibt die verbleibende Zeit in Sekunden zurück.
 * @return Restsekunden oder 0.
 */
unsigned long getTimerRemaining() {
  if (!timerRunning) return 0;
  timeClient.update();
  unsigned long elapsed = timeClient.getEpochTime() - timerStartEpoch;
  if (elapsed < 3600) return 3600 - elapsed;
  timerRunning = false;
  return 0;
}

// Sensoren
float readVoltage(int pin) {
  int raw = analogRead(pin); // wert zwischen 0-4095
  return raw * ADC_VOLT / ADC_MAX ; // in Volt
}

float readTemp(int pin) {
  int raw = analogRead(pin);
  float voltage = raw * ADC_VOLT/ADC_MAX;
  float temperatureC = voltage / 0.01; // 10mV pro Celsius
  return temperatureC;
}

float calculateOptimalMaxTemp() {
  float currTemp = getCurrentTemparture();
  float maxTemp = 0;
  
  if(currTemp > 25){
    maxTemp = currTemp - 5;
  } else if(currTemp >= 12) {
    maxTemp = currTemp + 5;
  } else {
    maxTemp = TEMP_MIN_OK;
  }

  return maxTemp;

}

float calculateOptimalMinTemp() {
  float currTemp = getCurrentTemparture();
  float minTemp = 0;
  
 if(currTemp > 25){
    minTemp = currTemp + 5;
  } else if(currTemp >= 12) {
    minTemp = currTemp - 5;
  } else {
    minTemp = TEMP_MAX_OK;
  }

  return minTemp;
}


void setup() {
  // WLAN-Verbindung
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // Clients starten
  timeClient.begin();
  owm.setCity(cityName);

  // LED Ring setup
  pinMode(BUTTON_RED_PIN, INPUT_PULLUP); // Initialize Red Button
  pinMode(BUTTON_BLACK_PIN, INPUT_PULLUP); // Initialize Black Button
  strip.begin();
  strip.setBrightness(HELLIGKEIT);
  strip.show(); // Initialize all pixels to 'off'
  int LAUFZEIT = 120;     // 120sec: 2 minutes
  int ABKLINGZEIT = 20;

  // Sensoren und Status LED
  analogReadResolution(12);  // 12 Bit 0 bis 4095
  pinMode(LED_AIR_PIN, OUTPUT);
  pinMode(LED_TEMP_PIN, OUTPUT);

}

void loop() {
  
   if(!digitalRead(BUTTON_BLACK_PIN)) {
    startTimer();
  }

   // Luftqualität
  float air_volt = readVoltage(GASSENSOR_PIN);

  if (air_volt < AirQ_OK) {
    digitalWrite(LED_AIR_PIN,HIGH);
  } else {
    digitalWrite(LED_AIR_PIN,LOW);
  }

  // Temperatur
  float temp1 = readTemp(TEMP1_PIN);
  float temp2 = readTemp(TEMP2_PIN);

  // calculate average temperature
  float tempAvg = (temp1 + temp2) / 2.0;

  // get Optimal Temperature intervall
  float maxTemp = calculateOptimalMaxTemp(); 
  float minTemp = calculateOptimalMinTemp(); 

  // switch Status LED
  if (tempAvg >= maxTemp && tempAvg <= minTemp) {
    digitalWrite(LED_AIR_PIN,HIGH);
  } else {
    digitalWrite(LED_AIR_PIN,LOW);
  }

  
  delay(2000);
}



