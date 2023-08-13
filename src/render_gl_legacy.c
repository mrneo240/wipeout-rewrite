
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
	GLuint texId;
} render_texture_t;

uint16_t RENDER_NO_TEXTURE;

static tris_t tris_buffer[RENDER_TRIS_BUFFER_CAPACITY];
static uint32_t tris_len = 0;

static vec2i_t screen_size;

static render_blend_mode_t blend_mode = RENDER_BLEND_NORMAL;

static mat4_t projection_mat_2d = mat4_identity();
static mat4_t projection_mat_3d = mat4_identity();
static mat4_t sprite_mat = mat4_identity();
static mat4_t view_mat = mat4_identity();
static mat4_t model_mat = mat4_identity();

static render_texture_t textures[TEXTURES_MAX];
static uint32_t textures_len = 0;

static void render_flush();

void render_init(vec2i_t size) {
	#if defined(__APPLE__) && defined(__MACH__)
		// OSX
		// (nothing to do here)
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

	// Create white texture
	rgba_t white_pixels[4] = {
		rgba(128,128,128,255), rgba(128,128,128,255),
		rgba(128,128,128,255), rgba(128,128,128,255)
	};
	RENDER_NO_TEXTURE = render_texture_create(2, 2, white_pixels);
}

void render_cleanup() {
	// TODO
}

static void render_setup_2d_projection_mat() {
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
}

void render_flush() {
	if (tris_len == 0) {
		return;
	}

	// Send all tris
	//glBindBuffer(GL_ARRAY_BUFFER, vbo);
	//glBufferData(GL_ARRAY_BUFFER, sizeof(tris_t) * tris_len, tris_buffer, GL_DYNAMIC_DRAW);
	//glDrawArrays(GL_TRIANGLES, 0, tris_len * 3);
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

	render_flush();
  glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(projection_mat_3d.m);
  glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(view_mat.m);
	glTranslatef(pos.x, pos.y, pos.z);
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
	glPolygonOffset(offset, 1.0);
}

void render_set_screen_position(vec2_t pos) {
	render_flush();
	if(pos.x ==0 && pos.y == 0){
		glPopMatrix();
		return;
	}

	glPushMatrix();
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

	render_texture_t *t = &textures[texture_index];

	for (int i = 0; i < 3; i++) {
		// resize back to (0,1) uv space
		tris.vertices[i].uv.x /= t->size.x;
		tris.vertices[i].uv.y /= t->size.y;
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

	glPushMatrix();
	glMultMatrixf(model_mat.m);

	glBindTexture(GL_TEXTURE_2D, t->texId);

  glVertexPointer(3, GL_FLOAT, sizeof(vertex_t), &tris.vertices[0].pos);
  glTexCoordPointer(2, GL_FLOAT, sizeof(vertex_t), &tris.vertices[0].uv);
  glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(vertex_t), &tris.vertices[0].color);
  glDrawArrays(GL_TRIANGLES, 0, 3);
	glPopMatrix();
	glBindTexture(GL_TEXTURE_2D, textures[RENDER_NO_TEXTURE].texId);

	// Figure out if we need buffer
	//tris_buffer[tris_len++] = tris;
}

void render_push_sprite(vec3_t pos, vec2i_t size, rgba_t color, uint16_t texture_index) {
	error_if(texture_index >= textures_len, "Invalid texture %d", texture_index);

	vec3_t p1 = vec3_add(pos, vec3_transform(vec3(-size.x * 0.5, -size.y * 0.5, 0), &sprite_mat));
	vec3_t p2 = vec3_add(pos, vec3_transform(vec3( size.x * 0.5, -size.y * 0.5, 0), &sprite_mat));
	vec3_t p3 = vec3_add(pos, vec3_transform(vec3(-size.x * 0.5,  size.y * 0.5, 0), &sprite_mat));
	vec3_t p4 = vec3_add(pos, vec3_transform(vec3( size.x * 0.5,  size.y * 0.5, 0), &sprite_mat));

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
	render_push_tris((tris_t){
		.vertices = {
			{
				.pos = {pos.x, pos.y + size.y, 0},
				.uv = {uv_offset.x , uv_offset.y + uv_size.y},
				.color = color
			},
			{
				.pos = {pos.x + size.x, pos.y, 0},
				.uv = {uv_offset.x +  uv_size.x, uv_offset.y},
				.color = color
			},
			{
				.pos = {pos.x, pos.y, 0},
				.uv = {uv_offset.x , uv_offset.y},
				.color = color
			},
		}
	}, texture_index);

	render_push_tris((tris_t){
		.vertices = {
			{
				.pos = {pos.x + size.x, pos.y + size.y, 0},
				.uv = {uv_offset.x + uv_size.x, uv_offset.y + uv_size.y},
				.color = color
			},
			{
				.pos = {pos.x + size.x, pos.y, 0},
				.uv = {uv_offset.x + uv_size.x, uv_offset.y},
				.color = color
			},
			{
				.pos = {pos.x, pos.y + size.y, 0},
				.uv = {uv_offset.x , uv_offset.y + uv_size.y},
				.color = color
			},
		}
	}, texture_index);
}


uint16_t render_texture_create(uint32_t tw, uint32_t th, rgba_t *pixels) {
	error_if(textures_len >= TEXTURES_MAX, "TEXTURES_MAX reached");

	GLuint texId;
  glGenTextures(1, &texId);
  glBindTexture(GL_TEXTURE_2D, texId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	uint16_t texture_index = textures_len;
	textures_len++;
	textures[texture_index] = (render_texture_t){ {tw, th}, texId};

	printf("created texture (%3dx%3d) size %dkb\n", tw, th, (tw*th)/1024);

	//render_texture_dump(texture_index);
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
	return;
	/*
	textures_len = len;

	// Clear completely and recreate the default white texture
	if (len == 0) {
		rgba_t white_pixels[4] = {
			rgba(128,128,128,255), rgba(128,128,128,255),
			rgba(128,128,128,255), rgba(128,128,128,255)
		};
		RENDER_NO_TEXTURE = render_texture_create(2, 2, white_pixels);
		return;
	}

	// Replay all texture grid insertions up to the reset len
	for (int i = 0; i < textures_len; i++) {
		uint32_t grid_x = (textures[i].offset.x - ATLAS_BORDER) / ATLAS_GRID;
		uint32_t grid_y = (textures[i].offset.y - ATLAS_BORDER) / ATLAS_GRID;
		uint32_t grid_width = (textures[i].size.x + ATLAS_BORDER * 2 + ATLAS_GRID - 1) / ATLAS_GRID;
		uint32_t grid_height = (textures[i].size.y + ATLAS_BORDER * 2 + ATLAS_GRID - 1) / ATLAS_GRID;
		for (uint32_t cx = grid_x; cx < grid_x + grid_width; cx++) {
			atlas_map[cx] = grid_y + grid_height;
		}
	}*/
}

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
	rgba_t *pixels = malloc(sizeof(rgba_t) * width * height);
	glBindTexture(GL_TEXTURE_2D, textures[textureNum].texId);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	char path[64];
	memset(path, 0, 64);
	sprintf(path,"dump/texture_%d.png", textureNum);
	stbi_write_png(path, width, height, 4, pixels, 0);
	free(pixels);
}