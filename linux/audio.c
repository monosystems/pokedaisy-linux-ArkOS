#include "audio.h"

#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include <mgba/core/blip_buf.h>

#include "log.h"

// Stereo, 16 bit: two int16 per sample frame. 8192 holds about five frames of
// GBA audio at 48 kHz, plenty between two pumps.
#define PD_AUDIO_RING 8192
// The GBA's and the Game Boy's frame rate (16777216 / 280896).
#define PD_AUDIO_FPS 59.727500569606

struct pd_audio {
    SDL_AudioDeviceID dev;
    SDL_AudioSpec spec;
    struct mAVStream stream;
    int16_t ring[PD_AUDIO_RING];
    size_t ringLen;       // int16 values waiting in the ring
    size_t bytesPerFrame; // one emulated frame of audio at spec.freq
    bool muted;
};

// mGBA's postAudioBuffer gets no context pointer, and there is one device.
static struct pd_audio* current;

static void post_audio(struct mAVStream* stream, struct blip_t* left, struct blip_t* right) {
    (void) stream;
    struct pd_audio* a = current;
    if (!a) {
        return;
    }
    if (a->muted) {
        blip_clear(left);
        blip_clear(right);
        return;
    }
    int avail = blip_samples_avail(left);
    if (avail <= 0) {
        return;
    }
    if (avail > PD_AUDIO_RING / 2) {
        avail = PD_AUDIO_RING / 2;
    }
    if (a->ringLen + (size_t) avail * 2 > PD_AUDIO_RING) {
        a->ringLen = 0; // the consumer stalled: drop what's there rather than grow
    }
    blip_read_samples(left, (short*) a->ring + a->ringLen, avail, true);
    blip_read_samples(right, (short*) a->ring + a->ringLen + 1, avail, true);
    a->ringLen += (size_t) avail * 2;
}

struct pd_audio* pd_audio_open(void) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        pd_logf("audio: SDL audio didn't start (%s): running silent", SDL_GetError());
        return NULL;
    }
    struct pd_audio* a = calloc(1, sizeof(*a));
    if (!a) {
        return NULL;
    }
    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = NULL; // we queue; the device pulls
    a->dev = SDL_OpenAudioDevice(NULL, 0, &want, &a->spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!a->dev) {
        pd_logf("audio: no device (%s): running silent", SDL_GetError());
        free(a);
        return NULL;
    }
    if (a->spec.format != AUDIO_S16SYS || a->spec.channels != 2) {
        pd_logf("audio: the device wants another format than S16 stereo: running silent");
        SDL_CloseAudioDevice(a->dev);
        free(a);
        return NULL;
    }
    a->bytesPerFrame = (size_t) ((double) a->spec.freq / PD_AUDIO_FPS * 4.0 + 0.5);
    a->stream.postAudioBuffer = post_audio;
    current = a;
    SDL_PauseAudioDevice(a->dev, 0);
    pd_logf("audio: %d Hz stereo, %u bytes per frame", a->spec.freq, (unsigned) a->bytesPerFrame);
    return a;
}

void pd_audio_close(struct pd_audio* a) {
    if (!a) {
        return;
    }
    if (current == a) {
        current = NULL;
    }
    if (a->dev) {
        SDL_ClearQueuedAudio(a->dev);
        SDL_CloseAudioDevice(a->dev);
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    free(a);
}

void pd_audio_attach(struct pd_audio* a, struct mCore* core) {
    if (!a || !core) {
        return;
    }
    double clock = (double) core->frequency(core);
    blip_t* left = core->getAudioChannel(core, 0);
    blip_t* right = core->getAudioChannel(core, 1);
    if (left) {
        blip_set_rates(left, clock, (double) a->spec.freq);
    }
    if (right) {
        blip_set_rates(right, clock, (double) a->spec.freq);
    }
    core->setAVStream(core, &a->stream);
}

void pd_audio_set_muted(struct pd_audio* a, bool muted) {
    if (!a || a->muted == muted) {
        return;
    }
    a->muted = muted;
    a->ringLen = 0;
    if (a->dev) {
        SDL_ClearQueuedAudio(a->dev); // stop the frames already queued too
    }
}

void pd_audio_pump(struct pd_audio* a) {
    if (!a || !a->dev) {
        return;
    }
    if (a->muted || a->ringLen == 0) {
        a->ringLen = 0;
        return;
    }
    Uint32 queued = SDL_GetQueuedAudioSize(a->dev);
    Uint32 target = (Uint32) (a->bytesPerFrame * 4); // ~66 ms of buffer
    if (queued > target * 4) {
        // Far behind (a stall, a slow storage read): resync instead of playing late.
        SDL_ClearQueuedAudio(a->dev);
        queued = 0;
    }
    if (queued >= target) {
        return; // the ring holds it until the device catches up
    }
    size_t room = target - queued;
    size_t bytes = a->ringLen * sizeof(int16_t);
    if (bytes > room) {
        bytes = room & ~(size_t) 3; // whole stereo frames only
    }
    if (bytes == 0) {
        return;
    }
    SDL_QueueAudio(a->dev, a->ring, (Uint32) bytes);
    size_t left = a->ringLen * sizeof(int16_t) - bytes;
    memmove(a->ring, (const uint8_t*) a->ring + bytes, left);
    a->ringLen = left / sizeof(int16_t);
}

int pd_audio_queued_frames(const struct pd_audio* a) {
    if (!a || !a->dev || !a->bytesPerFrame) {
        return 0;
    }
    return (int) (SDL_GetQueuedAudioSize(a->dev) / a->bytesPerFrame);
}

int pd_audio_rate(const struct pd_audio* a) {
    return a ? a->spec.freq : 0;
}

const char* pd_audio_driver(const struct pd_audio* a) {
    if (!a || !a->dev) {
        return "";
    }
    const char* driver = SDL_GetCurrentAudioDriver();
    return driver ? driver : "";
}
