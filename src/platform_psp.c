// Sony PSP
#if defined(__PSP__)
#include <pspsdk.h>
#include <pspkernel.h>
#define MODULE_NAME "wipeout-rewrite"
#ifndef SRC_VER
#define SRC_VER "UNKNOWN"
#endif

PSP_MODULE_INFO(MODULE_NAME, 0, 1, 1);
PSP_HEAP_SIZE_MAX();
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
// Everything else
#else
	#error "Renderer only valid for Sony PSP!"
#endif

#include "platform.h"
#include "input.h"
#include "system.h"

#include <string.h>
#include <sys/time.h>

#include <pspsdk.h>
#include <pspkernel.h>
#include <psppower.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <psprtc.h>
#include <pspctrl.h>


#define configDeadzone (0x20)

static bool wants_to_exit = false;
static void (*audio_callback)(float *buffer, uint32_t len) = NULL;

// Audio Threading
#define SAMPLES_HIGH 544
#define SAMPLES_LOW 528
#include "psp_audio_stack.h"
typedef int JobData;
int volatile mediaengine_sound = 0;
int volatile *mediaengine_sound_ptr = &mediaengine_sound;

static s16 audio_buffer[SAMPLES_HIGH * 2 * 2] __attribute__((aligned(64)));
extern struct Stack* stack;
struct Stack *stack;
extern void audio_psp_play(const uint8_t *buf, size_t len);

int __attribute__((optimize("O0"))) run_me_audio(JobData data) {
    (void)data;
    //create_next_audio_buffer(audio_buffer + 0 * (SAMPLES_HIGH * 2), SAMPLES_HIGH);
    //create_next_audio_buffer(audio_buffer + 1 * (SAMPLES_HIGH * 2), SAMPLES_HIGH);
    return 0;
}
static unsigned int last_time = 0;
static int audio_manager_thid = 0;

int audioOutput(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    bool running = true;
    sceKernelDelayThread(1000);

    while (running) {
        AudioTask task = stack_pop(stack);

        switch (task) {
            case NOP:       {; sceKernelDelayThread(1000 + 1000  * (mediaengine_sound)); }break;
            case QUIT:      {; running = false; }break;
            case GENERATE:  {;
            {
                run_me_audio(0);
                sceKernelDcacheWritebackInvalidateRange(audio_buffer,sizeof(audio_buffer));
            }
            stack_push(stack, PLAY);
            sceKernelDelayThread(250);
            }
            break;
            case PLAY:      {;
                //sceKernelDelayThread(100);
                //stack_clear(stack);
                //audio_api->play((u8 *)audio_buffer, 2 /* 2 buffers */ * SAMPLES_HIGH * sizeof(short) * 2 /* stereo */);
            }
            break;
        }
    }
    sceIoWrite(1,"Audio Manager Exit!\n",21);
    SceUID thid = sceKernelGetThreadId();
    sceKernelTerminateDeleteThread(thid);
    return 0;
}

void init_audiomanager(void) {
    extern int audioOutput(SceSize args, void *argp);
    extern int audio_manager_thid;
    audio_manager_thid = sceKernelCreateThread("AudioOutput", audioOutput, 0x12, 0x20000, THREAD_ATTR_USER | THREAD_ATTR_VFPU, NULL);
    sceKernelStartThread(audio_manager_thid, 0, NULL);
}

void kill_audiomanager(void) {
    sceKernelTerminateDeleteThread(audio_manager_thid);
    sceKernelDelayThread(250);
}

// Callbacks and Exit Routines
static int exitCallback(UNUSED int arg1, UNUSED int arg2, UNUSED void *common) {
    sceKernelTerminateDeleteThread(audio_manager_thid);
    sceKernelExitGame();
    return 0;
}

static int callbackThread(UNUSED SceSize args, UNUSED void *argp) {
    int cbid;

    cbid = sceKernelCreateCallback("Exit Callback", exitCallback, NULL);
    sceKernelRegisterExitCallback(cbid);

    sceKernelSleepThreadCB();

    return 0;
}

void platform_exit() {
	wants_to_exit = true;
}
void platform_pump_events() {

 static SceCtrlData data;

    if (!sceCtrlPeekBufferPositive(&data, 1))
        return;

    const char stickH = data.Lx+0x80;
    const char stickV = 0xff-(data.Ly+0x80);
		float stick_x = 0;
		float stick_y = 0;
    uint32_t magnitude_sq = (uint32_t)(stickH * stickH) + (uint32_t)(stickV * stickV);
    if (magnitude_sq > (uint32_t)(configDeadzone * configDeadzone)) {
        stick_x = ((float)stickH/127);
        stick_y = ((float)stickV/127);
    }

		float btnStart =(float)(!!(data.Buttons & PSP_CTRL_START));
		input_set_button_state(INPUT_GAMEPAD_START, btnStart);

		float btnCross =(float)(!!(data.Buttons & PSP_CTRL_CROSS));
		input_set_button_state(INPUT_GAMEPAD_A, btnCross);
		float btnCircle =(float)(!!(data.Buttons & PSP_CTRL_CIRCLE));
		input_set_button_state(INPUT_GAMEPAD_B, btnCircle);
		float btnSquare =(float)(!!(data.Buttons & PSP_CTRL_SQUARE));
		input_set_button_state(INPUT_GAMEPAD_X, btnSquare);
		float btnTriangle =(float)(!!(data.Buttons & PSP_CTRL_TRIANGLE));
		input_set_button_state(INPUT_GAMEPAD_Y, btnTriangle);

		float btnUp =(float)(!!(data.Buttons & PSP_CTRL_UP));
		input_set_button_state(INPUT_GAMEPAD_DPAD_UP, btnUp);
		float btnDown =(float)(!!(data.Buttons & PSP_CTRL_DOWN));
		input_set_button_state(INPUT_GAMEPAD_DPAD_DOWN, btnDown);
		float btnLeft =(float)(!!(data.Buttons & PSP_CTRL_LEFT));
		input_set_button_state(INPUT_GAMEPAD_DPAD_LEFT, btnLeft);
		float btnRight =(float)(!!(data.Buttons & PSP_CTRL_RIGHT));
		input_set_button_state(INPUT_GAMEPAD_DPAD_RIGHT, btnRight);

		// joystick
		/*
		if (stick_x < 0) {
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
		}*/
}


vec2i_t platform_screen_size() {
	return vec2i(480, 272);
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


#if defined(RENDERER_GU)

	void platform_video_init() {
	}

	void platform_prepare_frame() {
    /* Lets us yield to other threads*/
    sceKernelDelayThread(100);
	}

	void platform_video_cleanup() {
	}

	void platform_end_frame() {
	}
#else
	#error "Unsupported renderer for platform Sony PSP"
#endif

int main(int argc, char *argv[]) {

    int thid = 0;

    thid = sceKernelCreateThread("update_thread", callbackThread, 0x20, 0xFA0, 0, 0);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, 0);
    }

    scePowerSetClockFrequency(333, 333, 166);
    sceKernelDelayThread(1000);

    last_time = sceKernelGetSystemTimeLow();

	//audio_device = SDL_OpenAudioDevice(NULL, 0, &(SDL_AudioSpec){
	//	.freq = 44100,
	//	.format = AUDIO_F32,
	//	.channels = 2,
	//	.samples = 4096,
	//	.callback = platform_audio_callback
	//}, NULL, 0);

	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

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
