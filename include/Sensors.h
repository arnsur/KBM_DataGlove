/**
 * @file Sensors.h
 * @brief Handles initialization and reading of FSRs and flex sensors using the multiplexer.
 * 
 * 
 * 
 */
#pragma once
#include "Config.h"

extern const int NUM_SENSORS;
extern volatile int sensorValues[];

/**
 * @brief Initializes the multiplexer pins as outputs.
 * @note This function should be called once in main.cpp setup().
 */
void initMux();

/**
 * @brief Reads all the multiplex channels and updates the sensorValues array with the latest readings.
 * @note Applies smoothing to typing fingers (not clutch or FSRs).
 */
void readAllMuxChannels();

/**
 * @brief Print all current sensor values to the serial monitor for debugging purposes.
 * @note Add to main.cpp loop() when only necessary. It may slow down the glove's performance when called. 
 */
void debugSensors();

/**
 * @brief Get the row and column for a given finger based on its bend flex sensor and rotation flex sensor (if applicable) values.
 * 
 * @param profile The specific finger to check.
 * @return FingerGridPos A struct containing the row (BendZone) and column (RotZone) of the finger.
 * @note If a finger's rotation flex sensor does not exist (colSensor = -1), the column defaults to COL_MAIN.
 */
FingerGridPos getFingerRowCol(FingerProfile profile);