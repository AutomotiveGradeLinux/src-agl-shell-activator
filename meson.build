project('agl-shell-activator',
  'c',
  version: '0.0.1',
  default_options: [
    'warning_level=3',
    'c_std=gnu99',
  ],
  meson_version: '>= 0.50',
  license: 'MIT/Expat',
)

pkgconfig = import('pkgconfig')
cc = meson.get_compiler('c')

add_project_arguments(
  cc.get_supported_arguments([
    '-Wno-unused-parameter',
    '-Wno-pedantic',
    '-Wextra',
    '-Werror'
  ]),
  language: 'c'
)

add_project_arguments([
    '-DPACKAGE_STRING="agl-shell-activator @0@"'.format(meson.project_version()),
    '-D_GNU_SOURCE',
    '-D_ALL_SOURCE',
  ],
  language: 'c'
)

optional_libc_funcs = [ 'memfd_create', 'strchrnul' ]
foreach func: optional_libc_funcs
    if cc.has_function(func)
        add_project_arguments('-DHAVE_@0@=1'.format(func.to_upper()), language: 'c')
    endif
endforeach

env_modmap = ''

libwayland_dep = dependency('wayland-client')
agl_compositor_dep = dependency('agl-compositor-0.0.20-protocols')

dep_scanner = dependency('wayland-scanner', native: true)
prog_scanner = find_program(dep_scanner.get_pkgconfig_variable('wayland_scanner'))
dep_wp = dependency('wayland-protocols', version: '>= 1.18')
dir_wp_base = dep_wp.get_pkgconfig_variable('pkgdatadir')
dir_agl_compositor_base = agl_compositor_dep.get_pkgconfig_variable('pkgdatadir')

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

prefix_path = get_option('prefix')
binplugin_dir = join_paths(prefix_path, get_option('bindir'))
common_inc = include_directories('include')

deps_agl_activator = [
    libwayland_dep,
]

srcs_agl_activator = [
    'src/main.c',
    agl_shell_desktop_client_protocol_h,
    agl_shell_desktop_protocol_c,
]

exe_wth_receiver = executable(
    'agl-shell-activator',
    srcs_agl_activator,
    include_directories: common_inc,
    dependencies: deps_agl_activator,
    install_rpath: binplugin_dir,
    install: true
)
