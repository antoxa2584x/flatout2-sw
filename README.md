<div align="center">

<img src="https://images.steamusercontent.com/ugc/12151991102670343799/247D96761BDDD448C1667593611B5ADE767554DA/" alt="FlatOut 2" width="520">

### Xbox static recompilation for Nintendo Switch and Linux

The original Xbox (USA/Europe) release of **FlatOut 2**, lifted
instruction by instruction to C and running natively, with no emulator.

![Switch](https://img.shields.io/badge/Nintendo%20Switch-homebrew-E60012?logo=nintendoswitch&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-x86__64-FCC624?logo=linux&logoColor=black)
![Vulkan](https://img.shields.io/badge/Vulkan-1.3-AC162C?logo=vulkan&logoColor=white)
![Version](https://img.shields.io/badge/version-0.2.0-blue)

[Features](#-features) · [Playing on Switch](#%EF%B8%8F-playing-on-switch) · [Building](#%EF%B8%8F-building) · [Configuration](#%EF%B8%8F-configuration) · [Status](#-status)

</div>

---

> [!IMPORTANT]
> **No game data is included.** You need your own copy of the Xbox disc,
> extracted (`default.xbe`, `flatout2.bfs`, `Data/`, ...).

Built on the runtime of the NFS Underground 2 / Carbon / Burnout 3 Switch
ports (GPU renderers, Horizon support). FlatOut 2 links the same D3D8,
DirectSound and CRT library code as NFS Carbon, so the ports share their
library hooks.

## ✨ Features

- 🏁 **Native code**: the whole game runs as recompiled C, built with
  [xboxrecomp](https://github.com/sp00nznet/xboxrecomp)
- 🎮 **Vulkan renderer** (NVK on Switch) with render scaling, mipmaps,
  anisotropic filtering and FXAA
- 🚗 **Races and the full front end**: Quick Race and Career, the car
  select garage, menus with their video backgrounds
- 🖥️ **Widescreen** 16:9 output (`RECOMP_WIDESCREEN=0` for 4:3)
- 🔊 **Audio** through an emulated Xbox APU (FMOD music, engines, crashes),
  5.1 downmixed to stereo
- 🎬 **Movies** (XMV) through the emulated video overlay
- 💾 **Profiles** save and load (`game/UDATA`)
- 🔁 **Docked / handheld profiles**: render scale per mode, applied when
  you dock or undock; optional clock boost that steps aside when sys-clk
  or Horizon OC is running

## 🕹️ Playing on Switch

1. Copy `fo2x-vulkan.nro` to `sdmc:/switch/fo2x/`.
2. Copy the **extracted** disc (not the ISO) to `sdmc:/switch/fo2x/game/`.
3. Start it with **title takeover**: hold **R** while launching any game.
   Applet mode leaves too little memory.

```
sdmc:/switch/fo2x/
├── fo2x-vulkan.nro
├── fo2x_env.txt       optional settings, KEY=VALUE per line
├── fo2x_options.txt   optional picture settings per mode
└── game/
    ├── default.xbe
    ├── flatout2.bfs
    ├── UDATA/         your profiles
    └── ...
```

Buttons map by label (Switch A = Xbox A). Settings go in `fo2x_env.txt`
(see [Configuration](#%EF%B8%8F-configuration)) and the log is written to the
same folder (`fo2x_log.txt`, unless `NFSU2_NO_LOG=1`).

Picture settings live in `fo2x_options.txt`, all optional:

```ini
# sdmc:/switch/fo2x/fo2x_options.txt
scale_docked=200     # render scale in %: 640x480 x2 = 1280x960 for 1080p
scale_handheld=150   # x1.5 = 720 lines, the handheld panel
aniso=16             # anisotropic filtering 1/2/4/8/16
fxaa=1               # FXAA on the presented frame
```

## 🛠️ Building

<details open>
<summary><b>1. Unpack the disc and lift the XBE to C</b></summary>

```sh
# from xboxrecomp/
python3 -m tools.xiso unpack "FlatOut 2 (USA, Europe).iso" -o /path/to/game
# writes FO2_GEN_DIR (default /root/fo2x/gen)
FO2_XBE=/path/to/game/default.xbe FO2_GEN_DIR=/path/to/gen tools/regen.sh
```

</details>

<details open>
<summary><b>2a. Linux</b> (SDL2 + Vulkan)</summary>

```sh
cmake -S . -B build -G Ninja -DNFSU2_GEN_DIR=/path/to/gen -DNFSU2_VULKAN=ON
cmake --build build
NFSU2_GAME_DIR=/path/to/game build/fo2_recomp
```

</details>

<details open>
<summary><b>2b. Nintendo Switch</b> (devkitA64, switch-sdl2, mesa-switch NVK, glslang)</summary>

```sh
VULKAN=1 JOBS=6 NFSU2_GEN_DIR=/path/to/gen NFSU2_GAME_SRC=/path/to/game switch/build.sh
# -> switch_sd/switch/fo2x/ (NRO + game/)
```

</details>

### Project layout

| Path | What |
|---|---|
| `src/main.c` | boot and runtime defaults |
| `src/recomp_manual.c` | hand-written overrides of lifted functions (library hooks, movie stream priming, fence stats) |
| `src/switch_nx.c` | Switch log device, env file, clocks, docked/handheld profiles, exception handler, loading screen |
| `config/seed_functions.json` | entry points the static pass cannot see |
| `xboxrecomp/` | the toolkit (MIT), vendored with this port's changes: NV2A renderers, video overlay, SDL audio, Switch platform layer, translator fixes |
| `tools/regen.sh` | XBE → lifted C (`gen/`, never committed) |
| `switch/build.sh` | Switch NRO build + SD-card staging |

## ⚙️ Configuration

On Linux these are ordinary environment variables. On the Switch they go in
`sdmc:/switch/fo2x/fo2x_env.txt`, one `KEY=VALUE` per line (`#` starts a
comment). Anything left out runs at its default. Most of the list is for
debugging, so the sections below are folded.

```ini
# sdmc:/switch/fo2x/fo2x_env.txt
NFSU2_CPU_MHZ=1785
NFSU2_NO_LOG=1
```

<details>
<summary><b>Build and code generation</b></summary>

| Variable | Meaning |
|---|---|
| `XBOXRECOMP_DIR` | Toolkit to build or regenerate with (default: the vendored `xboxrecomp/`). |
| `FO2_XBE` | `default.xbe` to lift (`tools/regen.sh`). |
| `FO2_GEN_DIR` / `NFSU2_GEN_DIR` | Directory for the lifted C (default `/root/fo2x/gen`). |
| `LIFT_ONLY=1` | `regen.sh`: only re-lift (enough after translator or `recomp_manual.c` changes; seed changes need the full run). |
| `TRACE_FUNCS` | `regen.sh`: JSON list of addresses whose entries are logged (`[TRACE]`). |
| `NFSU2_GAME_SRC` | `switch/build.sh`: extracted disc to stage (default `/root/fo2x/game`). |
| `SD_ROOT` | `switch/build.sh`: staging SD-card root (default `<repo>/switch_sd`). |
| `BUILD_DIR` | `switch/build.sh`: build directory (default `/root/fo2x/build-switch-vk` with `VULKAN=1`). |
| `JOBS` | `switch/build.sh`: parallel compile jobs (6 fits `-O2` in RAM). |
| `VULKAN=1` | `switch/build.sh`: build the Vulkan renderer (`fo2x-vulkan.nro`). |
| `NVK_SDK`, `GLSLANG_DIR` | `switch/build.sh` with `VULKAN=1`: mesa-switch NVK install and Switch glslang. |
| `LTO=1` | `switch/build.sh`: link-time optimisation of the lifted code. |

</details>

<details>
<summary><b>Game and host</b></summary>

| Variable | Meaning |
|---|---|
| `NFSU2_GAME_DIR` | Extracted disc (Linux; default `./game`). The Switch always uses `sdmc:/switch/fo2x/game/`. |
| `RECOMP_WIDESCREEN=0` | 4:3 instead of 16:9. |
| `NFSU2_APU=0` | No emulated APU (no sound). |
| `FO2_FENCE_STATS=1` | GPU fence waits per caller every 10 s. |
| `NFSU2_EXIT_TRACE=1` | Print the guest state at exit. |

</details>

<details>
<summary><b>Switch (Horizon)</b></summary>

| Variable | Meaning |
|---|---|
| `NFSU2_CPU_MHZ`, `NFSU2_GPU_MHZ`, `NFSU2_MEM_MHZ` | Clocks in both modes (more heat and battery). Ignored while sys-clk or Horizon OC runs; a clock already higher is never lowered. The log shows `[clock] ...`. |
| `FO2_DOCK_BOOST=1` | With the three above unset: boost to 1785 / 768 / 1600 MHz while docked only. |
| `NFSU2_CLOCK_FORCE=1` | Set the clocks even when sys-clk or Horizon OC runs. |
| `NFSU2_NO_LOG=1` / `NFSU2_LOG=0` | No log file, no `[perf]` reports, no profiler. |
| `NFSU2_LOG_SYNC=1` | Write every log line to the card at once (slow; for hangs). |
| `NFSU2_LOADER=0` | No loading screen before the first frame. |
| `RECOMP_NX_PROFILE` | Sampling profiler into `prof.bin`: 1 = busy threads, 2 = every thread above 2%. |
| `RECOMP_NX_AUDIO_PRIO=0` | Leave the audio threads at the default priority. |

</details>

<details>
<summary><b>Scheduling and kernel</b></summary>

| Variable | Meaning |
|---|---|
| `RECOMP_GIL_EAGER=1` | Hand the guest lock over by guest priority. |
| `RECOMP_SPIN_SLEEP_US` | How long a polling loop sleeps waiting for the hardware (default 250; 0 = yield only). |
| `RECOMP_VBLANK` | Vblank interrupt for D3D (default 1). |
| `RECOMP_QUIET` | 1 = no periodic kernel/GPU summaries (Switch default), 0 = show them. |
| `XBOX_LOG_LEVEL` | Kernel log level (0 errors … trace). |

</details>

<details>
<summary><b>Graphics (Vulkan renderer)</b></summary>

| Variable | Meaning |
|---|---|
| `RECOMP_GL_SCALE` | Render resolution multiple (fractions allowed); overrides both `fo2x_options.txt` scales. |
| `RECOMP_PROG_CACHE` | Shader/pipeline cache file, `=0` off. |
| `RECOMP_CULL=0` | No off-screen batch culling (the port's default). |
| `RECOMP_VK_MIPS=0` | No mipmaps. |
| `RECOMP_VK_ANISO` | Anisotropic filtering cap, 1..16. |
| `RECOMP_VK_LOD_BIAS` | Mip bias, negative = sharper (default -0.25). |
| `RECOMP_VK_FXAA=0` | No FXAA. |
| `RECOMP_VK_HEADLESS=1` | Linux: no window (also implied by `SDL_VIDEODRIVER=offscreen`). |
| `RECOMP_VK_VALIDATION=1` | Linux: Khronos validation layer. |
| `RECOMP_GL_DUMP=<prefix>[,every]` | Write presented frames as BMP (every N frames, default 60). |
| `RECOMP_FRAME_TRACE=<flag file>` | When the file exists: one line per draw/clear of the next frame. |
| `RECOMP_FPS_LOG=1` | Presented frames per 10 s. |

</details>

<details>
<summary><b>Audio</b></summary>

| Variable | Meaning |
|---|---|
| `RECOMP_AUDIO=0` | No host audio device. |
| `RECOMP_AUDIO_VOLUME` | Master volume 0..100. |
| `RECOMP_APU_DOWNMIX` | Extra mixbins in the stereo downmix, `bin:left:right,...` (e.g. `10:0.25:0.25` keeps some of FMOD's reverb). |
| `RECOMP_AUDIO_DUMP=<path>,<start s>,<secs>` | Record the exact PCM sent to the device. |
| `RECOMP_APU_BIN_STATS=1` | RMS and peak per mixbin. |
| `RECOMP_APU_TRACE=1` | Front-end methods and a per-second voice summary. |

</details>

## 📊 Status

| | |
|---|---|
| Intro movies, title, menus with video backgrounds | ✅ working |
| Profile create / save / load | ✅ working |
| Car select garage, Quick Race, Career races | ✅ working |
| Audio: music, engines, crash effects | ✅ working |
| Docked / handheld picture and clock profiles | ✅ working |
| OpenGL renderer | ➖ Linux only, kept building; no Switch GL NRO |
| Switch | 🧪 runs on hardware (Vulkan), 20-30 fps in races; being tuned |
