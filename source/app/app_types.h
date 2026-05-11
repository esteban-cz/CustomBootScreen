#pragma once

#include <SDL_ttf.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef VERSION_MAJOR
#define VERSION_MAJOR 1
#endif

#ifndef VERSION_MINOR
#define VERSION_MINOR 3
#endif

#ifndef VERSION_MICRO
#define VERSION_MICRO 0
#endif

inline constexpr int SCREEN_W = 1280;
inline constexpr int SCREEN_H = 720;
inline constexpr int BOOTLOGO_W = 308;
inline constexpr int BOOTLOGO_H = 350;
inline constexpr int HEKATE_PREVIEW_W = 560;
inline constexpr int HEKATE_PREVIEW_H = 315;

inline constexpr const char* APP_DIR = "/switch/CustomBootScreen";
inline constexpr const char* SWITCH_PRESETS_DIR = "/switch/CustomBootScreen/switch-bootlogo";
inline constexpr const char* HEKATE_PRESETS_DIR = "/switch/CustomBootScreen/hekate-bootlogo";
inline constexpr const char* SWITCH_ACTIVE_FILE = "/switch/CustomBootScreen/switch-active.txt";
inline constexpr const char* HEKATE_ACTIVE_FILE = "/switch/CustomBootScreen/hekate-active.txt";
inline constexpr const char* SWITCH_BACKUP_DIR = "/switch/CustomBootScreen/_switch_current_backup";
inline constexpr const char* HEKATE_BACKUP_FILE = "/switch/CustomBootScreen/_hekate_bootlogo_backup.bmp";
inline constexpr const char* SWITCH_STAGING_DIR = "/switch/CustomBootScreen/_switch_staging_apply";
inline constexpr const char* ATMOSPHERE_PATCHES_DIR = "/atmosphere/exefs_patches";
inline constexpr const char* ATMOSPHERE_LOGO_DIR = "/atmosphere/exefs_patches/logo";
inline constexpr const char* BOOTLOADER_DIR = "/bootloader";
inline constexpr const char* HEKATE_BOOTLOGO_FILE = "/bootloader/bootlogo.bmp";

enum class BootMode {
    SwitchCfw,
    Hekate,
};

struct Fonts {
    TTF_Font* title;
    TTF_Font* section;
    TTF_Font* body;
    TTF_Font* small;
};

struct Preset {
    std::string name;
    std::string path;
    std::string logo_path;
    std::string preview_path;
    BootMode boot_mode;
    int patch_files;
    int total_files;
    bool active;
};

struct ApplyResult {
    bool ok;
    std::string message;
};

enum class Mode {
    Browse,
    ConfirmApply,
    Applying,
    RebootPrompt,
    SwitchingMode,
};

struct ApplyJob {
    std::atomic<bool> running;
    std::atomic<bool> done;
    std::thread thread;
    std::mutex mutex;
    ApplyResult result;
    std::string preset_name;

    ApplyJob() : running(false), done(false), result({false, ""}) {}
};

struct PresetScanJob {
    std::atomic<bool> running;
    std::atomic<bool> done;
    std::thread thread;
    std::mutex mutex;
    BootMode boot_mode;
    std::vector<Preset> presets;

    PresetScanJob() : running(false), done(false), boot_mode(BootMode::SwitchCfw) {}
};

const char* mode_title(BootMode boot_mode);
const char* mode_short_label(BootMode boot_mode);
BootMode other_mode(BootMode boot_mode);
