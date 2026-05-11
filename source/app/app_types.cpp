#include "app_types.h"

const char* mode_title(BootMode boot_mode) {
    return boot_mode == BootMode::SwitchCfw ? "Switch CFW bootlogo" : "Hekate bootlogo";
}

const char* mode_short_label(BootMode boot_mode) {
    return boot_mode == BootMode::SwitchCfw ? "Switch CFW" : "Hekate";
}

BootMode other_mode(BootMode boot_mode) {
    return boot_mode == BootMode::SwitchCfw ? BootMode::Hekate : BootMode::SwitchCfw;
}
