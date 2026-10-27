# Port rules

The rules for this repository live in `AGENTS.md` at the root: which trees are
upstream, how the generated tables are kept in sync, the probe rule for ROM
addresses, the save rules, and how a build reaches the handheld.

Current Cline reads `AGENTS.md` by itself. This file only exists so tools that
look in `.clinerules/` alone are sent there too, instead of a second copy of the
rules that would drift.

Read `AGENTS.md` before changing `3ds/core`, `linux/` or `linux/arkos/`.
