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


prefix_path = get_option('prefix')
binplugin_dir = join_paths(prefix_path, get_option('bindir'))
common_inc = include_directories('include')

deps_agl_activator = [
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
