// TODO: Add doxygen comments to DisplayUI.h and Motion.h
#include <Arduino.h>

#include "Config.h"
#include "Sensors.h"
#include "Motion.h"
#include "Comms.h"
#include "DisplayUI.h"

volatile int currentInputMode = 1; //0 - M | 1 - UI | 2 - K | 3 - R |

FingerProfile indexProfile = {
    .bottomValMain = 2600,
    .homeValMain = 2430,

    .bottomValAlt = 2550,
    .homeValAlt = 2400,

    .altColVal = 1400,

    .rowSensor = 7,
    .colSensor = 6
};

FingerProfile middleProfile = {
    .bottomValMain = 2530,
    .homeValMain = 2360,

    .bottomValAlt = 2530,
    .homeValAlt = 2360,
    
    .altColVal = 9999,

    .rowSensor = 8,
    .colSensor = -1
};

FingerProfile ringProfile = {
    .bottomValMain = 2610,
    .homeValMain = 2455,

    .bottomValAlt = 2610,
    .homeValAlt = 2455,
    
    .altColVal = 9999,

    .rowSensor = 9,
    .colSensor = -1
};

DataMessage gloveData;

int batteryLevel = -1;
float smoothedBatteryRaw = 0.0;

volatile int shared_battery_l = 0; // Placeholder until the second glove is made
volatile int shared_battery_r = 999;

volatile int16_t cursor_x = 160; 
volatile int16_t cursor_y = 120;
volatile bool lmbClicked = false;

void setup() {
    initDisplay();

    delay(3000);

    initIMU();
    initMux();
    initComms();

    startGUITask();
}

void loop() {
    analogRead(BATTERY_PCT_PIN);
    delayMicroseconds(10);
    int currentRaw = analogReadMilliVolts(BATTERY_PCT_PIN);
    if (smoothedBatteryRaw == 0.0) smoothedBatteryRaw = currentRaw;
    smoothedBatteryRaw = (0.05 * currentRaw) + (0.95 * smoothedBatteryRaw);
    batteryLevel = (int) smoothedBatteryRaw;

    int mapped_pct = constrain(map(batteryLevel, 1600, 2020, 0, 100), 0, 100);
    shared_battery_r = mapped_pct;

    // Prevent the glove from shutting down when plugged in to upload code.
    // If the battery level is below 2V (1000 mV at the voltage divider),
    // the battery cannot be connected as this is an impossible voltage for a 3.7V LiPo battery to discharge to.
    if (mapped_pct <= 0 && batteryLevel > 1000) {
        Serial.print("Battery dead. Shutting down. | batteryLevel: ");
        Serial.println(batteryLevel);
        digitalWrite(TFT_BL, LOW);
        esp_deep_sleep_start();
    }
    
    readAllMuxChannels();
    updateMotion();
    sendGloveData();
}