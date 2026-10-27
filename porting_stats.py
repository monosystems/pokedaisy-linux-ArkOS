#!/usr/bin/env python3
"""Count how much of the app is ported, from PORTING.md.

The number in README.md is this script's TOTAL line: a feature finished on both
hosts counts 1, a partial one (one host only, or narrower than the app) counts
0.5, and rows marked not applicable are left out. Run it after changing a row in
PORTING.md and paste the table in. Rows that are not player features are skipped:
the data pipeline (D-), the phone checks (V-) and the save editor (SE-), which
upstream keeps behind a flag that 1.3.2 does not ship.
"""

import collections
import re
import sys

AREAS = [
    ("host", "Emulation, saves, speed, screen, input"),
    ("companion", "The companion's tabs"),
    ("overlay", "Over the game: HUD, quick menu, battle buttons, makeover"),
    ("library", "Library, cheats and the other app surfaces"),
    ("support", "Game and language support"),
]


def area_of(feature_id):
    prefix = feature_id.split("-")[0]
    return {
        "H": "host",
        "C": "companion",
        "P": "overlay",
        "L": "library",
        "X": "library",
        "O": "library",
        "S": "support",
    }.get(prefix)


def rows(path):
    for line in open(path, encoding="utf-8"):
        match = re.match(r"^\\| ([A-Z][A-Z0-9-]*) \\|", line)
        if not match:
            continue
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) != 6 or cells[0] == "ID":
            continue
        yield cells[0], cells[3], cells[4]


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "PORTING.md"
    seen = collections.defaultdict(lambda: [0, 0, 0, 0])  # rows, done, partial, missing
    skipped = not_applicable = 0
    for feature_id, linux, handheld in rows(path):
        area = area_of(feature_id)
        if area is None:
            skipped += 1
            continue
        if linux == "n/a" or handheld == "n/a":
            not_applicable += 1
            continue
        counts = seen[area]
        counts[0] += 1
        if linux == "✅" and handheld == "✅":
            counts[1] += 1
        elif linux == "—" and handheld == "—":
            counts[3] += 1
        else:
            counts[2] += 1

    total = [0, 0, 0, 0]
    print("| Area | Features | Ported |")
    print("| --- | ---: | ---: |")
    for key, label in AREAS:
        n, done, partial, missing = seen[key]
        for i, value in enumerate((n, done, partial, missing)):
            total[i] += value
        share = (done + partial / 2) / n * 100 if n else 0.0
        print(f"| {label} | {n} | {share:.0f}% |")

    n, done, partial, missing = total
    share = (done + partial / 2) / n * 100 if n else 0.0
    print()
    print(f"features {n}, finished {done}, partial {partial}, missing {missing}, "
          f"not applicable {not_applicable}, skipped {skipped}")
    print(f"ported {share:.0f}%")


if __name__ == "__main__":
    main()
