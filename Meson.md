# Instructions

# PC Linux
## Setup
1. `meson builddir`
  1. specify either `-Dplatform` with [sokol, sdl]
  1. specify either `-Drenderer` with [gl, gl_legacy]

## Compile
1. `meson compile -C builddir`

# Dreamcast (kos)
## Setup
1. `meson dc_builddir --cross-file sh4-dreamcast-kos -Dplatform=dc -Drenderer=gl_legacy`
  1. `-Dplatform` must be dc
  1. `-Drenderer` must be gl_legacy

## Compile
1. `meson compile -C dc_builddir`
