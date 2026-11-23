#include "IrrigationController.h"

#include "Config.h"

bool updateIrrigationDecision(AppState& state, unsigned long now) {
  bool wantsPumpOn = false;
  String reason = "STANDBY";
  const bool heavyRain = state.rainingNow || state.rainEff >= RAIN_THRESHOLD;

  if (state.pumpMode == "MANUAL_ON") {
    state.irrigationCycle = false;
    state.survivalCycle = false;
    state.pulseIsOn = false;
    state.pulsePhaseStarted = 0;
    wantsPumpOn = true;
    reason = "APP BAT";
  } else if (state.pumpMode == "MANUAL_OFF") {
    state.irrigationCycle = false;
    state.survivalCycle = false;
    state.pulseIsOn = false;
    state.pulsePhaseStarted = 0;
    wantsPumpOn = false;
    reason = "APP TAT";
  } else if (state.currentMoisture >= TARGET_MOISTURE) {
    state.irrigationCycle = false;
    state.survivalCycle = false;
    state.pulseIsOn = false;
    state.pulsePhaseStarted = 0;
    wantsPumpOn = false;
    reason = "DU AM";
  } else {
    if (state.currentMoisture < CRITICAL_LOW) state.survivalCycle = true;
    const bool soilIsDry = state.currentMoisture < DRY_THRESHOLD;

    if (state.survivalCycle) {
      // Sinh tồn: tưới liên tục đến mức ẩm mục tiêu, không hoãn vì mưa.
      state.irrigationCycle = true;
      state.pulseIsOn = true;
      state.pulsePhaseStarted = 0;
      wantsPumpOn = true;
      reason = "SINH TON";
    } else {
      if (!state.irrigationCycle && soilIsDry && !heavyRain) {
        state.irrigationCycle = true;
        state.pulseIsOn = true;
        // Start the phase timer only after the relay actually turns on.
        // Otherwise the 10 s minimum-switch guard can consume part of the pulse.
        state.pulsePhaseStarted = 0;
      }

      if (state.irrigationCycle && heavyRain) {
        state.irrigationCycle = false;
        state.pulseIsOn = false;
        state.pulsePhaseStarted = 0;
      }

      if (state.irrigationCycle) {
        // Count ON time only while the pump is physically commanded ON, and
        // count rest time only after it has actually switched OFF.
        if (state.pumpActive == state.pulseIsOn) {
          if (state.pulsePhaseStarted == 0) {
            state.pulsePhaseStarted = now;
          } else if (now - state.pulsePhaseStarted >=
                     (state.pulseIsOn ? NORMAL_PULSE_MS : PULSE_REST_MS)) {
            state.pulseIsOn = !state.pulseIsOn;
            state.pulsePhaseStarted = 0;
          }
        }
        wantsPumpOn = state.pulseIsOn;
        reason = state.pulseIsOn ? "TUOI THEO XUNG" : "NGHI THAM";
      } else if (heavyRain) {
        reason = state.rainingNow ? "TROI MUA" : "CHO MUA";
      }
    }
  }

  if (wantsPumpOn != state.pumpActive &&
      now - state.lastPumpChange >= PUMP_MIN_SWITCH_MS) {
    state.pumpActive = wantsPumpOn;
    state.lastPumpChange = now;
    if (state.irrigationCycle) state.pulsePhaseStarted = now;
    Serial.printf("[Bom] -> %s (%s), am=%d mua=%d\n",
                  state.pumpActive ? "ON" : "OFF", reason.c_str(),
                  state.currentMoisture, state.rainEff);
  }

  if (wantsPumpOn == state.pumpActive) state.statusText = reason;
  return state.pumpActive;
}

