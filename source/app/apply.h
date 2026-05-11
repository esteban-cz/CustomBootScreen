#pragma once

#include "app_types.h"

#include <switch.h>

#include <vector>

ApplyResult apply_preset(const Preset& preset);
Result request_reboot();

void start_apply_job(ApplyJob* job, const Preset& preset);
ApplyResult finish_apply_job(ApplyJob* job);

void start_preset_scan_job(PresetScanJob* job, BootMode boot_mode);
std::vector<Preset> finish_preset_scan_job(PresetScanJob* job);
