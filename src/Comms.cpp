#include "Comms.h"
#include "Sensors.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

extern DataMessage gloveData;
extern volatile int currentInputMode;
extern volatile int sensorValues[];
extern FingerProfile indexProfile;
extern FingerProfile middleProfile;
extern FingerProfile ringProfile;

// A0:F2:62:F2:2B:70 -- SuperMini ESP32-S3
const uint8_t RECEIVER_ADDRESS[] = {0xA0, 0xF2, 0x62, 0xF2, 0x2B, 0x70};
esp_now_peer_info_t peerInfo;
volatile bool rRecvConnected = false;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (memcmp(mac_addr, RECEIVER_ADDRESS, 6) == 0) {
        if (status == ESP_NOW_SEND_SUCCESS) {
            rRecvConnected = true;
        } else {
            rRecvConnected = false;
        }
    }
}

void initComms() {
    WiFi.mode(WIFI_STA);
    esp_wifi_set_max_tx_power(WIFI_POWER_8_5dBm);
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        while(1) { delay(10); }
    }
    esp_now_register_send_cb(OnDataSent);
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, RECEIVER_ADDRESS, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if(esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }

    gloveData.hand_id = 1;
    memset(gloveData.keysPressed, '\0', sizeof(gloveData.keysPressed));
}

void sendGloveData() {
    // UI mode (1) or rest mode (3)
    if (currentInputMode == 1 || currentInputMode == 3) {
        return;
    }

    static uint32_t lastSendTime = 0;
    if (millis() - lastSendTime >= 10) {
        lastSendTime = millis();
        
        if (currentInputMode == 2) {
            gloveData.scrollTicks = 0;
            gloveData.mouseX = 0;
            gloveData.mouseY = 0;
            gloveData.leftClick = false;
            gloveData.rightClick = false;
            gloveData.middleClick = false;
            
            memset(gloveData.keysPressed, '\0', sizeof(gloveData.keysPressed));
            
            if (sensorValues[3] > 500) {
                gloveData.keysPressed[2] = KEY_MAP[RING][getFingerRowCol(ringProfile).row][getFingerRowCol(ringProfile).column];
            }

            if (sensorValues[2] > 500) {
                gloveData.keysPressed[1] = KEY_MAP[MIDDLE][getFingerRowCol(middleProfile).row][getFingerRowCol(middleProfile).column];
            }

            if (sensorValues[1] > 500) {
                gloveData.keysPressed[0] = KEY_MAP[INDEX][getFingerRowCol(indexProfile).row][getFingerRowCol(indexProfile).column];
            }
        } else if (currentInputMode == 0) {
            memset(gloveData.keysPressed, '\0', sizeof(gloveData.keysPressed));
        }

        esp_now_send(RECEIVER_ADDRESS, (uint8_t *) &gloveData, sizeof(gloveData));

        if (currentInputMode == 0) {
            gloveData.mouseX = 0;
            gloveData.mouseY = 0;
        }
    }
}