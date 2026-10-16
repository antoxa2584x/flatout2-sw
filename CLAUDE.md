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
- **Engine/crash sounds were noise (toolkit kernel_memory.c):** FMOD asks
  MmGetPhysicalAddress for its sample banks (heap, 0x0157xxxx) before the
  contiguous arena hands out that physical range, so the low-VA tag
  (XBOX_PHYS_LOWVA_TAG, Burnout 3's fix) was not applied and the APU read the
  arena: every ADPCM effect voice decoded as full-scale noise (bad block
  headers, peak 32739 on the console). Every VA below 64 MB is now tagged.
  Check: tools/adpcm.py <va> decodes guest memory as Xbox ADPCM (bad headers
  / spectral flatness). RECOMP_APU_DOWNMIX="10:0.25:0.25" keeps a little of
  FMOD's reverb send (6/7 effects, 0/1 music); RECOMP_APU_BIN_STATS=1 prints
  per-bin RMS/peak.
- **Aspect:** widescreen on (16:9 stretch, as a 16:9 TV shows the Xbox);
  the title screen is the same frame either way. RECOMP_WIDESCREEN=0 = 4:3.
- **Switch profiles (src/switch_nx.c):** clocks from fo2x_env.txt
  (switch/fo2x_env.txt, staged by build.sh when the SD has none): CPU 1785
  / GPU 768 / memory 1600 MHz in both modes; FO2_DOCK_BOOST=1 instead =
  docked only; render scale per mode from
  sdmc:/switch/fo2x/fo2x_options.txt (scale_docked=200, scale_handheld=150,
  aniso=16, fxaa=1; RECOMP_GL_SCALE overrides). Logged as [options]/[mode].
- Shader/pipeline caches on the Switch were read from sdmc:/switch/b3x/
  (hard-coded in the toolkit); now sdmc:/switch/fo2x/.
- **Options menu hang (seeds):** the Options handler 0x5D960 follows a
  jump table that decodes as junk, so no ret/jmp precedes its int3 padding
  and cc_boundary never fired: unresolved ICALL, the menu hung. Seeded with
  the 6 other uncovered 16-aligned starts after int3 runs (0x5DB60,
  0x1107C0, 0x117900, 0x161B70, 0x169BC0, 0x1AFC00).
- **Frame pacing:** the renderer waits once per frame (sub_001BEFD6 ->
  BlockOnFence) on the older of two fences ([0x42C3DC + idx*4]), i.e. two
  frames in flight already; on the console "GL thread idle 0%" -- the
  executor/Vulkan side is the limit, not game code (its Linux profile is
  flat: top lifted function sub_001D93F0 ~10% of game CPU). FO2_FENCE_STATS=1
  logs fence waits per caller every 10 s. Console profile 2026-10-13
  (dc83957, docked clocks): executor waits ~29% on flip retirement (not
  saturated), RECOMP_CULL cost 10.4% vs vk_draw_raw 6.6% -> off again;
  overlay presenter 10.7% (re-converted every 16 ms) -> only new frames.
  Game thread: real work spread thin (collision sub_001D93F0 5%, physics
  under sub_0007E6E0), ~20% lock/spin syscalls in the BlockOnTime wait,
  __aarch64_read_tp 4% (TLS guest registers). FPS 20-30 = missing the
  2-vblank deadline. Profiling: RECOMP_NX_PROFILE=1 -> prof.bin;
  tools/prof_report.py prof.bin <the NRO's elf> log; ELFs in /root/fo2x/elf/.
- **Switch CPU work (2026-10-13, after the console profile):**
  - *Spin waits sleep (toolkit):* RECOMP_SPIN_HINT used to wake the
    register threads every 16 turns and hand the guest lock over every 64
    (a quarter of the game thread in futex calls while it waited on the GPU
    fence). Now every 64 turns: one wake, then sleep (lock released) until
    recomp_hw_progress (after each pushbuffer scan, each vblank) or
    RECOMP_SPIN_SLEEP_US (250; 0 = old yield). Linux race: game thread from
    ~100% of a core to ~14%, same fps.
  - *Register file (toolkit):* g_eax ... g_mm7 are fields of one TLS struct
    (include/xbox/recomp_regs.h, `_rr`); every lifted function opens with
    RECOMP_RR_DECL (a local `_rr` pointer, opaque through an empty asm so GCC
    keeps it in a register). devkitA64 reads TLS via a call
    (__aarch64_read_tp), which lifted code repeated after every guest call;
    now once per function. The leaf/x87 "locals" modes name their locals
    L_* and #define the g_* names to them for the body.
  - *No tiled fold per access:* CMake RECOMP_FOLD_TILED=0. XMV joins the
    device-aware sections; mmio_rewrite folds MMX_MEM/XMM_MEM/*_STORE/
    XMM_LOAD_* addresses too; the native XMV converter folds its surface.
    Checked with RECOMP_NO_TILED=1 (Linux leaves the aperture unmapped):
    movies, menus, a race.
  - Result: NRO 52.2 -> 37.6 MB; hot collision/physics functions 20-50%
    less code (sub_001D93F0 46 -> 34 KB). Non-volatile guest memory
    (-DRECOMP_MEM_VOLATILE=) would only save another 2-3%: left on.
  - Collision/physics (sub_0007E6E0 chain: box-box sub_001D93F0,
    sub_001DA450, sub_001C9BB0/...) are large routines with 20 KB frames,
    not small leaves; no single one is worth a native rewrite yet.
- Main menu is data/scripts/MainMenu.bed in flatout2.bfs (bfs1 v0x20040505,
  24-byte entries, encoded names) -- for hiding the Xbox Live item later.
- Debug aids added: FRAME_TRACE prints texture formats/control0 per stage;
  `RECOMP_VK_SKIP_BLEND=<hex dfactor>` drops blended draws with that factor.

- **Crash sounds overdriven (toolkit apu_dsp.c / apu_xaudio2.c, 2026-10-14):**
  the mixdown (1.27 x full scale in a crash) went through tanh, then the
  output "safety" tanh, driven 2x into its -6 dBFS ceiling at
  RECOMP_AUDIO_VOLUME=100: crest factor 4.6 dB. Both are now one
  gain-envelope limiter (apu_limiter_run, 64-sample look-ahead, 120 ms
  release). Non-finite mix samples are silenced and counted ("non-finite mix
  samples silenced" in the [perf] ring line; RECOMP_APU_RING_STATS=1 on Linux).
- **Wavy shimmer on cars (toolkit nv2a_vk.c self_snapshot):** the bloom
  composite samples the scene surface (stage 3) while drawing into it -- a
  Vulkan feedback loop. A draw that samples its own target now samples a copy.
- **Stunts hung loading (High Jump, Bowling) -- toolkit lifter:**
  `__zf_from_dest` (a je/jne after a join of two ZF setters on one register)
  re-read the register at the branch; the bit reader 0x1C63F0 rewrites ecx in
  between, so every byte-aligned read took the unaligned path and the stunt's
  recorded frames (0x8E-byte records, decoder 0x59910 / slerp 0x1C3BD0)
  decoded to NaN: fmod's fprem loop, then the broad-phase, spun forever. Now
  tests _fa (test_zf_merge_snapshot.py). Also fixed on the way: fprem/fprem1
  set C2/quotient bits and the sahf state (test_fprem.py); shl/shr by cl mask
  the count to 5 bits (test_shift_mask.py). Ten branches changed game-wide.
- **Up to 60 fps:** the flip trap is NOP data (vblanks << 5) | 1 (method 1 of
  the table at 0x243024, flip queue 0x242DF0); FlatOut 2 asks for 2
  (0x10580141). The PGRAPH wrapper rewrites it to 1. FO2_FPS_CAP=30 keeps 30.
  Menus 30 -> 58 fps on Linux.
- **Input latency (recomp_manual.c, default on):** the frame-start fence wait
  (0x1BEFD1 -> 0x23B890 -> 0x240240, frame fences at 0x42C3DC, write index
  0x42C3E4) waits on the last frame's fence, not the one before:
  first-frame-after-press decoded ~190 -> ~130 ms at 18 fps (lavapipe), race
  20 -> 18 fps there. FO2_LOW_LATENCY=0 off. RECOMP_LAT_TRACE=1 logs "[lat]".
- Debug aids: RECOMP_VK_GLSL_DUMP=<dir> (every shader), RECOMP_VK_DUMP_BURST=
  <flag file> (next 30 frames dumped), Linux helpers /root/fo2x/stunt.sh,
  retry.sh (N race retries), watchnan.sh <va> (gdb: first NaN written there).

- **High Jump slow (toolkit, 2026-10-16):** 3243 draws/frame, 1756 with a
  CMP normal (type 6, 11:11:10 packed) the executor converted on the CPU every
  draw and could not instance. Now fed raw (NV2A_BACKEND_RAW_CMP; R32_UINT,
  unpacked in the vertex shader -- prog_cmp_glsl, mask in the program key and
  in progcache records' xform >> 16). Instanced 184 -> 975, 3058 -> 2267 draw
  calls, lavapipe 11 -> 13.5 fps. Next: 482 runs/frame broken by
  COMBINER_FACTOR0 (0x0A60, per-object tint) -- would need per-instance
  combiner constants. RECOMP_DRAW_STATS=1 prints why draws were not merged
  and which registers broke runs; FRAME_TRACE prints attribute formats.
- **Pops (toolkit apu_vp.c):** pops in the output sat on APU frame boundaries
  at one phase of every 256-sample block (FMOD's register writes land at the
  same sub-frame: the frame thread renders 8 frames per wait). Voice volume
  changes were applied as steps; mix gains now glide (RECOMP_APU_VOL_RAMP,
  default 8 frames): High Jump 11.7 -> 2.4 pops/min. RECOMP_APU_RING_STATS=1
  also reports stream starvation, ADPCM ring staleness (RING_TRACK_MAX 64 K),
  and frame-boundary jumps per voice / per bin. Linux helper hjpop.sh <tag>.

## Status (2026-10-13)

- Linux Vulkan (lavapipe): intro movies, title, profile create/save/load,
  menus with video backgrounds and music, car select garage, Quick Race and
  Career races render and drive (~15-30 fps on lavapipe).
- Switch: `fo2x-vulkan.nro` built and staged with the disc; not run on Eden or
  hardware yet. Icon from the title screen; loader logo still NFSU2's.
- Not done: no Switch GL NRO; RECOMP_CULL is off (Burnout setting, unchecked
  here); colour-keyed overlay over flipped frames not composited.
