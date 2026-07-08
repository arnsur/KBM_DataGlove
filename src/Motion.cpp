#include "Motion.h"
#include <Wire.h>
#include <Sparkfun_BNO08x_Arduino_Library.h>

BNO08x imu;

extern DataMessage gloveData;
extern volatile int currentInputMode;
extern volatile int16_t cursor_x;
extern volatile int16_t cursor_y;
extern volatile bool lmbClicked;
extern volatile int sensorValues[];

float mouseSensitivity = 60;
float guiMouseSensMult = 0.33;
float lastRoll = 0.0;
float lastYaw = 0.0;
volatile float smoothedYaw = 0.0;
volatile float smoothedRoll = 0.0;
float remainderX = 0.0;
float remainderY = 0.0;
float guiRemainderX = 0.0;
float guiRemainderY = 0.0;
float mouseSmoothingAlpha = 0.4;

volatile int currentMouseMode = 0; // 0 - Clutch, LMB, MMB, RMB, Switch | 1 - Scroll up, MB5, MB4, Scroll down, Switch

bool lastClutchState = false;
bool clutchBent = false;
const int clutchStartThreshold = 3000;
const int clutchExitThreshold = 2850;

bool lastSwitchState = false;
bool switchBent = false;
const int switchStartThreshold = 2650;
const int switchExitThreshold = 2500;

static uint32_t clutchPressTime = 0;
static bool clutchLongPressHandled = false;
const uint32_t LONG_PRESS_DELAY_MS = 550;
int lastInputModeBeforeSwitch = 0;

void initIMU() {
    pinMode(IMU_RST, OUTPUT);
    digitalWrite(IMU_RST, LOW);
    delay(50);
    digitalWrite(IMU_RST, HIGH);
    delay(150);

    Wire.begin();
    Wire.setClock(400000);

    pinMode(IMU_INT, INPUT_PULLUP);

    Serial.println("Initializing BNO085 IMU...");
    if (imu.begin(IMU_ADDRESS, Wire, IMU_INT, IMU_RST) == false) {
        Serial.print("BNO085 not detected at 0x");
        Serial.println(IMU_ADDRESS, HEX);
        Serial.println("Freezing code here.");
        while (1) { delay(10); }
    }

    imu.enableGameRotationVector(10);
}

void updateMotion() {
    float roll, pitch, yaw;
    if (imu.wasReset()) {
        Serial.println("BNO085 reset detected. Re-enabling sensors...");
        delay(50);
        imu.enableGameRotationVector(10);
        delay(10);
    }

    if (imu.getSensorEvent() == true) {
        if (imu.getSensorEventID() == SENSOR_REPORTID_GAME_ROTATION_VECTOR) {
            roll = (imu.getRoll()) * 180.0 / PI; // Physically pitch wrist based on IMU orientation, but reported as roll.
            pitch = (imu.getPitch()) * 180.0 / PI; // Physically roll wrist based on IMU orientation, but reported as pitch.
            yaw = (imu.getYaw()) * 180.0 / PI; // Aligns with physical yaw of the wrist.

            if (lastYaw == 0) {
                smoothedYaw = lastYaw;
                lastYaw = yaw;
            }

            if (lastRoll == 0) {
                smoothedRoll = lastRoll;
                lastRoll = roll;
            }

            smoothedYaw = (mouseSmoothingAlpha * yaw) + ((1.0 - mouseSmoothingAlpha) * smoothedYaw);
            smoothedRoll = (mouseSmoothingAlpha * roll) + ((1.0 - mouseSmoothingAlpha) * smoothedRoll);

            float yawDisplacement = smoothedYaw - lastYaw;
            lastYaw = smoothedYaw;
            float rollDisplacement = smoothedRoll - lastRoll;
            lastRoll = smoothedRoll;

            float mouseX = -yawDisplacement * mouseSensitivity;
            mouseX += remainderX;
            float mouseY = rollDisplacement * mouseSensitivity;
            mouseY += remainderY;

            remainderX = mouseX - (int) mouseX;
            remainderY = mouseY - (int) mouseY;

            if (sensorValues[5] >= clutchStartThreshold) {
                clutchBent = true;
            } else if (sensorValues[5] < clutchExitThreshold) {
                clutchBent = false;
            }

            bool clutchActsAsModeSwitch = !(currentInputMode == 0 && currentMouseMode == 1);
            if (clutchActsAsModeSwitch) {
                // Detect the exact moment the clutch is bent
                if (clutchBent && !lastClutchState) {
                    clutchPressTime = millis();
                    clutchLongPressHandled = false;
                }

                // LONG PRESS: Toggle Rest Mode (Mode 3)
                if (clutchBent && !clutchLongPressHandled) {
                    if (millis() - clutchPressTime > LONG_PRESS_DELAY_MS) {
                        if (currentInputMode == 3) {
                            currentInputMode = lastInputModeBeforeSwitch;
                        } else {
                            lastInputModeBeforeSwitch = currentInputMode;
                            currentInputMode = 3;
                        }
                        clutchLongPressHandled = true; 
                    }
                }

                // SHORT PRESS: Cycle Active Modes (0 -> 1 -> 2 -> 0)
                if (!clutchBent && lastClutchState) {
                    if (!clutchLongPressHandled && currentInputMode != 3) {
                        currentInputMode = (currentInputMode + 1) % 3;
                        currentMouseMode = 0; // Reset mouse mode if the input mode is cycled
                    }
                }
            }

            lastClutchState = clutchBent;

            if (sensorValues[10] >= switchStartThreshold) {
                switchBent = true;
            } else if (sensorValues[10] < switchExitThreshold) {
                switchBent = false;
            }
            if (currentInputMode == 0 && switchBent && lastSwitchState == false) {
                if (currentMouseMode == 0) {
                    currentMouseMode = 1;
                } else if (currentMouseMode == 1) {
                    currentMouseMode = 0;
                }
            }
            lastSwitchState = switchBent;
            
            
            lmbClicked = false;
            bool rmbClicked = false;
            bool mmbClicked = false;
            bool mb5Clicked = false;
            bool mb4Clicked = false;
            bool scrollUp = false;
            bool scrollDown = (currentInputMode <= 1 && currentMouseMode == 1 && (sensorValues[0] > 2100 || sensorValues[5] > 2800));

            if (currentInputMode <= 1 && (sensorValues[1] > 2250 || sensorValues[7] > 2600)) {
                if (currentMouseMode == 0) {
                    lmbClicked = true;
                    mb4Clicked = false;
                } else if (currentMouseMode == 1) {
                    lmbClicked = false;
                    mb4Clicked = true;
                }
            }

            if (currentInputMode <= 1 && (sensorValues[3] > 2150 || sensorValues[9] > 2600)) {
                if (currentMouseMode == 0) {
                    rmbClicked = true;
                    scrollUp = false;
                } else if (currentMouseMode == 1) {
                    rmbClicked = false;
                    scrollUp = true;
                }
            }

            if (currentInputMode <= 1 && (sensorValues[2] > 2150 || sensorValues[8] > 2600)) {
                if (currentMouseMode == 0) {
                    mmbClicked = true;
                    mb5Clicked = false;
                } else if (currentMouseMode == 1) {
                    mmbClicked = false;
                    mb5Clicked = true;
                }
            }

            int scrollTicks = 0;
            if (scrollUp) scrollTicks++;
            if (scrollDown) scrollTicks--;

            uint8_t currentButtons = 0;
            if (lmbClicked) bitSet(currentButtons, 0);
            if (rmbClicked) bitSet(currentButtons, 1);
            if (mmbClicked) bitSet(currentButtons, 2);

            static uint8_t previousButtons = 0;
            static uint32_t lastStateChangeTime = 0;
            const uint32_t CLICK_FREEZE_MS = 150;

            if (currentButtons != previousButtons) {
                lastStateChangeTime = millis();
            }
            previousButtons = currentButtons;

            if (millis() - lastStateChangeTime < CLICK_FREEZE_MS || currentInputMode > 1) {
                gloveData.mouseX = 0;
                gloveData.mouseY = 0;
            } else {
                gloveData.mouseX = (int8_t) mouseX;
                gloveData.mouseY = (int8_t) mouseY;
            }
            
            if (currentInputMode == 1) {
                float exactGuiX = (gloveData.mouseX * guiMouseSensMult) + guiRemainderX;
                float exactGuiY = (gloveData.mouseY * guiMouseSensMult) + guiRemainderY;
    
                cursor_x += (int16_t) exactGuiX;
                cursor_y += (int16_t) exactGuiY;
                guiRemainderX = exactGuiX - (int16_t) exactGuiX;
                guiRemainderY = exactGuiY - (int16_t) exactGuiY;
            }

            // Clamp to UI screen edges
            if (cursor_x < 0) cursor_x = 0;
            if (cursor_x > 319) cursor_x = 319;
            if (cursor_y < 0) cursor_y = 0;
            if (cursor_y > 239) cursor_y = 239;
            
            gloveData.leftClick = lmbClicked;
            gloveData.rightClick = rmbClicked;
            gloveData.middleClick = mmbClicked;
            gloveData.scrollTicks = scrollTicks;
            gloveData.mouseFwd = mb5Clicked;
            gloveData.mouseBack = mb4Clicked;
            
            // Serial.print("Yaw: ");
            // Serial.print(yaw);
            // Serial.print(" | Roll: ");
            // Serial.print(roll);
            // Serial.print(" | mouseX: ");
            // Serial.print(mouseX);
            // Serial.print(" | mouseY: ");
            // Serial.println(mouseY);
        }
    }
}