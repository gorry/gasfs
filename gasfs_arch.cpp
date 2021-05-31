// ◇
// gasfs: GORRY's archive and slice file system
// gasfs: gasfsファイルアクセス
// Copyright: (C)2021 Hiroaki GOTO as GORRY.
// License: see readme.txt
// =====================================================================

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include <time.h>
#include <stdarg.h>

#include "GasFs.h"

namespace GasFs {

// -------------------------------------------------------------

// =====================================================================
// 移植用関数群
// =====================================================================

MY_FILE my_fopen(const char* filename, const char* mode)
{
	return (MY_FILE)fopen(filename, mode);
}

int my_fseek(MY_FILE fp, long offset, int origin)
{
	return fseek((FILE*)fp, offset, origin);
}

int my_fgetpos(MY_FILE fp, my_fpos_t* pos)
{
	return fgetpos((FILE*)fp, (fpos_t*)pos);
}

size_t my_fread(void* buf, size_t size, size_t n, MY_FILE fp)
{
	return fread(buf, size, n, (FILE*)fp);
}

int my_fclose(MY_FILE fp)
{
	return fclose((FILE*)fp);
}

int my_printerr(const char* format, ...)
{
	va_list va;
	va_start(va, format);
	int ret = vfprintf(stderr, format, va);
	va_end(va);
	return ret;
}

// =====================================================================

};

// =====================================================================
// [EOF]
