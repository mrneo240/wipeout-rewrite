project('wipeout-rewrite', 'c',
  version : '0.1',
  default_options : ['warning_level=0', 'c_std=c11'])

# Compiler options
arg_base = ['-std=gnu99','-Wall','-Wno-unused-variable']
arg_code = []

src_platform = []
src_renderer = []
# Dependencies
cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : false)

# Determine Platform
platform_dep = []
opt_platform = get_option('platform')
if opt_platform == 'sdl'
  sdl2_dep = dependency('sdl2', fallback : ['sdl2', 'sdl2_dep'], native: false, default_options : ['default_library=static', 'buildtype=debugoptimized'])
  platform_dep += [sdl2_dep]
  src_platform += ['src/platform_sdl.c']

  # Compiler detection
  args_sdl = []
  sdl_include_path_full = '''#include <SDL2/SDL.h>
  void func() { SDL_Init(0); }
  '''
  sdl_include_path_short = '''#include <SDL.h>
  void func() { SDL_Init(0); }
  '''
  if sdl2_dep.type_name() == 'internal'
    args_sdl += [ '-DUSE_SDL2_INCLUDE_PATH_SHORT' ]
    message('Using SDL2 include path  <SDL.h>')
  else
    result_sdl_path_long = cc.compiles( sdl_include_path_full, name : 'sdl_include_long', dependencies : sdl2_dep )
    if result_sdl_path_long
      message('Using SDL2 include path <SDL2/SDL.h>')
    else
      result_sdl_path_short = cc.compiles( sdl_include_path_short, name : 'sdl_include_short', dependencies : sdl2_dep )
      if result_sdl_path_short
        args_sdl += [ '-DUSE_SDL2_INCLUDE_PATH_SHORT' ]
        message('Using SDL2 include path  <SDL.h>')
      else
        error('Could not find SDL2 include path')
      endif
    endif
  endif
elif opt_platform == 'sokol'
  src_platform += ['src/platform_sokol.c']

  x_dep = declare_dependency(link_args : ['-lX11','-lXcursor','-lXi', '-ldl'])
  pthread_dep = dependency('threads')
  alsa_dep = dependency('alsa')

  platform_dep += [pthread_dep, alsa_dep, x_dep]
else
  error('No platform chosen!')
endif

# Renderer Options
render_dep = []
opt_renderer = get_option('renderer')
if opt_renderer == 'gl'
  arg_base += ['-DRENDERER_GL']
  src_renderer += ['src/render_gl.c']
  glew_dep = dependency('glew')
  render_dep += [glew_dep]
else
  error('No renderer chosen!')
endif

arg_c = [ arg_base, arg_code ]
arg_linker = [ '-Wl,-Map=wipeout-rewrite.map' ]

inc_base = include_directories('src', 'src/libs', 'src/wipeout')

src_wipeout = ['src/wipeout/camera.c','src/wipeout/droid.c','src/wipeout/game.c','src/wipeout/hud.c','src/wipeout/image.c','src/wipeout/ingame_menus.c','src/wipeout/intro.c','src/wipeout/main_menu.c','src/wipeout/menu.c','src/wipeout/object.c','src/wipeout/particle.c','src/wipeout/race.c','src/wipeout/scene.c','src/wipeout/sfx.c','src/wipeout/ship_ai.c','src/wipeout/ship.c','src/wipeout/ship_player.c','src/wipeout/title.c','src/wipeout/track.c','src/wipeout/ui.c','src/wipeout/weapon.c']

src_pc = ['src/input.c','src/mem.c','src/system.c','src/types.c','src/utils.c']

src = [ src_wipeout ]
src_port = [ src_pc, src_platform, src_renderer ]
inc = [ inc_base ]

# wipeout-rewrite binary
exe = executable('wipeout-rewrite', sources: [src, src_port] , include_directories : inc,
  c_args : arg_c, link_args : arg_linker,
  dependencies : [m_dep, platform_dep, render_dep],
  install : true)
