#pragma once

#include "AppState.h"

void beginNetworkService(AppState& state);
void updateNetworkService(AppState& state, unsigned long now);
void publishTelemetry(AppState& state, unsigned long now);

