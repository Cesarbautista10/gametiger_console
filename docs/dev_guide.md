# Developer's Guide

This page contains details on setting up the development environment for writing your own software for GameTiger console with **RP2350** (Pico 2).

## Building the firmware

### Prerequisites

First install required packages:
```bash
sudo apt-get update
sudo apt-get install git cmake build-essential gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
```

### Clone repositories

Clone the main repo:
```bash
git clone https://github.com/codetiger/GameTiger-Console
cd GameTiger-Console
```

The project includes `pico-sdk` and `pico-extras` as submodules. If they're not already present:
```bash
git submodule update --init --recursive
```

### Set environment variables

Set required environment variables (adjust paths to match your setup):

```bash
export PICO_SDK_PATH=$(pwd)/pico-sdk
export PICO_EXTRAS_PATH=$(pwd)/pico-extras
export PICO_TOOLCHAIN_PATH=/usr
```

### Build with CMake

Option 1: Use the build script:
```bash
./build.sh
```

Option 2: Manual CMake build:
```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

The firmware will be generated in the `build` directory.

On successful completion, you should see something similar to below output:

```bash
[ 97%] Performing build step for 'PioasmBuild'
[ 10%] Building CXX object CMakeFiles/pioasm.dir/main.cpp.o
...
[100%] Built target pioasm
[100%] Completed 'PioasmBuild'
[100%] Built target GameTiger
```

### Output files

Once the compilation is successful, you'll find these files in the `build` directory:
- **GameTiger.uf2** - Firmware for USB bootloader (drag & drop)
- **GameTiger.elf** - ELF executable for debugging
- **GameTiger.hex** - Hex format for external programmers
- **GameTiger.dis** - Disassembly listing

## Flashing the Firmware

### Method 1: USB Bootloader (Recommended)

1. **Enter bootloader mode:**
   - Hold the **BOOTSEL** button on the RP2350
   - Connect USB cable while holding button
   - Release button
   - Device appears as **RPI-RP2** drive

2. **Flash firmware:**
   ```bash
   cp build/GameTiger.uf2 /media/RPI-RP2/
   ```
   Or simply drag and drop `GameTiger.uf2` to the RPI-RP2 drive.

3. **Verify:**
   - Device automatically reboots
   - Display shows splash screen

### Method 2: SWD Debug Probe

Using OpenOCD or picoprobe:
```bash
openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "program build/GameTiger.elf verify reset exit"
```

### Method 3: Using flash script

The project includes a flash script:
```bash
./flash.sh
```

## Project Structure

```
GameTiger-Console/
├── core/              # Core drivers and libraries
│   ├── display.cpp    # ST7789V2 display driver
│   ├── keyboard.cpp   # Button input handling
│   ├── audio.cpp      # PWM audio driver
│   ├── battery.cpp    # Battery monitoring
│   └── ...
├── screens/           # Game implementations
│   ├── menuscreen.cpp
│   ├── snakescreen.cpp
│   ├── tetrisscreen.cpp
│   └── ...
├── content/           # Graphics and level data
├── main.cpp           # Main entry point
└── CMakeLists.txt     # Build configuration
```

## Hardware Configuration

### Display (ST7789V2 320×240)
- Resolution: 320×240 pixels (landscape)
- Interface: SPI1 @ 110 MHz
- Color depth: RGB565 (16-bit)
- Pins: See [PINOUT.md](../PINOUT.md)

### Controls
- **D-Pad**: I2C ADC-based (address 0x56)
- **Buttons A/B**: GPIO 26, 27 (active low, pull-up)
- **Start/Select**: GPIO 8, 9 (active low, pull-up)

### Audio
- PAM8302A Class-D Amplifier
- PWM output on GPIO 23

## Creating a New Game

1. Create new screen class in `screens/` directory
2. Inherit from `Screen` base class
3. Implement required methods:
   - `update()` - Game logic (called every frame)
   - `draw()` - Rendering
4. Register in `main.cpp`
5. Add to menu in `menuscreen.cpp`

See existing game implementations for examples.

## Debugging

USB serial output is enabled:
```bash
# Monitor serial output
screen /dev/ttyACM0 115200
# or
minicom -D /dev/ttyACM0 -b 115200
```

Use `printf()` statements for debugging - output appears on USB serial.