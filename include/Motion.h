#pragma once
#include "Config.h"

extern volatile int currentMouseMode;

extern volatile float smoothedYaw;
extern volatile float smoothedRoll;

void initIMU();
void updateMotion();