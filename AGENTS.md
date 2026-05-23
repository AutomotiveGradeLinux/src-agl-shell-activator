# AGENTS.md — agl-shell-activator

This file documents conventions and guidance for AI agents (coding assistants,
automated tools, or LLM-powered agents) working on this codebase.

---

## Project at a Glance

`agl-shell-activator` is a minimal Wayland client utility for the
**Automotive Grade Linux (AGL)** platform. It sends a single
`agl_shell_desktop.activate_app` protocol message to the AGL compositor
(`agl-compositor`) to bring a named application to the foreground.

- **Language:** C (GNU99)
- **Build system:** Meson (>= 0.50)
- **License:** MIT/Expat (Collabora, Ltd.)
- **Platform:** AGL embedded Linux targets (e.g., Raspberry Pi with AGL image)

---

## Repository Layout

```
agl-shell-activator/
├── COPYING                   # MIT/Expat license
├── README.md                 # Two-sentence description
├── meson.build               # Sole build definition (126 lines)
├── include/
│   └── os-compatibility.h   # OS portability declarations
├── script/
│   └── agl-activator        # Bash wrapper for AGL devices
└── src/
    ├── main.c               # Application entry point and Wayland logic
    └── os-compatibility.c   # OS portability utilities (not compiled in)
```

---

## Build

### Prerequisites

The following must be available via `pkg-config` on the build host:

| pkg-config name | Notes |
|---|---|
| `wayland-client` | Core Wayland client library |
| `agl-compositor-0.0.19-protocols` | AGL compositor protocol XML; provides `agl-shell-desktop.xml` |
| `wayland-scanner` (native) | Code-generation tool; run at build time |
| `wayland-protocols >= 1.18` | Base Wayland protocol definitions |

### Build Commands

```sh
meson setup build
ninja -C build
sudo ninja -C build install   # installs to /usr/bin/ by default
```

### Generated Files (not in git)

`wayland-scanner` generates these at build time from `agl-shell-desktop.xml`:

- `agl-shell-desktop-client-protocol.h`
- `agl-shell-desktop-server-protocol.h`
- `agl-shell-desktop-protocol.c`

Do **not** add these generated files to version control.

---

## Code Conventions

### Language and Standard

- C, `gnu99` standard (`-std=gnu99`).
- `_GNU_SOURCE` and `_ALL_SOURCE` are always defined.

### Compiler Flags

Warnings are treated as errors:

```
-Wall -Wextra -Werror -Wno-unused-parameter -Wno-pedantic
```

All new code must compile cleanly at this warning level.

### Style

- Follow existing patterns in `src/main.c` (K&R-adjacent C style).
- No separate `.clang-format` or coding-style document exists; match the
  surrounding code's indentation (4 spaces, no tabs observed) and brace
  placement.
- Keep functions small and focused (see `create_display`, `destroy_display`
  as examples).
- Error messages go to `stderr` via `fprintf(stderr, ...)`.

### Memory and Resources

- Every `create_*` function has a corresponding `destroy_*` function.
- Free all allocations on every exit path (including error paths).
- The existing `destroy_display()` is the canonical teardown pattern.

### Wayland Protocol Usage

- Use the generated client-protocol header (`agl-shell-desktop-client-protocol.h`)
  for all `agl_shell_desktop` protocol calls.
- Use the `wl_output` at `display->output_list` head — the tool currently
  targets the first discovered output.
- Always call `wl_display_roundtrip()` twice after `wl_display_get_registry()`
  to ensure all globals and their events are processed before proceeding.

---

## Key Interfaces and Data Structures

### `struct display` (`src/main.c`)

Top-level application state. Always pass by pointer. Key fields:

| Field | Type | Description |
|---|---|---|
| `display` | `struct wl_display *` | Active Wayland connection |
| `registry` | `struct wl_registry *` | Global registry proxy |
| `shell_desktop` | `struct agl_shell_desktop *` | Bound AGL shell desktop extension |
| `output_list` | `struct wl_list` | Linked list of `window_output` nodes |

### `struct window_output` (`src/main.c`)

Represents one `wl_output` global. Stored in `display->output_list`.

---

## Extending the Tool

Common extension scenarios:

1. **Target a specific output by name/index:** Iterate `display->output_list`
   instead of always taking the first element. The `wl_output_listener`
   geometry callback stubs in `main.c` are the right place to capture output
   metadata (e.g., make/model, position).

2. **Add `agl_shell` (not `agl_shell_desktop`) support:** Bind the
   `agl_shell` interface in `registry_handle_global()` alongside the existing
   `agl_shell_desktop` binding.

3. **Pass additional protocol arguments:** `agl_shell_desktop_activate_app()`
   accepts an `app_id`, a `parameter` string (currently `NULL`), and a
   `wl_output`. Expose `--parameter` via `getopt` if needed.

4. **Return exit codes:** Currently exits `0` always after sending the
   protocol message. Add meaningful exit codes (e.g., `1` if
   `agl_shell_desktop` was not found, `2` if no outputs available).

---

## Review and Submission

This project uses **Gerrit** (AGL's review system) for code review:

- Remote: `ssh://gerrit.automotivelinux.org:29418/src/agl-shell-activator`
- Branch: `master` (Lucky Lamprey release line uses `lucky`)
- Commit messages must include a `Change-Id:` Gerrit trailer (use the
  `commit-msg` hook from Gerrit).
- Reference AGL Jira issues in commit messages as `Bug-AGL: SPEC-NNNN`.

---

## Testing

There are **no automated tests** in this repository. Testing is done
manually on an AGL target device:

```sh
# On an AGL device with agl-compositor running:
agl-activator <app-id>
# or:
XDG_RUNTIME_DIR=/run/platform/display agl-shell-activator <app-id>
```

When adding features, manually verify:

1. The binary connects to the Wayland socket without error.
2. The target application is brought to the foreground on the correct output.
3. The binary exits cleanly (no leaks, no hanging).

---

## Do Not

- Do not vendor or copy-paste generated Wayland protocol files into the tree.
- Do not add runtime configuration files; this tool is intentionally stateless.
- Do not link against libraries other than `libwayland-client` without
  a strong justification and updating `meson.build`.
- Do not use `exit()` inside library-like helper functions; propagate errors
  via return values instead.
