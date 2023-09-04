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
#else
	#include "types_generic.h"

	typedef double scalar_t;
#endif

#endif
