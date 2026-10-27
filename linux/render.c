#include "render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "log.h"
#include "pd_png.h"

// mGBA is built with COLOR_16_BIT (see CMakeLists.txt), so a frame is RGB565.
#if BYTES_PER_PIXEL == 2
#define PD_GAME_SDL_FORMAT SDL_PIXELFORMAT_RGB565
#else
#define PD_GAME_SDL_FORMAT SDL_PIXELFORMAT_ARGB8888
#endif

struct pd_render {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Surface* surface; // headless: what the software renderer draws into
    SDL_Texture* game;
    SDL_Texture* ui;
    SDL_Texture* target; // only for a rotated panel
    int targetW, targetH;
    int gw, gh;
    int uw, uh;
    int scale;
    int rotation;
    char name[32]; // the renderer's own name, for the log
    const color_t* lastGame;
    const uint32_t* lastUi;
};

// Integer, fit or stretch, centred in dw x dh.
static void fit_rect(int gw, int gh, int dw, int dh, int scale, SDL_Rect* out) {
    int w = dw;
    int h = dh;
    if (scale == PD_SCALE_INTEGER) {
        int s = dw / gw;
        if (dh / gh < s) {
            s = dh / gh;
        }
        if (s < 1) {
            s = 1;
        }
        w = gw * s;
        h = gh * s;
    } else if (scale == PD_SCALE_FIT) {
        double sx = (double) dw / gw;
        double sy = (double) dh / gh;
        double s = sx < sy ? sx : sy;
        w = (int) (gw * s + 0.5);
        h = (int) (gh * s + 0.5);
        if (w < 1) {
            w = 1;
        }
        if (h < 1) {
            h = 1;
        }
        if (w > dw) {
            w = dw;
        }
        if (h > dh) {
            h = dh;
        }
    }
    out->x = (dw - w) / 2;
    out->y = (dh - h) / 2;
    out->w = w;
    out->h = h;
}

static void draw_scene(struct pd_render* r, int lw, int lh) {
    SDL_SetRenderDrawColor(r->renderer, 0, 0, 0, 255);
    SDL_RenderClear(r->renderer);

    if (r->lastGame) {
        SDL_UpdateTexture(r->game, NULL, r->lastGame, r->gw * BYTES_PER_PIXEL);
        SDL_Rect dst;
        fit_rect(r->gw, r->gh, lw, lh, r->scale, &dst);
        SDL_RenderCopy(r->renderer, r->game, NULL, &dst);
    }
    if (r->lastUi && r->ui) {
        SDL_UpdateTexture(r->ui, NULL, r->lastUi, r->uw * 4);
        SDL_Rect dst = { 0, 0, lw, lh };
        SDL_RenderCopy(r->renderer, r->ui, NULL, &dst);
    }
}

static bool draw(struct pd_render* r, bool present) {
    int w = 0;
    int h = 0;
    if (SDL_GetRendererOutputSize(r->renderer, &w, &h) != 0 || w <= 0 || h <= 0) {
        return false;
    }
    bool sideways = r->rotation == 90 || r->rotation == 270;
    int lw = sideways ? h : w;
    int lh = sideways ? w : h;

    if (r->rotation == 0) {
        draw_scene(r, w, h);
    } else {
        if (!r->target || r->targetW != lw || r->targetH != lh) {
            if (r->target) {
                SDL_DestroyTexture(r->target);
                r->target = NULL;
            }
            r->target = SDL_CreateTexture(r->renderer, SDL_PIXELFORMAT_ARGB8888,
                                          SDL_TEXTUREACCESS_TARGET, lw, lh);
            if (!r->target) {
                pd_logf("render: no target texture for the rotation: %s", SDL_GetError());
                return false;
            }
            r->targetW = lw;
            r->targetH = lh;
        }
        if (SDL_SetRenderTarget(r->renderer, r->target) != 0) {
            return false;
        }
        draw_scene(r, lw, lh);
        SDL_SetRenderTarget(r->renderer, NULL);
        SDL_SetRenderDrawColor(r->renderer, 0, 0, 0, 255);
        SDL_RenderClear(r->renderer);
        SDL_Rect full = { 0, 0, w, h };
        SDL_RenderCopyEx(r->renderer, r->target, NULL, &full, (double) r->rotation, NULL, SDL_FLIP_NONE);
    }
    if (present) {
        SDL_RenderPresent(r->renderer);
    }
    return true;
}

struct pd_render* pd_render_open(const char* title, int gameW, int gameH, int scale, int rotation,
                                 bool fullscreen, bool headless, bool softwareOnly) {
    struct pd_render* r = calloc(1, sizeof(*r));
    if (!r) {
        return NULL;
    }
    r->gw = gameW;
    r->gh = gameH;
    r->scale = scale;
    r->rotation = ((rotation % 360) + 360) % 360;

    // Nearest neighbour: GBA pixels stay pixels.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    if (headless) {
        r->surface = SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32, SDL_PIXELFORMAT_ARGB8888);
        if (!r->surface) {
            pd_logf("render: no headless surface: %s", SDL_GetError());
            free(r);
            return NULL;
        }
        r->renderer = SDL_CreateSoftwareRenderer(r->surface);
    } else {
        Uint32 flags = SDL_WINDOW_RESIZABLE;
        if (fullscreen) {
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        }
        r->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, flags);
        if (r->window) {
            // The GPU driver (Mali blobs on these SoCs) is the one piece that
            // can't be fixed from here: --sw falls back to the software
            // renderer, which is slower but does everything in plain memory.
            if (!softwareOnly) {
                r->renderer = SDL_CreateRenderer(r->window, -1, SDL_RENDERER_ACCELERATED);
            }
            if (!r->renderer) {
                r->renderer = SDL_CreateRenderer(r->window, -1, SDL_RENDERER_SOFTWARE);
            }
        }
    }
    if (!r->renderer) {
        pd_logf("render: no renderer: %s", SDL_GetError());
        pd_render_close(r);
        return NULL;
    }

    r->game = SDL_CreateTexture(r->renderer, PD_GAME_SDL_FORMAT, SDL_TEXTUREACCESS_STREAMING, gameW, gameH);
    if (!r->game) {
        pd_logf("render: no game texture: %s", SDL_GetError());
        pd_render_close(r);
        return NULL;
    }
    SDL_SetTextureBlendMode(r->game, SDL_BLENDMODE_NONE);

    SDL_RendererInfo info;
    const char* name = SDL_GetRendererInfo(r->renderer, &info) == 0 ? info.name : "?";
    snprintf(r->name, sizeof(r->name), "%s", name);
    pd_logf("render: %dx%d, %s renderer, scale %d, rotation %d", pd_render_width(r), pd_render_height(r), r->name,
            r->scale, r->rotation);
    return r;
}

void pd_render_close(struct pd_render* r) {
    if (!r) {
        return;
    }
    if (r->target) {
        SDL_DestroyTexture(r->target);
    }
    if (r->ui) {
        SDL_DestroyTexture(r->ui);
    }
    if (r->game) {
        SDL_DestroyTexture(r->game);
    }
    if (r->renderer) {
        SDL_DestroyRenderer(r->renderer);
    }
    if (r->window) {
        SDL_DestroyWindow(r->window);
    }
    if (r->surface) {
        SDL_FreeSurface(r->surface);
    }
    free(r);
}

void pd_render_frame(struct pd_render* r, const color_t* game, const uint32_t* overlay, int ow, int oh) {
    if (!r) {
        return;
    }
    r->lastGame = game;
    r->lastUi = overlay;
    if (overlay && (ow != r->uw || oh != r->uh)) {
        if (r->ui) {
            SDL_DestroyTexture(r->ui);
            r->ui = NULL;
        }
        r->uw = ow;
        r->uh = oh;
        r->ui = SDL_CreateTexture(r->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, ow, oh);
        if (!r->ui) {
            pd_logf("render: no companion texture: %s", SDL_GetError());
            r->lastUi = NULL;
            return;
        }
        SDL_SetTextureBlendMode(r->ui, SDL_BLENDMODE_NONE);
    }
    draw(r, true);
}

void pd_render_set_scale(struct pd_render* r, int scale) {
    if (r) {
        r->scale = scale;
    }
}

bool pd_render_screenshot(struct pd_render* r, const char* path) {
    if (!r || !path) {
        return false;
    }
    // Draw again without presenting: after SDL_RenderPresent the back buffer
    // isn't something to read from.
    if (!draw(r, false)) {
        return false;
    }
    int w = pd_render_width(r);
    int h = pd_render_height(r);
    if (w <= 0 || h <= 0) {
        return false;
    }
    size_t count = (size_t) w * (size_t) h;
    uint32_t* argb = malloc(count * sizeof(uint32_t));
    uint32_t* rgb = malloc(count * sizeof(uint32_t));
    bool ok = false;
    if (argb && rgb && SDL_RenderReadPixels(r->renderer, NULL, SDL_PIXELFORMAT_ARGB8888, argb, w * 4) == 0) {
        for (size_t i = 0; i < count; i++) {
            rgb[i] = argb[i] & 0xFFFFFFu;
        }
        ok = pd_png_write(path, w, h, rgb);
    }
    free(argb);
    free(rgb);
    return ok;
}

int pd_render_width(const struct pd_render* r) {
    int w = 0;
    int h = 0;
    if (!r || SDL_GetRendererOutputSize(r->renderer, &w, &h) != 0) {
        return 0;
    }
    return w;
}

int pd_render_height(const struct pd_render* r) {
    int w = 0;
    int h = 0;
    if (!r || SDL_GetRendererOutputSize(r->renderer, &w, &h) != 0) {
        return 0;
    }
    return h;
}

const char* pd_render_name(const struct pd_render* r) {
    return r && r->name[0] ? r->name : "?";
}

void pd_render_display(char* out, size_t outLen) {
    SDL_DisplayMode mode;
    if (!out || !outLen) {
        return;
    }
    if (SDL_GetCurrentDisplayMode(0, &mode) != 0) {
        snprintf(out, outLen, "unknown");
        return;
    }
    snprintf(out, outLen, "%dx%d @%.1f Hz", mode.w, mode.h, (double) mode.refresh_rate);
}
