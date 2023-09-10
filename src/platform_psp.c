// Sony PSP
#if defined(__PSP__)
#include <pspkernel.h>
#include <pspsdk.h>
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

#include "input.h"
#include "platform.h"
#include "system.h"
#include "utils.h"

#include <string.h>
#include <sys/time.h>

#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspkernel.h>
#include <psppower.h>
#include <psprtc.h>
#include <pspsdk.h>

#define configDeadzone (0x20)

static bool wants_to_exit = false;
static void (*audio_callback)(float *buffer, uint32_t len) = NULL;

static char *path_assets = "";
static char *path_userdata = "";
static char *temp_path = NULL;

// Audio Threading
#define SAMPLES_HIGH 544
#define SAMPLES_LOW 528
#include "psp_audio_stack.h"
typedef int JobData;
int volatile mediaengine_sound = 0;
int volatile *mediaengine_sound_ptr = &mediaengine_sound;

static s16 audio_buffer[SAMPLES_HIGH * 2 * 2] __attribute__((aligned(64)));
extern struct Stack *stack;
struct Stack *stack;
extern void audio_psp_play(const uint8_t *buf, size_t len);

int __attribute__((optimize("O0"))) run_me_audio(JobData data) {
  (void)data;
  // create_next_audio_buffer(audio_buffer + 0 * (SAMPLES_HIGH * 2), SAMPLES_HIGH);
  // create_next_audio_buffer(audio_buffer + 1 * (SAMPLES_HIGH * 2), SAMPLES_HIGH);
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
    case NOP: {
      ;
      sceKernelDelayThread(1000 + 1000 * (mediaengine_sound));
    } break;
    case QUIT: {
      ;
      running = false;
    } break;
    case GENERATE: {
      ;
      {
        run_me_audio(0);
        sceKernelDcacheWritebackInvalidateRange(audio_buffer, sizeof(audio_buffer));
      }
      stack_push(stack, PLAY);
      sceKernelDelayThread(250);
    } break;
    case PLAY: {
      ;
      // sceKernelDelayThread(100);
      // stack_clear(stack);
      // audio_api->play((u8 *)audio_buffer, 2 /* 2 buffers */ * SAMPLES_HIGH * sizeof(short) * 2 /* stereo */);
    } break;
    }
  }
  sceIoWrite(1, "Audio Manager Exit!\n", 21);
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

  const char stickH = data.Lx + 0x80;
  const char stickV = 0xff - (data.Ly + 0x80);
  float stick_x = 0;
  float stick_y = 0;
  uint32_t magnitude_sq = (uint32_t)(stickH * stickH) + (uint32_t)(stickV * stickV);
  if (magnitude_sq > (uint32_t)(configDeadzone * configDeadzone)) {
    stick_x = ((float)stickH / 127);
    stick_y = ((float)stickV / 127);
  }

  float btnStart = (float)(!!(data.Buttons & PSP_CTRL_START));
  input_set_button_state(INPUT_GAMEPAD_START, btnStart);

  float btnCross = (float)(!!(data.Buttons & PSP_CTRL_CROSS));
  input_set_button_state(INPUT_GAMEPAD_A, btnCross);
  float btnCircle = (float)(!!(data.Buttons & PSP_CTRL_CIRCLE));
  input_set_button_state(INPUT_GAMEPAD_B, btnCircle);
  float btnSquare = (float)(!!(data.Buttons & PSP_CTRL_SQUARE));
  input_set_button_state(INPUT_GAMEPAD_X, btnSquare);
  float btnTriangle = (float)(!!(data.Buttons & PSP_CTRL_TRIANGLE));
  input_set_button_state(INPUT_GAMEPAD_Y, btnTriangle);

  float btnUp = (float)(!!(data.Buttons & PSP_CTRL_UP));
  input_set_button_state(INPUT_GAMEPAD_DPAD_UP, btnUp);
  float btnDown = (float)(!!(data.Buttons & PSP_CTRL_DOWN));
  input_set_button_state(INPUT_GAMEPAD_DPAD_DOWN, btnDown);
  float btnLeft = (float)(!!(data.Buttons & PSP_CTRL_LEFT));
  input_set_button_state(INPUT_GAMEPAD_DPAD_LEFT, btnLeft);
  float btnRight = (float)(!!(data.Buttons & PSP_CTRL_RIGHT));
  input_set_button_state(INPUT_GAMEPAD_DPAD_RIGHT, btnRight);

  // joystick
  if (stick_x > 0.25f) {
    input_set_button_state(INPUT_GAMEPAD_DPAD_LEFT, 0.0 + btnLeft);
    input_set_button_state(INPUT_GAMEPAD_DPAD_LEFT + 1, stick_x);
  } else if (stick_x < -0.25f) {
    input_set_button_state(INPUT_GAMEPAD_DPAD_LEFT, -stick_x);
    input_set_button_state(INPUT_GAMEPAD_DPAD_LEFT + 1, 0.0 + btnRight);
  }
  if (stick_y < -0.25f) {
    input_set_button_state(INPUT_GAMEPAD_DPAD_UP, 0.0 + btnUp);
    input_set_button_state(INPUT_GAMEPAD_DPAD_UP + 1, stick_y);
  } else if (stick_y > 0.25f) {
    input_set_button_state(INPUT_GAMEPAD_DPAD_UP, -stick_y);
    input_set_button_state(INPUT_GAMEPAD_DPAD_UP + 1, 0.0 + btnDown);
  }

  // printf("analog x %f y %f\n", stick_x, stick_y);
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

scalar_t platform_now() {
  return (scalar_t)Sys_FloatTime();
}

bool platform_get_fullscreen(void) {
	return true;
}

void platform_set_fullscreen(bool fullscreen) {
  // always fullscreen
}

void platform_audio_callback(void *userdata, uint8_t *stream, int len) {
  if (audio_callback) {
    audio_callback((float *)stream, len / sizeof(float));
  } else {
    memset(stream, 0, len);
  }
}

void platform_set_audio_mix_cb(void (*cb)(float *buffer, uint32_t len)) {
  audio_callback = cb;
  // SDL_PauseAudioDevice(audio_device, 0);
}

uint8_t *platform_load_asset(const char *name, uint32_t *bytes_read) {
	char *path = strcat(strcpy(temp_path, path_assets), name);
	return file_load(path, bytes_read);
}

uint8_t *platform_load_userdata(const char *name, uint32_t *bytes_read) {
	char *path = strcat(strcpy(temp_path, path_userdata), name);
	if (!file_exists(path)) {
		*bytes_read = 0;
		return NULL;
	}
	return file_load(path, bytes_read);
}

uint32_t platform_store_userdata(const char *name, void *bytes, int32_t len) {
	char *path = strcat(strcpy(temp_path, path_userdata), name);
	return file_store(path, bytes, len);
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

  char *_path_assets = NULL;
	#ifdef PATH_ASSETS
		path_assets = TOSTRING(PATH_ASSETS);
	#else
		_path_assets = "";
		if (_path_assets) {
			path_assets = _path_assets;
		}
	#endif

	char *_path_userdata = NULL;
	#ifdef PATH_USERDATA
		path_userdata = TOSTRING(PATH_USERDATA);
	#else
		_path_userdata = "";
		if (_path_userdata) {
			path_userdata = _path_userdata;
		}
	#endif

	// Reserve some space for concatenating the asset and userdata paths with
	// local filenames.
	temp_path = mem_bump(max(strlen(path_assets), strlen(path_userdata)) + 64);

  // audio_device = SDL_OpenAudioDevice(NULL, 0, &(SDL_AudioSpec){
  //	.freq = 44100,
  //	.format = AUDIO_F32,
  //	.channels = 2,
  //	.samples = 4096,
  //	.callback = platform_audio_callback
  // }, NULL, 0);

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

void memcpy_vfpu(void *dst, const void *src, size_t size) {
  // less than 16bytes or there is no 32bit alignment -> not worth optimizing
  if (((u32)src & 0x3) != ((u32)dst & 0x3) && (size < 16)) {
    memcpy(dst, src, size);
    return;
  }

  u8 *src8 = (u8 *)src;
  u8 *dst8 = (u8 *)dst;

  // Align dst to 4 bytes or just resume if already done
  while (((u32)dst8 & 0x3) != 0) {
    *dst8++ = *src8++;
    size--;
  }

  u32 *dst32 = (u32 *)dst8;
  u32 *src32 = (u32 *)src8;

  // Align dst to 16 bytes or just resume if already done
  while (((u32)dst32 & 0xF) != 0) {
    *dst32++ = *src32++;
    size -= 4;
  }

  dst8 = (u8 *)dst32;
  src8 = (u8 *)src32;

  if (((u32)src8 & 0xF) == 0) // Both src and dst are 16byte aligned
  {
    while (size > 63) {
      asm(".set	push\n"      // save assembler option
          ".set	noreorder\n" // suppress reordering
          "lv.q c000, 0(%1)\n"
          "lv.q c010, 16(%1)\n"
          "lv.q c020, 32(%1)\n"
          "lv.q c030, 48(%1)\n"
          "sv.q c000, 0(%0)\n"
          "sv.q c010, 16(%0)\n"
          "sv.q c020, 32(%0)\n"
          "sv.q c030, 48(%0)\n"
          "addiu  %2, %2, -64\n" // size -= 64;
          "addiu	%1, %1, 64\n"  // dst8 += 64;
          "addiu	%0, %0, 64\n"  // src8 += 64;
          ".set	pop\n"           // restore assembler option
          : "+r"(dst8), "+r"(src8), "+r"(size)
          :
          : "memory");
    }

    while (size > 15) {
      asm(".set	push\n"      // save assembler option
          ".set	noreorder\n" // suppress reordering
          "lv.q c000, 0(%1)\n"
          "sv.q c000, 0(%0)\n"
          "addiu  %2, %2, -16\n" // size -= 16;
          "addiu	%1, %1, 16\n"  // dst8 += 16;
          "addiu	%0, %0, 16\n"  // src8 += 16;
          ".set	pop\n"           // restore assembler option
          : "+r"(dst8), "+r"(src8), "+r"(size)
          :
          : "memory");
    }
  } else // At least src is 4byte and dst is 16byte aligned
  {
    while (size > 63) {
      asm(".set	push\n"      // save assembler option
          ".set	noreorder\n" // suppress reordering
          "ulv.q c000, 0(%1)\n"
          "ulv.q c010, 16(%1)\n"
          "ulv.q c020, 32(%1)\n"
          "ulv.q c030, 48(%1)\n"
          "sv.q c000, 0(%0)\n"
          "sv.q c010, 16(%0)\n"
          "sv.q c020, 32(%0)\n"
          "sv.q c030, 48(%0)\n"
          "addiu  %2, %2, -64\n" // size -= 64;
          "addiu	%1, %1, 64\n"  // dst8 += 64;
          "addiu	%0, %0, 64\n"  // src8 += 64;
          ".set	pop\n"           // restore assembler option
          : "+r"(dst8), "+r"(src8), "+r"(size)
          :
          : "memory");
    }

    while (size > 15) {
      asm(".set	push\n"      // save assembler option
          ".set	noreorder\n" // suppress reordering
          "ulv.q c000, 0(%1)\n"
          "sv.q c000, 0(%0)\n"
          "addiu  %2, %2, -16\n" // size -= 16;
          "addiu	%1, %1, 16\n"  // dst8 += 16;
          "addiu	%0, %0, 16\n"  // src8 += 16;
          ".set	pop\n"           // restore assembler option
          : "+r"(dst8), "+r"(src8), "+r"(size)
          :
          : "memory");
    }
  }

  // Most copies are completed with the VFPU, so fast out
  if (size == 0)
    return;

  dst32 = (u32 *)dst8;
  src32 = (u32 *)src8;

  // Copy remaning 32bit...
  while (size > 3) {
    *dst32++ = *src32++;
    size -= 4;
  }

  dst8 = (u8 *)dst32;
  src8 = (u8 *)src32;

  // Copy remaning bytes if any...
  while (size > 0) {
    *dst8++ = *src8++;
    size--;
  }
}

void __attribute__((noinline)) memcpy_vfpu_simple(void *dst, void *src, size_t size) {
  __asm__ volatile(
      "loop:\n"
      "beqz %2, loop_end\n"
      "lv.q C200, 0(%1)\n"
      "sv.q C200, 0(%0)\n"
      "addiu %0, %0, 16\n"
      "addiu %1, %1, 16\n"
      "addiu %2, %2, -16\n"
      "b loop\n"
      "loop_end:\n" ::
          "r"(dst),
      "r"(src), "r"(size));
}