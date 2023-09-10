#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// Avoid compiler warnings for unused variables
#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

// Dreamcast
#if defined(_arch_dreamcast)
	#include "types_dreamcast.h"

	typedef float scalar_t;
#elif defined(__PSP__)
	#include "types_psp.h"

	typedef float scalar_t;

	// Platform specific math funcs
	#define platform_sin(x) sin(x)
	#define platform_cos(x) cos(x)
	#define platform_sqrt(x) sqrt(x)
#else
	#include "types_generic.h"

	typedef double scalar_t;
#endif

typedef struct {
	tris_t tri;
	int16_t texture_id;
} tris_texid_t;

typedef struct ObjectVertexChunk {
	int16_t texture_id;
	int16_t tris_len;
	tris_t *tris;
} ObjectVertexChunk;

#endif
