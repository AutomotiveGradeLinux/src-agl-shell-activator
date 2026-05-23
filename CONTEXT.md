# CONTEXT.md — agl-shell-activator

This document provides background context for anyone (human or AI agent)
approaching this codebase for the first time.

---

## What This Project Is

`agl-shell-activator` is a **single-purpose Wayland client utility** for the
**Automotive Grade Linux (AGL)** platform. Its only job is to instruct the
AGL compositor (`agl-compositor`) to bring a named application to the
foreground of the display.

When invoked as:

```sh
agl-shell-activator <app-id>
```

it connects to the running Wayland compositor, locates the
`agl_shell_desktop` compositor extension, sends a single
`activate_app` protocol message, then exits.

---

## Why It Exists

Embedded automotive systems running AGL typically run multiple applications
managed by `agl-compositor`. Applications are identified by an **app-id**
(usually the application's desktop entry name or D-Bus well-known name).
There is no standard mechanism in core Wayland to programmatically switch
which application is visible; `agl_shell_desktop` is AGL's compositor
extension that provides this capability.

This utility gives shell scripts, init systems, or other processes a simple
command-line interface to that compositor extension, without requiring a
full-featured application framework.

---

## AGL Platform Context

| Concept | Explanation |
|---------|-------------|
| **AGL (Automotive Grade Linux)** | Linux Foundation collaborative project producing an embedded Linux distribution for in-vehicle infotainment (IVI) and other automotive use cases. |
| **agl-compositor** | AGL's custom Wayland compositor, based on `libweston`. Provides AGL-specific Wayland protocol extensions. |
| **agl-shell-desktop** | A Wayland protocol extension defined by the AGL project. Provides `activate_app`, which brings a registered application surface to the foreground on a given output. |
| **Lucky Lamprey** | An AGL release codename (the 12.x LTS release series). The `lucky` branch is this project's branch for that release. |
| **XDG_RUNTIME_DIR** | Standard environment variable pointing to the directory containing the Wayland socket (`wayland-0` by default). On AGL devices this is `/run/platform/display`. |

---

## Architecture

The program is a minimal Wayland client. The flow is:

```
main()
  └─ create_display()
       ├─ wl_display_connect()           # open Wayland socket
       ├─ wl_display_get_registry()      # get compositor global list
       ├─ wl_display_roundtrip() x2      # flush and receive all globals
       │    └─ registry_handle_global()  # called once per global
       │         ├─ bind agl_shell_desktop  → display->shell_desktop
       │         └─ bind wl_output         → display->output_list
       └─ returns struct display *
  └─ agl_shell_desktop_activate_app()   # send protocol message
  └─ destroy_display()                  # teardown and disconnect
  └─ exit(0)
```

The double `wl_display_roundtrip()` is required so that:
1. The first round-trip delivers the global announce events.
2. The second round-trip delivers any per-global events triggered by the
   first (e.g., `wl_output` events such as geometry/mode/done).

---

## File Map

| Path | Role |
|------|------|
| `src/main.c` | Entire application: Wayland connection, registry handling, protocol call, teardown. |
| `src/os-compatibility.c` | OS portability helpers (CLOEXEC, anonymous file creation). Present in tree but **not compiled** into this binary. |
| `include/os-compatibility.h` | Header for the above. Included in the header search path. |
| `meson.build` | Build definition: dependencies, protocol code generation, binary target, script install. |
| `script/agl-activator` | Bash wrapper that sets `XDG_RUNTIME_DIR` before calling the binary. Used directly on AGL devices. |
| `COPYING` | MIT/Expat license, copyright Collabora, Ltd. 2021. |
| `README.md` | Two-sentence project description. |
| `.gitreview` | Gerrit connection config for AGL's review server. |

---

## Key Data Structures

```c
struct display {
    struct wl_display     *display;       // Wayland connection
    struct wl_registry    *registry;      // compositor global registry
    struct agl_shell_desktop *shell_desktop; // AGL extension handle
    struct wl_list        output_list;    // linked list of window_output
};

struct window_output {
    struct wl_output  *output;            // Wayland output handle
    struct display    *display;           // back-pointer to parent
    struct wl_list     link;              // intrusive list node
};
```

---

## Dependencies

All resolved at **build time** via `pkg-config`:

| Package | Version | Purpose |
|---------|---------|---------|
| `wayland-client` | any | Core Wayland client library (`wl_display`, `wl_registry`, `wl_output`, …) |
| `agl-compositor-0.0.19-protocols` | 0.0.19 | Provides `agl-shell-desktop.xml` protocol definition |
| `wayland-scanner` | any (native tool) | Generates C glue code from the XML at build time |
| `wayland-protocols` | >= 1.18 | Provides base protocol XML data directory |

### Generated files (build artifacts, not in git)

```
build/
  agl-shell-desktop-client-protocol.h   # client-side protocol header
  agl-shell-desktop-server-protocol.h   # server-side protocol header
  agl-shell-desktop-protocol.c          # marshal/unmarshal implementation
```

---

## Protocol: `agl_shell_desktop`

The central API call made by this tool:

```c
agl_shell_desktop_activate_app(
    display->shell_desktop,   // the bound agl_shell_desktop proxy
    app_id,                   // string: application identifier
    NULL,                     // optional extra parameter (unused)
    output->output            // wl_output: target display
);
```

The `agl_shell_desktop` interface is defined in `agl-shell-desktop.xml`
(provided by the `agl-compositor` package). The version bound is **1**.

---

## Compiler Flags and Feature Macros

| Flag | Effect |
|------|--------|
| `-std=gnu99` | C99 with GNU extensions |
| `-D_GNU_SOURCE` | Enable GNU libc extensions |
| `-D_ALL_SOURCE` | Enable all platform extensions (AIX compat, ignored on Linux) |
| `-Wall -Wextra -Werror` | All warnings treated as errors |
| `-Wno-unused-parameter` | Suppress warnings for intentionally unused callback params |
| `-Wno-pedantic` | Suppress pedantic ISO C warnings |
| `HAVE_MEMFD_CREATE=1` | Defined if `memfd_create(2)` is present in libc |
| `HAVE_STRCHRNUL=1` | Defined if `strchrnul(3)` is present in libc |

---

## History and Ownership

| Attribute | Value |
|-----------|-------|
| Created | 2021-05-05 |
| Original author | Marius Vlad, Collabora, Ltd. |
| Maintainer (Gerrit) | Jan-Simon Möller, Linux Foundation |
| License | MIT/Expat |
| Tracker reference | AGL Jira SPEC-3817 (initial), SPEC-4006 (Lucky Lamprey branch) |
| Total commits | 3 |
| Code review | Gerrit at `gerrit.automotivelinux.org:29418` |

---

## Known Limitations

- **Always uses the first output.** If multiple displays are connected, the
  tool always targets `output_list.next`. There is no way to select a
  specific output by name, index, or geometry.
- **No error exit codes.** The binary always exits with status `0`, even if
  `agl_shell_desktop` was not available or no outputs were found.
- **No parameter passthrough.** The `parameter` argument to
  `activate_app` is hardcoded to `NULL`.
- **`os-compatibility.c` is unused.** The file is in the source tree but
  is not included in the Meson `srcs_agl_activator` list and is not compiled.

---

## Non-Goals

- This tool does not launch applications; it only brings an already-running
  application's surface to the foreground.
- It has no D-Bus interface, no systemd integration, and no configuration
  file support by design.
- It is not a general-purpose Wayland compositor controller.
