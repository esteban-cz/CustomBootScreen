#include "ui.h"

#include <switch.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>
#include <stdio.h>

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

SDL_Texture* load_preview_texture(SDL_Renderer* renderer, const std::string& path, int* out_w, int* out_h) {
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

bool load_fonts(Fonts* fonts) {
    fonts->title = TTF_OpenFont("romfs:/data/LeroyLetteringLightBeta01.ttf", 34);
    fonts->section = TTF_OpenFont("romfs:/data/LeroyLetteringLightBeta01.ttf", 27);
    fonts->body = TTF_OpenFont("romfs:/data/LeroyLetteringLightBeta01.ttf", 22);
    fonts->small = TTF_OpenFont("romfs:/data/LeroyLetteringLightBeta01.ttf", 18);
    return fonts->title && fonts->section && fonts->body && fonts->small;
}

void close_fonts(Fonts* fonts) {
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
