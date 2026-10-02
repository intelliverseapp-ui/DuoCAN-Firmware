#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// High-level vehicle command API (Siri / HTTP / BNA)

void duocan_cmd_lock_doors(void);
void duocan_cmd_unlock_doors(void);

void duocan_cmd_windows_down(void);
void duocan_cmd_windows_up(void);

void duocan_cmd_sunroof_open(void);
void duocan_cmd_sunroof_close(void);
void duocan_cmd_sunroof_vent(void);

void duocan_cmd_headlights_on(void);
void duocan_cmd_headlights_off(void);

void duocan_cmd_fog_lights_on(void);
void duocan_cmd_fog_lights_off(void);

void duocan_cmd_interior_lights_on(void);
void duocan_cmd_interior_lights_off(void);

void duocan_cmd_ac_on(void);
void duocan_cmd_ac_off(void);

void duocan_cmd_fan_up(void);
void duocan_cmd_fan_down(void);

void duocan_cmd_audio_mute(void);
void duocan_cmd_audio_unmute(void);

void duocan_cmd_trunk_open(void);

void duocan_cmd_horn(void);

void duocan_cmd_hazards_on(void);
void duocan_cmd_hazards_off(void);

void duocan_cmd_defrost_on(void);
void duocan_cmd_defrost_off(void);

#ifdef __cplusplus
}
#endif
