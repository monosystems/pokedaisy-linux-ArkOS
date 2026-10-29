// MORE: the game's tabs the bar had no room for (the app's MoreScreen, C-02).
// The app puts them on a grid of coloured app icons; the port has no tab
// icons, so they are labelled chips, two a row - 320 pixels are half a phone's
// width. Picking which tabs the bar holds is SETTINGS' TAB BAR page (C-03).
#include "pd_ui_internal.h"

#define TILE_H 30

void ui_more_tab(struct pd_ui* ui, struct pd_canvas* c) {
    int x = MARGIN, w = c->w - 2 * MARGIN, y = CONTENT_Y;
    int f = pd_title_box(c, x, y, w, 24, PD_TITLE_FILL);
    pd_text(c, x + f + 6, pd_text_y(y, 24), "MORE", PD_TITLE_TEXT, PD_TITLE_SHADOW);
    y += 28;
    int lf = pd_list_box(c, x, y, w, c->h - y - MARGIN);
    int ix = x + lf, iy = y + lf, iw = w - 2 * lf;
    int tw = (iw - 3 * MARGIN) / 2;
    for (int i = 0; i < ui->moreCount; i++) {
        int tx = ix + MARGIN + (i % 2) * (tw + MARGIN);
        int ty = iy + MARGIN + (i / 2) * (TILE_H + MARGIN);
        ui_button(ui, c, tx, ty, tw, TILE_H, pd_tab_label(ui->more[i]), HIT_TAB + (int) ui->more[i]);
    }
    // Under the grid, the app's line: it says why these tabs are here instead
    // of on the bar, and the TAB BAR button that picks them. BACK from one of
    // the tabs comes back to this page.
    int ly = iy + MARGIN + (ui->moreCount + 1) / 2 * (TILE_H + MARGIN) + 2;
    int lines = pd_text_wrap(c, ix + MARGIN, ly, iw - 2 * MARGIN, 14, 2,
                             "Tabs that aren't on the tab bar are here.", PD_LABEL, PD_LABEL_SHADOW);
    ui_button(ui, c, ix + MARGIN, ly + lines * 14 + 4, iw - 2 * MARGIN, 24, "TAB BAR", HIT_MORE_TAB_BAR);
}
