#pragma once
#include "Config.h"

extern volatile int currentMouseMode;

extern volatile float smoothedYawDeg;
extern volatile float smoothedRollDeg;
extern volatile float keyboardStartYawDeg;

void initIMU();
void updateMotion();