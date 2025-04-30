# Wayfinder Engine

Wayfinder is a modern C++ game engine based on raylib. It is designed with modularity and flexibility in mind and provides a foundation for building both 2D and 3D games.

## Features

- Modern C++ (C++23) codebase
- Cross-platform support (Windows, Linux, macOS, Web via Emscripten)
- Modular architecture with clear separation of concerns
- Raylib-based rendering system
- Component-based scene system
- Platform-agnostic input handling

## Project Structure

- `engine/wayfinder/` - Core engine library
- `sandbox/` - Example projects and testing environments
  - `journey/` - Sandbox application
  - `waystone/` - Sandbox application
- `apps/` - Tools and applications
  - `cartographer/` - Editor (planned)
  - `compass/` - Project manager (planned)
- `tools/` - Development and build tools
  - `surveyor/` - Debugger suite
  - `expedition/` - Build orchestrator
  - `beacon/` - Diagnostics console
  - `waypoint/` - Asset processor
  - `navigator/` - Headless server