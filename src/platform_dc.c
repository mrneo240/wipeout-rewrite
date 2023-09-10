// Sega Dreamcast
#if defined(_arch_dreamcast)

// Everything else
#else
	#error "Renderer only valid for Sega Dreamcast!"
#endif

#include "platform.h"
#include "input.h"
#include "system.h"

#include <string.h>
#include <sys/time.h>

#include <dc/maple.h>
#include <dc/maple/controller.h>

#define configDeadzone (0x20)

static bool wants_to_exit = false;
static void (*audio_callback)(float *buffer, uint32_t len) = NULL;

void platform_exit() {
	wants_to_exit = true;
}
void platform_pump_events() {
    maple_device_t *cont;
    cont_state_t *state;

    cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    if (!cont)
        return;
    state = (cont_state_t *) maple_dev_status(cont);

    const char stickH =state->joyx;
    const char stickV = 0xff-((uint8_t)(state->joyy));
    const uint32_t magnitude_sq = (uint32_t)(stickH * stickH) + (uint32_t)(stickV * stickV);
		float stick_x = 0;
		float stick_y = 0;
    if (magnitude_sq > (uint32_t)(configDeadzone * configDeadzone)) {
      stick_x = ((float)stickH/127);
    	stick_y = ((float)stickV/127);
    }
    if (state->buttons & CONT_START){
			input_set_button_state(INPUT_GAMEPAD_START, 1.0f);
		} else {
			input_set_button_state(INPUT_GAMEPAD_START, 0.0f);
		}
    if (state->buttons & CONT_A){
			input_set_button_state(INPUT_GAMEPAD_A, 1.0f);
		} else {
			input_set_button_state(INPUT_GAMEPAD_A, 0.0f);
		}
		if (state->buttons & CONT_B){
			input_set_button_state(INPUT_GAMEPAD_B, 1.0f);
		} else {
			input_set_button_state(INPUT_GAMEPAD_B, 0.0f);
		}
		if (state->buttons & CONT_X){
			input_set_button_state(INPUT_GAMEPAD_X, 1.0f);
		} else {
			input_set_button_state(INPUT_GAMEPAD_X, 0.0f);
		}
		if (state->buttons & CONT_Y){
			input_set_button_state(INPUT_GAMEPAD_Y, 1.0f);
		} else {
			input_set_button_state(INPUT_GAMEPAD_Y, 0.0f);
		}
    if (((uint8_t) state->ltrig & 0x80 /* 128 */))
		{
			input_set_button_state(INPUT_GAMEPAD_L_TRIGGER, 1.0f);
		} else {
			input_set_button_state(INPUT_GAMEPAD_L_TRIGGER, 0.0f);
		}
    if (((uint8_t) state->rtrig & 0x80 /* 128 */)){
			input_set_button_state(INPUT_GAMEPAD_R_TRIGGER, 1.0f);
		} else {
			input_set_button_state(INPUT_GAMEPAD_R_TRIGGER, 0.0f);
		}
    if (state->buttons & CONT_DPAD_UP){
			input_set_button_state(INPUT_GAMEPAD_DPAD_UP, 1.0f);
		} else {
			input_set_button_state(INPUT_GAMEPAD_DPAD_UP, 0.0f);
		}
		if (state->buttons & CONT_DPAD_DOWN){
			input_set_button_state(INPUT_GAMEPAD_DPAD_DOWN, 1.0f);
		} else {
			input_set_button_state(INPUT_GAMEPAD_DPAD_DOWN, 0.0f);
		}
		if (state->buttons & CONT_DPAD_LEFT){
			input_set_button_state(INPUT_GAMEPAD_DPAD_LEFT, 1.0f);
		} else {
			input_set_button_state(INPUT_GAMEPAD_DPAD_LEFT, 0.0f);
		}
		if (state->buttons & CONT_DPAD_RIGHT){
			input_set_button_state(INPUT_GAMEPAD_DPAD_RIGHT, 1.0f);
		} else {
			input_set_button_state(INPUT_GAMEPAD_DPAD_RIGHT, 0.0f);
		}

		// joystick
		if (stick_x > 0) {
			input_set_button_state(INPUT_GAMEPAD_L_STICK_LEFT, 0.0);
			input_set_button_state(INPUT_GAMEPAD_L_STICK_LEFT+1, stick_x);
		}
		else {
			input_set_button_state(INPUT_GAMEPAD_L_STICK_LEFT, -stick_x);
			input_set_button_state(INPUT_GAMEPAD_L_STICK_LEFT+1, 0.0);
		}
		if (stick_y < 0) {
			input_set_button_state(INPUT_GAMEPAD_L_STICK_UP, 0.0);
			input_set_button_state(INPUT_GAMEPAD_L_STICK_UP+1, stick_y);
		}
		else {
			input_set_button_state(INPUT_GAMEPAD_L_STICK_UP, -stick_y);
			input_set_button_state(INPUT_GAMEPAD_L_STICK_UP+1, 0.0);
		}
}


vec2i_t platform_screen_size() {
	return vec2i(640, 480);
}

float Sys_FloatTime(void) {
  struct timeval tp;
  struct timezone tzp;
  static int secbase;

  gettimeofday(&tp, &tzp);

#define divisor (1 / 1000000.0)

  if (!secbase) {
    secbase = tp.tv_sec;
    return tp.tv_usec * divisor;
  }

  return (tp.tv_sec - secbase) + tp.tv_usec * divisor;
}


//static float cur_time = 5.0f;
scalar_t platform_now() {
	//cur_time += (1.0f/60.0f);
	//return (scalar_t)cur_time;
	return (scalar_t)Sys_FloatTime();
}

void platform_set_fullscreen(bool fullscreen) {
	// always fullscreen
}

void platform_audio_callback(void* userdata, uint8_t* stream, int len) {
	if (audio_callback) {
		audio_callback((float *)stream, len/sizeof(float));
	}
	else {
		memset(stream, 0, len);
	}
}

void platform_set_audio_mix_cb(void (*cb)(float *buffer, uint32_t len)) {
	audio_callback = cb;
	//SDL_PauseAudioDevice(audio_device, 0);
}


#if defined(RENDERER_GL_LEGACY)

	void platform_video_init() {
	}

	void platform_prepare_frame() {
		// nothing
	}

	void platform_video_cleanup() {
	}

	void platform_end_frame() {
	}
#else
	#error "Unsupported renderer for platform Dreamcast"
#endif

int main(int argc, char *argv[]) {

	//audio_device = SDL_OpenAudioDevice(NULL, 0, &(SDL_AudioSpec){
	//	.freq = 44100,
	//	.format = AUDIO_F32,
	//	.channels = 2,
	//	.samples = 4096,
	//	.callback = platform_audio_callback
	//}, NULL, 0);

	platform_video_init();
	system_init();

	while (!wants_to_exit) {
		platform_pump_events();
		platform_prepare_frame();
		system_update();
		platform_end_frame();
	}

	system_cleanup();
	platform_video_cleanup();

	return 0;
}
