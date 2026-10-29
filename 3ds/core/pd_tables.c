#include "pd_tables.h"

#include <stdio.h>
#include <string.h>

// Upper-cases ASCII letters, leaving é (UTF-8 C3 A9) and the rest alone.
static const char* game_case(const char* src, char* buf, size_t len) {
    size_t i = 0;
    for (; src[i] && i + 1 < len; i++) {
        char c = src[i];
        buf[i] = (c >= 'a' && c <= 'z') ? (char) (c - 'a' + 'A') : c;
    }
    buf[i] = 0;
    return buf;
}

const char* pd_species_name(int species, char* buf, size_t len) {
    if (species == 0) return "-";
    if (species == PD_SPECIES_EGG) return "EGG";
    if (species > 0 && species < pd_species_names_count && pd_species_names[species]) {
        return game_case(pd_species_names[species], buf, len);
    }
    snprintf(buf, len, "#%d", species);
    return buf;
}

const char* pd_move_name(int move, char* buf, size_t len) {
    if (move <= 0) return "-";
    if (move < pd_moves_count && pd_moves[move].name) return game_case(pd_moves[move].name, buf, len);
    snprintf(buf, len, "MOVE#%d", move);
    return buf;
}

const char* pd_type_name(int type) {
    if (type >= 0 && type < pd_type_names_count && pd_type_names[type]) return pd_type_names[type];
    return "";
}

int pd_move_type(int move) {
    if (move > 0 && move < pd_moves_count && pd_moves[move].name) return pd_moves[move].type;
    return PD_TYPE_NONE;
}

int pd_move_power(int move) {
    if (move > 0 && move < pd_moves_count && pd_moves[move].name) return pd_moves[move].power;
    return 0;
}

static int single_pct(int atk, int def) {
    for (int i = 0; i < pd_type_chart_count; i++) {
        if (pd_type_chart[i].atk == atk && pd_type_chart[i].def == def) return pd_type_chart[i].pct;
    }
    return 100;
}

int pd_type_multiplier_pct(int atk, int def1, int def2) {
    int pct = single_pct(atk, def1);
    if (def2 != PD_TYPE_NONE && def2 != def1) pct = pct * single_pct(atk, def2) / 100;
    return pct;
}

void pd_species_type_pair(int species, int* t1, int* t2) {
    *t1 = *t2 = PD_TYPE_NONE;
    if (species <= 0 || species >= pd_species_type_table_count) return;
    *t1 = pd_species_type_table[species][0];
    *t2 = pd_species_type_table[species][1];
}

int pd_exp_for_level(int species, int level) {
    if (level < 0 || level >= PD_EXP_LEVELS) return -1;
    // An unknown species takes the first curve, which is the fastest of the
    // six - a guess that at least doesn't sail past the level.
    int curve = species > 0 && species < pd_growth_rates_count ? pd_growth_rates[species] : 0;
    if (curve >= PD_EXP_CURVES) curve = 0;
    return pd_exp_tables[curve][level];
}
