#include <WiFi.h>

#define BLYNK_TEMPLATE_ID "TMPL3e1WGk52H"
#define BLYNK_TEMPLATE_NAME "Water Tank Alarm"
#define BLYNK_FIRMWARE_VERSION "0.1.0"

#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG
#define APP_DEBUG

#include <BlynkEdgent.h>  // Blynk “Edgent” template-based connector

//— Tank geometry & calibration
const float TANK_EMPTY_CM   = 45.0f;   // sensor→water when tank is EMPTY
const float TANK_FULL_CM    =  20.0f;   // sensor→water when tank is FULL
const float TANK_RADIUS_CM  = 10.0f;   // radius of cylindrical tank

// Precomputed constants
const float TANK_CROSS_AREA   = 3.14159f * TANK_RADIUS_CM * TANK_RADIUS_CM;  
const float TANK_HEIGHT_CM    = TANK_EMPTY_CM - TANK_FULL_CM;        // usable measurement span
const float TANK_VOLUME_CM3   = TANK_CROSS_AREA * TANK_HEIGHT_CM;    // max fill volume

//— Pins & virtual pins
#define TRIG_PIN            27
#define ECHO_PIN            26

#define VP_CONTINUOUS_MODE  V1
#define VP_ONE_SHOT         V3
#define VP_DISTANCE         V0
#define VP_TANK_VOLUME      V6
#define VP_PERCENT          V7

//— Timing
const unsigned long SEND_INTERVAL = 1000;
unsigned long lastSendTime = 0;

//— State
bool continuousMode = false;

BLYNK_WRITE(VP_CONTINUOUS_MODE) {
  continuousMode = param.asInt();
  Serial.printf("Continuous mode %s\n",
                continuousMode ? "ON" : "OFF");
  lastSendTime = millis();
}

BLYNK_WRITE(VP_ONE_SHOT) {
  if (param.asInt()) {
    float d = measureDistanceCm();
    processAndSend(d);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  BlynkEdgent.begin();
  Blynk.virtualWrite(VP_DISTANCE, 0);
  Blynk.syncVirtual(VP_CONTINUOUS_MODE);
}

void loop() {
  BlynkEdgent.run();

  if (continuousMode && (millis() - lastSendTime >= SEND_INTERVAL)) {
    lastSendTime = millis();
    float d = measureDistanceCm();
    processAndSend(d);
  }
}

//— Measure the distance once, then clamp between FULL and EMPTY
float measureDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long dur = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (dur == 0) {
    Serial.println("Out of range → treating as EMPTY");
    return TANK_EMPTY_CM;
  }

  float cm = dur / 58.0f;
  // Clamp into [TANK_FULL_CM, TANK_EMPTY_CM]
  cm = constrain(cm, TANK_FULL_CM, TANK_EMPTY_CM);

  Serial.printf("Distance: %.1f cm\n", cm);
  return cm;
}

//— Compute & send raw distance, volume, and percentage
void processAndSend(float dist_cm) {
  // 1) Raw distance
  Blynk.virtualWrite(VP_DISTANCE, int(dist_cm + 0.5f));

  // 2) Fill height = how many cm of water are in the tank
  float fill_cm = TANK_EMPTY_CM - dist_cm;  // 0 → TANK_HEIGHT_CM

  // 3) Volume (cm³) and % full
  float vol_filled = TANK_CROSS_AREA * fill_cm;
  float pct       = (fill_cm / TANK_HEIGHT_CM) * 100.0f;

  Blynk.virtualWrite(VP_TANK_VOLUME, int(vol_filled + 0.5f));
  Blynk.virtualWrite(VP_PERCENT,     int(pct       + 0.5f));

  Serial.printf("Volume: %.0f cm³  |  %.1f%% full\n",
                vol_filled, pct);
}
