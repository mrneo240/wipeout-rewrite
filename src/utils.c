#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "utils.h"
#include "mem.h"

char temp_path[64];
char *get_path(const char *dir, const char *file) {
	strcpy(temp_path, dir);
	strcpy(temp_path + strlen(dir), file);
	return temp_path;
}


bool file_exists(const char *path) {
#if defined(_arch_dreamcast)
	char _path[256];
	memset(_path, 0, sizeof(path));
	strcpy(_path, "/cd/");
	strcat(_path, path);

	struct stat s;
	int f = (stat(_path, &s) == 0);
	if(!f){
		// if not on cd, try /pc/
		memset(_path, 0, sizeof(path));
		strcpy(_path, "/pc/");
		strcat(_path, path);
		struct stat s;
		f = (stat(_path, &s) == 0);
	}
	return f;
#else
	struct stat s;
	return (stat(path, &s) == 0);
#endif
}

uint8_t *file_load(const char *path, uint32_t *bytes_read) {
#if defined(_arch_dreamcast)
	char _path[256];
	memset(_path, 0, sizeof(path));
	strcpy(_path, "/cd/");
	strcat(_path, path);
	FILE *f = fopen(_path, "rb");
	if(!f){
		// if not on cd, try /pc/
		memset(_path, 0, sizeof(path));
		strcpy(_path, "/pc/");
		strcat(_path, path);
		f = fopen(_path, "rb");
	}
#else
	FILE *f = fopen(path, "rb");
#endif
	error_if(!f, "Could not open file for reading: %s", path);

	fseek(f, 0, SEEK_END);
	int32_t size = ftell(f);
	if (size <= 0) {
		fclose(f);
		return NULL;
	}
	fseek(f, 0, SEEK_SET);

	uint8_t *bytes = mem_temp_alloc(size);
	if (!bytes) {
		fclose(f);
		return NULL;
	}

	*bytes_read = fread(bytes, 1, size, f);
	fclose(f);
	
	error_if(*bytes_read != size, "Could not read file: %s", path);
	return bytes;
}

uint32_t file_store(const char *path, void *bytes, int32_t len) {
	#if defined(_arch_dreamcast)
		printf("file writing ignored! %s\n", path);
		return len;
	#endif
	FILE *f = fopen(path, "wb");
	error_if(!f, "Could not open file for writing: %s", path);

	if (fwrite(bytes, 1, len, f) != len) {
		die("Could not write file file %s", path);
	}
	
	fclose(f);
	return len;
}

bool str_starts_with(const char *haystack, const char *needle) {
	return (strncmp(haystack, needle, strlen(needle)) == 0);
}

float rand_float(float min, float max) {
	return min + ((float)rand() / (float)RAND_MAX) * (max - min);
}

int32_t rand_int(int32_t min, int32_t max) {
	return min + rand() % (max - min);
}
