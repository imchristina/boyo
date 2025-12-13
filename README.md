# Boyo

> A Nintendo Gameboy (DMG) and Gameboy Color (CGB) emulator written in C and SDL2.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Language](https://img.shields.io/badge/language-C-orange.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Cross--Platform-lightgrey.svg)

**Boyo** is a lightweight emulator designed to be platform-agnostic. It is compatible with the majority of the commercial game library and passes a significant number of hardware conformance tests.

## Features

* Emulates both the original Gameboy (DMG) and Gameboy Color (CGB)
* Runs the majority of the commercial game library
* Written in C (using C2x/C23 features)
* SDL2 for easy porting to different operating systems
* Supports both keyboard input and SDL-compatible Gamepads

## Prerequisites

To build Boyo, you will need a modern, up-to-date C compiler supporting C2x/C23 (like `gcc`), `make`, and the SDL2 development libraries installed.

**Ubuntu/Debian:**
```bash
sudo apt-get install build-essential libsdl2-dev
```

**macOS (Homebrew):**

```bash
brew install sdl2
```

## Building

You can compile Boyo for either the original Gameboy or the Gameboy Color.

### 1\. Clone the repository

```bash
git clone https://github.com/imchristina/boyo.git
cd boyo
```

### 2\. Compile

**For Gameboy (DMG):**

```bash
make
```

**For Gameboy Color (CGB):**

```bash
make CGB=1
```

Run `make clean` before building a different target.

## Setup & Usage

### 1\. Bootrom Requirements

Boyo requires original BIOS bootroms to function. You must provide these files yourself and place them in the **same directory** as the executable.

  * **DMG Mode:** Requires `dmg_boot.bin`
  * **CGB Mode:** Requires `cgb_boot.bin`

### 2\. Running a ROM

```bash
./boyo path/to/rom.gb
```

*Note: Boyo currently runs in a window matching the native Gameboy resolution. Window scaling is not yet supported.*

## Controls

Boyo supports keyboard input and SDL-compatible game controllers.

| Button | Keyboard Key |
| :--- | :--- |
| **D-Pad Up** | `W` |
| **D-Pad Left** | `A` |
| **D-Pad Down** | `S` |
| **D-Pad Right** | `D` |
| **A** | `L` |
| **B** | `K` |
| **Start** | `H` |
| **Select** | `G` |

## License

Distributed under the MIT License. See `LICENSE` for more information.
