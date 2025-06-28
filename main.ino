#include <ESP8266WiFi.h>
#include <OpenWeather.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Adafruit_Neopixel.h>

// ---PINS---
// LED Ring Parameter
#define LED_PIN D5 
#define BUTTON_RED_PIN D2 //  Starts measurement only
#define BUTTON_BLACK_PIN D1 // Starts measurement + LED ring
#define STRIPSIZE 12

#define GASSENSOR_PIN A0
#define TEMP1_PIN D3
#define TEMP2_PIN D4
#define LED_AIR_PIN D6
#define LED_TEMP_PIN D7

// Parameter 
const float ADC_VOLT = 3.3;   // Board‑Versorgungsspannung
const int   ADC_MAX = 1023;  // 10‑Bit Auflösung

const float AirQ_OK = 1.00; // < 1.0 V = gute Luft

const float TEMP_MIN_OK = 16.0; // Untergrenze in Celsius
const float TEMP_MAX_OK = 20.0; // Obergrenze in Celsius



Adafruit_NeoPixel strip = Adafruit_NeoPixel(STRIPSIZE, LED_PIN, NEO_GRB + NEO_KHZ800);
int HELLIGKEIT = 20;

bool ringActive = false;
int LAUFZEIT = 120; //2 Minuten als Test
int ABKLINGZEIT = 20; //Alle 20 sec anpassen

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
  //
  long sollZeit = timeClient.getEpochTime();
  int ROT = 255;
  int GRUEN = 147;
  int BLAU = 41;
  sollZeit = sollZeit + LAUFZEIT;
  colorFill(ROT, GRUEN, BLAU);
  //
  while (timeClient.getEpochTime() < sollZeit){
    delay(1000);
    timeClient.update();
  }
  timeClient.update();
  double i = ABKLINGZEIT / HELLIGKEIT;
  int j = 1;
  long sollZeit = timeClient.getEpochTime();
  sollZeit = sollZeit + ABKLINGZEIT;
  while (timeClient.getEpochTime() < sollZeit){
    delay(1000);
    strip.setBrightness(HELLIGKEIT - (i*j));
    j++;
  }
  strip.setBrightness(0);
  timerRunning = false;
}

void colorFill(int r, int g, int b) {
  for(int i=0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.Color(r, g, b));
      strip.show();
  }
}

void showStatus(){
      digitalWrite(LED_AIR_PIN,HIGH);
      digitalWrite(LED_TEMP_PIN,HIGH);
  delay(10000);
      digitalWrite(LED_AIR_PIN,LOW);
      digitalWrite(LED_TEMP_PIN,LOW);
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

float OptimalMaxTemp() {
  float currTemp = getCurrentTemperature();
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

float OptimalMinTemp() {
  float currTemp = getCurrentTemperature();
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

void startSensors(){
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
  float maxTemp = OptimalMaxTemp(); 
  float minTemp = OptimalMinTemp(); 

  // switch Status LED
  if (tempAvg >= maxTemp && tempAvg <= minTemp) {
    digitalWrite(LED_AIR_PIN,HIGH);
  } else {
    digitalWrite(LED_AIR_PIN,LOW);
  }

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

  // NeoPixel
  pinMode(BUTTON_RED_PIN, INPUT_PULLUP); // Initialize Red Button
  pinMode(BUTTON_BLACK_PIN, INPUT_PULLUP); // Initialize Black Button

  // LED Ring setup
  strip.begin();
  strip.setBrightness(HELLIGKEIT);
  strip.show(); // Initialize all pixels to 'off'

  // Sensoren und Status LED
  analogReadResolution(12);  // 12 Bit 0 bis 4095
  pinMode(LED_AIR_PIN, OUTPUT);
  pinMode(LED_TEMP_PIN, OUTPUT);

}

void loop() {

  timeClient.update();

  // Reset während Ausführung: Leerlauf-Zustand wiederherstellen //HIER
  if (timerRunning && (!digitalRead(BUTTON_BLACK_PIN) || !digitalRead(BUTTON_RED_PIN))) {
    timerRunning = false;
    ringActive   = false;
    strip.clear();
    strip.show();
    // LEDs zurücksetzen
    digitalWrite(LED_AIR_PIN, LOW);
    digitalWrite(LED_TEMP_PIN, LOW);
    // warte Entprellung
    delay(200);
    return; // zurück in Leerlauf
  }

  // Button: black -> start measurement timer + ring
  if(!digitalRead(BUTTON_BLACK_PIN) && !timerRunning) {
    ringActive = true;
    startSensors();
    delay(200);
    startTimer();
  }

  // Button: red -> start measurement 
  if(!digitalRead(BUTTON_RED_PIN) && !timerRunning) {
    timerStartEpoch = timeClient.getEpochTime();
    timerRunning = true;
    ringActive = false;
    startSensors();
    delay(200);
  }
  
  delay(2000);
}



