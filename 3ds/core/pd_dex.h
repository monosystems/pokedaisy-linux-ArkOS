// The POKéDEX's ROM side (the app's Pokedex.kt): the species <-> national
// number tables, dex entries, base stats and the front sprite, read from the
// player's ROM; the save side (seen / caught) is in the snapshot.
#ifndef PD_DEX_H
#define PD_DEX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pd_game.h"

#define PD_SPRITE 64

// The longest family the EVOLVE section shows: Eevee's and the like (the app's
// MAX_FAMILY_LINKS).
#define PD_EVO_MAX 24

struct pd_dex_entry {
    int national, species;
    char category[24]; // "SEED"
    int heightDm, weightHg;
    char description[240];
    int type1, type2;
    int stats[6]; // HP, ATK, DEF, SP.ATK, SP.DEF, SPEED (the app's order)
    char ability1[16], ability2[16];
};

// One link of a family: [from] becomes [to] by [method] with [param] (both
// species ids, see the EVO_* constants in pokemon.h - the app's Evolutions.kt).
struct pd_evo_link {
    int from, to, method, param;
};

// Whether g's ROM has this port's POKéDEX tables (the app's probe: entry 1
// is SEED, 7 dm, 69 hg; species 1 is national 1).
bool pd_dex_available(const struct pd_game* g);
// The species for a national number (0 = none), and back.
int pd_dex_species(const struct pd_game* g, int national);
int pd_dex_national(const struct pd_game* g, int species);
// The national number at regional position i (1-based); 0 = none.
int pd_dex_regional(const struct pd_game* g, int i);
bool pd_dex_entry(const struct pd_game* g, int national, struct pd_dex_entry* out);
// Whether the ROM has the evolution table, checked the app's way (species 1
// evolves by LEVEL 16 into species 2) so a moved table is never used.
bool pd_dex_has_evolutions(const struct pd_game* g);
// Every link in [species]' family - what it comes from and what it becomes,
// both ways, in chain order (the app's DexDetails.family). Returns the count
// (0 = it doesn't evolve, -1 = no table).
int pd_dex_family(const struct pd_game* g, int species, struct pd_evo_link* out, int max);
// An evolution's requirements in plain words: "LV 16", "Trade holding METAL
// COAT", "Level up, high friendship, by day" (the app's evolutionHow).
const char* pd_evo_how(const struct pd_game* g, int method, int param, char* buf, size_t len);
// The front sprite, 64x64 0xRRGGBB with PD_MAP_CLEAR-style see-through
// (0xFF000000); false if it can't be decoded.
bool pd_dex_sprite(const struct pd_game* g, int species, uint32_t* out);
// "2'04\"" and "15.2 lbs." - the app's imperial conversions.
void pd_dex_height(int dm, char* buf, int len);
void pd_dex_weight(int hg, char* buf, int len);

#endif
