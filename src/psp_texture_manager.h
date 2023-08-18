/*
 * File: psp_texture_manager.h
 * Project: gfx
 * File Created: Friday, 7th August 2020 9:11:56 pm
 * Author: HaydenKow
 * -----
 * Copyright (c) 2020 Hayden Kowalchuk, Hayden Kowalchuk
 * License: BSD 3-clause "New" or "Revised" License, http://www.opensource.org/licenses/BSD-3-Clause
 */

#pragma once
#define TEX_ALIGNMENT (16)

struct PSP_Texture {
    unsigned char *location;
    int width, height;
    unsigned int type;
    unsigned int swizzled;
};

typedef struct Texman_State {
    struct PSP_Texture textures[512];
    void *psp_tex_buffer;
    void *psp_tex_buffer_start;
    void *psp_tex_buffer_max;
    unsigned int buffer_size;
    unsigned int psp_tex_number;
    unsigned int psp_tex_bound;
}Texman_State;



/* used for initialization */
int texman_inited(Texman_State *state);
void texman_reset(Texman_State *state, void *buf, unsigned int size);
void texman_set_buffer(Texman_State *state, void *buf, unsigned int size);

/* management funcs for clients
Steps:
1. create
2. reserve memory & upload by pointer OR upload_swizzle

note: texture will be bound
*/
unsigned int texman_create(Texman_State *state);
void texman_clear(Texman_State *state);
int texman_space_available(Texman_State *state, unsigned int size);
unsigned char *texman_get_tex_data(Texman_State *state, unsigned int num);
struct PSP_Texture *texman_reserve_memory(Texman_State *state, int width, int height, unsigned int type);
void texman_upload_swizzle(Texman_State *state, int width, int height, unsigned int type, const void *buffer);
void texman_upload(Texman_State *state, int width, int height, unsigned int type, const void *buffer);
void texman_store(Texman_State *state, int width, int height, unsigned int type, const void *buffer);
void texman_bind_tex(Texman_State *state, unsigned int num);

static inline int ispow2(int x)
{
	return (x & (x - 1)) == 0;
}

static inline unsigned int upper_power_of_two(unsigned int v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v |= v >> 32;
	v |= v >> 64;
	v++;
	return v;
}