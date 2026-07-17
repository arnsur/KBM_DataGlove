#include "Sensors.h"
#include "Motion.h"

extern volatile float smoothedYawDeg;
extern volatile float keyboardStartYawDeg;

const int NUM_SENSORS = 12;
volatile int sensorValues[NUM_SENSORS];

void initMux() {
    pinMode(MUX_S0, OUTPUT);
    pinMode(MUX_S1, OUTPUT);
    pinMode(MUX_S2, OUTPUT);
    pinMode(MUX_S3, OUTPUT);
}

void readAllMuxChannels() {
    for (int i = 0; i < NUM_SENSORS; i++) {
        digitalWrite(MUX_S0, bitRead(i, 0));
        digitalWrite(MUX_S1, bitRead(i, 1));
        digitalWrite(MUX_S2, bitRead(i, 2));
        digitalWrite(MUX_S3, bitRead(i, 3));

        delayMicroseconds(150);
        analogRead(MUX_SIG);
        delayMicroseconds(50);
        int signalValue;

        // Alpha-trimmed mean to filter noise for flex sensors (index 5+)
        if (i >= 5) {
            const int sampleCount = 7;
            int samples[sampleCount];
            for (int s = 0; s < sampleCount; s++) {
                samples[s] = analogRead(MUX_SIG);
            }

            // Sort samples from lowest to highest using insertion sort
            for (int j = 1; j < sampleCount; j++) {
                int key = samples[j];
                int k = j - 1;
                while (k >= 0 && samples[k] > key) {
                    samples[k + 1] = samples[k];
                    k--;
                }
                samples[k + 1] = key;
            }

            signalValue = (samples[2] + samples[3] + samples[4]) / 3;
        } else {
            signalValue = analogRead(MUX_SIG);
        }

        sensorValues[i] = signalValue;
    }
}


FingerGridPos getFingerRowCol(FingerProfile profile) {
  FingerGridPos fingerPos = {TOP_ROW, COL_MAIN};

  int rowVal = sensorValues[profile.rowSensor];
  int colVal = -1;

  fingerPos.column = COL_MAIN;
  if (profile.colSensor >= 0) {
    colVal = profile.altColVal;
    if (smoothedYawDeg - keyboardStartYawDeg >= colVal) {
      fingerPos.column = COL_ALT;
    }
  }

  if (fingerPos.column == COL_ALT) {
    if (rowVal > profile.bottomValAlt) {
      fingerPos.row = BOTTOM_ROW;
    } else if (rowVal > profile.homeValAlt) {
      fingerPos.row = HOME_ROW;
    } else {
      fingerPos.row = TOP_ROW;
    }
  } else {
    if (rowVal > profile.bottomValMain) {
      fingerPos.row = BOTTOM_ROW;
    } else if (rowVal > profile.homeValMain) {
      fingerPos.row = HOME_ROW;
    } else {
      fingerPos.row = TOP_ROW;
    }
  }

  return fingerPos;
}

void debugSensors() {
  for(int i = 0; i < 5; i++) {
    Serial.print("| FSR");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(sensorValues[i]);
  }

  for(int i = 5; i < NUM_SENSORS; i++) {
    Serial.print("| Flex");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(sensorValues[i]);
  }
  Serial.println();
}