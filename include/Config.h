/**
 * @file Config.h
 * @brief Defines all the pins, structs, and enums for the glove.
 * 
 * 
 * 
 */
#pragma once
#include <Arduino.h>

/**
 * @brief Represents the different fingers on the hand.
 * 
 */
enum Finger { INDEX, MIDDLE, RING, PINKY };

/**
 * @brief Represents the different rows on the virtual keyboard grid.
 * 
 */
enum BendZone { NUM_ROW, TOP_ROW, HOME_ROW, BOTTOM_ROW };

/**
 * @brief Represents the different columns on the virtual keyboard grid.
 * 
 */
enum RotZone { COL_MAIN, COL_ALT };

/**
 * @brief Represents the virtual keyboard layout and the corresponding key mappings for each finger.
 * 
 */
const char KEY_MAP[4][4][2] = {
  //--------------------------INDEX--------------------------
  {
    //MAIN | ALT
    { '7', '6'}, // NUM
    { 'u', 'y'}, // TOP
    { 'j', 'h'}, // HOME
    { 'm', 'n'} // BOTTOM
  },

  //-------------------------MIDDLE--------------------------
  {
    //MAIN | ALT (NONE)
    { '8', '\0'}, // NUM
    { 'i', '\0'}, // TOP
    { 'k', '\0'}, // HOME
    { ',', '\0'} // BOTTOM
  },

  //---------------------------RING--------------------------
  {
    //MAIN | ALT (NONE)
    { '9', '\0'}, // NUM
    { 'o', '\0'}, // TOP
    { 'l', '\0'}, // HOME
    { '.', '\0'} // BOTTOM
  },

  //--------------------------PINKY--------------------------
  {
    //MAIN | ALT -- ADD ALT COLUMN AFTER SOLDERING LAST FLEX SENSOR
    { '0', '\0'}, // NUM
    { 'p', '\0'}, // TOP
    { ';', '\0'}, // HOME
    { '/', '\0'} // BOTTOM
  }
};

//----------------------------------STRUCTS-------------------------------------
/**
 * @brief Represents the profile of a finger, including its flex sensor thresholds and associated multiplexer channels.
 * 
 */
struct FingerProfile {
  int bottomValMain; /**< The non-rotated sensor threshold for the bottom row */
  int homeValMain; /**< The non-rotated sensor threshold for the home row */

  int bottomValAlt; /**< The rotated sensor threshold for the bottom row */
  int homeValAlt; /**< The rotated sensor threshold for the home row */
  
  
  int altColValTop; /**< The sensor threshold for the alternate column on the top row */
  int altColValHome; /**< The sensor threshold for the alternate column on the home row */
  int altColValBottom; /**< The sensor threshold for the alternate column on the bottom row */

  int rowSensor; /**< The multiplexer channel for the bend flex sensor */
  int colSensor; /**< The multiplexer channel for the rotation flex sensor, if applicable. Otherwise, default to -1. */
};

/**
 * @brief Represents the row and column position of a finger on the virtual keyboard grid.
 * 
 */
struct FingerGridPos {
  BendZone row; /**< The row on the virtual keyboard grid (NUM_ROW, TOP_ROW, HOME_ROW, BOTTOM_ROW) */
  RotZone column; /**< The column on the virtual keyboard grid (COL_MAIN, COL_ALT). Only applies to index and pinky fingers. */
};

/**
 * @brief Represents the data structure that is sent to the receiver using ESP-NOW.
 * 
 */
typedef struct DataMessage {
  uint8_t hand_id; /**< 0 for left hand, 1 for right hand */
  int8_t mouseX; /**< Mouse X-axis displacement delta */
  int8_t mouseY; /**< Mouse Y-axis displacement delta */
  int8_t scrollTicks; /**< Number of scroll ticks when in mode 1*/
  bool leftClick; /**< True if the index FSR/primary flex sensor crosses the threshold in mode 0 */
  bool rightClick; /**< True if the ring FSR/flex sensor crosses the threshold in mode 0 */
  bool middleClick; /**< True if the middle FSR/flex sensor crosses the threshold in mode 0 */
  bool mouseFwd;
  bool mouseBack;
  char keysPressed[6]; /**< Array of active keystrokes based on KEY_MAP */
} DataMessage;

//--------------------------------PINS-----------------------------------
const int MUX_S0 = 14;
const int MUX_S1 = 27;
const int MUX_S2 = 26;
const int MUX_S3 = 25;
const int MUX_SIG = 34;

const int IMU_INT = 4;
const int IMU_RST = 23;
const int IMU_ADDRESS = 0x4B;

const int BATTERY_PCT_PIN = 35;

const int TFT_DIN = 32;
const int TFT_CLK = 18;
const int TFT_CS = 19;
const int TFT_DC = 33;
const int TFT_RST = 16;
const int TFT_BL = 17;