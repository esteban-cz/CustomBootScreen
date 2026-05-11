#include <switch.h>
#include <SDL.h>
#include <SDL_ttf.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>
#include <atomic>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <mutex>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

#ifndef VERSION_MAJOR
#define VERSION_MAJOR 1
#endif

#ifndef VERSION_MINOR
#define VERSION_MINOR 2
#endif

#ifndef VERSION_MICRO
#define VERSION_MICRO 1
#endif

static constexpr int SCREEN_W = 1280;
static constexpr int SCREEN_H = 720;
static constexpr int BOOTLOGO_W = 308;
static constexpr int BOOTLOGO_H = 350;
static constexpr int HEKATE_PREVIEW_W = 560;
static constexpr int HEKATE_PREVIEW_H = 315;
static constexpr const char* APP_DIR = "/switch/CustomBootScreen";
static constexpr const char* SWITCH_PRESETS_DIR = "/switch/CustomBootScreen/switch-bootlogo";
static constexpr const char* HEKATE_PRESETS_DIR = "/switch/CustomBootScreen/hekate-bootlogo";
static constexpr const char* SWITCH_ACTIVE_FILE = "/switch/CustomBootScreen/switch-active.txt";
static constexpr const char* HEKATE_ACTIVE_FILE = "/switch/CustomBootScreen/hekate-active.txt";
static constexpr const char* SWITCH_BACKUP_DIR = "/switch/CustomBootScreen/_switch_current_backup";
static constexpr const char* HEKATE_BACKUP_FILE = "/switch/CustomBootScreen/_hekate_bootlogo_backup.bmp";
static constexpr const char* SWITCH_STAGING_DIR = "/switch/CustomBootScreen/_switch_staging_apply";
static constexpr const char* ATMOSPHERE_PATCHES_DIR = "/atmosphere/exefs_patches";
static constexpr const char* ATMOSPHERE_LOGO_DIR = "/atmosphere/exefs_patches/logo";
static constexpr const char* BOOTLOADER_DIR = "/bootloader";
static constexpr const char* HEKATE_BOOTLOGO_FILE = "/bootloader/bootlogo.bmp";

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

static constexpr SDL_Color COLOR_BG = {5, 8, 14, 255};
static constexpr SDL_Color COLOR_TOP = {9, 12, 20, 255};
static constexpr SDL_Color COLOR_PANEL = {12, 16, 25, 246};
static constexpr SDL_Color COLOR_PANEL_2 = {20, 26, 38, 246};
static constexpr SDL_Color COLOR_TEXT = {248, 250, 252, 255};
static constexpr SDL_Color COLOR_MUTED = {143, 157, 176, 255};
static constexpr SDL_Color COLOR_DIM = {70, 82, 101, 255};
static constexpr SDL_Color COLOR_ACCENT = {24, 205, 255, 255};
static constexpr SDL_Color COLOR_ACCENT_SOFT = {15, 145, 190, 150};
static constexpr SDL_Color COLOR_ACCENT_RED = {255, 76, 88, 255};
static constexpr SDL_Color COLOR_ACCENT_RED_SOFT = {190, 35, 48, 145};
static constexpr SDL_Color COLOR_WARN = {255, 169, 61, 255};
static constexpr SDL_Color COLOR_DANGER = {255, 76, 88, 255};

static std::string join_path(const std::string& base, const std::string& name) {
    if (base.empty() || base == "/") {
        return "/" + name;
    }
    return base + "/" + name;
}

static std::string lower_copy(std::string value) {
    for (char& c : value) {
        c = (char)tolower((unsigned char)c);
    }
    return value;
}

static bool path_exists(const std::string& path) {
    struct stat st = {};
    return stat(path.c_str(), &st) == 0;
}

static bool is_dir(const std::string& path) {
    struct stat st = {};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static bool mkdir_one(const std::string& path) {
    if (path.empty() || path == "/" || is_dir(path)) {
        return true;
    }
    return mkdir(path.c_str(), 0777) == 0 || errno == EEXIST;
}

static bool mkdir_recursive(const std::string& path) {
    std::string current;
    for (size_t i = 0; i < path.size(); i++) {
        current.push_back(path[i]);
        if (path[i] == '/' && current.size() > 1) {
            if (!mkdir_one(current.substr(0, current.size() - 1))) {
                return false;
            }
        }
    }
    return mkdir_one(path);
}

static bool is_dot_entry(const char* name) {
    return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

static bool is_preview_file(const std::string& name) {
    const std::string lower = lower_copy(name);
    return lower == "preview.png" || lower == "preview.jpg" || lower == "preview.jpeg" || lower == "preview.bmp" ||
        lower == "logo.png" || lower == "logo.pnh" || lower == "logo.jpg" || lower == "logo.jpeg" || lower == "logo.bmp";
}

static bool is_patch_file(const std::string& name) {
    const std::string lower = lower_copy(name);
    return lower.size() >= 4 &&
        (lower.substr(lower.size() - 4) == ".ips" || lower.substr(lower.size() - 4) == ".isp");
}

static const char* mode_title(BootMode boot_mode) {
    return boot_mode == BootMode::SwitchCfw ? "Switch CFW bootlogo" : "Hekate bootlogo";
}

static const char* mode_short_label(BootMode boot_mode) {
    return boot_mode == BootMode::SwitchCfw ? "Switch CFW" : "Hekate";
}

static const char* mode_presets_dir(BootMode boot_mode) {
    return boot_mode == BootMode::SwitchCfw ? SWITCH_PRESETS_DIR : HEKATE_PRESETS_DIR;
}

static const char* mode_active_file(BootMode boot_mode) {
    return boot_mode == BootMode::SwitchCfw ? SWITCH_ACTIVE_FILE : HEKATE_ACTIVE_FILE;
}

static BootMode other_mode(BootMode boot_mode) {
    return boot_mode == BootMode::SwitchCfw ? BootMode::Hekate : BootMode::SwitchCfw;
}

static std::string parent_dir(const std::string& path) {
    const size_t pos = path.find_last_of('/');
    if (pos == std::string::npos || pos == 0) {
        return "/";
    }
    return path.substr(0, pos);
}

static bool remove_recursive(const std::string& path) {
    if (!path_exists(path)) {
        return true;
    }
    if (!is_dir(path)) {
        return remove(path.c_str()) == 0;
    }

    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return false;
    }

    bool ok = true;
    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (is_dot_entry(entry->d_name)) {
            continue;
        }
        if (!remove_recursive(join_path(path, entry->d_name))) {
            ok = false;
        }
    }
    closedir(dir);

    return rmdir(path.c_str()) == 0 && ok;
}

static bool copy_file(const std::string& src, const std::string& dst) {
    if (!mkdir_recursive(parent_dir(dst))) {
        return false;
    }

    FILE* in = fopen(src.c_str(), "rb");
    if (!in) {
        return false;
    }

    FILE* out = fopen(dst.c_str(), "wb");
    if (!out) {
        fclose(in);
        return false;
    }

    unsigned char buffer[64 * 1024];
    bool ok = true;
    while (true) {
        const size_t read = fread(buffer, 1, sizeof(buffer), in);
        if (read > 0 && fwrite(buffer, 1, read, out) != read) {
            ok = false;
            break;
        }
        if (read < sizeof(buffer)) {
            if (ferror(in)) {
                ok = false;
            }
            break;
        }
    }

    if (fclose(out) != 0) {
        ok = false;
    }
    fclose(in);
    return ok;
}

static bool copy_directory(const std::string& src, const std::string& dst, bool skip_previews, int* copied_files) {
    DIR* dir = opendir(src.c_str());
    if (!dir) {
        return false;
    }
    if (!mkdir_recursive(dst)) {
        closedir(dir);
        return false;
    }

    bool ok = true;
    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (is_dot_entry(entry->d_name)) {
            continue;
        }

        const std::string name = entry->d_name;
        if (skip_previews && is_preview_file(name)) {
            continue;
        }

        const std::string child_src = join_path(src, name);
        const std::string child_dst = join_path(dst, name);
        if (is_dir(child_src)) {
            if (!copy_directory(child_src, child_dst, skip_previews, copied_files)) {
                ok = false;
            }
        } else {
            if (copy_file(child_src, child_dst)) {
                if (copied_files) {
                    (*copied_files)++;
                }
            } else {
                ok = false;
            }
        }
    }

    closedir(dir);
    return ok;
}

static void count_files_recursive(const std::string& path, int* patch_files, int* total_files) {
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return;
    }

    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (is_dot_entry(entry->d_name)) {
            continue;
        }
        const std::string child = join_path(path, entry->d_name);
        if (is_dir(child)) {
            count_files_recursive(child, patch_files, total_files);
        } else {
            if (is_patch_file(entry->d_name)) {
                (*patch_files)++;
            }
            if (!is_preview_file(entry->d_name)) {
                (*total_files)++;
            }
        }
    }
    closedir(dir);
}

static void ensure_app_dirs() {
    mkdir_recursive(APP_DIR);
    mkdir_recursive(SWITCH_PRESETS_DIR);
    mkdir_recursive(HEKATE_PRESETS_DIR);
}

static std::string read_active_name(BootMode boot_mode) {
    FILE* file = fopen(mode_active_file(boot_mode), "rb");
    if (!file) {
        return "";
    }

    char buffer[256] = {};
    const size_t read = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    std::string value(buffer, read);
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == ' ' || value.back() == '\t')) {
        value.pop_back();
    }
    return value;
}

static void write_active_name(BootMode boot_mode, const std::string& name) {
    FILE* file = fopen(mode_active_file(boot_mode), "wb");
    if (!file) {
        return;
    }
    fwrite(name.c_str(), 1, name.size(), file);
    fwrite("\n", 1, 1, file);
    fclose(file);
}

static std::string find_preview(const std::string& preset_path, BootMode boot_mode) {
    if (boot_mode == BootMode::Hekate) {
        const std::string path = join_path(preset_path, "bootlogo.bmp");
        return path_exists(path) ? path : "";
    }

    static constexpr const char* names[] = {
        "preview.png",
        "preview.jpg",
        "preview.jpeg",
        "preview.bmp",
        "logo.png",
        "logo.pnh",
        "logo.jpg",
        "logo.jpeg",
        "logo.bmp",
    };

    for (const char* name : names) {
        const std::string path = join_path(preset_path, name);
        if (path_exists(path)) {
            return path;
        }
    }
    return "";
}

static std::vector<Preset> scan_presets(BootMode boot_mode) {
    ensure_app_dirs();

    const std::string active = read_active_name(boot_mode);
    std::vector<Preset> presets;
    const std::string root = mode_presets_dir(boot_mode);
    DIR* dir = opendir(root.c_str());
    if (!dir) {
        return presets;
    }

    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (is_dot_entry(entry->d_name)) {
            continue;
        }

        const std::string name = entry->d_name;
        if (!name.empty() && name[0] == '_') {
            continue;
        }

        const std::string path = join_path(root, name);
        if (!is_dir(path)) {
            continue;
        }

        Preset preset = {};
        preset.name = name;
        preset.path = path;
        preset.boot_mode = boot_mode;
        preset.logo_path = boot_mode == BootMode::SwitchCfw ? (is_dir(join_path(path, "logo")) ? join_path(path, "logo") : path) : join_path(path, "bootlogo.bmp");
        preset.preview_path = find_preview(path, boot_mode);
        preset.active = name == active;
        if (boot_mode == BootMode::SwitchCfw) {
            count_files_recursive(preset.logo_path, &preset.patch_files, &preset.total_files);
        } else if (path_exists(preset.logo_path)) {
            preset.patch_files = 1;
            preset.total_files = 1;
        }
        presets.push_back(preset);
    }
    closedir(dir);

    std::sort(presets.begin(), presets.end(), [](const Preset& a, const Preset& b) {
        return lower_copy(a.name) < lower_copy(b.name);
    });
    return presets;
}

static ApplyResult apply_preset(const Preset& preset) {
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

static Result request_reboot() {
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

static void start_apply_job(ApplyJob* job, const Preset& preset) {
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

static ApplyResult finish_apply_job(ApplyJob* job) {
    if (job->thread.joinable()) {
        job->thread.join();
    }

    std::lock_guard<std::mutex> lock(job->mutex);
    return job->result;
}

static void start_preset_scan_job(PresetScanJob* job, BootMode boot_mode) {
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

static std::vector<Preset> finish_preset_scan_job(PresetScanJob* job) {
    if (job->thread.joinable()) {
        job->thread.join();
    }

    std::lock_guard<std::mutex> lock(job->mutex);
    std::vector<Preset> presets = std::move(job->presets);
    job->presets.clear();
    job->done.store(false);
    return presets;
}

static void set_color(SDL_Renderer* renderer, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

static void fill_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
    set_color(renderer, color);
    SDL_RenderFillRect(renderer, &rect);
}

static void fill_rounded_rect(SDL_Renderer* renderer, SDL_Rect rect, int radius, SDL_Color color) {
    set_color(renderer, color);

    SDL_Rect center = {rect.x + radius, rect.y, rect.w - radius * 2, rect.h};
    SDL_Rect left = {rect.x, rect.y + radius, radius, rect.h - radius * 2};
    SDL_Rect right = {rect.x + rect.w - radius, rect.y + radius, radius, rect.h - radius * 2};

    SDL_RenderFillRect(renderer, &center);
    SDL_RenderFillRect(renderer, &left);
    SDL_RenderFillRect(renderer, &right);

    for (int dy = 0; dy < radius; dy++) {
        for (int dx = 0; dx < radius; dx++) {
            if (dx * dx + dy * dy <= radius * radius) {
                SDL_RenderDrawPoint(renderer, rect.x + radius - dx, rect.y + radius - dy);
                SDL_RenderDrawPoint(renderer, rect.x + rect.w - radius + dx - 1, rect.y + radius - dy);
                SDL_RenderDrawPoint(renderer, rect.x + radius - dx, rect.y + rect.h - radius + dy - 1);
                SDL_RenderDrawPoint(renderer, rect.x + rect.w - radius + dx - 1, rect.y + rect.h - radius + dy - 1);
            }
        }
    }
}

static SDL_Rect inset_rect(SDL_Rect rect, int amount) {
    rect.x += amount;
    rect.y += amount;
    rect.w -= amount * 2;
    rect.h -= amount * 2;
    return rect;
}

static void draw_rect_outline(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
    set_color(renderer, color);
    SDL_RenderDrawRect(renderer, &rect);
}

static void draw_neon_panel(SDL_Renderer* renderer, SDL_Rect rect, int radius, SDL_Color edge) {
    SDL_Color glow = edge;
    glow.a = 56;
    fill_rounded_rect(renderer, {rect.x - 5, rect.y - 5, rect.w + 10, rect.h + 10}, radius + 5, glow);
    fill_rounded_rect(renderer, rect, radius, edge);
    fill_rounded_rect(renderer, inset_rect(rect, 2), radius, COLOR_PANEL);
}

static void draw_background(SDL_Renderer* renderer) {
    fill_rect(renderer, {0, 0, SCREEN_W, SCREEN_H}, COLOR_BG);
    fill_rect(renderer, {0, 0, SCREEN_W, 104}, COLOR_TOP);
    fill_rect(renderer, {0, 646, SCREEN_W, 74}, COLOR_TOP);

    fill_rect(renderer, {0, 100, SCREEN_W / 2, 4}, COLOR_ACCENT);
    fill_rect(renderer, {SCREEN_W / 2, 100, SCREEN_W / 2, 4}, COLOR_ACCENT_RED);
    fill_rect(renderer, {0, 646, SCREEN_W / 2, 3}, COLOR_ACCENT_SOFT);
    fill_rect(renderer, {SCREEN_W / 2, 646, SCREEN_W / 2, 3}, COLOR_ACCENT_RED_SOFT);

    fill_rounded_rect(renderer, {-52, 22, 190, 38}, 19, COLOR_ACCENT_SOFT);
    fill_rect(renderer, {78, 62, 4, 20}, COLOR_TEXT);
    fill_rect(renderer, {70, 70, 20, 4}, COLOR_TEXT);
}

static void draw_text(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, SDL_Color color);

static void draw_brand_mark(SDL_Renderer* renderer, Fonts* fonts) {
    SDL_Rect badge = {60, 24, 98, 58};
    fill_rounded_rect(renderer, {badge.x - 3, badge.y - 3, badge.w + 6, badge.h + 6}, 10, COLOR_ACCENT_SOFT);
    fill_rounded_rect(renderer, badge, 8, COLOR_PANEL_2);
    fill_rect(renderer, {badge.x + 8, badge.y + 8, 5, badge.h - 16}, COLOR_ACCENT);
    fill_rect(renderer, {badge.x + badge.w - 13, badge.y + 8, 5, badge.h - 16}, COLOR_ACCENT_RED);
    draw_text(renderer, fonts->section, "CBS", badge.x + 24, badge.y + 17, COLOR_TEXT);
}

static void draw_exit_badge(SDL_Renderer* renderer, Fonts* fonts) {
    SDL_Rect badge = {1128, 32, 112, 44};
    fill_rounded_rect(renderer, {badge.x - 3, badge.y - 3, badge.w + 6, badge.h + 6}, 10, COLOR_ACCENT_RED_SOFT);
    fill_rounded_rect(renderer, badge, 8, COLOR_PANEL_2);
    fill_rect(renderer, {badge.x + 18, badge.y + 12, 4, 20}, COLOR_TEXT);
    fill_rect(renderer, {badge.x + 10, badge.y + 20, 20, 4}, COLOR_TEXT);
    draw_text(renderer, fonts->small, "EXIT", badge.x + 43, badge.y + 13, COLOR_TEXT);
}

static std::string ellipsize(TTF_Font* font, const std::string& text, int max_width) {
    int w = 0;
    int h = 0;
    if (TTF_SizeUTF8(font, text.c_str(), &w, &h) == 0 && w <= max_width) {
        return text;
    }

    std::string out = text;
    while (!out.empty()) {
        out.pop_back();
        const std::string candidate = out + "...";
        if (TTF_SizeUTF8(font, candidate.c_str(), &w, &h) == 0 && w <= max_width) {
            return candidate;
        }
    }
    return "...";
}

static void draw_text(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) {
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dst = {x, y, surface->w, surface->h};
    SDL_FreeSurface(surface);

    if (!texture) {
        return;
    }

    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
}

static void draw_text_right(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int right, int y, SDL_Color color) {
    int w = 0;
    int h = 0;
    TTF_SizeUTF8(font, text.c_str(), &w, &h);
    draw_text(renderer, font, text, right - w, y, color);
}

static void draw_text_center(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int center_x, int y, SDL_Color color) {
    int w = 0;
    int h = 0;
    TTF_SizeUTF8(font, text.c_str(), &w, &h);
    draw_text(renderer, font, text, center_x - w / 2, y, color);
}

static void draw_button_hint(SDL_Renderer* renderer, Fonts* fonts, int x, const std::string& key, const std::string& label) {
    SDL_Rect key_rect = {x, 662, 42, 34};
    fill_rounded_rect(renderer, {key_rect.x - 1, key_rect.y - 1, key_rect.w + 2, key_rect.h + 2}, 8, COLOR_ACCENT_SOFT);
    fill_rounded_rect(renderer, key_rect, 8, COLOR_PANEL_2);
    draw_text(renderer, fonts->small, key, x + 14, 669, COLOR_TEXT);
    draw_text(renderer, fonts->small, label, x + 52, 669, COLOR_MUTED);
}

static void draw_spinner(SDL_Renderer* renderer, int cx, int cy, int frame) {
    static constexpr SDL_Point points[] = {
        {0, -24},
        {12, -21},
        {21, -12},
        {24, 0},
        {21, 12},
        {12, 21},
        {0, 24},
        {-12, 21},
        {-21, 12},
        {-24, 0},
        {-21, -12},
        {-12, -21},
    };

    for (int i = 0; i < 12; i++) {
        const int age = (i - frame + 12) % 12;
        SDL_Color color = age < 3 ? COLOR_ACCENT : (age < 7 ? COLOR_MUTED : COLOR_DIM);
        SDL_Rect dot = {cx + points[i].x - 4, cy + points[i].y - 4, 8, 8};
        fill_rounded_rect(renderer, dot, 4, color);
    }
}

static SDL_Texture* load_preview_texture(SDL_Renderer* renderer, const std::string& path, int* out_w, int* out_h) {
    if (path.empty()) {
        return nullptr;
    }

    int w = 0;
    int h = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
    if (!pixels) {
        return nullptr;
    }

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(pixels, w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        stbi_image_free(pixels);
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    stbi_image_free(pixels);

    if (texture) {
        *out_w = w;
        *out_h = h;
    }
    return texture;
}

static void draw_preview(SDL_Renderer* renderer, Fonts* fonts, BootMode boot_mode, const Preset* preset, SDL_Texture* preview, int preview_w, int preview_h) {
    SDL_Rect area = {520, 132, 700, 492};
    const bool hekate = boot_mode == BootMode::Hekate;
    const SDL_Color mode_color = hekate ? COLOR_ACCENT_RED : COLOR_ACCENT;
    draw_neon_panel(renderer, area, 8, mode_color);

    draw_text(renderer, fonts->section, "Bootlogo preview", 558, 160, COLOR_TEXT);

    SDL_Rect frame_outer = hekate
        ? SDL_Rect{558, 224, HEKATE_PREVIEW_W + 12, HEKATE_PREVIEW_H + 12}
        : SDL_Rect{558, 206, BOOTLOGO_W + 12, BOOTLOGO_H + 12};
    fill_rounded_rect(renderer, {frame_outer.x - 2, frame_outer.y - 2, frame_outer.w + 4, frame_outer.h + 4}, 8, mode_color);
    fill_rounded_rect(renderer, frame_outer, 8, COLOR_PANEL_2);
    SDL_Rect frame = hekate
        ? SDL_Rect{frame_outer.x + 6, frame_outer.y + 6, HEKATE_PREVIEW_W, HEKATE_PREVIEW_H}
        : SDL_Rect{frame_outer.x + 6, frame_outer.y + 6, BOOTLOGO_W, BOOTLOGO_H};
    fill_rect(renderer, frame, {8, 10, 14, 255});
    draw_rect_outline(renderer, inset_rect(frame, 1), {37, 48, 67, 255});

    if (preview && preview_w > 0 && preview_h > 0) {
        if (hekate) {
            const float rotated_w = (float)preview_h;
            const float rotated_h = (float)preview_w;
            const float scale = std::min(frame.w / rotated_w, frame.h / rotated_h);
            const int final_w = (int)(rotated_w * scale);
            const int final_h = (int)(rotated_h * scale);
            SDL_Rect dst = {
                frame.x + (frame.w - final_h) / 2,
                frame.y + (frame.h - final_w) / 2,
                final_h,
                final_w
            };
            SDL_RenderCopyEx(renderer, preview, NULL, &dst, 90.0, NULL, SDL_FLIP_NONE);
        } else {
            const float scale = std::min(frame.w / (float)preview_w, frame.h / (float)preview_h);
            const int draw_w = (int)(preview_w * scale);
            const int draw_h = (int)(preview_h * scale);
            SDL_Rect dst = {frame.x + (frame.w - draw_w) / 2, frame.y + (frame.h - draw_h) / 2, draw_w, draw_h};
            SDL_RenderCopy(renderer, preview, NULL, &dst);
        }
    } else {
        draw_text_center(renderer, fonts->section, "No preview", frame.x + frame.w / 2, frame.y + 128, COLOR_DIM);
        draw_text_center(renderer, fonts->small, hekate ? "Add bootlogo.bmp" : "Add preview.* or logo.*", frame.x + frame.w / 2, frame.y + 174, COLOR_MUTED);
    }

    if (preset) {
        SDL_Rect meta = hekate ? SDL_Rect{558, 568, 560, 40} : SDL_Rect{916, 206, 250, 350};
        if (hekate) {
            draw_text(renderer, fonts->section, ellipsize(fonts->section, preset->name, 370), meta.x, meta.y + 4, COLOR_TEXT);
            draw_text_right(renderer, fonts->body, preset->patch_files > 0 ? "bootlogo.bmp" : "missing bootlogo.bmp", 1118, meta.y + 8, preset->patch_files > 0 ? COLOR_ACCENT : COLOR_WARN);
        } else {
            draw_text(renderer, fonts->small, "SELECTED", meta.x, meta.y, COLOR_MUTED);
            draw_text(renderer, fonts->section, ellipsize(fonts->section, preset->name, 240), meta.x, meta.y + 34, COLOR_TEXT);
            char info[128] = {};
            snprintf(info, sizeof(info), "%d patch file%s", preset->patch_files, preset->patch_files == 1 ? "" : "s");
            draw_text(renderer, fonts->body, info, meta.x, meta.y + 96, preset->patch_files > 0 ? COLOR_ACCENT : COLOR_WARN);
            char copied[128] = {};
            snprintf(copied, sizeof(copied), "%d copied file%s", preset->total_files, preset->total_files == 1 ? "" : "s");
            draw_text(renderer, fonts->small, copied, meta.x, meta.y + 132, COLOR_MUTED);
            draw_text(renderer, fonts->small, "Source folder", meta.x, meta.y + 190, COLOR_MUTED);
            draw_text(renderer, fonts->body, "logo/", meta.x, meta.y + 222, COLOR_TEXT);
        }
        if (preset->active) {
            SDL_Rect badge = hekate ? SDL_Rect{1014, 160, 104, 36} : SDL_Rect{meta.x, meta.y + 286, 104, 36};
            fill_rounded_rect(renderer, badge, 8, mode_color);
            draw_text(renderer, fonts->small, "ACTIVE", badge.x + 22, badge.y + 9, COLOR_BG);
        }
    }
}

static void draw_presets(SDL_Renderer* renderer, Fonts* fonts, BootMode boot_mode, const std::vector<Preset>& presets, int selected) {
    SDL_Rect panel = {60, 142, 420, 478};
    const SDL_Color mode_color = boot_mode == BootMode::SwitchCfw ? COLOR_ACCENT : COLOR_ACCENT_RED;
    draw_neon_panel(renderer, panel, 8, mode_color);
    draw_text(renderer, fonts->section, "Presets", 88, 166, COLOR_TEXT);
    draw_text_right(renderer, fonts->small, mode_short_label(boot_mode), 452, 173, mode_color);

    if (presets.empty()) {
        draw_text(renderer, fonts->body, "No boot screens found.", 88, 236, COLOR_TEXT);
        if (boot_mode == BootMode::SwitchCfw) {
            draw_text(renderer, fonts->small, "Add folders in /switch/CustomBootScreen/switch-bootlogo/", 88, 276, COLOR_MUTED);
            draw_text(renderer, fonts->small, "Each preset should contain a logo/ patch folder.", 88, 306, COLOR_MUTED);
        } else {
            draw_text(renderer, fonts->small, "Add folders in /switch/CustomBootScreen/hekate-bootlogo/", 88, 276, COLOR_MUTED);
            draw_text(renderer, fonts->small, "Each preset should contain bootlogo.bmp.", 88, 306, COLOR_MUTED);
        }
        return;
    }

    const int visible = 8;
    int start = selected - visible / 2;
    if (start < 0) {
        start = 0;
    }
    if (start > (int)presets.size() - visible) {
        start = std::max(0, (int)presets.size() - visible);
    }

    for (int i = 0; i < visible && start + i < (int)presets.size(); i++) {
        const int index = start + i;
        const Preset& preset = presets[index];
        SDL_Rect row = {84, 216 + i * 46, 368, 38};
        if (index == selected) {
            SDL_Color row_glow = mode_color;
            row_glow.a = 42;
            fill_rounded_rect(renderer, {row.x - 3, row.y - 3, row.w + 6, row.h + 6}, 8, row_glow);
            fill_rounded_rect(renderer, row, 8, COLOR_PANEL_2);
            SDL_Rect accent = {row.x, row.y, 5, row.h};
            fill_rect(renderer, accent, mode_color);
        }

        SDL_Color name_color = preset.patch_files > 0 ? COLOR_TEXT : COLOR_DIM;
        draw_text(renderer, fonts->body, ellipsize(fonts->body, preset.name, 255), row.x + 18, row.y + 8, name_color);
        if (preset.active) {
            draw_text_right(renderer, fonts->small, "ACTIVE", row.x + row.w - 16, row.y + 10, mode_color);
        } else {
            char count[32] = {};
            if (boot_mode == BootMode::SwitchCfw) {
                snprintf(count, sizeof(count), "%d ips", preset.patch_files);
            } else {
                snprintf(count, sizeof(count), "%s", preset.patch_files > 0 ? "bmp" : "missing");
            }
            draw_text_right(renderer, fonts->small, count, row.x + row.w - 16, row.y + 10, COLOR_MUTED);
        }
    }
}

static void draw_overlay(SDL_Renderer* renderer, Fonts* fonts, BootMode boot_mode, Mode mode, const Preset* preset, const std::string& message, const ApplyJob& apply_job) {
    if (mode == Mode::Browse) {
        return;
    }

    fill_rect(renderer, {0, 0, SCREEN_W, SCREEN_H}, {0, 0, 0, 140});
    SDL_Rect dialog = {310, 214, 660, 260};
    const SDL_Color mode_color = boot_mode == BootMode::SwitchCfw ? COLOR_ACCENT : COLOR_ACCENT_RED;
    draw_neon_panel(renderer, dialog, 8, mode_color);

    if (mode == Mode::ConfirmApply) {
        draw_text(renderer, fonts->section, "Apply boot screen?", 352, 254, COLOR_TEXT);
        if (preset) {
            draw_text(renderer, fonts->body, ellipsize(fonts->body, preset->name, 560), 352, 304, mode_color);
        }
        draw_text(renderer, fonts->small,
            boot_mode == BootMode::SwitchCfw ? "This replaces /atmosphere/exefs_patches/logo/ after making a backup." : "This replaces /bootloader/bootlogo.bmp after making a backup.",
            352, 348, COLOR_MUTED);
        draw_text(renderer, fonts->small, "A Apply", 352, 410, COLOR_TEXT);
        draw_text(renderer, fonts->small, "B Cancel", 482, 410, COLOR_MUTED);
    } else if (mode == Mode::Applying) {
        const int spinner_frame = (int)((armTicksToNs(armGetSystemTick()) / 90000000ULL) % 12);
        draw_spinner(renderer, 370, 344, spinner_frame);
        draw_text(renderer, fonts->section, "Applying boot screen", 430, 286, COLOR_TEXT);
        draw_text(renderer, fonts->body, ellipsize(fonts->body, apply_job.preset_name, 470), 430, 332, mode_color);
        draw_text(renderer, fonts->small,
            boot_mode == BootMode::SwitchCfw ? "Backing up current logo and copying patch files..." : "Backing up current Hekate bootlogo and copying BMP...",
            430, 382, COLOR_MUTED);
        draw_text(renderer, fonts->small, "Please wait. Do not close the app while this is running.", 430, 414, COLOR_WARN);
    } else if (mode == Mode::SwitchingMode) {
        const int spinner_frame = (int)((armTicksToNs(armGetSystemTick()) / 90000000ULL) % 12);
        draw_spinner(renderer, 370, 344, spinner_frame);
        draw_text(renderer, fonts->section, "Switching mode", 430, 286, COLOR_TEXT);
        draw_text(renderer, fonts->body, mode_title(boot_mode), 430, 332, mode_color);
        draw_text(renderer, fonts->small, "Loading boot screen presets...", 430, 382, COLOR_MUTED);
    } else {
        const bool ok = message.find("Installed") == 0;
        draw_text(renderer, fonts->section, ok ? "Preset installed" : "Could not apply preset", 352, 254, ok ? COLOR_TEXT : COLOR_DANGER);
        draw_text(renderer, fonts->body, ellipsize(fonts->body, message, 570), 352, 306, ok ? mode_color : COLOR_WARN);
        if (ok) {
            draw_text(renderer, fonts->small, "A Reboot now", 352, 390, COLOR_TEXT);
            draw_text(renderer, fonts->small, "B Stay in app", 512, 390, COLOR_MUTED);
        } else {
            draw_text(renderer, fonts->small, "B Back", 352, 390, COLOR_MUTED);
        }
    }
}

static void render_app(
    SDL_Renderer* renderer,
    Fonts* fonts,
    BootMode boot_mode,
    const std::vector<Preset>& presets,
    int selected,
    Mode mode,
    const std::string& message,
    SDL_Texture* preview,
    int preview_w,
    int preview_h,
    const ApplyJob& apply_job
) {
    draw_background(renderer);

    const SDL_Color mode_color = boot_mode == BootMode::SwitchCfw ? COLOR_ACCENT : COLOR_ACCENT_RED;
    draw_brand_mark(renderer, fonts);
    draw_exit_badge(renderer, fonts);
    draw_text(renderer, fonts->title, "CustomBootScreen", 176, 20, COLOR_TEXT);
    char version[64] = {};
    snprintf(version, sizeof(version), "v%d.%d.%d made by estyxq", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);
    draw_text(renderer, fonts->small, version, 178, 60, COLOR_MUTED);
    draw_text(renderer, fonts->small, mode_title(boot_mode), 560, 48, mode_color);

    draw_presets(renderer, fonts, boot_mode, presets, selected);
    const Preset* preset = presets.empty() ? nullptr : &presets[selected];
    draw_preview(renderer, fonts, boot_mode, preset, preview, preview_w, preview_h);

    draw_button_hint(renderer, fonts, 60, "A", "Apply");
    draw_button_hint(renderer, fonts, 188, "Y", "Mode");
    draw_button_hint(renderer, fonts, 320, "X", "Refresh");
    draw_button_hint(renderer, fonts, 472, "+", "Exit");

    if (mode == Mode::Applying) {
        draw_text(renderer, fonts->small, "Applying selected bootlogo...", 682, 669, mode_color);
    }
    if (!message.empty() && mode == Mode::Browse) {
        draw_text(renderer, fonts->small, ellipsize(fonts->small, message, 540), 680, 669, COLOR_MUTED);
    }

    draw_overlay(renderer, fonts, boot_mode, mode, preset, message, apply_job);
    SDL_RenderPresent(renderer);
}

static bool load_fonts(Fonts* fonts) {
    fonts->title = TTF_OpenFont("romfs:/data/LeroyLetteringLightBeta01.ttf", 34);
    fonts->section = TTF_OpenFont("romfs:/data/LeroyLetteringLightBeta01.ttf", 27);
    fonts->body = TTF_OpenFont("romfs:/data/LeroyLetteringLightBeta01.ttf", 22);
    fonts->small = TTF_OpenFont("romfs:/data/LeroyLetteringLightBeta01.ttf", 18);
    return fonts->title && fonts->section && fonts->body && fonts->small;
}

static void close_fonts(Fonts* fonts) {
    if (fonts->title) {
        TTF_CloseFont(fonts->title);
    }
    if (fonts->section) {
        TTF_CloseFont(fonts->section);
    }
    if (fonts->body) {
        TTF_CloseFont(fonts->body);
    }
    if (fonts->small) {
        TTF_CloseFont(fonts->small);
    }
}

int main(int argc, char* argv[]) {
    Result rc = romfsInit();
    if (R_FAILED(rc)) {
        return -1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        romfsExit();
        return -1;
    }

    if (TTF_Init() < 0) {
        SDL_Quit();
        romfsExit();
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow("CustomBootScreen", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
    if (!window) {
        TTF_Quit();
        SDL_Quit();
        romfsExit();
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        romfsExit();
        return -1;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    Fonts fonts = {};
    if (!load_fonts(&fonts)) {
        close_fonts(&fonts);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        romfsExit();
        return -1;
    }

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    BootMode boot_mode = BootMode::SwitchCfw;
    std::vector<Preset> presets = scan_presets(boot_mode);
    int selected = 0;
    Mode mode = Mode::Browse;
    std::string message;
    SDL_Texture* preview = nullptr;
    int preview_w = 0;
    int preview_h = 0;
    int loaded_preview_index = -1;
    ApplyJob apply_job;
    PresetScanJob scan_job;

    while (appletMainLoop()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                goto cleanup;
            }
        }

        if (selected >= (int)presets.size()) {
            selected = std::max(0, (int)presets.size() - 1);
        }

        if (mode == Mode::Applying && apply_job.done.load()) {
            ApplyResult result = finish_apply_job(&apply_job);
            message = result.message;
            presets = scan_presets(boot_mode);
            const std::string active_name = read_active_name(boot_mode);
            for (int i = 0; i < (int)presets.size(); i++) {
                if (presets[i].name == active_name) {
                    selected = i;
                    break;
                }
            }
            loaded_preview_index = -1;
            mode = Mode::RebootPrompt;
        }

        if (mode == Mode::SwitchingMode && scan_job.done.load()) {
            presets = finish_preset_scan_job(&scan_job);
            selected = 0;
            loaded_preview_index = -1;
            message = std::string("Mode: ") + mode_short_label(boot_mode);
            mode = Mode::Browse;
        }

        if (mode != Mode::SwitchingMode && !presets.empty() && loaded_preview_index != selected) {
            if (preview) {
                SDL_DestroyTexture(preview);
                preview = nullptr;
            }
            preview_w = 0;
            preview_h = 0;
            preview = load_preview_texture(renderer, presets[selected].preview_path, &preview_w, &preview_h);
            loaded_preview_index = selected;
        } else if (presets.empty() && preview) {
            SDL_DestroyTexture(preview);
            preview = nullptr;
            loaded_preview_index = -1;
        }

        render_app(renderer, &fonts, boot_mode, presets, selected, mode, message, preview, preview_w, preview_h, apply_job);

        padUpdate(&pad);
        const u64 down = padGetButtonsDown(&pad);

        if (mode == Mode::Browse) {
            if (down & HidNpadButton_Plus) {
                break;
            }
            if (down & HidNpadButton_Y) {
                boot_mode = other_mode(boot_mode);
                selected = 0;
                if (preview) {
                    SDL_DestroyTexture(preview);
                    preview = nullptr;
                }
                loaded_preview_index = -1;
                preview_w = 0;
                preview_h = 0;
                message.clear();
                start_preset_scan_job(&scan_job, boot_mode);
                mode = Mode::SwitchingMode;
            }
            if ((down & HidNpadButton_AnyUp) && !presets.empty()) {
                selected = (selected + (int)presets.size() - 1) % (int)presets.size();
                message.clear();
            }
            if ((down & HidNpadButton_AnyDown) && !presets.empty()) {
                selected = (selected + 1) % (int)presets.size();
                message.clear();
            }
            if (down & HidNpadButton_X) {
                presets = scan_presets(boot_mode);
                selected = 0;
                loaded_preview_index = -1;
                message = "Preset list refreshed.";
            }
            if ((down & HidNpadButton_A) && !presets.empty()) {
                mode = Mode::ConfirmApply;
            }
        } else if (mode == Mode::ConfirmApply) {
            if (down & HidNpadButton_B) {
                mode = Mode::Browse;
                message.clear();
            }
            if ((down & HidNpadButton_A) && !presets.empty()) {
                message.clear();
                start_apply_job(&apply_job, presets[selected]);
                mode = Mode::Applying;
            }
        } else if (mode == Mode::RebootPrompt) {
            if (message.find("Installed") == 0 && (down & HidNpadButton_A)) {
                Result reboot_rc = request_reboot();
                char error[96] = {};
                snprintf(error, sizeof(error), "Reboot request failed: 0x%x", reboot_rc);
                message = error;
            }
            if (down & HidNpadButton_B) {
                mode = Mode::Browse;
            }
        }
    }

cleanup:
    if (apply_job.thread.joinable()) {
        apply_job.thread.join();
    }
    if (scan_job.thread.joinable()) {
        scan_job.thread.join();
    }
    if (preview) {
        SDL_DestroyTexture(preview);
    }
    close_fonts(&fonts);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    romfsExit();
    return 0;
}
