// ucontext in the crash handler is XSI on the Mac (and POSIX everywhere);
// _DARWIN_C_SOURCE keeps dladdr and the rest of Darwin's API visible. glibc
// only declares dladdr/Dl_info with _GNU_SOURCE, hence the two branches.
#if defined(__APPLE__)
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE 1
#else
#define _GNU_SOURCE 1
#endif

// PokeDaisy for Linux handhelds: mGBA (libmgba) in-process, the game on the
// screen, and the companion (3ds/core's pd_ui) over it when the player asks for
// it. See linux/PLAN.md for the phases.
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if defined(__linux__)
#include <link.h> // dl_iterate_phdr, dl_phdr_info
#include <sys/syscall.h>
#endif
#include <unistd.h>
#include <ucontext.h>

#include <SDL.h>

#include <mgba/core/config.h>
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba-util/vfs.h>

#include "audio.h"
#include "companion.h"
#include "files.h"
#include "input.h"
#include "log.h"
#include "menu.h"
#include "render.h"
#include "state.h"

// 16777216 Hz / 280896 cycles: the GBA's frame rate (the Game Boy's is the same).
#define PD_GBA_FPS 59.727500569606
#define PD_WINDOW_TITLE "PokeDaisy"
#define PD_PATH_MAX 1024

struct options {
    const char* rom;
    const char* logPath;
    const char* screenshot;
    const char* settingsPath;
    const char* romsDir;
    const char* pick;
    const char* menuShot;
    const char* testNav;
    int frames; // 0 = until the player quits
    int navDelay; // test-nav waits this many frames (the fixture ROM's RAM copy)
    int ffSpeed;
    int scale;
    int rotation;
    bool ffSet;
    bool scaleSet;
    bool rotationSet;
    bool fullscreen;
    bool headless;
    bool noAudio;
    bool fillScreen;
    bool startCompanion;
    bool stats;
    bool noResume;
    bool testCrash;
    bool softwareRenderer;
    bool help;
};

static void usage(FILE* out) {
    fprintf(out,
            "PokeDaisy for Linux handhelds\n"
            "\n"
            "  pokedaisy <game.gba> [options]\n"
            "\n"
            "  --roms DIR          with no game given, list this folder's .gba files\n"
            "  --pick NAME         with --roms, choose a game without a pad (tests)\n"
            "  --menu-shot FILE    with --pick, write the list to a PNG (tests)\n"
            "  --fullscreen        fill the screen (the handheld's default)\n"
            "  --sw                software renderer (if the GPU driver crashes)\n"
            "  --scale MODE        integer | fit | stretch (default: fit)\n"
            "  --rotation DEG     0 | 90 | 180 | 270, for a panel mounted rotated\n"
            "  --ff N              fast-forward speed, frames per tick (default: 3)\n"
            "  --settings FILE     where screen mode and fast-forward are kept\n"
            "  --log FILE          also write the log there (the port script does)\n"
            "  --stats             log frames per second and frame time every second\n"
            "  --no-resume         always start at the game's own title screen\n"
            "  --companion         open the companion at start (tests, screenshots)\n"
            "  --test-nav LIST     overlay presses, one per frame: down,tabnext,ok (tests)\n"
            "  --test-nav-delay N  wait N frames before the first --test-nav press\n"
            "  --frames N          run N frames and exit (tests)\n"
            "  --screenshot FILE   write the last frame as a PNG (tests)\n"
            "  --headless          no window or sound, offscreen rendering (tests)\n"
            "  --no-audio          play silent\n"
            "  --test-fill         fill VRAM red after loading (smoke test only)\n"
            "  --test-crash        crash on purpose, to check the crash line (tests only)\n"
            "  -h, --help\n");
}

static bool parse_scale(const char* name, int* out) {
    if (strcmp(name, "integer") == 0) {
        *out = PD_SCALE_INTEGER;
        return true;
    }
    if (strcmp(name, "fit") == 0) {
        *out = PD_SCALE_FIT;
        return true;
    }
    if (strcmp(name, "stretch") == 0) {
        *out = PD_SCALE_STRETCH;
        return true;
    }
    return false;
}

static bool parse_options(int argc, char** argv, struct options* o) {
    memset(o, 0, sizeof(*o));
    o->ffSpeed = 3;
    o->scale = PD_SCALE_FIT;
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            o->help = true;
        } else if (strcmp(arg, "--fullscreen") == 0) {
            o->fullscreen = true;
        } else if (strcmp(arg, "--headless") == 0) {
            o->headless = true;
            o->noAudio = true;
        } else if (strcmp(arg, "--no-audio") == 0) {
            o->noAudio = true;
        } else if (strcmp(arg, "--test-fill") == 0) {
            o->fillScreen = true;
        } else if (strcmp(arg, "--stats") == 0) {
            o->stats = true;
        } else if (strcmp(arg, "--no-resume") == 0) {
            o->noResume = true;
        } else if (strcmp(arg, "--test-crash") == 0) {
            o->testCrash = true;
        } else if (strcmp(arg, "--sw") == 0) {
            o->softwareRenderer = true;
        } else if (strcmp(arg, "--companion") == 0) {
            o->startCompanion = true;
        } else if (strcmp(arg, "--settings") == 0 && i + 1 < argc) {
            o->settingsPath = argv[++i];
        } else if (strcmp(arg, "--test-nav") == 0 && i + 1 < argc) {
            o->testNav = argv[++i];
        } else if (strcmp(arg, "--test-nav-delay") == 0 && i + 1 < argc) {
            o->navDelay = atoi(argv[++i]);
        } else if (strcmp(arg, "--roms") == 0 && i + 1 < argc) {
            o->romsDir = argv[++i];
        } else if (strcmp(arg, "--pick") == 0 && i + 1 < argc) {
            o->pick = argv[++i];
        } else if (strcmp(arg, "--menu-shot") == 0 && i + 1 < argc) {
            o->menuShot = argv[++i];
        } else if (strcmp(arg, "--scale") == 0 && i + 1 < argc) {
            if (!parse_scale(argv[++i], &o->scale)) {
                fprintf(stderr, "--scale takes integer, fit or stretch\n");
                return false;
            }
            o->scaleSet = true;
        } else if (strcmp(arg, "--rotation") == 0 && i + 1 < argc) {
            o->rotation = atoi(argv[++i]);
            o->rotationSet = true;
        } else if (strcmp(arg, "--frames") == 0 && i + 1 < argc) {
            o->frames = atoi(argv[++i]);
        } else if (strcmp(arg, "--ff") == 0 && i + 1 < argc) {
            o->ffSpeed = atoi(argv[++i]);
            if (o->ffSpeed < 1) {
                o->ffSpeed = 1;
            }
            o->ffSet = true;
        } else if (strcmp(arg, "--screenshot") == 0 && i + 1 < argc) {
            o->screenshot = argv[++i];
        } else if (strcmp(arg, "--log") == 0 && i + 1 < argc) {
            o->logPath = argv[++i];
        } else if (arg[0] == '-' && arg[1] != '\0') {
            fprintf(stderr, "unknown option: %s\n", arg);
            return false;
        } else if (o->rom) {
            fprintf(stderr, "only one game at a time (%s and %s)\n", o->rom, arg);
            return false;
        } else {
            o->rom = arg;
        }
    }
    return true;
}

// mGBA's default logger prints every unmapped I/O write; the companion debug
// comes from pd_logf instead.
static void silent_log(struct mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args) {
    (void) logger;
    (void) category;
    (void) level;
    (void) format;
    (void) args;
}
static struct mLogger silentLogger = { .log = silent_log, .filter = NULL };

// --- crash reporting ---------------------------------------------------------
// There is no SSH on the handheld: a crash has to explain itself in log.txt.
// The handler writes one line - signal, faulting address, program counter, and
// what the companion was drawing - with the plain write() the kernel allows
// inside a signal handler. The PC resolves to a file and line with
//     addr2line -e pokedaisy -f -C 0x<pc>
// against the same build (that is why the release keeps its symbols for now).

static int put_text(char* out, int at, const char* text) {
    while (*text) {
        out[at++] = *text++;
    }
    return at;
}

static int put_num(char* out, int at, unsigned long v) {
    char tmp[24];
    int n = 0;
    do {
        tmp[n++] = (char) ('0' + (v % 10));
        v /= 10;
    } while (v);
    while (n) {
        out[at++] = tmp[--n];
    }
    return at;
}

static int put_hex(char* out, int at, unsigned long v) {
    static const char digits[] = "0123456789abcdef";
    char tmp[20];
    int n = 0;
    do {
        tmp[n++] = digits[v & 0xFu];
        v >>= 4;
    } while (v);
    out[at++] = '0';
    out[at++] = 'x';
    while (n) {
        out[at++] = tmp[--n];
    }
    return at;
}

// Where the code was loaded: the crash line reports pc-base, which is what
// addr2line wants from a PIE. codeEnd bounds it, so the frame walk can tell a
// return address in the program from a stale frame's junk.
static unsigned long codeBase;
#if defined(__linux__)
static unsigned long codeEnd; // where the program ends (set by measure_code)
#endif

static void find_code_base(void) {
    Dl_info info;
    if (dladdr((void*) (uintptr_t) &find_code_base, &info) && info.dli_fbase) {
        codeBase = (unsigned long) (uintptr_t) info.dli_fbase;
    }
}

// Walk the frame pointer chain from the faulting frame: return addresses that
// are in the program resolve with addr2line even when pc sat in a library.
// Anything outside the program is skipped - a stale frame pointer would
// otherwise print addresses that look resolvable but are not.
#if defined(__linux__) && defined(__aarch64__)
static int put_backtrace(char* line, int at, unsigned long fp) {
    for (int i = 0; i < 6; i++) {
        if (!fp || at > 300) {
            break;
        }
        unsigned long frame[2];
        memcpy(frame, (void*) (uintptr_t) fp, sizeof(frame)); // fp[0] = next, fp[1] = return address
        if (frame[1] > codeBase && frame[1] < codeEnd) {
            at = put_text(line, at, " <- ");
            at = put_hex(line, at, frame[1] - codeBase);
        }
        if (frame[0] <= fp || frame[0] - fp > 1024 * 1024) {
            break; // not a frame pointer any more
        }
        fp = frame[0];
    }
    return at;
}
#endif
static unsigned long thread_id(void) {
#if defined(__linux__)
    return (unsigned long) syscall(SYS_gettid);
#else
    return 0;
#endif
}

static void crash_handler(int sig, siginfo_t* info, void* uctx) {
    unsigned long pc = 0;
    unsigned long lr = 0;
    unsigned long sp = 0;
    unsigned long fp = 0;
#if defined(__aarch64__) || defined(__arm64__)
    ucontext_t* uc = (ucontext_t*) uctx;
#if defined(__APPLE__)
    pc = (unsigned long) uc->uc_mcontext->__ss.__pc;
    lr = (unsigned long) uc->uc_mcontext->__ss.__lr;
    sp = (unsigned long) uc->uc_mcontext->__ss.__sp;
#else
    pc = (unsigned long) uc->uc_mcontext.pc;
    lr = (unsigned long) uc->uc_mcontext.regs[30];
    sp = (unsigned long) uc->uc_mcontext.sp;
    fp = (unsigned long) uc->uc_mcontext.regs[29];
#endif
#elif defined(__x86_64__)
    ucontext_t* uc = (ucontext_t*) uctx;
#if defined(__APPLE__)
    pc = (unsigned long) uc->uc_mcontext->__ss.__rip;
    sp = (unsigned long) uc->uc_mcontext->__ss.__rsp;
#else
    pc = (unsigned long) uc->uc_mcontext.gregs[REG_RIP];
    sp = (unsigned long) uc->uc_mcontext.gregs[REG_RSP];
#endif
#else
    (void) uctx;
#endif
    int tab = -1;
    int hits = 0;
    int focus = -1;
    pd_companion_debug(&tab, &hits, &focus);

    char line[512];
    int at = put_text(line, 0, "crash: signal ");
    at = put_num(line, at, (unsigned long) sig);
    at = put_text(line, at, " at ");
    at = put_hex(line, at, (unsigned long) (info ? info->si_addr : 0));
    at = put_text(line, at, ", pc ");
    at = put_hex(line, at, pc);
    at = put_text(line, at, ", lr ");
    at = put_hex(line, at, lr);
    at = put_text(line, at, ", sp ");
    at = put_hex(line, at, sp);
    at = put_text(line, at, ", off ");
    at = put_hex(line, at, pc - codeBase);
    at = put_text(line, at, ", companion tab ");
    at = put_num(line, at, (unsigned long) tab);
    at = put_text(line, at, " hits ");
    at = put_num(line, at, (unsigned long) hits);
    at = put_text(line, at, " focus ");
    if (focus < 0) {
        line[at++] = '-';
        at = put_num(line, at, (unsigned long) (-focus));
    } else {
        at = put_num(line, at, (unsigned long) focus);
    }
    at = put_text(line, at, " tid ");
    at = put_num(line, at, thread_id());
#if defined(__linux__) && defined(__aarch64__)
    at = put_backtrace(line, at, fp);
#else
    (void) fp;
#endif
    line[at++] = '\n';
    ssize_t written = write(STDERR_FILENO, line, (size_t) at);
    (void) written;

    // Out right away: a nested fault in the handler would otherwise loop.
    _exit(128 + sig);
}

static void install_crash_handler(void) {
    // PD_NO_CRASH_HANDLER=1 leaves the signal to the runtime - ASan and gdb
    // need it to report the real stack (local debugging only).
    if (getenv("PD_NO_CRASH_HANDLER")) {
        return;
    }
    // A stack of its own, so even a runaway recursion still reports.
    static char altStack[64 * 1024];
    stack_t ss;
    memset(&ss, 0, sizeof(ss));
    ss.ss_sp = altStack;
    ss.ss_size = sizeof(altStack);
    sigaltstack(&ss, NULL);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
}

// The libraries' load addresses, so a crash line whose pc isn't in the program
// (a GPU driver, SDL) can be attributed: pc - base of that module. Linux only:
// the Mac has no dl_iterate_phdr, and this is for the handheld.
#if defined(__linux__)
static int log_module(struct dl_phdr_info* info, size_t size, void* data) {
    (void) size;
    bool quiet = data != NULL;
    if (info->dlpi_name && info->dlpi_name[0]) {
        if (!quiet) {
            pd_logf("system: module %s at 0x%lx", info->dlpi_name, (unsigned long) (uintptr_t) info->dlpi_addr);
        }
        return 0;
    }
    // The program itself (an empty name): its last PT_LOAD ends the code range.
    unsigned long end = 0;
    for (int i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr)* ph = &info->dlpi_phdr[i];
        if (ph->p_type == PT_LOAD) {
            unsigned long at = (unsigned long) info->dlpi_addr + (unsigned long) ph->p_vaddr + (unsigned long) ph->p_memsz;
            if (at > end) {
                end = at;
            }
        }
    }
    codeEnd = end;
    return 0;
}

// The program's range first (a crash during start-up still gets a backtrace),
// every library's address once SDL and its drivers are loaded.
static void measure_code(void) {
    dl_iterate_phdr(log_module, (void*) 1);
}

static void log_modules(void) {
    dl_iterate_phdr(log_module, NULL);
}
#else
static void measure_code(void) {
}

static void log_modules(void) {
}
#endif

static bool is_dir(const char* path) {
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// The GBA's memory as mGBA holds it: what pd_snapshot reads the game through.
struct game_mem {
    uint8_t* ewram;
    size_t ewramSize;
    uint8_t* iwram;
    size_t iwramSize;
    const uint8_t* rom;
    size_t romSize;
};

static bool bus_read(void* ctx, uint32_t addr, void* out, size_t len) {
    const struct game_mem* m = ctx;
    if (addr >= 0x02000000u && (size_t) (addr - 0x02000000u) + len <= m->ewramSize) {
        memcpy(out, m->ewram + (addr - 0x02000000u), len);
        return true;
    }
    if (addr >= 0x03000000u && (size_t) (addr - 0x03000000u) + len <= m->iwramSize) {
        memcpy(out, m->iwram + (addr - 0x03000000u), len);
        return true;
    }
    // Pointers in RAM often lead into the ROM (a map's layout, a table).
    if (addr >= 0x08000000u && m->rom && (size_t) (addr - 0x08000000u) + len <= m->romSize) {
        memcpy(out, m->rom + (addr - 0x08000000u), len);
        return true;
    }
    return false;
}

// The companion's screen modes keep the core's names: PIXEL is whole-pixel
// scaling, SHARP fills the screen, STRETCH fills it both ways.
static int scale_for_screen_mode(int mode) {
    if (mode == PD_SCREEN_PIXEL) {
        return PD_SCALE_INTEGER;
    }
    if (mode == PD_SCREEN_STRETCH) {
        return PD_SCALE_STRETCH;
    }
    return PD_SCALE_FIT;
}

static int screen_mode_for_scale(int scale) {
    if (scale == PD_SCALE_INTEGER) {
        return PD_SCREEN_PIXEL;
    }
    if (scale == PD_SCALE_STRETCH) {
        return PD_SCREEN_STRETCH;
    }
    return PD_SCREEN_SHARP;
}

static const char* scale_name(int scale) {
    if (scale == PD_SCALE_INTEGER) {
        return "integer";
    }
    if (scale == PD_SCALE_STRETCH) {
        return "stretch";
    }
    return "fit";
}

// The device log has to say whether the overlay was up: a stats line alone
// can't tell whether the frame's time went to the game or to the companion.
static void log_overlay(const struct pd_companion* companion) {
    if (!companion) {
        return;
    }
    pd_logf("companion: %s", pd_companion_visible(companion) ? "shown" : "hidden");
}

// Tests drive the overlay without a pad or a screen: the presses of --test-nav
// go in one per frame, in the order given, so each redraw's hits are used in
// turn (see linux/test/smoke.sh).
struct test_nav {
    const char* spec;
    size_t at;
};

static bool nav_from_name(const char* name, size_t len, enum pd_nav* out) {
    static const struct {
        const char* name;
        enum pd_nav nav;
    } names[] = {
        { "up", PD_NAV_UP },           { "down", PD_NAV_DOWN },         { "left", PD_NAV_LEFT },
        { "right", PD_NAV_RIGHT },     { "ok", PD_NAV_OK },             { "back", PD_NAV_BACK },
        { "tabnext", PD_NAV_TAB_NEXT }, { "tabprev", PD_NAV_TAB_PREV },
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (strlen(names[i].name) == len && strncmp(names[i].name, name, len) == 0) {
            *out = names[i].nav;
            return true;
        }
    }
    return false;
}

static bool test_nav_next(struct test_nav* t, enum pd_nav* out) {
    if (!t->spec) {
        return false;
    }
    while (t->spec[t->at]) {
        while (t->spec[t->at] == ',') {
            t->at++;
        }
        size_t start = t->at;
        while (t->spec[t->at] && t->spec[t->at] != ',') {
            t->at++;
        }
        if (t->at == start) {
            return false;
        }
        if (nav_from_name(t->spec + start, t->at - start, out)) {
            return true;
        }
    }
    return false;
}

// Our own copy of the ROM. The companion reads the game's tables through this
// while it runs, and mGBA's ROM block is not ours to keep: it is rebuilt on a
// reset and on a save load, so the pointer handed out at startup goes stale -
// that is what crashed the GUIDE tab (a table read through the old block).
static uint8_t* read_rom_copy(const char* path, size_t size) {
    if (size < 0xC0) {
        return NULL;
    }
    FILE* f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    uint8_t* data = malloc(size);
    bool ok = data && fread(data, 1, size, f) == size;
    fclose(f);
    if (!ok) {
        free(data);
        return NULL;
    }
    return data;
}

int main(int argc, char** argv) {
    struct options opts;
    if (!parse_options(argc, argv, &opts)) {
        usage(stderr);
        return 2;
    }
    if (opts.help) {
        usage(stdout);
        return 0;
    }
    if (!opts.rom && !opts.romsDir) {
        // A launcher (the port) starts without a game: ask the ROMs folder.
        if (is_dir("/roms/gba")) {
            opts.romsDir = "/roms/gba";
        } else if (is_dir("roms")) {
            opts.romsDir = "roms";
        }
    }
    if (!opts.rom && !opts.romsDir) {
        usage(stderr);
        return 2;
    }

    // A panel that is mounted sideways, or a screen mode to try, can come from
    // the environment too: POKEDAISY_ROTATION=90 POKEDAISY_SCALE=integer
    const char* envRotation = getenv("POKEDAISY_ROTATION");
    if (!opts.rotationSet && envRotation) {
        opts.rotation = atoi(envRotation);
    }
    const char* envScale = getenv("POKEDAISY_SCALE");
    if (!opts.scaleSet && envScale) {
        parse_scale(envScale, &opts.scale);
    }
    const char* envRenderer = getenv("POKEDAISY_RENDERER");
    if (envRenderer && strcmp(envRenderer, "software") == 0) {
        opts.softwareRenderer = true;
    }
    if (!pd_log_open(opts.logPath)) {
        fprintf(stderr, "can't write the log to %s\n", opts.logPath);
    }
    install_crash_handler();
    find_code_base();
    measure_code();

    struct VFile* rom = NULL;
    struct mCore* core = NULL;
    color_t* video = NULL;
    struct pd_render* render = NULL;
    struct pd_audio* audio = NULL;
    struct pd_input* input = NULL;
    struct pd_companion* companion = NULL;
    struct game_mem mem = { 0 };
    char savePath[PD_PATH_MAX] = { 0 };
    char statePath[PD_PATH_MAX] = { 0 };
    char resumePath[PD_PATH_MAX] = { 0 };
    char when[24] = { 0 };
    bool hasState = false;
    bool coreUp = false;
    int ffSpeed = opts.ffSpeed;
    // The scale really on screen. The companion's screen mode wins over --scale
    // once it has opened, and the handheld's launch script passes no --scale at
    // all, so the ini's screen= decides there; a device report has to name that
    // mode, not what the command line said.
    int shownScale = opts.scale;
    int status = 1;

    if (opts.headless) {
        // No window and no sound card: the same loop, into an offscreen surface.
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        pd_logf("SDL didn't start: %s", SDL_GetError());
        goto cleanup;
    }
    mLogSetDefaultLogger(&silentLogger);

    // Started without a game (the port does): let the player pick one.
    char chosen[PD_PATH_MAX];
    if (!opts.rom) {
        if (!render) {
            render = pd_render_open(PD_WINDOW_TITLE, 240, 160, opts.scale, opts.rotation, opts.fullscreen,
                                    opts.headless, opts.softwareRenderer);
            if (!render) {
                goto cleanup;
            }
        }
        if (!input) {
            input = pd_input_open();
            if (!input) {
                pd_logf("out of memory for the input");
                goto cleanup;
            }
        }
        pd_logf("pokedaisy: no game given, listing %s", opts.romsDir);
        if (!pd_pick_rom(opts.romsDir, opts.pick, opts.menuShot, chosen, sizeof(chosen), render, input)) {
            pd_logf("menu: no game picked");
            status = 0;
            goto cleanup;
        }
        opts.rom = chosen;
    }
    pd_logf("pokedaisy: %s", opts.rom);

    rom = VFileOpen(opts.rom, O_RDONLY);
    if (!rom) {
        // Say why: "can't open" alone sent a real user hunting for a typo in a
        // path that was correct (macOS blocks ~/Desktop for the terminal).
        int err = errno;
        struct stat st;
        if (stat(opts.rom, &st) != 0) {
            pd_logf("can't open %s: %s", opts.rom, strerror(err));
        } else if (!S_ISREG(st.st_mode)) {
            pd_logf("can't open %s: not a file", opts.rom);
        } else {
            pd_logf("can't open %s: %s (%lld bytes, but reading it was refused)", opts.rom, strerror(err),
                    (long long) st.st_size);
#ifdef __APPLE__
            if (err == EPERM || err == EACCES) {
                pd_logf("  macOS keeps the terminal out of Desktop, Documents and Downloads: allow it under "
                        "System Settings > Privacy & Security > Files and Folders, or move the game elsewhere");
            }
#endif
        }
        goto cleanup;
    }
    core = mCoreFindVF(rom);
    if (!core) {
        pd_logf("%s isn't a Game Boy Advance game", opts.rom);
        rom->close(rom);
        rom = NULL;
        goto cleanup;
    }
    if (!core->init(core)) {
        pd_logf("the core didn't start");
        goto cleanup;
    }
    coreUp = true;
    mCoreInitConfig(core, "pokedaisy");

    unsigned gw = 240;
    unsigned gh = 160;
    core->desiredVideoDimensions(core, &gw, &gh);
    video = calloc((size_t) gw * gh, sizeof(*video));
    if (!video) {
        pd_logf("out of memory for the screen");
        goto cleanup;
    }
    core->setVideoBuffer(core, video, gw);

    if (!core->loadROM(core, rom)) {
        pd_logf("%s didn't load", opts.rom);
        rom = NULL; // the core owns it now
        goto cleanup;
    }
    rom = NULL;

    if (!pd_save_path(opts.rom, savePath, sizeof(savePath))) {
        pd_logf("can't work out where the save goes: playing without one");
    } else {
        pd_backup_save(savePath);
        struct VFile* save = VFileOpen(savePath, O_CREAT | O_RDWR);
        if (!save) {
            pd_logf("the save %s isn't writable: playing without one", savePath);
        } else if (!core->loadSave(core, save)) {
            pd_logf("couldn't use the save %s", savePath);
        } else {
            pd_logf("save: %s", savePath);
        }
    }

    // The ROM, read into our own buffer: the companion reads the game's tables
    // through it while the game runs, and mGBA's block is not ours to keep (the
    // core rebuilds its ROM mapping, and a pointer kept from startup goes stale
    // - the GUIDE tab then reads a table through it). The size the game is
    // judged by is the file's own, not mGBA's power-of-two padded block.
    struct stat romStat;
    size_t romFileSize = 0;
    if (stat(opts.rom, &romStat) == 0 && romStat.st_size > 0) {
        romFileSize = (size_t) romStat.st_size;
    }
    uint8_t* romData = read_rom_copy(opts.rom, romFileSize);
    if (!romData) {
        pd_logf("couldn't read the ROM: no companion this run");
    }
    mem.ewram = core->getMemoryBlock(core, 0x02, &mem.ewramSize);
    mem.iwram = core->getMemoryBlock(core, 0x03, &mem.iwramSize);
    mem.rom = romData;
    mem.romSize = romData ? romFileSize : 0;

    char settingsPath[PD_PATH_MAX];
    snprintf(settingsPath, sizeof(settingsPath), "pokedaisy.ini");
    if (opts.settingsPath) {
        snprintf(settingsPath, sizeof(settingsPath), "%s", opts.settingsPath);
    } else {
        char stem[PD_PATH_MAX];
        if (pd_path_stem(opts.rom, stem, sizeof(stem))) {
            const char* slash = strrchr(stem, '/');
            if (slash) {
                size_t n = (size_t) (slash - stem);
                if (n >= sizeof(settingsPath) - sizeof("/pokedaisy.ini")) {
                    n = sizeof(settingsPath) - sizeof("/pokedaisy.ini");
                }
                memcpy(settingsPath, stem, n);
                memcpy(settingsPath + n, "/pokedaisy.ini", sizeof("/pokedaisy.ini"));
            }
        }
    }

    if (romData && romFileSize >= 0xC0) {
        companion = pd_companion_open(opts.rom, settingsPath, romData, romFileSize);
    }
    if (!companion) {
        pd_logf("the companion didn't start: the game plays without it");
    } else {
        if (opts.scaleSet) {
            pd_companion_set_screen_mode(companion, screen_mode_for_scale(opts.scale));
        }
        if (!opts.ffSet) {
            ffSpeed = pd_companion_ff_speed(companion);
        }
        if (pd_state_path(opts.rom, statePath, sizeof(statePath))) {
            hasState = pd_state_info(statePath, when, sizeof(when));
            pd_companion_set_state(companion, hasState, when);
        }
    }
    pd_resume_path(opts.rom, resumePath, sizeof(resumePath));

    if (!opts.noAudio) {
        audio = pd_audio_open();
        if (audio) {
            pd_audio_attach(audio, core);
        }
    }

    if (!render) {
        render = pd_render_open(PD_WINDOW_TITLE, (int) gw, (int) gh, opts.scale, opts.rotation, opts.fullscreen,
                                opts.headless, opts.softwareRenderer);
        if (!render) {
            goto cleanup;
        }
    }
    if (companion) {
        shownScale = scale_for_screen_mode(pd_companion_screen_mode(companion));
        pd_render_set_scale(render, shownScale);
    }

    // What a device report needs: the video and audio paths actually in use,
    // the display, and the ROM.
    SDL_version sdlVersion;
    SDL_GetVersion(&sdlVersion);
    char display[64];
    pd_render_display(display, sizeof(display));
    const char* videoDriver = SDL_GetCurrentVideoDriver();
    pd_logf("system: SDL %d.%d.%d, video %s, renderer %s, display %s", sdlVersion.major, sdlVersion.minor,
            sdlVersion.patch, videoDriver ? videoDriver : "?", pd_render_name(render), display);
    pd_logf("system: scale %s (%s), rotation %d, audio %s at %d Hz", scale_name(shownScale),
            companion ? "screen setting" : "command line", opts.rotation,
            audio ? pd_audio_driver(audio) : "none", pd_audio_rate(audio));
    pd_logf("system: %s (%zu bytes, own copy)", opts.rom, romFileSize);
    pd_logf("system: code base 0x%lx (a crash line's off is pc minus this), pid %ld", codeBase, (long) getpid());
    log_modules();

    core->reset(core);

    // Carry on where the player left off - unless they played the game
    // somewhere else since (a newer save beats an older moment).
    if (!opts.noResume && resumePath[0] && pd_resume_is_fresh(resumePath, savePath)) {
        if (pd_state_load(core, resumePath)) {
            pd_logf("resume: carried on from %s", resumePath);
            // C-05: that moment was in the game, so the companion's waiting
            // tabs (title screen) show at once.
            pd_companion_resumed(companion);
        }
    }

    if (opts.fillScreen) {
        // Smoke test: paint the GBA's Mode 3 screen red, so a screenshot shows
        // that the emulator, the PPU, the texture and the scaling all worked.
        for (unsigned i = 0; i < gw * gh; i++) {
            core->busWrite16(core, 0x06000000 + i * 2, 0x001F);
        }
        core->busWrite16(core, 0x04000000, 0x0403); // Mode 3, BG2 on
    }

    if (!input) {
        input = pd_input_open();
        if (!input) {
            pd_logf("out of memory for the input");
            goto cleanup;
        }
    }

    uint64_t counter = SDL_GetPerformanceFrequency();
    double frameTicks = (double) counter / PD_GBA_FPS;
    uint64_t next = SDL_GetPerformanceCounter();
    uint64_t lastTick = next;
    unsigned long long frames = 0;
    bool ffToggled = false;
    bool running = true;
    unsigned quitHeldMs = 0;
    unsigned statsFrames = 0;
    uint64_t statsSince = SDL_GetPerformanceCounter();
    uint64_t emuTicks = 0;
    uint64_t audioTicks = 0;
    uint64_t drawTicks = 0;
    struct test_nav tn = { opts.testNav, 0 };
    if (opts.startCompanion) {
        pd_companion_show(companion, true);
        log_overlay(companion);
    }

    while (running && !pd_input_quit(input)) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            pd_input_event(input, &ev);
        }
        if (pd_input_quit(input)) {
            break;
        }
        pd_input_poll(input);

        // L2 brings the companion up and puts it away; the game keeps running
        // underneath and doesn't see the buttons while it is up.
        if (pd_input_pressed(input, PD_BTN_L2)) {
            pd_companion_toggle(companion);
            log_overlay(companion);
        }
        bool overlay = pd_companion_visible(companion);
        if (overlay) {
            if (pd_input_pressed(input, PD_BTN_START)) {
                pd_companion_show(companion, false);
                log_overlay(companion);
            }
            if (pd_input_pressed(input, PD_BTN_UP)) {
                pd_companion_nav(companion, PD_NAV_UP);
            }
            if (pd_input_pressed(input, PD_BTN_DOWN)) {
                pd_companion_nav(companion, PD_NAV_DOWN);
            }
            if (pd_input_pressed(input, PD_BTN_LEFT)) {
                pd_companion_nav(companion, PD_NAV_LEFT);
            }
            if (pd_input_pressed(input, PD_BTN_RIGHT)) {
                pd_companion_nav(companion, PD_NAV_RIGHT);
            }
            if (pd_input_pressed(input, PD_BTN_A)) {
                pd_companion_nav(companion, PD_NAV_OK);
            }
            if (pd_input_pressed(input, PD_BTN_B)) {
                pd_companion_nav(companion, PD_NAV_BACK);
            }
            if (pd_input_pressed(input, PD_BTN_L1)) {
                pd_companion_nav(companion, PD_NAV_TAB_PREV);
            }
            if (pd_input_pressed(input, PD_BTN_R1)) {
                pd_companion_nav(companion, PD_NAV_TAB_NEXT);
            }
            // Tests: one queued press per frame, so each redraw is used in turn.
            if (opts.testNav && frames >= (unsigned long long) opts.navDelay) {
                enum pd_nav nav;
                if (test_nav_next(&tn, &nav)) {
                    pd_companion_nav(companion, nav);
                }
            }
        }

        uint64_t now = SDL_GetPerformanceCounter();
        unsigned elapsedMs = (unsigned) ((now - lastTick) * 1000 / counter);
        lastTick = now;
        bool ff = ffToggled || pd_input_down(input, PD_BTN_R2);

        // START+SELECT held: leave the game (the combo every emulator here
        // has, and the only way out from the Ports menu).
        if (pd_input_down(input, PD_BTN_START) && pd_input_down(input, PD_BTN_SELECT)) {
            quitHeldMs += elapsedMs;
            if (quitHeldMs >= 1500) {
                pd_logf("pokedaisy: START+SELECT, leaving the game");
                running = false;
                break;
            }
        } else {
            quitHeldMs = 0;
        }

        switch (pd_companion_tick(companion, bus_read, &mem, (unsigned) frames, elapsedMs, ff)) {
        case PD_ACTION_TOGGLE_FF:
            ffToggled = !ffToggled;
            break;
        case PD_ACTION_SAVE_STATE:
            if (pd_state_save(core, statePath)) {
                pd_logf("state: saved %s", statePath);
            }
            hasState = pd_state_info(statePath, when, sizeof(when));
            pd_companion_set_state(companion, hasState, when);
            break;
        case PD_ACTION_LOAD_STATE:
            if (savePath[0]) {
                pd_backup_save(savePath);
            }
            if (pd_state_load(core, statePath)) {
                pd_logf("state: loaded %s", statePath);
            }
            break;
        case PD_ACTION_SETTINGS_CHANGED: {
            pd_companion_save_settings(companion);
            int modeScale = scale_for_screen_mode(pd_companion_screen_mode(companion));
            pd_render_set_scale(render, modeScale);
            // A mode picked in SETTINGS lands in the log too: a run that was
            // started on one SCREEN row and switched mid-game stays readable.
            if (modeScale != shownScale) {
                shownScale = modeScale;
                pd_logf("screen: %s", scale_name(modeScale));
            }
            if (!opts.ffSet) {
                ffSpeed = pd_companion_ff_speed(companion);
            }
            break;
        }
        case PD_ACTION_LEAVE_GAME:
            pd_logf("companion: leaving the game");
            running = false;
            break;
        case PD_ACTION_NONE:
            break;
        }
        if (!running) {
            break;
        }

        ff = ffToggled || pd_input_down(input, PD_BTN_R2);
        pd_audio_set_muted(audio, ff);
        int n = ff ? ffSpeed : 1;
        core->setKeys(core, overlay ? 0 : pd_input_keys(input));
        uint64_t emuStart = SDL_GetPerformanceCounter();
        for (int i = 0; i < n; i++) {
            core->runFrame(core);
        }
        uint64_t emuEnd = SDL_GetPerformanceCounter();
        pd_audio_pump(audio);
        uint64_t audioEnd = SDL_GetPerformanceCounter();

        const uint32_t* ui = overlay ? pd_companion_draw(companion) : NULL;
        pd_render_frame(render, video, ui, PD_COMPANION_W, PD_COMPANION_H);
        uint64_t drawEnd = SDL_GetPerformanceCounter();
        frames += (unsigned long long) n;
        if (opts.testCrash && frames >= 2) {
            pd_logf("pokedaisy: --test-crash, writing through a null pointer");
            volatile int* nullAt = NULL;
            *nullAt = 1;
        }
        if (opts.stats) {
            // What the device test needs: is it holding 60 fps, and is the time
            // going into the emulator or into getting the picture out.
            statsFrames += (unsigned) n;
            emuTicks += emuEnd - emuStart;
            audioTicks += audioEnd - emuEnd;
            drawTicks += drawEnd - audioEnd;
            uint64_t stamp = SDL_GetPerformanceCounter();
            if (stamp - statsSince >= counter) {
                double ms = (double) (stamp - statsSince) * 1000.0 / (double) counter;
                double perFrame = (double) statsFrames;
                pd_logf("stats: %.1f fps, %.2f ms per frame (emu %.2f, audio %.2f, draw %.2f), %d audio frames "
                        "queued",
                        perFrame * 1000.0 / ms, ms / perFrame,
                        (double) emuTicks * 1000.0 / (double) counter / perFrame,
                        (double) audioTicks * 1000.0 / (double) counter / perFrame,
                        (double) drawTicks * 1000.0 / (double) counter / perFrame,
                        pd_audio_queued_frames(audio));
                statsFrames = 0;
                emuTicks = 0;
                audioTicks = 0;
                drawTicks = 0;
                statsSince = stamp;
            }
        }
        if (opts.frames > 0 && frames >= (unsigned long long) opts.frames) {
            break;
        }

        // Pace to the GBA's own frame rate; a slow device falls behind instead
        // of running away to catch up.
        next += (uint64_t) (frameTicks * n);
        now = SDL_GetPerformanceCounter();
        if (now < next) {
            double ms = (double) (next - now) * 1000.0 / (double) counter;
            if (ms >= 1.0) {
                SDL_Delay((Uint32) (ms - 0.5));
            }
            while (SDL_GetPerformanceCounter() < next) {
                SDL_Delay(0);
            }
        } else if (now - next > counter / 4) {
            next = now;
        }
    }
    pd_logf("pokedaisy: %llu frames, %d queued audio frames", frames, pd_audio_queued_frames(audio));

    // What "carry on where you left off" loads next time (never the battery
    // save itself: a state holds the RTC only).
    if (!opts.noResume && resumePath[0] && frames > 0) {
        if (pd_state_save(core, resumePath)) {
            pd_logf("resume: saved %s", resumePath);
        }
    }

    if (opts.screenshot) {
        if (pd_render_screenshot(render, opts.screenshot)) {
            pd_logf("screenshot: %s", opts.screenshot);
        } else {
            pd_logf("couldn't write the screenshot to %s", opts.screenshot);
        }
    }
    status = 0;

cleanup:
    if (companion) {
        pd_companion_close(companion);
    }
    if (input) {
        pd_input_close(input);
    }
    if (render) {
        pd_render_close(render);
    }
    if (audio) {
        pd_audio_close(audio);
    }
    if (core) {
        if (coreUp) {
            // The config first: mCore's deinit frees the core struct itself,
            // and mCoreConfigDeinit reads core->config (use after free).
            mCoreConfigDeinit(&core->config);
            core->deinit(core); // the save goes back through its VFile here
        }
    }
    if (rom) {
        rom->close(rom);
    }
    free(video);
    SDL_Quit();
    pd_log_close();
    return status;
}

