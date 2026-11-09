#pragma once

#include <Arduino.h>
#include "AppState.h"

int readAveragedAdc(uint8_t pin);
int soilPercentFromRaw(int raw);
void readSensors(AppState& state);

