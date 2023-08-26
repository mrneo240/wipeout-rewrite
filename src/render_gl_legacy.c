
// macOS
#if defined(__APPLE__) && defined(__MACH__)
	#include <OpenGL/gl.h>
	#include <OpenGL/glext.h>

	void glCreateTextures(GLuint ignored, GLsizei n, GLuint *name) {
		glGenTextures(1, name);
	}
// Linux
#elif defined(__unix__)
	#include <GL/glew.h>

// Dreamcast
#elif defined(_arch_dreamcast)
	#include "gl.h"
	#include "glext.h"
	#include "glkos.h"

	#define CUSTOM_OPENGL_IMPL 1

// WINDOWS
#else
	#include <windows.h>

	#define GL3_PROTOTYPES 1
	#include <glew.h>
	#pragma comment(lib, "glew32.lib")

	#include <gl/GL.h>
	#pragma comment(lib, "opengl32.lib")
#endif


#include "libs/stb_image_write.h"

#include "render.h"
#include "mem.h"
#include "utils.h"

#undef RENDER_USE_MIPMAPS
#define RENDER_USE_MIPMAPS 0

#define NEAR_PLANE 16.0
#define FAR_PLANE 262144.0

#define RENDER_TRIS_BUFFER_CAPACITY 2048
#define TEXTURES_MAX 1024

typedef struct {
	vec2i_t size;
	vec2_t scale;
	GLuint texId;
} render_texture_t;

uint16_t RENDER_NO_TEXTURE;

static tris_t __attribute__((aligned(32))) tris_buffer[RENDER_TRIS_BUFFER_CAPACITY];
static uint32_t tris_len = 0;
static float screen_2d_z = -1;

static vec2i_t screen_size;

static render_blend_mode_t blend_mode = RENDER_BLEND_NORMAL;

static mat4_t projection_mat_2d = mat4_identity();
static mat4_t projection_mat_3d = mat4_identity();
static mat4_t sprite_mat = mat4_identity();
static mat4_t view_mat = mat4_identity();
static mat4_t model_mat = mat4_identity();

static render_texture_t textures[TEXTURES_MAX];
static uint32_t textures_len = 0;
static uint16_t texture_index_prev = (uint16_t)0;

static void render_flush();
void render_textures_dump(const char *path);
void render_texture_dump(unsigned int textureNum);
void create_white_texture();

void render_init(vec2i_t size) {
	#if defined(__APPLE__) && defined(__MACH__)
		// OSX
		// (nothing to do here)
		// Dreamcast
	#elif defined(_arch_dreamcast)
		// (nothing to do here)
		int i=GL_DIRECT_BUFFER_KOS;

		GLdcConfig config;
    glKosInitConfig(&config);
    config.autosort_enabled = GL_TRUE;
    config.fsaa_enabled = GL_FALSE;
    /*@Note: These should be adjusted at some point */
    config.initial_op_capacity = 1024;
    config.initial_pt_capacity = 1024;
    config.initial_tr_capacity = 1024;
    config.initial_immediate_capacity = 0;
    glKosInitEx(&config);
	#else
		// Windows, Linux
		glewExperimental = GL_TRUE;
		glewInit();
	#endif

	// Defaults

	render_resize(size);
	render_set_view(vec3(0, 0, 0), vec3(0, 0, 0));
	render_set_model_mat(&mat4_identity());

  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glEnableClientState(GL_COLOR_ARRAY);
	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glAlphaFunc (GL_GREATER, 0.0f);

	create_white_texture();
}

void create_white_texture(void) {
	// Dreamcast
#if defined(_arch_dreamcast)
	// Create white texture
	rgba_t white_pixels[64];
	for(uint32_t i = 0;i < 64;i++)
	{
		white_pixels[i] = rgba(128,128,128,255);
	}
	RENDER_NO_TEXTURE = render_texture_create(8, 8, white_pixels);
#else
	// Create white texture
	rgba_t white_pixels[4] = {
		rgba(128,128,128,255), rgba(128,128,128,255),
		rgba(128,128,128,255), rgba(128,128,128,255)
	};
	RENDER_NO_TEXTURE = render_texture_create(2, 2, white_pixels);
#endif
}

void render_cleanup(void) {
	// TODO
}

static void render_setup_2d_projection_mat(void) {
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
		-2 * lr,  0,  0,  0,
		0,  -2 * bt,  0,  0,
		0,        0,  2 * nf,    0,
		(left + right) * lr, (top + bottom) * bt, (far + near) * nf, 1
	);
}

static void render_setup_3d_projection_mat() {
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
		0, 0, 2 * FAR_PLANE * NEAR_PLANE * nf, 0
	);
}

void render_resize(vec2i_t size) {
	glViewport(0, 0, size.x, size.y);
	screen_size = size;

	render_setup_2d_projection_mat();
	render_setup_3d_projection_mat();
}

vec2i_t render_size() {
	return screen_size;
}

void render_set_resolution(render_resolution_t res) {}
void render_set_post_effect(render_post_effect_t post) {}
void render_set_screen_size(vec2i_t size) {}

void render_frame_prepare() {
	glLoadIdentity();
	glEnable(GL_DEPTH_TEST);
	glDepthMask(true);
	glDisable(GL_POLYGON_OFFSET_FILL);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, textures[RENDER_NO_TEXTURE].texId);
}

void render_frame_end() {
	render_flush();
	// Dreamcast
	#if defined(_arch_dreamcast)
		/* Lets us yield to other threads*/
		glKosSwapBuffers();
	#endif
	screen_2d_z = -1;
}

void render_flush() {
	if (tris_len == 0) {
		return;
	}

	// Send all tris
	render_texture_t *t = &textures[texture_index_prev];
	glBindTexture(GL_TEXTURE_2D, t->texId);
  glVertexPointer(3, GL_FLOAT, sizeof(vertex_t), &tris_buffer[0].vertices[0].pos);
  glTexCoordPointer(2, GL_FLOAT, sizeof(vertex_t), &tris_buffer[0].vertices[0].uv);
  glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(vertex_t), &tris_buffer[0].vertices[0].color);
  glDrawArrays(GL_TRIANGLES, 0, tris_len * 3);
	tris_len = 0;
}


void render_set_view(vec3_t pos, vec3_t angles) {
	render_flush();
	render_set_depth_write(true);
	render_set_depth_test(true);

	view_mat = mat4_identity();
	mat4_set_translation(&view_mat, vec3(0, 0, 0));
	mat4_set_roll_pitch_yaw(&view_mat, vec3(angles.x, -angles.y + M_PI, angles.z + M_PI));
	mat4_translate(&view_mat, vec3_inv(pos));
	mat4_set_yaw_pitch_roll(&sprite_mat, vec3(-angles.x, angles.y - M_PI, 0));

	render_set_model_mat(&mat4_identity());

  glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(projection_mat_3d.m);
  glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(view_mat.m);
	//glUniform2f(u_fade, RENDER_FADEOUT_NEAR, RENDER_FADEOUT_FAR);
}

void render_set_view_2d() {
	render_flush();
	render_set_depth_test(false);
	render_set_depth_write(false);

	render_set_model_mat(&mat4_identity());

  glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(projection_mat_2d.m);
  glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(mat4_identity().m);
}

void render_set_model_mat(mat4_t *m) {
	memcpy(&model_mat, m, sizeof(mat4_t));
}

void render_push_matrix() {
	glPushMatrix();
	glMultMatrixf(model_mat.m);
}

void render_pop_matrix() {
	render_flush();
	glPopMatrix();
}

void render_set_depth_write(bool enabled) {
	render_flush();
	glDepthMask(enabled);
}

void render_set_depth_test(bool enabled) {
	render_flush();
	if (enabled) {
		glEnable(GL_DEPTH_TEST);
	}
	else {
		glDisable(GL_DEPTH_TEST);
	}
}

void render_set_depth_offset(float offset) {
	render_flush();
	if (offset == 0) {
		glDisable(GL_POLYGON_OFFSET_FILL);
		return;
	}

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(offset, 5.0);
}

void render_set_screen_position(vec2_t pos) {
	render_flush();
	if(pos.x ==0 && pos.y == 0){
		//glPopMatrix();
		return;
	}

	//glPushMatrix();
	// Todo:
	// recalc new modified view matrix to move correct units in NDC
	// load that new matrix
}

void render_set_blend_mode(render_blend_mode_t new_mode) {
	if (new_mode == blend_mode) {
		return;
	}
	render_flush();

	blend_mode = new_mode;
	if (blend_mode == RENDER_BLEND_NORMAL) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else if (blend_mode == RENDER_BLEND_LIGHTER) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	}
}

void render_set_cull_backface(bool enabled) {
	render_flush();
	if (enabled) {
		glEnable(GL_CULL_FACE);
	}
	else {
		glDisable(GL_CULL_FACE);
	}
}

vec3_t render_transform(vec3_t pos) {
	return vec3_transform(vec3_transform(pos, &view_mat), &projection_mat_3d);
}

void render_push_tris(tris_t tris, uint16_t texture_index) {
	error_if(texture_index >= textures_len, "Invalid texture %d", texture_index);

	if (tris_len >= RENDER_TRIS_BUFFER_CAPACITY) {
		render_flush();
	}
	if(texture_index != texture_index_prev){
		render_flush();
	}
	texture_index_prev = texture_index;

	render_texture_t *t = &textures[texture_index];

	for (int i = 0; i < 3; i++) {
		// resize back to (0,1) uv space
		tris.vertices[i].uv.x = (tris.vertices[i].uv.x / t->size.x) * t->scale.x;
		tris.vertices[i].uv.y = (tris.vertices[i].uv.y / t->size.y) * t->scale.y;
		if(tris.vertices[i].color.as_rgba.a == 0){
			continue;
		}

		// move colors back to (0,255)
		uint8_t R = tris.vertices[i].color.as_rgba.r;
		uint8_t G = tris.vertices[i].color.as_rgba.g;
		uint8_t B = tris.vertices[i].color.as_rgba.b;
		if(R == 128){
			R = 255;
		} else {
			R *=2;
		}
		if(G == 128){
			G = 255;
		} else {
			G *=2;
		}
		if(B == 128){
			B = 255;
		} else {
			B *=2;
		}
		tris.vertices[i].color.as_rgba.r = R;
		tris.vertices[i].color.as_rgba.g = G;
		tris.vertices[i].color.as_rgba.b = B;
	}
	tris_buffer[tris_len++] = tris;
}

void render_push_sprite(vec3_t pos, vec2i_t size, rgba_t color, uint16_t texture_index) {
	error_if(texture_index >= textures_len, "Invalid texture %d", texture_index);

	screen_2d_z += 0.001f;
	vec3_t p1 = vec3_add(pos, vec3_transform(vec3(-size.x * 0.5, -size.y * 0.5, screen_2d_z), &sprite_mat));
	vec3_t p2 = vec3_add(pos, vec3_transform(vec3( size.x * 0.5, -size.y * 0.5, screen_2d_z), &sprite_mat));
	vec3_t p3 = vec3_add(pos, vec3_transform(vec3(-size.x * 0.5,  size.y * 0.5, screen_2d_z), &sprite_mat));
	vec3_t p4 = vec3_add(pos, vec3_transform(vec3( size.x * 0.5,  size.y * 0.5, screen_2d_z), &sprite_mat));

	render_texture_t *t = &textures[texture_index];
	render_push_tris((tris_t){
		.vertices = {
			{
				.pos = p1,
				.uv = {0, 0},
				.color = color
			},
			{
				.pos = p2,
				.uv = {0 + t->size.x ,0},
				.color = color
			},
			{
				.pos = p3,
				.uv = {0, 0 + t->size.y},
				.color = color
			},
		}
	}, texture_index);
	render_push_tris((tris_t){
		.vertices = {
			{
				.pos = p3,
				.uv = {0, 0 + t->size.y},
				.color = color
			},
			{
				.pos = p2,
				.uv = {0 + t->size.x, 0},
				.color = color
			},
			{
				.pos = p4,
				.uv = {0 + t->size.x, 0 + t->size.y},
				.color = color
			},
		}
	}, texture_index);
}

void render_push_2d(vec2i_t pos, vec2i_t size, rgba_t color, uint16_t texture_index) {
	render_push_2d_tile(pos, vec2i(0, 0), render_texture_size(texture_index), size, color, texture_index);
}

void render_push_2d_tile(vec2i_t pos, vec2i_t uv_offset, vec2i_t uv_size, vec2i_t size, rgba_t color, uint16_t texture_index) {
	error_if(texture_index >= textures_len, "Invalid texture %d", texture_index);

	screen_2d_z += 0.001f;
	render_push_tris((tris_t){
		.vertices = {
			{
				.pos = {pos.x, pos.y + size.y, screen_2d_z},
				.uv = {uv_offset.x , uv_offset.y + uv_size.y},
				.color = color
			},
			{
				.pos = {pos.x + size.x, pos.y, screen_2d_z},
				.uv = {uv_offset.x +  uv_size.x, uv_offset.y},
				.color = color
			},
			{
				.pos = {pos.x, pos.y, screen_2d_z},
				.uv = {uv_offset.x , uv_offset.y},
				.color = color
			},
		}
	}, texture_index);

	render_push_tris((tris_t){
		.vertices = {
			{
				.pos = {pos.x + size.x, pos.y + size.y, screen_2d_z},
				.uv = {uv_offset.x + uv_size.x, uv_offset.y + uv_size.y},
				.color = color
			},
			{
				.pos = {pos.x + size.x, pos.y, screen_2d_z},
				.uv = {uv_offset.x + uv_size.x, uv_offset.y},
				.color = color
			},
			{
				.pos = {pos.x, pos.y + size.y, screen_2d_z},
				.uv = {uv_offset.x , uv_offset.y + uv_size.y},
				.color = color
			},
		}
	}, texture_index);
}

uint32_t upper_power_of_two(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

uint16_t render_texture_create(uint32_t tw, uint32_t th, rgba_t *pixels) {
	error_if(textures_len >= TEXTURES_MAX, "TEXTURES_MAX reached");
	void *_pixels = pixels;
	rgba_t *pb = 0x0;
	uint32_t tex_width = tw;
	uint32_t tex_height = th;

	if(tw < 2 || th < 2){
		return 0;
	}

	// Dreamcast, or other platform that requires pow2 textures
#if defined(_arch_dreamcast)
	if(tw < 8 || th < 8){
		return 0;
	}

	tex_width = upper_power_of_two(tw);
	tex_height = upper_power_of_two(th);
	// Check if npot and pad if not
	if(tw != tex_width || th != tex_height) {
		pb = mem_temp_alloc(sizeof(rgba_t) * tex_width * tex_height);
		memset(pb, 0, sizeof(rgba_t) * tex_width * tex_height);

		// Texture
		for (int32_t y = 0; y < th; y++) {
			memcpy(pb + tex_width * y , pixels + tw * y, tw * sizeof(rgba_t));
		}
		_pixels = pb;
		printf("padding texture (%3d x %3d) -> (%3d x %3d)\n", tw, th, tex_width, tex_height);
	}
#endif
	GLuint texId;
  glGenTextures(1, &texId);
  glBindTexture(GL_TEXTURE_2D, texId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, _pixels);

	if(pb){
		mem_temp_free(pb);
	}

	uint16_t texture_index = textures_len;
	textures_len++;
	textures[texture_index] = (render_texture_t){ {tw, th}, {((float)tw)/((float)tex_width), ((float)th)/((float)tex_height)}, texId};

	printf("created texture (%3d x %3d) size %dkb\n", tw, th, (tw*th)/1024);

	// render_texture_dump(texture_index);
	return texture_index;
}

vec2i_t render_texture_size(uint16_t texture_index) {
	error_if(texture_index >= textures_len, "Invalid texture %d", texture_index);
	return textures[texture_index].size;
}

// Only used by pl_mpeg for intro video
void render_texture_replace_pixels(int16_t texture_index, rgba_t *pixels) {
	error_if(texture_index >= textures_len, "Invalid texture %d", texture_index);

	render_texture_t *t = &textures[texture_index];
  glBindTexture(GL_TEXTURE_2D, t->texId);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, t->size.x, t->size.y, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

uint16_t render_textures_len() {
	return textures_len;
}

void render_textures_reset(uint16_t len) {
	error_if(len > textures_len, "Invalid texture reset len %d >= %d", len, textures_len);
	printf("render_textures_reset: resetting to %d of %d\n", len, textures_len);
	render_flush();
	GLuint texId;

	// Clear completely and recreate the default white texture
	if (len == 0) {
		for(int i=0;i < textures_len; i++){
			texId = textures[i].texId;
			glDeleteTextures(1, &texId);
		}
		create_white_texture();
		return;
	}

	// Delete everything above
	for(int i=len;i < textures_len; i++){
		texId = textures[i].texId;
		glDeleteTextures(1, &texId);
	}

	textures_len = len;
}

#if defined(CUSTOM_OPENGL_IMPL)
void render_textures_dump(const char *path) { }
void render_texture_dump(unsigned int textureNum) { }
#else
void render_textures_dump(const char *path) {
	/*int width = ATLAS_SIZE * ATLAS_GRID;
	int height = ATLAS_SIZE * ATLAS_GRID;
	rgba_t *pixels = malloc(sizeof(rgba_t) * width * height);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	stbi_write_png(path, width, height, 4, pixels, 0);
	free(pixels);
	*/
}

void render_texture_dump(unsigned int textureNum) {
	int width = textures[textureNum].size.x;
	int height = textures[textureNum].size.y;
	width = upper_power_of_two(width);
	height = upper_power_of_two(height);

	rgba_t *pixels = malloc(sizeof(rgba_t) * width * height);
	glBindTexture(GL_TEXTURE_2D, textures[textureNum].texId);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	char path[64];
	memset(path, 0, 64);
	sprintf(path,"dump_pad/texture_%d.png", textureNum);
	stbi_write_png(path, width, height, 4, pixels, 0);
	free(pixels);
}
#endif
