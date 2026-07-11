/**
 * @file Comms.h
 * @brief Handles all ESP-NOW wireless communication for the glove.
 * * This module is responsible for configuring the Wi-Fi radio, registering the MAC address
 * of the receiver, and packaging/transmitting the gloveData struct at a fixed interval.
 */
#pragma once
#include "Config.h"

extern volatile int rRecvConnStatus;

/**
 * @brief Initializes ESP-NOW communication, sets up WiFi, and registers the receiver's MAC address.
 * 
 */
void initComms();

/**
 * @brief Sends the current gloveData struct to the receiver using ESP-NOW.
 */
void sendGloveData();

void wakeUpComms();