# ccleste

![screenshot](https://raw.githubusercontent.com/lemon32767/ccleste/master/screenshot.png)

This is a C source port of the [original celeste (Celeste classic)](https://www.lexaloffle.com/bbs/?tid=2145) for the PICO-8, designed to be portable. PC and 3DS are the main supported platforms, though other people are [maintaining ports to other platforms](https://github.com/lemon32767/ccleste/network/members).

Go to [this project's releases](https://github.com/CatlordReal/celeste-classic-desktop/releases) for Windows and macOS downloads.

## Desktop downloads

Download and unzip the archive for your platform: `ccleste-windows.zip`, `ccleste-windows-installer.zip`, `ccleste-macos-arm64.zip` for Apple Silicon, or `ccleste-macos-x86_64.zip` for Intel Macs. Windows portable: run `ccleste.exe`. macOS: open `Celeste Classic.app`. Both launchers set the game directory before starting, so the bundled `data/`, controller database, and SDL runtime libraries work regardless of the directory from which they were launched.

For a per-user Windows install, unzip `ccleste-windows-installer.zip` and run `Install-CelesteClassic.cmd`. It installs to `%LOCALAPPDATA%\CelesteClassic` without administrator access and creates a Start menu shortcut. It refuses to replace an unknown folder. Reinstalling a known copy requires `Install-CelesteClassic.cmd -Force`; it stages the new files before replacing the old copy. Run the installed `Uninstall-CelesteClassic.cmd` to remove the game and its shortcut. The `.cmd` launchers use a process-scoped PowerShell execution policy so they work with default Windows script restrictions. Uninstalling keeps saved progress at `%LOCALAPPDATA%\celeste-classic\celeste-classic\progress.dat`.

The macOS release builds target macOS 15 or later. Windows builds target 64-bit Windows.

The macOS app is ad-hoc signed, not Developer ID signed. macOS may require Control-clicking the app and choosing **Open** the first time.

The release workflow builds 64-bit Windows with MSYS2 MinGW SDL2 packages and macOS with Homebrew's SDL2 packages. Each archive bundles its executable, game data, controller database, and non-system SDL runtime dependencies.

An experimental web port is also available [here](https://lemon32767.github.io/ccleste.html).

celeste.c + celeste.h is where the game code is, translated from the pico 8 lua code by hand.
These files don't depend on anything other than the c standard library and don't perform any allocations (it uses its own internal global state).

sdl12main.c provides a "frontend" written in SDL (plus SDL mixer) which implements graphics and audio output. It can be compiled on unix-like platforms by running
```
make
```
By default, SDL2 will be used. Read the Makefile for more information.

3DS is also a supported platform. Compile to 3DS with:
```
make -f Makefile.3ds    # this will generate ccleste.3dsx
```
You will need devkitPro with these dkp packages installed:
```
3ds-sdl
3ds-sdl_mixer
libctru
devkitARM
```

# Controls

|PC                |3DS                |Action              |
|:----------------:|:-----------------:|-------------------:|
|LEFT              |LEFT               | Move left          |
|RIGHT             |RIGHT              | Move right         |
|DOWN              |DOWN               | Look down          |
|UP                |UP                 | Look up            |
|Z/C               |A                  | Jump               |
|X/V               |B/X                | Dash               |
|ESCAPE            |START              | Host menu: resume, stats, reset, quit |
|E                 |L+R                | Toggle screenshake |
|SHIFT+D           |Y+L                | Load state         |
|SHIFT+S           |Y+R                | Save state         |
|Hold F9           |Hold SELECT+START+Y| Reset current run  |
|F11               |SELECT             | Fullscreen         |

Controller input is also supported on PC (SDL2 ver) and web version. The controller must be plugged in when opening the game.
The default mappings are: jump with A and dash with B (xbox360 controller layout), move with d-pad or the left stick, pause with start, save/load state with left/right shoulder, exit with guide (logo button).
You can change these mappings by modifying the `ccleste-input-cfg.txt` file that will be created when you first run the game.

You can make the game start up in fullscreen by setting the environment variable `CCLESTE_START_FULLSCREEN` to "1", or by creating the file `ccleste-start-fullscreen.txt` in the game directory (contents don't matter).

## Saves and run statistics

Progress saves automatically when a stage changes, every 10 seconds, on reset, and on exit. Reopening the game restores the saved stage and run. Press Escape or controller Start for the host menu. **Stats** shows current-run and all-time jumps, dashes, deaths, berries, stages, climbed metres, and time. **Reset to start** clears the run and keeps all-time stats. The side panel shows a speedrun timer that stops at the first summit completion; menus and lost focus pause it.

Climbed distance counts upward player movement, with 128 game pixels displayed as 100 metres. Total in-game time counts active gameplay, including time after a run finishes. V2 save migration preserves existing six current and all-time fields, then seeds berries from collected fruit and stages from the saved room; it cannot reconstruct older lifetime totals for either metric. `SHIFT+S` and `SHIFT+D` remain separate in-memory save/load controls. Persistent saves use the platform's SDL preference directory; controller mappings remain in `ccleste-input-cfg.txt` in the game directory or the path set by `CCLESTE_INPUT_CFG_PATH`.

# TAS playback and the fixed point question

In order to playback a TAS, specify it as the first argument to the program when running it. On Windows you can drag the TAS file to the .exe to do this.
The format for the TAS should be a text file that looks like "0,0,3,5,1,34,0,", where each number is the input bitfield and each frame is ended by a comma.
The inputs in the TAS should start in the first loading frame of 100m (neglecting the title screen). When playing back a TAS the starting RNG seed will always be the same.

Most other Celeste Classic ports use floating point numbers, but PICO-8 actually uses 16.16 fixed point numbers.
For casual play and RTA speedrunning, the discrepancies are minor enough to be essentially negligible, however with TASing it might make a difference.
Defining the preprocessor macro `CELESTE_P8_FIXEDP` when compiling celeste.c will use a bunch of preprocessor hacks to replace the float type for all the
code of that file with a fixed point type that matches that of PICO-8. The use of this preprocessor macro requires compiling celeste.c with a C++ compiler, however (but not linking with the C++ standard library).

Using make you can compile this fixed point version with `make USE_FIXEDP=1`.

When playing back TASes made with other tools that work under the assumption of ideal RNG for balloons (since their hitbox depends on that), you can ensure that they do not desync by
defining the preprocessor macro `CELESTE_P8_HACKED_BALLOONS`, which will make balloons static and always expand their hitbox to their full range.

Using make you can turn on this feature with `make HACKED_BALLOONS=1`.

You can combine both of these with `make HACKED_BALLOONS=1 USE_FIXEDP=1`.

# Credits and rights

Sound wave files are taken from [https://github.com/JeffRuLz/Celeste-Classic-GBA/tree/master/maxmod_data](https://github.com/JeffRuLz/Celeste-Classic-GBA/tree/master/maxmod_data),
music ogg files were obtained by converting the .wav dumps from pico 8, which I did using audacity & ffmpeg.

All credit for the original game goes to the original developers (Maddy Thorson & Noel Berry).

Celeste Classic, its art, music, and characters remain property of their respective rights holders. This repository is a community source port; distributing a build does not grant rights beyond those supplied with the project and its upstream assets. See the linked source material above before redistributing it.
