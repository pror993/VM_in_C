# LC-3 Virtual Machine in C

A lightweight, Unix-compatible implementation of the LC-3 (Little Computer 3) virtual machine written in C.

## Features

- **16-bit Architecture**: Emulates 65,536 (2^16) memory locations.
- **10 Registers**:
  - 8 general-purpose registers (R0-R7)
  - Program Counter (PC)
  - Condition flags register (COND: Positive, Zero, Negative)
- **Supported Opcodes**:
  - Arithmetic/Logic: `ADD`, `AND`, `NOT`
  - Memory: `LD`, `LDI`, `LDR`, `LEA`, `ST`, `STI`, `STR`
  - Control Flow: `BR`, `JMP` (and `RET`), `JSR`/`JSRR`
  - System: `TRAP`
- **Trap Routines**:
  - `GETC` (0x20): Read a single character from the keyboard (no echo).
  - `OUT` (0x21): Output a character to the console.
  - `PUTS` (0x22): Output a null-terminated string of ASCII characters.
  - `IN` (0x23): Prompt user for a character and echo it.
  - `PUTSP` (0x24): Output a packed string of characters.
  - `HALT` (0x25): Halt execution and print halt message.
- **Hardware Simulation**:
  - Memory-mapped keyboard status register (KBSR at `0xFE00`) and data register (KBDR at `0xFE02`).
  - Terminal raw mode configuration (`termios`) for immediate keyboard input responsiveness.

## Build

Compile the virtual machine using `gcc`:

```bash
gcc -o lc3 vm.c -Wall
```

## Running Programs

Run assembled LC-3 `.obj` files by passing them as arguments to the VM:

```bash
./lc3 <program.obj>
```

### Examples Included

- `hello.obj` / `hello-world.obj`: Classic hello world examples.
- `guess.obj`: A number guessing game.
- `2048.obj`: The popular 2048 sliding tile game (use `W`, `A`, `S`, `D` to play).
- `rogue.obj`: A retro terminal dungeon crawler.

## Credits & Acknowledgments

This implementation was created using:
- [Write your own Virtual Machine](https://www.jmeiners.com/lc3-vm/) tutorial by Justin Meiners.
- Google Gemini (Antigravity developer assistant) for pair programming and debugging.
