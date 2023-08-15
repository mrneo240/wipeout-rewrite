#ifndef RENDER_GL_TYPES_H
#define RENDER_GL_TYPES_H

#include "types.h"

typedef struct {
	vec3_t pos;
	vec2_t uv;
	rgba_t color;
} vertex_t;

typedef struct {
	vertex_t vertices[3];
} tris_t;

#endif
