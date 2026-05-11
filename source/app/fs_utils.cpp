#include "fs_utils.h"

#include <algorithm>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static std::string lower_copy(std::string value) {
    for (char& c : value) {
        c = (char)tolower((unsigned char)c);
    }
    return value;
}

std::string join_path(const std::string& base, const std::string& name) {
    if (base.empty() || base == "/") {
        return "/" + name;
    }
    return base + "/" + name;
}

bool path_exists(const std::string& path) {
    struct stat st = {};
    return stat(path.c_str(), &st) == 0;
}

bool is_dir(const std::string& path) {
    struct stat st = {};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static bool mkdir_one(const std::string& path) {
    if (path.empty() || path == "/" || is_dir(path)) {
        return true;
    }
    return mkdir(path.c_str(), 0777) == 0 || errno == EEXIST;
}

bool mkdir_recursive(const std::string& path) {
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

static const char* mode_presets_dir(BootMode boot_mode) {
    return boot_mode == BootMode::SwitchCfw ? SWITCH_PRESETS_DIR : HEKATE_PRESETS_DIR;
}

static const char* mode_active_file(BootMode boot_mode) {
    return boot_mode == BootMode::SwitchCfw ? SWITCH_ACTIVE_FILE : HEKATE_ACTIVE_FILE;
}

static std::string parent_dir(const std::string& path) {
    const size_t pos = path.find_last_of('/');
    if (pos == std::string::npos || pos == 0) {
        return "/";
    }
    return path.substr(0, pos);
}

bool remove_recursive(const std::string& path) {
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

bool copy_file(const std::string& src, const std::string& dst) {
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

bool copy_directory(const std::string& src, const std::string& dst, bool skip_previews, int* copied_files) {
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

void ensure_app_dirs() {
    mkdir_recursive(APP_DIR);
    mkdir_recursive(SWITCH_PRESETS_DIR);
    mkdir_recursive(HEKATE_PRESETS_DIR);
}

std::string read_active_name(BootMode boot_mode) {
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

void write_active_name(BootMode boot_mode, const std::string& name) {
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

std::vector<Preset> scan_presets(BootMode boot_mode) {
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
