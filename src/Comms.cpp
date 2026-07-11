#include "Comms.h"
#include "Sensors.h"
#include "Config.h"
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

volatile int rRecvConnStatus = SEARCHING; // Go to SEARCHING mode on startup
const int MAX_SEARCH_TIME_SECONDS = 10;

// R<>RECV connection struct
PeerConnection recvPeer = {RECEIVER_ADDRESS, false, 0};

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    bool success = (status == ESP_NOW_SEND_SUCCESS);

    if (memcmp(mac_addr, RECEIVER_ADDRESS, 6) == 0) {
        if (success) {
            // Reset sleep vars on connection success
            recvPeer.searchStopped = false;
            recvPeer.firstFailTime = 0;
            rRecvConnStatus = CONNECTED;
        } else {
            // Start timer on first fail, but keep it in SEARCHING mode
            if (recvPeer.firstFailTime == 0) {
                recvPeer.firstFailTime = millis();
                rRecvConnStatus = SEARCHING; 
            }
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
    // Check if the 10-second timeout has been reached
    if (!recvPeer.searchStopped && recvPeer.firstFailTime != 0 && (millis() - recvPeer.firstFailTime > (MAX_SEARCH_TIME_SECONDS * 1000))) {
        recvPeer.searchStopped = true;
        rRecvConnStatus = DISCONNECTED;
    }

    // Do not run this function if searching has stopped
    if (recvPeer.searchStopped) {
        return;
    }

    static uint32_t lastSendTime = 0;
    
    // Use 10ms for active PC modes (0, 2), and 500ms for UI/Rest modes (1, 3) to save battery
    uint32_t sendInterval = (currentInputMode == 1 || currentInputMode == 3) ? 500 : 10;

    if (millis() - lastSendTime >= sendInterval) {
        lastSendTime = millis();
        
        // Keep sending packets to maintain connection in modes 1 and 3 but zero out HID data.
        if (currentInputMode == 1 || currentInputMode == 3) {
            gloveData.mouseX = 0;
            gloveData.mouseY = 0;
            gloveData.scrollTicks = 0;
            gloveData.leftClick = false;
            gloveData.rightClick = false;
            gloveData.middleClick = false;
            gloveData.mouseFwd = false;
            gloveData.mouseBack = false;
            memset(gloveData.keysPressed, '\0', sizeof(gloveData.keysPressed));
        } else if (currentInputMode == 2) {
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

void wakeUpComms() {
    recvPeer.searchStopped = false;
    recvPeer.firstFailTime = 0;
    rRecvConnStatus = SEARCHING;
}