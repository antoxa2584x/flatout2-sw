# FlatOut 2 (Xbox) static recompilation — notes for Claude

Xbox FlatOut 2 (USA/Europe, title 0x454D0020, XDK 5849, Bugbear engine with
Lua 5.1 scripts, FMOD on DirectSound, XMV movies), lifted to C with xboxrecomp
and built for Linux and Nintendo Switch (libnx NRO, Vulkan). Started
2026-10-13 from the Burnout 3 port (`..\burnout3-xbox`, newest toolkit), with
the library hooks taken from NFS Carbon (`..\nfsc-xbox`): FlatOut 2 links the
same non-LTCG D3D8/DSOUND/XPP/CRT 5849 code as Carbon, byte for byte apart
from relocations. Read `..\nfsu2-xbox\CLAUDE.md` for everything the runtime
does on Horizon. Repo: git@github.com:antoxa2584x/flatout2-sw.git (`main`,
commits as `Anton Artemov <antoxa2584@gmail.com>`).

## Rules

- Never commit game data (disc, `default.xbe`, `switch_sd/`) or generated C
  (`gen/`). Ask before committing or pushing.
- The Switch build reads the **unpacked** disc at `sdmc:/switch/fo2x/game/`.
- Renderer work targets Vulkan (nv2a_vk). Verify in `build-vk-linux`
  (lavapipe, headless); GL (`build-linux`) is kept building only.
- Linux test hygiene: one run at a time, kill by PID (`run/pid`) with -9.

## Layout and builds

| Where | What |
|---|---|
| `/root/fo2x/` (WSL) | `game/` extracted disc, `gen/` lifted C, `build-vk-linux/`, `build-linux/` (GL), `build-abi/` (`-DRECOMP_ABI_CHECK`), `build-switch-vk/`, `run/` |
| `/root/fo2x/iter.sh` | regen (`FULL=1` = with disasm; default lift only) + stub tracing + vk-linux build |
| `/root/fo2x/live.sh` | start with a live pad file `run/pad.cmd` (`VK=1` lavapipe, `BIN=`, `EXTRA="VAR=1 ..."`), dumps every `DUMPN` frames to `run/d/` |
| `/root/fo2x/boot.sh` | live.sh + wait for the save scan + Start presses (`VK=` empty and `BIN=build-linux/...` for GL) |
| `/root/fo2x/nav.sh [steps]` | boot.sh, title -> profile -> main menu, then pad steps (`right:200 a:200 x5` ~ a race; the Single Player cursor is remembered, so check the frame) |
| `/root/fo2x/step.sh "<pad>" N` | press, wait for N new dumps, newest frame -> `run/last.png` |
| `/root/fo2x/tools/match.py A.xbe B.xbe VA..` | find A's functions in B (immediates masked): how the library hooks were mapped |
| `/root/fo2x/tools/dasm.py VA [n]` | capstone disassembly of the XBE |
| `/root/fo2x/peek.py PID ADDR` | read guest memory of a running build |

- Disc: `xboxrecomp\FlatOut 2 (USA, Europe) (En,Fr,De,Es,It).iso`, unpacked with `python3 -m tools.xiso unpack ISO -o /root/fo2x/game`.
- Regenerate: `tools/regen.sh` (`LIFT_ONLY=1` after recomp_manual.c edits; `TRACE_FUNCS=<json>`).
- Seeds: `config/seed_functions.json`; grow with `python3 -m tools.seed_from_log run.log default.xbe --functions tools/disasm/output/functions.json --seeds ../config/seed_functions.json` (from `xboxrecomp/`).
- Switch: `VULKAN=1 JOBS=8 bash switch/build.sh` -> `switch_sd/switch/fo2x/fo2x-vulkan.nro` + `game/` (UDATA/TDATA/CACHE excluded).
- Saves on Linux land in `/root/fo2x/game/UDATA/454d0020/` (profile "PLAYER").

## Findings

- **Library hooks (Carbon -> FlatOut 2):** memmove 0x32C800/0x32ED70 ->
  0x218550/0x21BFD0, _ftol2 -> 0x218FD4, BlockOnTime 0x35A0C0 -> 0x240240
  (device ptr 0x24C9D8), vblank 0x362050 -> 0x242AF0, PGRAPH 0x3625C0 ->
  0x243060, flip queue -> 0x242DF0, DSP ack 0x39406B -> 0x252E89, EP watchdog
  0x3947ED -> 0x25360B (check 0x25384B, ptr 0x26CF50, skipped return
  0x2538EF), AC97 reset 0x399A64 -> 0x2596A8 (table 0x26C8F8, lock
  0x24EF10/0x24EF32, re-arm 0x2594B2). XMV YUV converter (Burnout 3's
  0x2CD892) -> 0x2A5F31. Entry 0x13216D. XPP seeds from Burnout 3/Carbon.
- **cpuid was a no-op (toolkit lifter, test_cpuid.py):** the memcpy
  dispatcher 0x1C8560 stores the SSE copy 0x1C8380 in [0x32DA78] only if the
  feature bits say SSE, else 0; every Lua chunk read through the null
  pointer broke the stack and Lua's GC walked a null state forever. cpuid now
  answers as the Xbox's Pentium III (family 6 model 8, SSE, no SSE2). The SSE
  copy itself (computed jumps into movsb runs) is replaced by host memmove.
- **Movie stream never primed (src/recomp_manual.c, 0xE01D0):** six 256 KB
  overlapped reads; "primed" = cursor on slot 5, which only happens while a
  DVD read is in flight. Reads complete at once here, so the ring wrapped
  full; full ring now counts as primed.
- **PVIDEO overlay (toolkit):** XMV intros play through the video overlay
  (D3D UpdateOverlay 0x249200, EnableOverlay(FALSE) 0x249380 spins on
  PVIDEO_BUFFER after STOP). xbox_memory_layout.c pvideo_tick honours STOP;
  nv2a_vk.c nv2a_vk_overlay_tick converts the YUY2 frame and presents it
  when the title has not flipped for 100 ms (intros play at ~60 fps).
- **GetExitCodeThread always "exited" (toolkit kernel_bridge.c):** the KTHREAD
  handed out was the TIB, whose +4 (SignalState) is the TLS pointer. Each
  movie's decoder thread was told to stop and never waited for: four zombie
  decoders by the title screen, random crashes (DirectSound div by zero on a
  null voice, heap corruption at race load). Threads now get a real
  dispatcher-shaped object (signalled + ExitStatus at +0x120 on exit).
- **Lone int3 inside functions (toolkit disasm, test_cc_boundary.py):** this
  compiler leaves single int3 bytes after jmp/ret inside functions;
  cc_boundary split sub_000709E0 there and its switch epilogue became an
  empty stub. Padding must now end 4-aligned and not on a jcc target.
- **fcmovcc dropped (toolkit lifter, test_fcmov.py):** float min/max/clamp in
  the math library were comments; the cars drove upside down (world flipped,
  sky fine). fcmov* now use the last compare's flags; fxtract implemented
  (fast log2 at 0x15AA70).
- **Teal car shadows (toolkit nv2a_vk vk_clear):** the shadow target is
  R5G6B5, cleared to 0xFFFF; the clear value is in the surface's format.
- **Distorted sound in races (src/main.c RECOMP_APU_DOWNMIX, toolkit
  apu_dsp.c):** FMOD loads its own GP DSP image (Data/Sound/fmodfx.bin);
  the runtime's DSP is a passthrough that sums mixbins to stereo. Race
  effects go to bins 6/7 (RMS ~0.28) plus the same signal at half level to
  the reverb send 10; music to 0/1 (RMS ~0.04). Summed dry, the console mix
  hit full scale (peak 32739 in [perf] audio out) and the final tanh
  crushed it. FlatOut 2's gains: 6/7 x0.5, 8/9 x0.35, 10 off.
  `RECOMP_APU_BIN_STATS=1` prints per-bin RMS/peak every 5 s.
- **Aspect:** the title draws the same 4:3 frame with or without the
  widescreen flag, so main.c sets RECOMP_WIDESCREEN=0 (4:3, full height);
  stretching to 16:9 widened everything.
- **Switch profiles (src/switch_nx.c):** clocks boosted while docked only
  (CPU 1785 / GPU 768 / memory 1600 MHz; FO2_DOCK_BOOST=0 off, NFSU2_*_MHZ
  in fo2x_env.txt = always those); render scale per mode from
  sdmc:/switch/fo2x/fo2x_options.txt (scale_docked=200, scale_handheld=150,
  aniso=16, fxaa=1; RECOMP_GL_SCALE overrides). Logged as [options]/[mode].
- Shader/pipeline caches on the Switch were read from sdmc:/switch/b3x/
  (hard-coded in the toolkit); now sdmc:/switch/fo2x/.
- Debug aids added: FRAME_TRACE prints texture formats/control0 per stage;
  `RECOMP_VK_SKIP_BLEND=<hex dfactor>` drops blended draws with that factor.

## Status (2026-10-13)

- Linux Vulkan (lavapipe): intro movies, title, profile create/save/load,
  menus with video backgrounds and music, car select garage, Quick Race and
  Career races render and drive (~15-30 fps on lavapipe).
- Switch: `fo2x-vulkan.nro` built and staged with the disc; not run on Eden or
  hardware yet. Icon from the title screen; loader logo still NFSU2's.
- Not done: no Switch GL NRO; RECOMP_CULL is off (Burnout setting, unchecked
  here); colour-keyed overlay over flipped frames not composited.
