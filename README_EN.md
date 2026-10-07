# DualForge C++

A standalone C++17 / SDL 3.4.18 library for ordinary Sony DualSense controllers over USB. CMake 3.24+ is required. Python sources in the parent repository are untouched.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

The default build fetches a checksum-pinned SDL archive. Offline builds accept `-DDUALFORGE_SDL_SOURCE_DIR=/path/to/SDL3-3.4.18`. Windows executables are under `build/Release` when using Visual Studio. SDL's DLL is copied beside the examples and tests.

```sh
./build/dualforge_cli list
./build/dualforge_cli monitor 10
./build/dualforge_cli led 5
./build/dualforge_cli rumble 3
./build/dualforge_cli trigger feedback 3
./build/dualforge_cli trigger
./build/dualforge_cli bow 10
```

All commands except `list` open a controller. Output commands operate lights/motors; bow, galloping and machine are experimental firmware effects. Durations are bounded to 1..300 seconds. Ctrl+C performs best-effort cleanup.

The original Python demos also have standalone C++ counterparts: `dualforge_test_led`, `dualforge_test_trigger_effects`, `dualforge_bow_simulation`, and `dualforge_touchpad`. They retain controller-driven menus and accept `--help`, `--device ID`, and `--seconds N`. The touchpad demo supports `--dry-run`; native mouse output uses Linux uinput, Windows SendInput, or macOS CoreGraphics and requires platform permissions. See [demo guide](examples/README.md).

Features: normalized sticks/triggers, buttons, two touch contacts, battery, calibrated gyro (rad/s) and acceleration (m/s²), RGB, player-light mask, brightness/animation, timed rumble, adaptive triggers and all legacy preset names. Audio, Bluetooth, raw calibration/MAC/full firmware information and Edge-specific controls are deferred.

Use `dualforge::Context` and `open(id)` to obtain a shared RAII `Controller`. Standalone mode calls `poll()`; existing SDL applications use `EventMode::External`, pass events through `handle_event()` and call `refresh()` after their own event pump. All operations and destruction belong on the main thread. Each input callback represents the latest sampled state, not necessarily one USB packet. A controller may outlive its Context, but needs explicit closure and no longer gets automatic event updates. Reinserted devices must be reopened explicitly.

Device operations return `Status`; effect factories throw `std::invalid_argument` on invalid parameters. Rumble uses strengths 0..65535 and a duration in milliseconds. Vendor effects require the native PS5 HIDAPI USB backend; on Linux ensure the user has hidraw access. `Availability::Unverified` means the command may be attempted, not that the physical behavior has been verified. Debug/calibration trigger modes are rejected.

```cmake
find_package(DualForge 0.1 CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE DualForge::DualForge)
```

Install with `cmake --install build --config Release --prefix /your/prefix`. SDL is installed too; ensure its runtime library is discoverable by your application. See [Chinese API guide](README.md), [protocol notes](docs/protocol.md) and [validation status](docs/validation.md).

The GitHub workflow runs only when this directory is used as the repository root. Windows/macOS builds and physical testing are pending until actually run; no lower-latency claim is made.

MIT licensed. Trigger encoding attribution and SDL notices are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
