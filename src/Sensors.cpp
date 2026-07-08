#include "Sensors.h"

const int NUM_SENSORS = 11;
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

  if (profile.colSensor >= 0) {
    colVal = sensorValues[profile.colSensor];
  }

  // Guess row based on main column
  BendZone estimatedRow;
  if (rowVal >= profile.bottomValMain) {
    estimatedRow = BOTTOM_ROW;
  } else if (rowVal >= profile.homeValMain) {
    estimatedRow = HOME_ROW;
  } else {
    estimatedRow = TOP_ROW;
  }
  
  // Fetch alt column threshold based on guessed row
  int currentAltColThreshold = 9999;
  if (estimatedRow == BOTTOM_ROW) {
    currentAltColThreshold = profile.altColValBottom;
  } else if (estimatedRow == HOME_ROW) {
    currentAltColThreshold = profile.altColValHome;
  } else {
    currentAltColThreshold = profile.altColValTop;
  }

  // Check if guessed row lines up with the current alt column threshold. If it does not, the guess must be wrong.
  if (colVal >= 0 && colVal > currentAltColThreshold) {
    fingerPos.column = COL_ALT;
  } else {
    fingerPos.column = COL_MAIN;
  }

  // Recalculate the row after finding the correct column
  int bottomRowThreshold = (fingerPos.column == COL_ALT) ? profile.bottomValAlt : profile.bottomValMain;
  int homeRowThreshold = (fingerPos.column == COL_ALT) ? profile.homeValAlt : profile.homeValMain;

  if (rowVal >= bottomRowThreshold) {
    fingerPos.row = BOTTOM_ROW;
  } else if (rowVal >= homeRowThreshold) {
    fingerPos.row = HOME_ROW;
  } else {
    fingerPos.row = TOP_ROW;
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