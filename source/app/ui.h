#pragma once

#include "app_types.h"

#include <SDL.h>

#include <string>
#include <vector>

SDL_Texture* load_preview_texture(SDL_Renderer* renderer, const std::string& path, int* out_w, int* out_h);

void render_app(
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
);

bool load_fonts(Fonts* fonts);
void close_fonts(Fonts* fonts);
