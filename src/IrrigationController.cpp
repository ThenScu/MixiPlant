#include "IrrigationController.h"

#include "Config.h"

bool updateIrrigationDecision(AppState& state, unsigned long now) {
  bool wantsPumpOn = false;
  String reason = "STANDBY";
  const bool heavyRain = state.rainingNow || state.rainEff >= RAIN_THRESHOLD;

  if (state.pumpMode == "MANUAL_ON") {
    state.irrigationCycle = false;
    wantsPumpOn = true;
    reason = "APP BAT";
  } else if (state.pumpMode == "MANUAL_OFF") {
    state.irrigationCycle = false;
    state.pulseIsOn = false;
    wantsPumpOn = false;
    reason = "APP TAT";
  } else if (state.currentMoisture >= TARGET_MOISTURE) {
    state.irrigationCycle = false;
    state.pulseIsOn = false;
    wantsPumpOn = false;
    reason = "DU AM";
  } else {
    const bool survival = state.currentMoisture < CRITICAL_LOW;
    const bool soilIsDry = state.currentMoisture < DRY_THRESHOLD;

    if (!state.irrigationCycle && (survival || (soilIsDry && !heavyRain))) {
      state.irrigationCycle = true;
      state.pulseIsOn = true;
      state.pulsePhaseStarted = now;
    }

    if (state.irrigationCycle && heavyRain && !survival) {
      state.irrigationCycle = false;
      state.pulseIsOn = false;
    }

    if (state.irrigationCycle) {
      const unsigned long phaseDuration = state.pulseIsOn
          ? (survival ? SURVIVAL_PULSE_MS : NORMAL_PULSE_MS)
          : PULSE_REST_MS;
      if (now - state.pulsePhaseStarted >= phaseDuration) {
        state.pulseIsOn = !state.pulseIsOn;
        state.pulsePhaseStarted = now;
      }
      wantsPumpOn = state.pulseIsOn;
      reason = survival ? "SINH TON" : (state.pulseIsOn ? "TUOI THEO XUNG" : "NGHI THAM");
    } else if (heavyRain) {
      reason = state.rainingNow ? "TROI MUA" : "CHO MUA";
    }
  }

  if (wantsPumpOn != state.pumpActive &&
      now - state.lastPumpChange >= PUMP_MIN_SWITCH_MS) {
    state.pumpActive = wantsPumpOn;
    state.lastPumpChange = now;
    Serial.printf("[Bom] -> %s (%s), am=%d mua=%d\n",
                  state.pumpActive ? "ON" : "OFF", reason.c_str(),
                  state.currentMoisture, state.rainEff);
  }

  if (wantsPumpOn == state.pumpActive) state.statusText = reason;
  return state.pumpActive;
}

