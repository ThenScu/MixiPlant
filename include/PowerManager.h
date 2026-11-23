#pragma once

#include "AppState.h"

void rememberPumpModeBeforeDeepSleep(const AppState& state);
void restorePumpModeAfterDeepSleep(AppState& state);
void enterDeepSleepDemoIfRequested(AppState& state, unsigned long now);
