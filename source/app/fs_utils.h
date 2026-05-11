#pragma once

#include "app_types.h"

#include <string>
#include <vector>

std::string join_path(const std::string& base, const std::string& name);
bool path_exists(const std::string& path);
bool is_dir(const std::string& path);
bool mkdir_recursive(const std::string& path);
bool remove_recursive(const std::string& path);
bool copy_file(const std::string& src, const std::string& dst);
bool copy_directory(const std::string& src, const std::string& dst, bool skip_previews, int* copied_files);

void ensure_app_dirs();
std::string read_active_name(BootMode boot_mode);
void write_active_name(BootMode boot_mode, const std::string& name);
std::vector<Preset> scan_presets(BootMode boot_mode);
