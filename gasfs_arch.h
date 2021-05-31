// ◇
// gasfs: GORRY's archive and slice file system
// GasFs: gasfsファイルの構造
// Copyright: (C)2021 Hiroaki GOTO as GORRY.
// License: see readme.txt
// =====================================================================

#if !defined(__GASFS_ARCH_H__)
#define __GASFS_ARCH_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

namespace GasFs {

// -------------------------------------------------------------

typedef void* MY_FILE;
typedef int64_t my_fpos_t;
MY_FILE my_fopen(const char* filename, const char* mode);
int my_fseek(MY_FILE fp, long offset, int origin);
int my_fgetpos(MY_FILE fp, my_fpos_t* pos);
size_t my_fread(void* buf, size_t size, size_t n, MY_FILE fp);
int my_fclose(MY_FILE fp);
int my_printerr(const char* format, ...);


// -------------------------------------------------------------

};

#endif  // !defined(__GASFS_H__)

// =====================================================================
// [EOF]
