#include "Comms.h"
#include "Sensors.h"
#include "Config.h"
#include "DisplayUI.h"
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
// E0:8C:FE:E5:FE:04 -- Left glove ESP32-WROOM-32
const uint8_t LEFT_GLOVE_ADDRESS[] = {0xE0, 0x8C, 0xFE, 0xE5, 0xFE, 0x04};

// Go to SEARCHING mode on startup
volatile ConnectionStatus rToRecvConnStatus = SEARCHING;
volatile ConnectionStatus lToRConnStatus = SEARCHING;
extern volatile ConnectionStatus lToRecvConnStatus;
const int MAX_SEARCH_TIME_SECONDS = 10;

ModeUpdateMessage leftModeUpdateMsg;
volatile bool pendingLeftUpdate = false;
uint32_t lastLeftSendTime = 0;

volatile LeftTelemetryMessage leftTelemetryData;
volatile bool newLeftTelemetryAvailable = false;
volatile uint32_t lastLeftTelemetryRecvTime = 0;

// R<>RECV connection struct
PeerConnection recvPeer = {RECEIVER_ADDRESS, false, 0};
// L<>R connection struct
PeerConnection leftPeer = {LEFT_GLOVE_ADDRESS, false, 0};

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    bool success = (status == ESP_NOW_SEND_SUCCESS);

    if (memcmp(mac_addr, RECEIVER_ADDRESS, 6) == 0) {
        if (success) {
            // Reset sleep vars on connection success
            recvPeer.searchStopped = false;
            recvPeer.firstFailTime = 0;
            rToRecvConnStatus = CONNECTED;
        } else {
            // Start timer on first fail, but keep it in SEARCHING mode
            if (recvPeer.firstFailTime == 0) {
                recvPeer.firstFailTime = millis();
                rToRecvConnStatus = SEARCHING; 
            }
        }
    } else if (memcmp(mac_addr, LEFT_GLOVE_ADDRESS, 6) == 0) {
        if (success) {
            // Reset sleep vars on connection success
            leftPeer.searchStopped = false;
            leftPeer.firstFailTime = 0;
            lToRConnStatus = CONNECTED;

            pendingLeftUpdate = false;
        } else {
            // Start timer on first fail, but keep it in SEARCHING mode
            if (leftPeer.firstFailTime == 0) {
                leftPeer.firstFailTime = millis();
                lToRConnStatus = SEARCHING;
            }
        }
    }
}

void OnDataRecv(const uint8_t* mac_addr, const uint8_t *incomingData, int len) {
    if (memcmp(mac_addr, LEFT_GLOVE_ADDRESS, 6) == 0) {
        if (len == sizeof(LeftTelemetryMessage)) {
            memcpy((void*) &leftTelemetryData, incomingData, sizeof(LeftTelemetryMessage));
            newLeftTelemetryAvailable = true;

            lastLeftTelemetryRecvTime = millis();
        }
    }
}

void addPeerDevice(const uint8_t* macAddr) {
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, macAddr, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if(esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
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
    esp_now_register_recv_cb(OnDataRecv);

    addPeerDevice(RECEIVER_ADDRESS);
    addPeerDevice(LEFT_GLOVE_ADDRESS);

    gloveData.hand_id = 1;
    memset(gloveData.keysPressed, '\0', sizeof(gloveData.keysPressed));
}

void sendGloveData() {
    // Check if the 10-second timeout has been reached
    if (!recvPeer.searchStopped && recvPeer.firstFailTime != 0 && (millis() - recvPeer.firstFailTime > (MAX_SEARCH_TIME_SECONDS * 1000))) {
        recvPeer.searchStopped = true;
        rToRecvConnStatus = DISCONNECTED;
    }

    if (!leftPeer.searchStopped && leftPeer.firstFailTime != 0 && (millis() - leftPeer.firstFailTime > (MAX_SEARCH_TIME_SECONDS * 1000))) {
        leftPeer.searchStopped = true;
        lToRConnStatus = DISCONNECTED;
    }

    // Do not run this function if searching has stopped
    if (recvPeer.searchStopped && leftPeer.searchStopped) {
        return;
    }    

    static uint32_t lastRecvSendTime = 0;

    bool activelySearching = (lToRConnStatus == SEARCHING && !leftPeer.searchStopped);
    bool isConnected = (lToRConnStatus == CONNECTED);
    
    uint32_t leftInterval = 0; // 0 means do not send

    if (pendingLeftUpdate || activelySearching) {
        leftInterval = 250; // Fast interval: aggressively sync or search
        leftModeUpdateMsg.wakeUpComms = true;
    } else if (isConnected) {
        leftInterval = 3000; // Slow interval: heartbeat to detect if it was turned off
        leftModeUpdateMsg.wakeUpComms = false;
    }

    if (leftInterval > 0) {
        if (lastLeftSendTime == 0 || millis() - lastLeftSendTime >= leftInterval) {
            lastLeftSendTime = millis();
            
            leftModeUpdateMsg.currentInputMode = currentInputMode;
            esp_now_send(LEFT_GLOVE_ADDRESS, (uint8_t *) &leftModeUpdateMsg, sizeof(leftModeUpdateMsg));
        }
    }
    
    // Use 10ms for active PC modes (0, 2), and 500ms for UI/Rest modes (1, 3) to save battery
    uint32_t sendInterval = (currentInputMode == 1 || currentInputMode == 3) ? 500 : 10;

    if (millis() - lastRecvSendTime >= sendInterval) {
        lastRecvSendTime = millis();
        
        if (!recvPeer.searchStopped) {
            
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
}

void queueLeftGloveModeUpdate() {
    pendingLeftUpdate = true;

    lastLeftSendTime = 0;

    leftPeer.searchStopped = false;
    leftPeer.firstFailTime = 0;
    lToRConnStatus = SEARCHING;
}

void wakeUpComms() {
    recvPeer.searchStopped = false;
    recvPeer.firstFailTime = 0;
    rToRecvConnStatus = SEARCHING;

    leftPeer.searchStopped = false;
    leftPeer.firstFailTime = 0;
    lToRConnStatus = SEARCHING;
    
    lastLeftSendTime = 0;

    queueLeftGloveModeUpdate();
}