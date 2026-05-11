#include "app_types.h"
#include "apply.h"
#include "fs_utils.h"
#include "ui.h"

#include <switch.h>
#include <SDL.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <stdio.h>
#include <string>
#include <vector>

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
