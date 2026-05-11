#include "apply.h"

#include "fs_utils.h"

#include <mutex>
#include <stdio.h>
#include <utility>

ApplyResult apply_preset(const Preset& preset) {
    if (preset.boot_mode == BootMode::Hekate) {
        if (!path_exists(preset.logo_path)) {
            return {false, "Selected Hekate preset has no bootlogo.bmp."};
        }

        ensure_app_dirs();
        if (!mkdir_recursive(BOOTLOADER_DIR)) {
            return {false, "Could not create /bootloader directory."};
        }

        if (path_exists(HEKATE_BOOTLOGO_FILE)) {
            copy_file(HEKATE_BOOTLOGO_FILE, HEKATE_BACKUP_FILE);
        }

        if (!copy_file(preset.logo_path, HEKATE_BOOTLOGO_FILE)) {
            return {false, "Could not copy bootlogo.bmp into /bootloader."};
        }

        write_active_name(BootMode::Hekate, preset.name);
        return {true, "Installed Hekate bootlogo. Reboot is required to see it."};
    }

    if (preset.patch_files <= 0) {
        return {false, "Selected folder has no .ips/.isp patch files."};
    }

    ensure_app_dirs();
    if (!mkdir_recursive(ATMOSPHERE_PATCHES_DIR)) {
        return {false, "Could not create required SD card directories."};
    }

    remove_recursive(SWITCH_STAGING_DIR);
    int copied_files = 0;
    if (!copy_directory(preset.logo_path, SWITCH_STAGING_DIR, true, &copied_files) || copied_files <= 0) {
        remove_recursive(SWITCH_STAGING_DIR);
        return {false, "Could not copy preset into staging directory."};
    }

    if (is_dir(ATMOSPHERE_LOGO_DIR)) {
        remove_recursive(SWITCH_BACKUP_DIR);
        int backup_files = 0;
        if (!copy_directory(ATMOSPHERE_LOGO_DIR, SWITCH_BACKUP_DIR, false, &backup_files)) {
            remove_recursive(SWITCH_STAGING_DIR);
            return {false, "Could not back up current Atmosphere logo directory."};
        }
    }

    if (!remove_recursive(ATMOSPHERE_LOGO_DIR)) {
        remove_recursive(SWITCH_STAGING_DIR);
        return {false, "Could not remove current Atmosphere logo directory."};
    }

    if (rename(SWITCH_STAGING_DIR, ATMOSPHERE_LOGO_DIR) != 0) {
        if (is_dir(SWITCH_BACKUP_DIR)) {
            int restored_files = 0;
            copy_directory(SWITCH_BACKUP_DIR, ATMOSPHERE_LOGO_DIR, false, &restored_files);
        }
        remove_recursive(SWITCH_STAGING_DIR);
        return {false, "Could not install staged logo directory."};
    }

    write_active_name(BootMode::SwitchCfw, preset.name);
    return {true, "Installed Switch CFW bootlogo. Reboot is required to see it."};
}

Result request_reboot() {
    Result rc = appletRequestToReboot();
    if (R_SUCCEEDED(rc)) {
        return rc;
    }

    Result init = spsmInitialize();
    if (R_SUCCEEDED(init)) {
        rc = spsmShutdown(true);
        spsmExit();
        return rc;
    }
    return rc;
}

void start_apply_job(ApplyJob* job, const Preset& preset) {
    if (job->thread.joinable()) {
        job->thread.join();
    }

    job->preset_name = preset.name;
    job->running.store(true);
    job->done.store(false);
    {
        std::lock_guard<std::mutex> lock(job->mutex);
        job->result = {false, ""};
    }

    Preset preset_copy = preset;
    job->thread = std::thread([job, preset_copy]() {
        ApplyResult result = apply_preset(preset_copy);
        {
            std::lock_guard<std::mutex> lock(job->mutex);
            job->result = result;
        }
        job->running.store(false);
        job->done.store(true);
    });
}

ApplyResult finish_apply_job(ApplyJob* job) {
    if (job->thread.joinable()) {
        job->thread.join();
    }

    std::lock_guard<std::mutex> lock(job->mutex);
    return job->result;
}

void start_preset_scan_job(PresetScanJob* job, BootMode boot_mode) {
    if (job->thread.joinable()) {
        job->thread.join();
    }

    job->boot_mode = boot_mode;
    job->presets.clear();
    job->running.store(true);
    job->done.store(false);
    job->thread = std::thread([job, boot_mode]() {
        std::vector<Preset> scanned = scan_presets(boot_mode);
        {
            std::lock_guard<std::mutex> lock(job->mutex);
            job->presets = std::move(scanned);
        }
        job->running.store(false);
        job->done.store(true);
    });
}

std::vector<Preset> finish_preset_scan_job(PresetScanJob* job) {
    if (job->thread.joinable()) {
        job->thread.join();
    }

    std::lock_guard<std::mutex> lock(job->mutex);
    std::vector<Preset> presets = std::move(job->presets);
    job->presets.clear();
    job->done.store(false);
    return presets;
}
