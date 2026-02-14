# Zelda3 - Claude Code Project Guide

## Build
```
make clean_obj && make    # full rebuild
make                      # incremental build
```
Build uses `-Werror` — all warnings are fatal errors. New `.c` files in `src/` are auto-included by Makefile wildcard.

## Project Structure
- `src/` — main game C source (messaging, dungeon, overworld, sprites, etc.)
- `src/platform/macos/` — macOS-specific Objective-C code (speech synthesis)
- `src/platform/win32/` — Windows-specific code
- `snes/` — SNES hardware emulation (PPU, APU, CPU, SPC, DMA)
- `assets/` — Python tools for asset extraction (`text_compression.py` has text encoding tables)
- `third_party/` — OpenGL core, Opus decoder

## Key Files
- `src/messaging.c` — Dialog/text system. `Text_LoadCharacterBuffer()` decodes dialog into `messaging_text_buffer`
- `src/main.c` — SDL event loop, init/shutdown
- `src/accessibility.c/h` — Accessibility layer (TTS dialog announcements)
- `src/platform/macos/speechsynthesis.m/h` — macOS AVSpeechSynthesizer wrapper
- `src/variables.h` — Game RAM variable macros (requires `types.h` included first)
- `Makefile` — Build system; macOS needs special handling for `.m` files and framework linking

## Learnings
Detailed implementation notes are stored in the Claude memory directory:
- `MEMORY.md` — Quick reference for build system, text system, current state
- `a11y-learnings.md` — Accessibility implementation details, macOS TTS pitfalls, character encoding table

## Conventions
- C99 style, types defined in `src/types.h` (`uint8`, `uint16`, `uint32`, etc.)
- Game state accessed via macros over `g_ram[]` array (see `variables.h`)
- Platform-specific code guarded by `#ifdef __APPLE__`, `#ifdef _WIN32`, etc.
- No C++ — pure C and Objective-C only
