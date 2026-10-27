// Sound: mGBA's blip output straight into an SDL2 audio queue, muted while
// fast-forwarding (proper resampling while fast-forwarding is a later phase).
#ifndef PD_AUDIO_H
#define PD_AUDIO_H

#include <stdbool.h>

#include <mgba/core/core.h>

struct pd_audio;

// NULL when the device can't be opened: the game then runs silent instead of
// failing to start.
struct pd_audio* pd_audio_open(void);
void pd_audio_close(struct pd_audio* a);

// Sends the core's audio to this device: sets the blip rates from the core's
// clock and installs the mAVStream (call after loadROM, before reset).
void pd_audio_attach(struct pd_audio* a, struct mCore* core);

void pd_audio_set_muted(struct pd_audio* a, bool muted);
// Hands the samples the last frames produced to the device.
void pd_audio_pump(struct pd_audio* a);

// Emulated frames' worth of audio the device hasn't played yet (pacing, logs).
int pd_audio_queued_frames(const struct pd_audio* a);
int pd_audio_rate(const struct pd_audio* a);
// SDL's audio driver ("alsa", "pulseaudio", ...), or "" before it opened.
const char* pd_audio_driver(const struct pd_audio* a);

#endif
