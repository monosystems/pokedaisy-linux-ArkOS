#include "pd_dex.h"

#include <stdio.h>
#include <string.h>

#include "pd_map.h"
#include "pd_snapshot.h"
#include "pd_tables.h"

#define SPECIES_COUNT 412

static uint16_t u16(const uint8_t* p) {
    return (uint16_t) (p[0] | p[1] << 8);
}

static uint32_t u32(const uint8_t* p) {
    return (uint32_t) p[0] | (uint32_t) p[1] << 8 | (uint32_t) p[2] << 16 | (uint32_t) p[3] << 24;
}

// species -> national and back, built once per ROM.
static unsigned tablesRom;
static uint16_t toNational[SPECIES_COUNT];
static uint16_t toSpecies[PD_NATIONAL_COUNT + 1];
static bool available;

static void build(const struct pd_game* g) {
    if (tablesRom == pd_rom_id(g)) return;
    tablesRom = pd_rom_id(g);
    available = false;
    memset(toNational, 0, sizeof(toNational));
    memset(toSpecies, 0, sizeof(toSpecies));
    const struct pd_config* cfg = g->cfg;
    if (!cfg || !cfg->dexEntries || !g->rom) return;
    const uint8_t* nat = pd_rom_at(g, cfg->speciesToNational, (SPECIES_COUNT - 1) * 2);
    const uint8_t* e1 = pd_rom_at(g, cfg->dexEntries + cfg->dexEntryStride, cfg->dexEntryStride);
    if (!nat || !e1) return;
    char category[24];
    pd_gen3_text(e1, 12, category, sizeof(category));
    if (strcmp(category, "SEED") || u16(e1 + 0x0C) != 7 || u16(e1 + 0x0E) != 69 || u16(nat) != 1) return;
    for (int s = 1; s < SPECIES_COUNT; s++) {
        int n = u16(nat + (s - 1) * 2);
        toNational[s] = (uint16_t) n;
        // The lowest species id wins (the app's inverse).
        if (n >= 1 && n <= PD_NATIONAL_COUNT && !toSpecies[n]) toSpecies[n] = (uint16_t) s;
    }
    available = true;
}

bool pd_dex_available(const struct pd_game* g) {
    build(g);
    return available;
}

int pd_dex_species(const struct pd_game* g, int national) {
    build(g);
    return available && national >= 1 && national <= PD_NATIONAL_COUNT ? toSpecies[national] : 0;
}

int pd_dex_national(const struct pd_game* g, int species) {
    build(g);
    return available && species >= 1 && species < SPECIES_COUNT ? toNational[species] : 0;
}

int pd_dex_regional(const struct pd_game* g, int i) {
    const struct pd_config* cfg = g->cfg;
    if (!cfg || i < 1 || i > cfg->regionalCount) return 0;
    if (!cfg->regionalOrder) return i; // Kanto: national 1..151
    const uint8_t* p = pd_rom_at(g, cfg->regionalOrder + (uint32_t) (i - 1) * 2, 2);
    return p ? u16(p) : 0;
}

bool pd_dex_entry(const struct pd_game* g, int national, struct pd_dex_entry* out) {
    memset(out, 0, sizeof(*out));
    const struct pd_config* cfg = g->cfg;
    if (!pd_dex_available(g)) return false;
    const uint8_t* e = pd_rom_at(g, cfg->dexEntries + (uint32_t) national * cfg->dexEntryStride, cfg->dexEntryStride);
    if (!e) return false;
    out->national = national;
    out->species = pd_dex_species(g, national);
    pd_gen3_text(e, 12, out->category, sizeof(out->category));
    out->heightDm = u16(e + 0x0C);
    out->weightHg = u16(e + 0x0E);
    const uint8_t* d = pd_rom_at(g, u32(e + 0x10), 200);
    if (d) pd_gen3_text(d, 200, out->description, sizeof(out->description));

    const uint8_t* b = pd_rom_at(g, cfg->speciesInfo + (uint32_t) out->species * 28, 28);
    if (b) {
        // gBaseStats: HP ATK DEF SPEED SP.ATK SP.DEF; shown with SPEED last.
        static const int ORDER[6] = { 0, 1, 2, 4, 5, 3 };
        for (int i = 0; i < 6; i++) out->stats[i] = b[ORDER[i]];
        out->type1 = b[6];
        out->type2 = b[7];
        int a1 = b[0x16], a2 = b[0x17];
        const uint8_t* n1 = a1 ? pd_rom_at(g, cfg->abilityNames + (uint32_t) a1 * 13, 13) : NULL;
        const uint8_t* n2 = a2 && a2 != a1 ? pd_rom_at(g, cfg->abilityNames + (uint32_t) a2 * 13, 13) : NULL;
        if (n1) pd_gen3_text(n1, 13, out->ability1, sizeof(out->ability1));
        if (n2) pd_gen3_text(n2, 13, out->ability2, sizeof(out->ability2));
    }
    return true;
}

bool pd_dex_sprite(const struct pd_game* g, int species, uint32_t* out) {
    const struct pd_config* cfg = g->cfg;
    if (!pd_dex_available(g) || species < 1 || species >= SPECIES_COUNT) return false;
    const uint8_t* pic = pd_rom_at(g, cfg->frontPics + (uint32_t) species * 8, 8);
    const uint8_t* pal = pd_rom_at(g, cfg->palettes + (uint32_t) species * 8, 8);
    if (!pic || !pal) return false;
    const uint8_t* picLz = pd_rom_at(g, u32(pic), 4);
    const uint8_t* palLz = pd_rom_at(g, u32(pal), 4);
    if (!picLz || !palLz) return false;
    // The decoded sizes come from the LZ77 headers: one 64x64 frame is 2048
    // bytes (Emerald's pics hold two frames; the first is used).
    size_t picSize = picLz[1] | (size_t) picLz[2] << 8 | (size_t) picLz[3] << 16;
    size_t palSize = palLz[1] | (size_t) palLz[2] << 8 | (size_t) palLz[3] << 16;
    if (picSize < 2048 || picSize > 8192 || palSize < 32 || palSize > 64) return false;
    static uint8_t tiles[8192], colours[64];
    size_t picAvail = g->romSize - (size_t) (picLz - g->rom), palAvail = g->romSize - (size_t) (palLz - g->rom);
    if (pd_lz77(picLz, picAvail, tiles, picSize) != picSize || pd_lz77(palLz, palAvail, colours, palSize) != palSize) {
        return false;
    }
    uint32_t rgb[16];
    for (int i = 0; i < 16; i++) {
        uint16_t c = (uint16_t) (colours[i * 2] | colours[i * 2 + 1] << 8);
        uint32_t r = c & 0x1F, gg = (c >> 5) & 0x1F, bb = (c >> 10) & 0x1F;
        rgb[i] = (r << 3 | r >> 2) << 16 | (gg << 3 | gg >> 2) << 8 | (bb << 3 | bb >> 2);
    }
    for (int y = 0; y < PD_SPRITE; y++) {
        for (int x = 0; x < PD_SPRITE; x++) {
            int tile = (y / 8) * 8 + x / 8;
            uint8_t v = tiles[tile * 32 + (y % 8) * 4 + (x % 8) / 2];
            int c = x & 1 ? v >> 4 : v & 0xF;
            out[y * PD_SPRITE + x] = c ? rgb[c] : 0xFF000000u;
        }
    }
    return true;
}

// --- EVOLVE (the app's EvolutionSource / DexDetails.family) ------------------
//
// gEvolutionTable: 5 rows of {u16 method, u16 param, u16 target, pad} per
// species. Read whole, once per ROM (412 * 5 * 8 bytes), and checked with the
// app's probe (BULBASAUR evolves at LEVEL 16 into IVYSAUR) so a hack that moved
// the table never gets wrong data.
#define EVO_PER_MON 5
#define EVO_ROW 8
#define EVO_LEVEL 4
#define EVO_FRIENDSHIP 1
#define EVO_FRIENDSHIP_DAY 2
#define EVO_FRIENDSHIP_NIGHT 3
#define EVO_TRADE 5
#define EVO_TRADE_ITEM 6
#define EVO_ITEM 7
#define EVO_LEVEL_ATK_GT_DEF 8
#define EVO_LEVEL_ATK_EQ_DEF 9
#define EVO_LEVEL_ATK_LT_DEF 10
#define EVO_LEVEL_SILCOON 11
#define EVO_LEVEL_CASCOON 12
#define EVO_LEVEL_NINJASK 13
#define EVO_LEVEL_SHEDINJA 14
#define EVO_BEAUTY 15
#define EVO_FAMILY_MEMBERS 40 // the app's cap on a family walk

static unsigned evoRom;
static uint16_t evoTable[SPECIES_COUNT][EVO_PER_MON][3];
static bool evoOk;

static void build_evolutions(const struct pd_game* g) {
    if (evoRom == pd_rom_id(g)) return;
    evoRom = pd_rom_id(g);
    evoOk = false;
    memset(evoTable, 0, sizeof(evoTable));
    const struct pd_config* cfg = g->cfg;
    if (!cfg || !cfg->evolutions || !pd_dex_available(g)) return;
    const uint8_t* t = pd_rom_at(g, cfg->evolutions, SPECIES_COUNT * EVO_PER_MON * EVO_ROW);
    if (!t) return;
    for (int s = 0; s < SPECIES_COUNT; s++) {
        for (int i = 0; i < EVO_PER_MON; i++) {
            const uint8_t* r = t + (s * EVO_PER_MON + i) * EVO_ROW;
            evoTable[s][i][0] = u16(r);     // method
            evoTable[s][i][1] = u16(r + 2); // param
            evoTable[s][i][2] = u16(r + 4); // target
        }
    }
    evoOk = evoTable[1][0][0] == EVO_LEVEL && evoTable[1][0][1] == 16 && evoTable[1][0][2] == 2;
}

bool pd_dex_has_evolutions(const struct pd_game* g) {
    build_evolutions(g);
    return evoOk;
}

// The app's `link()`: Megas / EVO_NONE (0xF0 and up), no target and the
// species itself are not links; a method past EVO_BEAUTY is a special one.
static bool evo_link(int from, int i, struct pd_evo_link* out) {
    int method = evoTable[from][i][0], param = evoTable[from][i][1], target = evoTable[from][i][2];
    if (method == 0 || method >= 0xF0 || target == 0 || target == from || target >= SPECIES_COUNT) return false;
    out->from = from;
    out->to = target;
    out->method = method;
    out->param = param;
    return true;
}

static int evo_outgoing(int s, struct pd_evo_link* out) {
    int n = 0;
    for (int i = 0; i < EVO_PER_MON; i++) {
        if (evo_link(s, i, &out[n])) n++;
    }
    return n;
}

static bool evo_parent(int s) {
    struct pd_evo_link l;
    for (int from = 1; from < SPECIES_COUNT; from++) {
        for (int i = 0; i < EVO_PER_MON; i++) {
            if (evo_link(from, i, &l) && l.to == s) return true;
        }
    }
    return false;
}

static bool evo_same(const struct pd_evo_link* a, const struct pd_evo_link* b) {
    return a->from == b->from && a->to == b->to && a->method == b->method && a->param == b->param;
}

int pd_dex_family(const struct pd_game* g, int species, struct pd_evo_link* out, int max) {
    build_evolutions(g);
    if (!evoOk || max <= 0) return -1;
    if (species < 1 || species >= SPECIES_COUNT) return 0;

    // The connected family: everything reachable both ways, capped like the
    // app (a corrupted table must not walk forever).
    static bool member[SPECIES_COUNT];
    static int queue[EVO_FAMILY_MEMBERS];
    static struct pd_evo_link links[EVO_PER_MON];
    memset(member, 0, sizeof(member));
    int head = 0, tail = 0, members = 0;
    member[species] = true;
    queue[tail++] = species;
    members++;
    while (head < tail && members < EVO_FAMILY_MEMBERS) {
        int s = queue[head++];
        for (int from = 1; from < SPECIES_COUNT && members < EVO_FAMILY_MEMBERS; from++) {
            for (int i = 0; i < EVO_PER_MON; i++) {
                struct pd_evo_link l;
                if (evo_link(from, i, &l) && l.to == s && !member[from]) {
                    member[from] = true;
                    queue[tail++] = from;
                    members++;
                }
            }
        }
        int n = evo_outgoing(s, links);
        for (int i = 0; i < n && members < EVO_FAMILY_MEMBERS; i++) {
            if (!member[links[i].to]) {
                member[links[i].to] = true;
                queue[tail++] = links[i].to;
                members++;
            }
        }
    }

    // Chain order: walk from the family's roots, breadth first (the app's
    // familyOf), so BULBASAUR -> IVYSAUR comes before IVYSAUR -> VENUSAUR.
    int count = 0;
    static bool seen[SPECIES_COUNT];
    static int walk[EVO_FAMILY_MEMBERS];
    memset(seen, 0, sizeof(seen));
    head = tail = 0;
    for (int i = 0; i < members; i++) {
        if (!evo_parent(queue[i])) walk[tail++] = queue[i];
    }
    if (tail == 0) walk[tail++] = species;
    while (head < tail) {
        int s = walk[head++];
        if (seen[s]) continue;
        seen[s] = true;
        int n = evo_outgoing(s, links);
        for (int i = 0; i < n && count < max; i++) {
            bool dup = false;
            for (int j = 0; j < count; j++) {
                if (evo_same(&out[j], &links[i])) dup = true;
            }
            if (!dup) out[count++] = links[i];
            if (tail < EVO_FAMILY_MEMBERS) walk[tail++] = links[i].to;
        }
    }
    return count;
}

const char* pd_evo_how(const struct pd_game* g, int method, int param, char* buf, size_t len) {
    bool level = param >= 1 && param <= 100;
    const char* item;
    switch (method) {
    case EVO_FRIENDSHIP: snprintf(buf, len, "Level up, high friendship"); break;
    case EVO_FRIENDSHIP_DAY: snprintf(buf, len, "Level up, high friendship, by day"); break;
    case EVO_FRIENDSHIP_NIGHT: snprintf(buf, len, "Level up, high friendship, at night"); break;
    case EVO_LEVEL:
    case EVO_LEVEL_NINJASK: snprintf(buf, len, level ? "LV %d" : "Level up", param); break;
    case EVO_LEVEL_ATK_GT_DEF:
        snprintf(buf, len, level ? "LV %d, ATTACK > DEFENSE" : "Level up, ATTACK > DEFENSE", param);
        break;
    case EVO_LEVEL_ATK_EQ_DEF:
        snprintf(buf, len, level ? "LV %d, ATTACK = DEFENSE" : "Level up, ATTACK = DEFENSE", param);
        break;
    case EVO_LEVEL_ATK_LT_DEF:
        snprintf(buf, len, level ? "LV %d, ATTACK < DEFENSE" : "Level up, ATTACK < DEFENSE", param);
        break;
    case EVO_LEVEL_SILCOON:
    case EVO_LEVEL_CASCOON:
        snprintf(buf, len, level ? "LV %d, by chance" : "Level up, by chance", param);
        break;
    case EVO_LEVEL_SHEDINJA:
        snprintf(buf, len, level ? "LV %d, with a spare slot" : "Level up, with a spare slot", param);
        break;
    case EVO_TRADE: snprintf(buf, len, "Trade"); break;
    case EVO_TRADE_ITEM:
        item = pd_item_name(g, param);
        snprintf(buf, len, item && *item ? "Trade holding %s" : "Trade holding an item", item ? item : "");
        break;
    case EVO_ITEM:
        item = pd_item_name(g, param);
        snprintf(buf, len, item && *item ? "Use %s" : "Use an item", item ? item : "");
        break;
    case EVO_BEAUTY: snprintf(buf, len, "Level up, BEAUTY %d+", param); break;
    default: snprintf(buf, len, "Special condition"); break;
    }
    return buf;
}

void pd_dex_height(int dm, char* buf, int len) {
    long inches = 10000L * dm / 254;
    if (inches % 10 >= 5) inches += 10;
    long feet = inches / 120, rest = (inches - feet * 120) / 10;
    snprintf(buf, (size_t) len, "%ld'%02ld\"", feet, rest);
}

void pd_dex_weight(int hg, char* buf, int len) {
    long lbs = 100000L * hg / 4536;
    if (lbs % 10 >= 5) lbs += 10;
    snprintf(buf, (size_t) len, "%ld.%ld lbs.", lbs / 100, (lbs % 100) / 10);
}
