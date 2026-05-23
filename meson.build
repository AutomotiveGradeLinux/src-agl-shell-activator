# SPDX-License-Identifier: MIT
# Copyright 2021 Collabora, Ltd.
#
# Meson build definition for agl-shell-activator.
#
# Overview
# --------
# This file is the sole build system entry point.  It:
#   1. Declares project metadata (name, language, version, license).
#   2. Sets compiler flags (warnings-as-errors, GNU99, feature macros).
#   3. Probes the host libc for optional functions (memfd_create, strchrnul).
#   4. Resolves all external dependencies via pkg-config.
#   5. Runs wayland-scanner to code-generate the agl-shell-desktop protocol
#      glue (client header, server header, and implementation source).
#   6. Builds the agl-shell-activator executable.
#   7. Installs the agl-activator wrapper script alongside the binary.
#   8. Optionally builds and installs the libcheck unit test suite
#      (enable with -Denable-tests=enabled).
#
# Dependencies (all resolved via pkg-config)
# ------------------------------------------
#   wayland-client                    Core Wayland client library
#   agl-compositor-0.0.19-protocols   AGL compositor protocol XML files
#   wayland-scanner  (native tool)    Protocol code-generation tool
#   wayland-protocols >= 1.18         Base Wayland protocol data directory
#   check (optional)                  libcheck unit test framework
#
# Generated files (build directory, not in version control)
# ----------------------------------------------------------
#   agl-shell-desktop-client-protocol.h
#   agl-shell-desktop-server-protocol.h
#   agl-shell-desktop-protocol.c
#
# Install targets
# ---------------
#   <prefix>/bin/agl-shell-activator              Compiled binary
#   <prefix>/bin/agl-activator                    Bash wrapper script
#   <prefix>/lib/agl-shell-activator/ptest/       Test binaries + run-ptest
#     test-os-compat                              OS compatibility unit tests
#     test-args                                   Argument validation tests
#     run-ptest                                   ptest entry point script

project('agl-shell-activator',
  'c',
  version: '0.0.1',
  default_options: [
    'warning_level=3',  # Enables -Wall -Wextra
    'c_std=gnu99',      # C99 with GNU extensions
  ],
  meson_version: '>= 0.50',
  license: 'MIT/Expat',
)

pkgconfig = import('pkgconfig')
cc = meson.get_compiler('c')

# Additional compiler flags beyond warning_level=3.
# -Werror promotes all warnings to errors; -Wno-* silences specific
# categories that are intentionally not enforced in this codebase.
add_project_arguments(
  cc.get_supported_arguments([
    '-Wno-unused-parameter',  # Wayland listener stubs have intentionally unused params
    '-Wno-pedantic',          # Generated protocol code may not be strictly ISO C
    '-Wextra',
    '-Werror'
  ]),
  language: 'c'
)

# Preprocessor feature macros.
# PACKAGE_STRING is used for version reporting.
# _GNU_SOURCE / _ALL_SOURCE expose GNU and platform-specific libc extensions.
add_project_arguments([
    '-DPACKAGE_STRING="agl-shell-activator @0@"'.format(meson.project_version()),
    '-D_GNU_SOURCE',
    '-D_ALL_SOURCE',
  ],
  language: 'c'
)

# Probe for optional libc functions and define HAVE_<FUNC>=1 when present.
# os-compatibility.c uses these guards to select the best available
# implementation (e.g., mkostemp vs mkstemp, strchrnul vs fallback).
optional_libc_funcs = [ 'memfd_create', 'strchrnul' ]
foreach func: optional_libc_funcs
    if cc.has_function(func)
        add_project_arguments('-DHAVE_@0@=1'.format(func.to_upper()), language: 'c')
    endif
endforeach

env_modmap = ''

# --- External dependencies ---------------------------------------------------

# Core Wayland client library (libwayland-client).
libwayland_dep = dependency('wayland-client')

# AGL compositor protocol package.  Provides the pkgdatadir where
# agl-shell-desktop.xml lives.  Pinned to the 0.0.19 API version.
agl_compositor_dep = dependency('agl-compositor-0.0.19-protocols')

# wayland-scanner: native (build-host) tool that generates C code from
# Wayland protocol XML files.  Queried for its executable path via pkgconfig.
dep_scanner = dependency('wayland-scanner', native: true)
prog_scanner = find_program(dep_scanner.get_pkgconfig_variable('wayland_scanner'))

# wayland-protocols: provides the base protocol XML data directory.
# The pkgdatadir is used to locate stable/unstable protocol XML files.
dep_wp = dependency('wayland-protocols', version: '>= 1.18')
dir_wp_base = dep_wp.get_pkgconfig_variable('pkgdatadir')
dir_agl_compositor_base = agl_compositor_dep.get_pkgconfig_variable('pkgdatadir')

# --- Protocol code generation ------------------------------------------------
#
# The 'protocols' list drives wayland-scanner for each required Wayland
# protocol extension.  Each entry is a dict with:
#   name:    base name of the protocol (also the XML file stem)
#   source:  where to find the XML:
#              'internal'      – protocol/ subdirectory of this repo
#              'wp-stable'     – stable protocols from wayland-protocols
#              'agl-compositor'– XML shipped with agl-compositor-protocols
#              anything else   – unstable wayland-protocols (requires 'version')
#
# For each protocol, wayland-scanner is run three times to produce:
#   <name>-client-protocol.h   – client-side API header
#   <name>-server-protocol.h   – server-side API header (generated, not used)
#   <name>-protocol.c          – marshal/unmarshal implementation
#
# Each output is stored as a build variable named after the output filename
# (underscorified) so it can be referenced in source lists below.
protocols = [
  { 'name': 'agl-shell-desktop', 'source': 'agl-compositor' },
]

foreach proto: protocols
  proto_name = proto['name']
  if proto['source'] == 'internal'
    base_file = proto_name
    xml_path = join_paths('protocol', '@0@.xml'.format(base_file))
  elif proto['source'] == 'wp-stable'
    base_file = proto_name
    xml_path = join_paths(dir_wp_base, 'stable', proto_name, '@0@.xml'.format(base_file))
  elif proto['source'] == 'agl-compositor'
    base_file = proto_name
    xml_path = join_paths(dir_agl_compositor_base, '@0@.xml'.format(base_file))
  else
    base_file = '@0@-unstable-@1@'.format(proto_name, proto['version'])
    xml_path = join_paths(dir_wp_base, 'unstable', proto_name, '@0@.xml'.format(base_file))
  endif

  foreach output_type: [ 'client-header', 'server-header', 'private-code' ]
    if output_type == 'client-header'
      output_file = '@0@-client-protocol.h'.format(base_file)
    elif output_type == 'server-header'
      output_file = '@0@-server-protocol.h'.format(base_file)
    else
      output_file = '@0@-protocol.c'.format(base_file)
    # wayland-scanner < 1.14.91 uses 'code' instead of 'private-code'.
    if dep_scanner.version().version_compare('< 1.14.91')
      output_type = 'code'
    endif
  endif

    var_name = output_file.underscorify()
      target = custom_target(
	  '@0@ @1@'.format(base_file, output_type),
	  command: [ prog_scanner, output_type, '@INPUT@', '@OUTPUT@' ],
	  input: xml_path,
	  output: output_file,
	  )

    set_variable(var_name, target)
  endforeach
endforeach

# --- Build targets -----------------------------------------------------------

# Installation directory for both the binary and the wrapper script.
prefix_path = get_option('prefix')
binplugin_dir = join_paths(prefix_path, get_option('bindir'))

# Include directory exposing include/os-compatibility.h to all targets.
common_inc = include_directories('include')

# Runtime dependencies for the agl-shell-activator executable.
deps_agl_activator = [
    libwayland_dep,
]

# Source files for the agl-shell-activator executable.
# Includes main.c and the two wayland-scanner-generated files.
# Note: os-compatibility.c is intentionally excluded (not needed by this tool).
srcs_agl_activator = [
    'src/main.c',
    agl_shell_desktop_client_protocol_h,  # generated client header
    agl_shell_desktop_protocol_c,          # generated protocol implementation
]

# Compile and link the agl-shell-activator binary.
exe_wth_receiver = executable(
    'agl-shell-activator',
    srcs_agl_activator,
    include_directories: common_inc,
    dependencies: deps_agl_activator,
    install_rpath: binplugin_dir,
    install: true
)

# Install the agl-activator wrapper script into the same bin directory.
# Uses cp via a custom_target so Meson tracks the install correctly.
cp = find_program('cp')
script_target = custom_target('install_script',
    command: [ cp, '@INPUT@', '@OUTPUT@' ],
    output : 'agl-activator',
    input : 'script/agl-activator',
    install : true,
    install_dir: binplugin_dir,
)

# --- Unit tests (libcheck) ---------------------------------------------------
#
# Enabled with:  meson setup build -Denable-tests=enabled
# Run with:      ninja -C build test
# ptest format:  CK_AUTOMAKE=1 ./test-os-compat
#
# Two test executables are built:
#   test-os-compat   – unit tests for every function in os-compatibility.c
#   test-args        – black-box argument-validation tests for the binary
#
# Both are installed to <libdir>/agl-shell-activator/ptest/ together with
# the run-ptest script so that the Yocto ptest infrastructure can invoke
# them on target without any additional tooling.

if get_option('enable-tests').enabled()
  check_dep = dependency('check', version: '>= 0.9.4')

  # Installation directory for all ptest artefacts.
  ptest_dir = get_option('libdir') / meson.project_name() / 'ptest'

  # --- test-os-compat --------------------------------------------------------
  # Compiles os-compatibility.c directly into the test binary so the test
  # executable has no runtime dependency on the main binary.
  test_os_compat = executable('test-os-compat',
      [
          'tests/test_os_compat.c',
          'src/os-compatibility.c',
      ],
      dependencies     : [check_dep],
      include_directories: common_inc,
      install          : true,
      install_dir      : ptest_dir,
  )

  test('os-compat unit tests',
      test_os_compat,
      protocol : 'exitcode',
      env      : ['CK_AUTOMAKE=1'],
  )

  # --- test-args -------------------------------------------------------------
  # Black-box tests: fork+exec the main binary and check its exit code.
  # The binary path is passed via the AGL_ACTIVATOR_BIN env variable so the
  # test works both from the build directory and when installed.
  test_args = executable('test-args',
      'tests/test_args.c',
      dependencies     : [check_dep],
      include_directories: common_inc,
      install          : true,
      install_dir      : ptest_dir,
  )

  test('argument validation tests',
      test_args,
      protocol : 'exitcode',
      env      : [
          'CK_AUTOMAKE=1',
          'AGL_ACTIVATOR_BIN=' + exe_wth_receiver.full_path(),
      ],
      depends  : [exe_wth_receiver],
  )

  # --- run-ptest -------------------------------------------------------------
  # Install the ptest entry-point script alongside the test binaries.
  install_data('script/run-ptest',
      install_dir  : ptest_dir,
      install_mode : 'rwxr-xr-x',
  )

endif
