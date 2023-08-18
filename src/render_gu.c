// Sony PSP
#if defined(__PSP__)

// Everything else
#else
#error "Renderer only valid for Sony PSP!"
#endif

#include "render.h"
#include "mem.h"
#include "utils.h"

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <string.h>

#include "psp_texture_manager.h"

#define BUF_WIDTH (512)
#define SCR_WIDTH (480)
#define SCR_HEIGHT (272)

#undef RENDER_USE_MIPMAPS
#define RENDER_USE_MIPMAPS 0

#define NEAR_PLANE 16.0
#define FAR_PLANE 262144.0

#define RENDER_TRIS_BUFFER_CAPACITY 2048
#define TEXTURES_MAX 1024

typedef struct
{
	vec2i_t size;
	vec2_t scale;
	uint32_t texId;
} render_texture_t;

uint16_t RENDER_NO_TEXTURE;

static tris_t __attribute__((aligned(32))) tris_buffer[RENDER_TRIS_BUFFER_CAPACITY];
static uint32_t tris_len = 0;

static vec2i_t screen_size;

static render_blend_mode_t blend_mode = RENDER_BLEND_NORMAL;

static mat4_t __attribute__((aligned(16))) projection_mat_2d = mat4_identity();
static mat4_t __attribute__((aligned(16))) projection_mat_3d = mat4_identity();
static mat4_t __attribute__((aligned(16))) sprite_mat = mat4_identity();
static mat4_t __attribute__((aligned(16))) view_mat = mat4_identity();
static mat4_t __attribute__((aligned(16))) model_mat = mat4_identity();
static mat4_t __attribute__((aligned(16))) identity_matrix = mat4_identity();

static render_texture_t textures[TEXTURES_MAX];
static uint32_t textures_len = 0;
static uint16_t texture_index_prev = (uint16_t)0;

Texman_State ramTexman;
Texman_State vramTexman;

// PSP Render setup
static unsigned int staticOffset = 0;
static unsigned int __attribute__((aligned(64))) list[(256+64) * 1024 * 2];

static unsigned int getMemorySize(unsigned int width, unsigned int height, unsigned int psm)
{
	switch (psm)
	{
	case GU_PSM_T4:
		return (width * height) >> 1;

	case GU_PSM_T8:
		return width * height;

	case GU_PSM_5650:
	case GU_PSM_5551:
	case GU_PSM_4444:
	case GU_PSM_T16:
		return 2 * width * height;

	case GU_PSM_8888:
	case GU_PSM_T32:
		return 4 * width * height;

	default:
		return 0;
	}
}

#define TEX_ALIGNMENT (16)
void *getStaticVramBuffer(unsigned int width, unsigned int height, unsigned int psm)
{
	unsigned int memSize = getMemorySize(width, height, psm);
	void *result = (void *)(staticOffset | 0x40000000);
	staticOffset += memSize;

	return result;
}

void *getStaticVramBufferBytes(size_t bytes)
{
	unsigned int memSize = bytes;
	void *result = (void *)(staticOffset | 0x40000000);
	staticOffset += memSize;

	return (void *)(((unsigned int)result) + ((unsigned int)sceGeEdramGetAddr()));
}

static void render_flush();
void render_textures_dump(const char *path);
void render_texture_dump(unsigned int textureNum);
void create_white_texture();

void render_init(vec2i_t size)
{
	sceGuInit();

	void *fbp0 = getStaticVramBuffer(BUF_WIDTH, SCR_HEIGHT, GU_PSM_5650);
	void *fbp1 = getStaticVramBuffer(BUF_WIDTH, SCR_HEIGHT, GU_PSM_5650);
	void *zbp = getStaticVramBuffer(BUF_WIDTH, SCR_HEIGHT, GU_PSM_4444);

	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_5650, fbp0, BUF_WIDTH);
	sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, fbp1, BUF_WIDTH);
	sceGuDepthBuffer(zbp, BUF_WIDTH);
	sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
	sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
	sceGuDepthRange(0xffff, 0);
	sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuEnable(GU_DEPTH_TEST);
	sceGuDepthFunc(GU_GEQUAL);
	sceGuShadeModel(GU_SMOOTH);
	sceGuEnable(GU_CLIP_PLANES);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuDisable(GU_LIGHTING);
	sceGuEnable(GU_BLEND);
	sceGuDisable(GU_CULL_FACE);
	sceGuFrontFace(GU_CCW);
	sceGuDepthMask(GU_FALSE);
	sceGuTexEnvColor(0xffffffff);
	sceGuTexOffset(0.0f, 0.0f);
	sceGuTexWrap(GU_CLAMP, GU_CLAMP);
	sceGuAlphaFunc(GU_GREATER, 0, 0xff);

	sceGuFinish();
	sceGuSync(0, 0);
	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);

	uint32_t buffer_size = 10 * 1024 * 1024;
	void *texman_buffer = malloc(buffer_size); // getStaticVramBufferBytes(buffer_size);
	void *texman_aligned = (void *)((((unsigned int)texman_buffer + TEX_ALIGNMENT - 1) / TEX_ALIGNMENT) * TEX_ALIGNMENT);
	texman_reset(&vramTexman, texman_aligned, buffer_size);
	if (!texman_buffer)
	{
		fprintf(2,"OUT OF MEMORY!\n");
		fflush(2);

		sceKernelExitGame();
	}

	// Defaults

	render_resize(size);
	render_set_view(vec3(0, 0, 0), vec3(0, 0, 0));
	render_set_model_mat(&mat4_identity());

	create_white_texture();
}

void create_white_texture(void)
{
	// Create white texture
	rgba_t white_pixels[4] = {
			rgba(128, 128, 128, 255), rgba(128, 128, 128, 255),
			rgba(128, 128, 128, 255), rgba(128, 128, 128, 255)};
	RENDER_NO_TEXTURE = render_texture_create(2, 2, white_pixels);
}

void render_cleanup(void)
{
	// TODO
}

static void render_setup_2d_projection_mat(void)
{
	float near = -1;
	float far = 1;
	float left = 0;
	float right = screen_size.x;
	float bottom = screen_size.y;
	float top = 0;
	float lr = 1 / (left - right);
	float bt = 1 / (bottom - top);
	float nf = 1 / (near - far);
	projection_mat_2d = mat4(
			-2 * lr, 0, 0, 0,
			0, -2 * bt, 0, 0,
			0, 0, 2 * nf, 0,
			(left + right) * lr, (top + bottom) * bt, (far + near) * nf, 1);
}

static void render_setup_3d_projection_mat()
{
	// wipeout has a horizontal fov of 90deg, but we want the fov to be fixed
	// for the vertical axis, so that widescreen displays just have a wider
	// view. For the original 4/3 aspect ratio this equates to a vertial fov
	// of 73.75deg.
	float aspect = (float)screen_size.x / (float)screen_size.y;
	float fov = (73.75 / 180.0) * 3.14159265358;
	float f = 1.0 / tan(fov / 2);
	float nf = 1.0 / (NEAR_PLANE - FAR_PLANE);
	projection_mat_3d = mat4(
			f / aspect, 0, 0, 0,
			0, f, 0, 0,
			0, 0, (FAR_PLANE + NEAR_PLANE) * nf, -1,
			0, 0, 2 * FAR_PLANE * NEAR_PLANE * nf, 0);
}

void render_resize(vec2i_t size)
{
	sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
	sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
	screen_size = size;

	render_setup_2d_projection_mat();
	render_setup_3d_projection_mat();
}

vec2i_t render_size()
{
	return screen_size;
}

void render_frame_prepare()
{
	sceGuSetMatrix(GU_VIEW, (const ScePspFMatrix4 *)identity_matrix.m);
	sceGuSetMatrix(GU_MODEL, (const ScePspFMatrix4 *)identity_matrix.m);
	sceGuStart(GU_DIRECT, list);
	sceGuDisable(GU_SCISSOR_TEST);
	sceGuDepthMask(GU_TRUE); // Must be set to clear Z-buffer
	sceGuClearColor(0xFF000000);
	sceGuClearDepth(0);
	sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
	sceGuEnable(GU_SCISSOR_TEST);

	//sceGuEnable(GU_DEPTH_TEST);
	//sceGuDepthMask(GU_TRUE);
	//sceGuDepthOffset(0);
	sceGuEnable(GU_TEXTURE_2D);
}

void render_frame_end()
{
	render_flush();

	sceGuFinish();
	sceGuSync(0, 0);
	sceDisplayWaitVblankStart();
	sceGuSwapBuffers();
}

void render_flush()
{
	if (tris_len == 0)
	{
		return;
	}

	// Send all tris
	render_texture_t *t = &textures[texture_index_prev];
	texman_bind_tex(&vramTexman, t->texId);

	void *buf = (void *)ALIGN((unsigned int)sceGuGetMemory(sizeof(tris_t) * tris_len + 15), 16);
	memcpy(buf, tris_buffer, sizeof(tris_t) * tris_len);
	sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 3 * tris_len, 0, buf);

	tris_len = 0;
}

void render_set_view(vec3_t pos, vec3_t angles)
{
	render_flush();
	render_set_depth_write(true);
	render_set_depth_test(true);

	view_mat = mat4_identity();
	mat4_set_translation(&view_mat, vec3(0, 0, 0));
	mat4_set_roll_pitch_yaw(&view_mat, vec3(angles.x, -angles.y + M_PI, angles.z + M_PI));
	mat4_translate(&view_mat, vec3_inv(pos));
	mat4_set_yaw_pitch_roll(&sprite_mat, vec3(-angles.x, angles.y - M_PI, 0));

	render_set_model_mat(&mat4_identity());

	sceGuSetMatrix(GU_PROJECTION, (const ScePspFMatrix4 *)projection_mat_3d.m);

	/* Allocate space in DL for current model matrix */
	void *matrix_inline_view = (void *)ALIGN((unsigned int)sceGuGetMemory(sizeof(mat4_t) + 15), 16);
	memcpy(matrix_inline_view, view_mat.m, sizeof(mat4_t));
	sceGuSetMatrix(GU_VIEW, (const ScePspFMatrix4 *)matrix_inline_view);

	// glUniform2f(u_fade, RENDER_FADEOUT_NEAR, RENDER_FADEOUT_FAR);
}

void render_set_view_2d()
{
	render_flush();
	render_set_depth_test(false);
	render_set_depth_write(false);

	render_set_model_mat(&mat4_identity());

	sceGuSetMatrix(GU_PROJECTION, (const ScePspFMatrix4 *)projection_mat_2d.m);
	sceGuSetMatrix(GU_VIEW, (const ScePspFMatrix4 *)identity_matrix.m);
	sceGuSetMatrix(GU_MODEL, (const ScePspFMatrix4 *)identity_matrix.m);
}

void render_set_model_mat(mat4_t *m)
{
	memcpy(&model_mat, m, sizeof(mat4_t));
}

void render_push_matrix()
{
	/* Allocate space in DL for current model matrix */
	void *matrix_inline_model = (void *)ALIGN((unsigned int)sceGuGetMemory(sizeof(mat4_t) + 15), 16);
	memcpy(matrix_inline_model, model_mat.m, sizeof(mat4_t));
	sceGuSetMatrix(GU_MODEL, (const ScePspFMatrix4 *)matrix_inline_model);
}
void render_pop_matrix()
{
	render_flush();

	sceGuSetMatrix(GU_MODEL, (const ScePspFMatrix4 *)identity_matrix.m);
}

void render_set_depth_write(bool enabled)
{
	render_flush();
	sceGuDepthMask(enabled ? GU_FALSE : GU_TRUE);
}

void render_set_depth_test(bool enabled)
{
	render_flush();
	if (enabled)
	{
		sceGuEnable(GU_DEPTH_TEST);
	}
	else
	{
		sceGuDisable(GU_DEPTH_TEST);
	}
}

void render_set_depth_offset(float offset)
{
	render_flush();
	if (offset == 0)
	{
		sceGuDepthOffset(0);
		return;
	}

	sceGuDepthOffset((unsigned int)offset); /* I think we need a little more on psp because of 16bit depth buffer */
}

void render_set_screen_position(vec2_t pos)
{
	render_flush();
	// glUniform2f(u_screen, pos.x, -pos.y);
}

void render_set_blend_mode(render_blend_mode_t new_mode)
{
	if (new_mode == blend_mode)
	{
		return;
	}
	render_flush();

	blend_mode = new_mode;
	if (blend_mode == RENDER_BLEND_NORMAL)
	{
		sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	}
	else if (blend_mode == RENDER_BLEND_LIGHTER)
	{
		sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_DST_ALPHA, 0, 0);
	}
}

void render_set_cull_backface(bool enabled)
{
	render_flush();
	if (enabled)
	{
		sceGuEnable(GU_CULL_FACE);
	}
	else
	{
		sceGuDisable(GU_CULL_FACE);
	}
}

vec3_t render_transform(vec3_t pos)
{
	return vec3_transform(vec3_transform(pos, &view_mat), &projection_mat_3d);
}

void render_push_tris(tris_t tris, uint16_t texture_index)
{
	error_if(texture_index >= textures_len, "Invalid texture %d", texture_index);

	if (tris_len >= RENDER_TRIS_BUFFER_CAPACITY)
	{
		render_flush();
	}
	if (texture_index != texture_index_prev)
	{
		render_flush();
	}
	texture_index_prev = texture_index;

	render_texture_t *t = &textures[texture_index];

	for (int i = 0; i < 3; i++)
	{
		// resize back to (0,1) uv space
		tris.vertices[i].uv.x = (tris.vertices[i].uv.x / t->size.x) * t->scale.x;
		tris.vertices[i].uv.y = (tris.vertices[i].uv.y / t->size.y) * t->scale.y;
		if (tris.vertices[i].color.as_rgba.a == 0)
		{
			continue;
		}

		// move colors back to (0,255)
		uint8_t R = tris.vertices[i].color.as_rgba.r;
		uint8_t G = tris.vertices[i].color.as_rgba.g;
		uint8_t B = tris.vertices[i].color.as_rgba.b;
		if (R == 128)
		{
			R = 255;
		}
		else
		{
			R *= 2;
		}
		if (G == 128)
		{
			G = 255;
		}
		else
		{
			G *= 2;
		}
		if (B == 128)
		{
			B = 255;
		}
		else
		{
			B *= 2;
		}
		tris.vertices[i].color.as_rgba.r = R;
		tris.vertices[i].color.as_rgba.g = G;
		tris.vertices[i].color.as_rgba.b = B;
	}
	tris_buffer[tris_len++] = tris;
}

void render_push_sprite(vec3_t pos, vec2i_t size, rgba_t color, uint16_t texture_index)
{
	error_if(texture_index >= textures_len, "Invalid texture %d", texture_index);

	vec3_t p1 = vec3_add(pos, vec3_transform(vec3(-size.x * 0.5, -size.y * 0.5, 0), &sprite_mat));
	vec3_t p2 = vec3_add(pos, vec3_transform(vec3(size.x * 0.5, -size.y * 0.5, 0), &sprite_mat));
	vec3_t p3 = vec3_add(pos, vec3_transform(vec3(-size.x * 0.5, size.y * 0.5, 0), &sprite_mat));
	vec3_t p4 = vec3_add(pos, vec3_transform(vec3(size.x * 0.5, size.y * 0.5, 0), &sprite_mat));

	render_texture_t *t = &textures[texture_index];
	render_push_tris((tris_t){
											 .vertices = {
													 {.pos = p1,
														.uv = {0, 0},
														.color = color},
													 {.pos = p2,
														.uv = {0 + t->size.x, 0},
														.color = color},
													 {.pos = p3,
														.uv = {0, 0 + t->size.y},
														.color = color},
											 }},
									 texture_index);
	render_push_tris((tris_t){
											 .vertices = {
													 {.pos = p3,
														.uv = {0, 0 + t->size.y},
														.color = color},
													 {.pos = p2,
														.uv = {0 + t->size.x, 0},
														.color = color},
													 {.pos = p4,
														.uv = {0 + t->size.x, 0 + t->size.y},
														.color = color},
											 }},
									 texture_index);
}

void render_push_2d(vec2i_t pos, vec2i_t size, rgba_t color, uint16_t texture_index)
{
	render_push_2d_tile(pos, vec2i(0, 0), render_texture_size(texture_index), size, color, texture_index);
}

void render_push_2d_tile(vec2i_t pos, vec2i_t uv_offset, vec2i_t uv_size, vec2i_t size, rgba_t color, uint16_t texture_index)
{
	error_if(texture_index >= textures_len, "Invalid texture %d", texture_index);

	render_push_tris((tris_t){
											 .vertices = {
													 {.pos = {pos.x, pos.y + size.y, 0},
														.uv = {uv_offset.x, uv_offset.y + uv_size.y},
														.color = color},
													 {.pos = {pos.x + size.x, pos.y, 0},
														.uv = {uv_offset.x + uv_size.x, uv_offset.y},
														.color = color},
													 {.pos = {pos.x, pos.y, 0},
														.uv = {uv_offset.x, uv_offset.y},
														.color = color},
											 }},
									 texture_index);

	render_push_tris((tris_t){
											 .vertices = {
													 {.pos = {pos.x + size.x, pos.y + size.y, 0},
														.uv = {uv_offset.x + uv_size.x, uv_offset.y + uv_size.y},
														.color = color},
													 {.pos = {pos.x + size.x, pos.y, 0},
														.uv = {uv_offset.x + uv_size.x, uv_offset.y},
														.color = color},
													 {.pos = {pos.x, pos.y + size.y, 0},
														.uv = {uv_offset.x, uv_offset.y + uv_size.y},
														.color = color},
											 }},
									 texture_index);
}

uint16_t render_texture_create(uint32_t tw, uint32_t th, rgba_t *pixels)
{
	error_if(textures_len >= TEXTURES_MAX, "TEXTURES_MAX reached");
	rgba_t *pb = 0x0;
	uint32_t tex_width = tw;
	uint32_t tex_height = th;
	uint32_t texId = 0xffff;

	if( tw < 4 || th < 4){
		return RENDER_NO_TEXTURE;
	}

	if (ispow2(tex_width) && ispow2(tex_height))
	{
		if (!texman_space_available(&vramTexman, tex_width * tex_height * 4))
		{
			texman_clear(&vramTexman);
			printf("EMPTYING TEXTURE CACHE!\n");
		}

		texId = texman_create(&vramTexman);
		texman_upload_swizzle(&vramTexman, tex_width, tex_height, GU_PSM_8888, pixels);
	}
	else
	{
		tex_width = upper_power_of_two(tw);
		tex_height = upper_power_of_two(th);
		if (!texman_space_available(&vramTexman, tex_width * th * 4))
		{
			printf("EMPTYING TEXTURE CACHE!\n");
			texman_clear(&vramTexman);
		}
		texId = texman_create(&vramTexman);
		pb = mem_temp_alloc(sizeof(rgba_t) * tex_width * th);
		memset(pb, 0, sizeof(rgba_t) * tex_width * th);

		// Texture
		for (int32_t y = 0; y < th; y++)
		{
			memcpy(pb + tex_width * y, pixels + tw * y, tw * sizeof(rgba_t));
		}
		printf("padding texture (%d x %d) -> (%d x %d)\n", tw, th, tex_width, th);
		texman_upload_swizzle(&vramTexman, tex_width, th, GU_PSM_8888, (void *)pb);
	}

	sceGuTexFilter(GU_NEAREST, GU_LINEAR);
	sceGuTexWrap(GU_CLAMP, GU_CLAMP);

	if (pb)
	{
		mem_temp_free(pb);
	}

	if (texId == 0xffff)
	{
		fprintf(2, "Texman Creation error!\n");
		return 0;
	}

	uint16_t texture_index = textures_len;
	textures_len++;
	textures[texture_index] = (render_texture_t){{tw, th}, {((float)tw) / ((float)tex_width), ((float)th) / ((float)tex_height)}, texId};

	printf("created texture (%d x %d)  scaled (%f x %f)\n", tw, th, ((float)tw) / ((float)tex_width), ((float)th) / ((float)tex_height));

	// render_texture_dump(texture_index);
	return texture_index;
}

vec2i_t render_texture_size(uint16_t texture_index)
{
	error_if(texture_index >= textures_len, "Invalid texture %d", texture_index);
	return textures[texture_index].size;
}

// Only used by pl_mpeg for intro video
void render_texture_replace_pixels(int16_t texture_index, rgba_t *pixels)
{
	error_if(texture_index >= textures_len, "Invalid texture %d", texture_index);

	render_texture_t *t = &textures[texture_index];
	// glBindTexture(GL_TEXTURE_2D, t->texId);
	// glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, t->size.x, t->size.y, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

uint16_t render_textures_len()
{
	return textures_len;
}

void render_textures_reset(uint16_t len)
{
	error_if(len > textures_len, "Invalid texture reset len %d >= %d", len, textures_len);
	printf("render_textures_reset: resetting to %d of %d\n", len, textures_len);
	render_flush();
	uint32_t texId;

	// Clear completely and recreate the default white texture
	if (len == 0)
	{
		for (int i = 0; i < textures_len; i++)
		{
			texId = textures[i].texId;
			// glDeleteTextures(1, &texId);
		}
		create_white_texture();
		return;
	}

	// Delete everything above
	for (int i = len; i < textures_len; i++)
	{
		texId = textures[i].texId;
		// glDeleteTextures(1, &texId);
	}

	textures_len = len;
}

void render_textures_dump(const char *path) {}
void render_texture_dump(unsigned int textureNum) {}
