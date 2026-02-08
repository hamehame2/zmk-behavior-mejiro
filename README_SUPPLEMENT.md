# zmk-behavior-mejiro (starter zip)

This zip is a **starter module** based on your working ZMK Naginata module.

## What is included
- `src/` / `include/` : the current **ZMK Naginata behavior** sources (as uploaded)
- `dts/` : behavior binding and dtsi (`&ng` compatible stays as-is for now)
- `reference/qmk_mejiro/` : your **QMK Mejiro** sources (reference only, not compiled yet)
- `reference/plover_mejiro/` : your **Plover Mejiro** sources (reference only)
- `tools/` : generator scripts (as uploaded)

## Next steps (planned)
1) Change `behavior_naginata.c` to **finalize on release only** (disable press-time commit).
2) Add a new `src/mejiro_transform_zmk.c` (or replace outfunc) that calls the QMK transform logic,
   then maps the result to ZMK output (send-string / key presses).
3) Later rename `&ng` -> `&mg` if desired (after it works).

Generated on: 2026-02-08T13:05:11
