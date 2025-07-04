#include <ESP8266WiFi.h>
#include <Adafruit_NeoPixel.h>

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

const float AirQ_OK = 1.80;  // < 1.0 V = gute Luft

const float TEMP_MIN_OK = 16.0;  // Untergrenze in Celsius
const float TEMP_MAX_OK = 21.0;  // Obergrenze in Celsius

Adafruit_NeoPixel strip = Adafruit_NeoPixel(STRIPSIZE, LED_PIN, NEO_GRB + NEO_KHZ800);
int HELLIGKEIT = 20;

bool ringActive = false;
int LAUFZEIT = 120;    //2 Minuten als Test
int ABKLINGZEIT = 20;  //Alle 20 sec anpassen

// Timer-Variablen
unsigned long timerStartMillis = 0;
bool timerRunning = false;

// Button Variablen
bool wasRedPressed = false;
bool wasBlackPressed = false;

const unsigned long DEBOUNCE_DELAY = 50;  // ms
unsigned long lastDebounceTimeRed   = 0;
unsigned long lastDebounceTimeBlack = 0;
bool lastButtonStateRed   = HIGH;
bool lastButtonStateBlack = HIGH;
bool stableButtonStateRed   = HIGH;
bool stableButtonStateBlack = HIGH;

// Milis variablen für den loop
unsigned long lastSensorReadMillis = 0;
const unsigned long SENSOR_INTERVAL = 200;  

unsigned long abklingStartMillis = 0;
bool abklingRunning = false;


bool readButtonDebounced(int pin, bool &lastStableState, bool &lastRawState, unsigned long &lastTime) {
    bool raw = digitalRead(pin);
    if (raw != lastRawState) {
      // Wechsel registriert: starte neuen Timer
      lastTime = millis();
      lastRawState = raw;
    }
    // Wenn seit dem letzten Wechsel genug Zeit vergangen ist ...
    if (millis() - lastTime > DEBOUNCE_DELAY) {
      // ... und der Zustand hat sich seit der letzten Stabilisierung geändert:
      if (raw != lastStableState) {
        lastStableState = raw;
        return true;  // nur beim wirklichen Zustandswechsel liefern wir „true“
      }
    }
    return false;
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

// Sensoren
float readVoltage(int pin) {
  int raw = analogRead(pin);        // wert zwischen 0-4095
  return raw * ADC_VOLT / ADC_MAX;  // in Volt
}

float readTemp(int pin) {
  int raw = analogRead(pin);
  float voltage = raw * 1.0 / ADC_MAX;
  float temperatureC = (voltage / 0.01);  // 10mV pro Celsius
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

  // zeigt aktuelle werte an
  Serial.begin(115200);
  Serial.print(" Air voltage:  ");
  Serial.println(air_volt);
  Serial.print(" Temp: ");
  Serial.println(temp);
}

void resetToStandby() {
  // Timer stoppen
  timerRunning   = false;
  abklingRunning = false;
  ringActive     = false;

  // NeoPixel ausschalten
  strip.clear();
  strip.show();

  // Status‑LEDs wieder auf „Bereitschaft“ (z. B. HIGH = eingebauter LED‑Pullup)
  digitalWrite(LED_AIR_PIN,  HIGH);
  digitalWrite(LED_TEMP_PIN, HIGH);

  // Optional: Serielle Ausgabe zur Kontrolle
  Serial.println(">>> RESET: zurück in Bereitschaft");

  
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

  // Status LEDs zurücksetzen
  pinMode(LED_AIR_PIN, OUTPUT);
  pinMode(LED_TEMP_PIN, OUTPUT);

  digitalWrite(LED_AIR_PIN, HIGH);
  digitalWrite(LED_TEMP_PIN, HIGH);
 
}

// ---------- LOOP ----------
void loop() {

  bool redChanged   = readButtonDebounced(BUTTON_RED_PIN,   stableButtonStateRed,   lastButtonStateRed,   lastDebounceTimeRed);
  bool blackChanged = readButtonDebounced(BUTTON_BLACK_PIN, stableButtonStateBlack, lastButtonStateBlack, lastDebounceTimeBlack);

  // Wenn gedrückt und es ist wirklich ein Wechsel zum LOW-Zustand:
  bool redPressed   = (stableButtonStateRed   == LOW) && redChanged;
  bool blackPressed = (stableButtonStateBlack == LOW) && blackChanged;

    // Reset während Ausführung: Leerlauf-Zustand wiederherstellen //HIER
 if (timerRunning && (redPressed || blackPressed)) {
    resetToStandby();
    return;  // sofort raus aus loop(), bis zum nächsten Durchlauf
  }


 if (timerRunning) {
  unsigned long currentMillis = millis();
  unsigned long elapsed = currentMillis - timerStartMillis;

  // Sensorwerte alle 200ms abfragen (nicht-blockierend)
  if (currentMillis - lastSensorReadMillis >= SENSOR_INTERVAL) {
    lastSensorReadMillis = currentMillis;
    startSensors();
  }

    if (!ringActive) {
      if (!abklingRunning) {
        if (elapsed >= LAUFZEIT * 1000) {
          // Start Abklingen
          abklingStartMillis = currentMillis;
          abklingRunning = true;
        }
      } else {
        unsigned long abklingElapsed = currentMillis - abklingStartMillis;
        if (abklingElapsed >= ABKLINGZEIT * 1000) {
          // Timer beenden
          timerRunning = false;
          abklingRunning = false;
          digitalWrite(LED_AIR_PIN, HIGH);
          digitalWrite(LED_TEMP_PIN, HIGH);
        }
      }
    } else {
      if (!abklingRunning) {
        if (elapsed < LAUFZEIT * 1000) {
          colorFill(255, 147, 41);
          strip.setBrightness(HELLIGKEIT);
          strip.show();
        } else {
          abklingStartMillis = currentMillis;
          abklingRunning = true;
        }
      } else {
        unsigned long abklingElapsed = currentMillis - abklingStartMillis;
        if (abklingElapsed < ABKLINGZEIT * 1000) {
          float fraction = 1.0 - (float)abklingElapsed / (ABKLINGZEIT * 1000);
          int newBrightness = (int)(HELLIGKEIT * fraction);
          strip.setBrightness(newBrightness);
          strip.show();
        } else {
          strip.setBrightness(0);
          strip.clear();
          strip.show();
          timerRunning = false;
          abklingRunning = false;
          digitalWrite(LED_AIR_PIN, HIGH);
          digitalWrite(LED_TEMP_PIN, HIGH);
          abklingStartMillis = 0;
        }
      }
    }
  }

  if (blackPressed && !wasBlackPressed && !timerRunning) {
    timerStartMillis = millis();
    ringActive = true;
    timerRunning = true;
    startSensors();
    
    // startTimer();
  }

  if (redPressed && !wasRedPressed && !timerRunning) {
    timerStartMillis = millis();
    timerRunning = true;
    ringActive = false;
    startSensors();
  }
}
