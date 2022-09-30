project('agl-shell-activator',
  'c', 'cpp',
  version: '0.0.2',
  default_options: [
    'warning_level=3',
    'c_std=gnu99',
  ],
  meson_version: '>= 0.60',
  license: 'MIT/Expat',
)

pkgconfig = import('pkgconfig')
cpp = meson.get_compiler('cpp')

grpcpp_reflection_dep = cpp.find_library('grpc++_reflection')
protoc = find_program('protoc')
grpc_cpp = find_program('grpc_cpp_plugin')


add_project_arguments([
    '-DPACKAGE_STRING="agl-shell-activator @0@"'.format(meson.project_version()),
    '-D_GNU_SOURCE',
    '-D_ALL_SOURCE',
  ],
  language: 'cpp'
)

protoc_gen = generator(protoc, \
                       output : ['@BASENAME@.pb.cc', '@BASENAME@.pb.h'],
                       arguments : ['--proto_path=@CURRENT_SOURCE_DIR@/src',
                         '--cpp_out=@BUILD_DIR@',
                         '@INPUT@'])

generated_protoc_sources = protoc_gen.process('src/agl_shell.proto')

grpc_gen = generator(protoc, \
                     output : ['@BASENAME@.grpc.pb.cc', '@BASENAME@.grpc.pb.h'],
                     arguments : ['--proto_path=@CURRENT_SOURCE_DIR@/src',
                       '--grpc_out=@BUILD_DIR@',
                       '--plugin=protoc-gen-grpc=' + grpc_cpp.path(),
                       '@INPUT@'])
generated_grpc_sources = grpc_gen.process('src/agl_shell.proto')

grpc_deps = [
    dependency('protobuf'),
    dependency('grpc'),
    dependency('grpc++'),
    grpcpp_reflection_dep,
]


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
    grpc_deps,
]

srcs_agl_activator = [
    'src/main.cpp',
    'src/AglShellGrpcClient.cpp',
    'src/AglShellGrpcClient.h',
    generated_protoc_sources,
    generated_grpc_sources,
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
