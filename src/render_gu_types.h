#ifndef RENDER_GU_TYPES_H
#define RENDER_GU_TYPES_H

#include "types.h"

typedef struct __attribute__((packed, aligned(4))) {
	vec2_t uv;
	rgba_t color;
	vec3_t pos;
} vertex_t;

typedef struct __attribute__((packed, aligned(4))) {
	vertex_t vertices[3];
} tris_t;

#endif
