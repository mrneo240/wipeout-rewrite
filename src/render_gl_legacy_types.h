#ifndef RENDER_GL_LEGACY_TYPES_H
#define RENDER_GL_LEGACY_TYPES_H

#include "types.h"

typedef struct __attribute__((packed, aligned(4))) {
  uint32_t flags;
  vec3_t pos;
  vec2_t uv;
  rgba_t color;  //bgra
  union {
    float pad;
    uint32_t vertindex;
  } pad0;
} vertex_t;

typedef struct {
	vertex_t vertices[3];
} tris_t;

#endif
