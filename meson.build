project('notte', 'c')

src = [
  'src/main.c',
  'src/log.c',
  'src/error.c',
  'src/membuf.c',
  'src/model.c',
  'src/memory.c',
  'src/obj.c',
  'src/renderer.c',
  'src/plat_win32.c',
  'src/bson.c',
  'src/linear_allocator.c',
  'src/dict.c',
  'src/string.c',
  'src/fs.c',
  'src/material.c',
  'src/render_graph.c',
  'src/vk_mem.c',
  'src/thread.c',
  'src/image.c',
]

cc = meson.get_compiler('c')

add_global_arguments('-DUNICODE', language : 'c')

deps_path = meson.current_source_dir() / 'deps'
deps_inc = include_directories(deps_path)

shaderc_inc = include_directories(deps_path / 'shaderc/include')
shaderc_dep = cc.find_library(
  'shaderc_combined', 
  dirs : [deps_path / 'shaderc/lib']
)

inc = include_directories('include')

vk = dependency('vulkan')

notte = executable(
  'notte', 
  src, 
  dependencies : [vk, shaderc_dep], 
  include_directories : [inc, deps_inc, shaderc_inc]
)
