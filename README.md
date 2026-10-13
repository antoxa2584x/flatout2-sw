# FlatOut 2 (Xbox) — static recompilation for Nintendo Switch

FlatOut 2 (Xbox, USA/Europe) lifted to C with [xboxrecomp](xboxrecomp/) and
built as a Switch homebrew NRO (Vulkan via mesa NVK) and for Linux.

No game data is included: you need your own disc, unpacked.

## Build

    python3 -m tools.xiso unpack "FlatOut 2 (USA, Europe).iso" -o /root/fo2x/game   # from xboxrecomp/
    tools/regen.sh                                       # lift default.xbe to C in /root/fo2x/gen
    VULKAN=1 bash switch/build.sh                        # -> switch_sd/switch/fo2x/

Copy `switch_sd/switch/fo2x/` (NRO + `game/`) to `sdmc:/switch/fo2x/` and
start it from hbmenu with title takeover (hold R on a game).

Linux: `cmake -S . -B build -DNFSU2_GEN_DIR=/root/fo2x/gen -DNFSU2_VULKAN=ON && cmake --build build`.

See CLAUDE.md for the port's notes.
