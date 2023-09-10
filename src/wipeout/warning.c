#include "../system.h"
#include "../input.h"
#include "../utils.h"

#include "warning.h"
#include "platform.h"
#include "ui.h"
#include "image.h"
#include "game.h"

static uint16_t warning_image;
static float start_time;

void warning_init(void) {
	const char *warning_path = "wipeout/textures/wipeout2.tim";
	if(platform_file_exists(warning_path)){
		warning_image = image_get_texture(warning_path);
	} else {
		warning_image = image_get_texture("wipeout/textures/wipeout1.tim");
	}
	start_time = system_time();
	sfx_music_mode(SFX_MUSIC_RANDOM);
}

void warning_update(void) {
	render_set_view_2d();
	vec2i_t screen =  platform_screen_size();
	vec2i_t image_scaled = ui_scaled_pos( UI_POS_MIDDLE | UI_POS_CENTER, vec2i(0, 0));
	render_push_2d((vec2i_t){.x = screen.x/2-(image_scaled.x/2), .y = screen.y/2-(image_scaled.y/2)}, ui_scaled_pos( UI_POS_MIDDLE | UI_POS_CENTER, vec2i(0, 0)), rgba(128, 128, 128, 255), warning_image);
	ui_draw_text_centered("IN PROGRESS VERSION", ui_scaled_pos(UI_POS_TOP | UI_POS_CENTER, vec2i(0, 20)), UI_SIZE_8, UI_COLOR_DEFAULT);
	ui_draw_text_centered("BUILT SEPT 2023", ui_scaled_pos(UI_POS_TOP | UI_POS_CENTER, vec2i(0, 40)), UI_SIZE_8, UI_COLOR_DEFAULT);
	ui_draw_text_centered("SLAVA UKRAINI", ui_scaled_pos(UI_POS_BOTTOM | UI_POS_CENTER, vec2i(0, -40)), UI_SIZE_8, UI_COLOR_ACCENT);
	ui_draw_text_centered("HEROYAM SLAVA", ui_scaled_pos(UI_POS_BOTTOM | UI_POS_CENTER, vec2i(0, -20)), UI_SIZE_8, UI_COLOR_ACCENT);

	if (input_pressed(A_MENU_SELECT) || input_pressed(A_MENU_START)) {
		sfx_play(SFX_MENU_SELECT);
		game_set_scene(GAME_SCENE_TITLE);
	}

	float duration = system_time() - start_time;
	if (duration > 5) {
		game_set_scene(GAME_SCENE_TITLE);
	}
}
