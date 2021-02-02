// ◇
// gasfs: GORRY's archive and slice file system
// MkBin: バイナリファイルの作成
// Copyright: (C)2021 Hiroaki GOTO as GORRY.
// License: see readme.txt
// =====================================================================

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>

#include "WStrUtil.h"

#if defined(_WINDOWS)
#include <Windows.h>
#endif

// =====================================================================
// ヘルプ表示
// =====================================================================

void showHelp()
{
	printf("%s", 
	   "MkBin: Make New Binary File: Version 20210115a GORRY.\n"
	   "Usage:\n"
	   "  mkbin [outfile.bin]  Make [outfile.bin].\n"
	   "Option:\n"
	   "  --size [size]         File size (default=0) / unit symbol K(Kib)/M(MiB)/G(GiB).\n"
	   "  --unit [num]          Data unit (1[default]/2/4).\n"
	   "  --order [l/b]         Data order (little[default]/big).\n"
	   "  --start [num]         Start number (default=0).\n"
	   "  --end [num]           End number (default=MAX).\n"
	   "  --step [num]          Step number (default=1).\n"
	   "  --initial [num]       Initial number (default=start).\n"
	   "  --truncate            Create all zero file faster.\n"
	   "  --silent              No output percentage.\n"
	   "  --help                Show this.\n"
	);
}

// =====================================================================
// メイン
// =====================================================================

int
wmain(int argc, wchar_t** argv, wchar_t** envp)
{
	int err;

	uint64_t size = 0;
	int unit = 1;
	int order = 1;
	uint32_t startnum = 0;
	uint32_t endnum = 0xffffffff;
	int32_t stepnum = 0;
	int64_t initialnum = -1;
	bool step = false;
	bool isTruncate = false;
	bool silent = false;
	std::string outputFilename;

	// ロケール設定
#if defined(_WINDOWS)
	const char* env = getenv("LANG");
	if ((env == nullptr) || (env[0] == '\0')) {
		// env = "ja-JP";
		env = ".utf8";
	}
	env = setlocale(LC_ALL, env);
#endif

	for (int i=1; i<argc; i++) {
		std::wstring warg(argv[i]);
		std::string arg = WStrUtil::wstr2str(warg);
		if (arg == "--help") {
			showHelp();
			return 0;
		}
		if (arg == "--size") {
			if (i + 1 >= argc) {
				fprintf(stderr, "Failed: Specify --size param.\n");
				exit(EXIT_FAILURE);
			}
			std::wstring warg2(argv[++i]);
			std::string arg2 = WStrUtil::wstr2str(warg2);
			char* p = const_cast<char*>(arg2.c_str());
			uint64_t d = strtoul(p, &p, 0);
			if (p[0] == 'K') d *= 1024;
			if (p[0] == 'M') d *= 1024*1024;
			if (p[0] == 'G') d *= 1024*1024*1024;
			size = d;
			continue;
		}
		if (arg == "--unit") {
			if (i + 1 >= argc) {
				fprintf(stderr, "Failed: Specify --unit param.\n");
				exit(EXIT_FAILURE);
			}
			std::wstring warg2(argv[++i]);
			std::string arg2 = WStrUtil::wstr2str(warg2);
			char* p = const_cast<char*>(arg2.c_str());
			unit = 0;
			if (p[0] == '1') unit = 1;
			if (p[0] == '2') unit = 2;
			if (p[0] == '4') unit = 4;
			if (unit == 0) {
				fprintf(stderr, "Failed: Specify --unit param 1/2/4.\n");
				exit(EXIT_FAILURE);
			}
			continue;
		}
		if (arg == "--order") {
			if (i + 1 >= argc) {
				fprintf(stderr, "Failed: Specify --order param.\n");
				exit(EXIT_FAILURE);
			}
			std::wstring warg2(argv[++i]);
			std::string arg2 = WStrUtil::wstr2str(warg2);
			char* p = const_cast<char*>(arg2.c_str());
			order = 0;
			if (p[0] == 'l') order = 1;
			if (p[0] == 'b') order = 2;
			if (order == 0) {
				fprintf(stderr, "Failed: Specify --order param l(ittle)/b(ig).\n");
				exit(EXIT_FAILURE);
			}
			continue;
		}
		if (arg == "--start") {
			if (i + 1 >= argc) {
				fprintf(stderr, "Failed: Specify --start param.\n");
				exit(EXIT_FAILURE);
			}
			std::wstring warg2(argv[++i]);
			std::string arg2 = WStrUtil::wstr2str(warg2);
			char* p = const_cast<char*>(arg2.c_str());
			uint32_t d = strtoul(p, &p, 0);
			startnum = d;
			continue;
		}
		if (arg == "--end") {
			if (i + 1 >= argc) {
				fprintf(stderr, "Failed: Specify --end param.\n");
				exit(EXIT_FAILURE);
			}
			std::wstring warg2(argv[++i]);
			std::string arg2 = WStrUtil::wstr2str(warg2);
			char* p = const_cast<char*>(arg2.c_str());
			uint32_t d = strtoul(p, &p, 0);
			endnum = d;
			continue;
		}
		if (arg == "--step") {
			if (i + 1 >= argc) {
				fprintf(stderr, "Failed: Specify --step param.\n");
				exit(EXIT_FAILURE);
			}
			std::wstring warg2(argv[++i]);
			std::string arg2 = WStrUtil::wstr2str(warg2);
			char* p = const_cast<char*>(arg2.c_str());
			int32_t d = strtol(p, &p, 0);
			stepnum = d;
			step = true;
			continue;
		}
		if (arg == "--initial") {
			if (i + 1 >= argc) {
				fprintf(stderr, "Failed: Specify --initial param.\n");
				exit(EXIT_FAILURE);
			}
			std::wstring warg2(argv[++i]);
			std::string arg2 = WStrUtil::wstr2str(warg2);
			char* p = const_cast<char*>(arg2.c_str());
			uint32_t d = strtoul(p, &p, 0);
			initialnum = (int64_t)d;
			continue;
		}
		if (arg == "--truncate") {
			isTruncate = true;
			continue;
		}
		if (arg == "--silent") {
			silent = true;
			continue;
		}
		{
			std::wstring wfilename = WStrUtil::pathBackslash2Slash(warg);
			outputFilename = WStrUtil::wstr2str(wfilename);
		}
	}

	// 入力がなければ終了
	if (outputFilename.empty()) {
		fprintf(stderr, "Failed: Specify [output.bin].\n");
		exit(EXIT_FAILURE);
	}

	// 省略時処理
	if (initialnum == -1) {
		initialnum = startnum;
	}
	if (!step) {
		stepnum = (startnum < endnum) ? 1 : -1;
	}

	if (isTruncate) {
#if defined(_WINDOWS)
		std::wstring wfilename = WStrUtil::str2wstr(outputFilename);
		HANDLE hFile = CreateFile(
		  wfilename.c_str(),
		  GENERIC_READ | GENERIC_WRITE,
		  FILE_SHARE_READ | FILE_SHARE_WRITE,
		  NULL,
		  CREATE_ALWAYS,
		  FILE_ATTRIBUTE_NORMAL,
		  NULL
		);
		if (hFile == INVALID_HANDLE_VALUE) {
			fprintf(stderr, "Failed: Cannot open [%s].", outputFilename.c_str());
			exit(EXIT_FAILURE);
		}
		LONG high = (LONG)(size>>32);
		SetFilePointer(hFile, (LONG)size, &high, FILE_BEGIN);
		SetEndOfFile(hFile);
		CloseHandle(hFile);
#else
		err = remove(outputFilename.c_str());
		if (err) {
			fprintf(stderr, "Failed: Cannot open [%s].", outputFilename.c_str());
			exit(EXIT_FAILURE);
		}
		ftruncate(outputFilename.c_str(), (off_t)size);
#endif
		exit(EXIT_SUCCESS);
	}

	FILE* fout = fopen(outputFilename.c_str(), "wb");
	if (fout == nullptr) {
		fprintf(stderr, "Failed: Cannot open file [%s].\n", outputFilename.c_str());
		exit(EXIT_FAILURE);
	}

	uint32_t allocsize = 1024*1024*100;
	uint8_t* buf = new uint8_t[allocsize];
	uint64_t rest = size;
	int64_t data = (int64_t)initialnum;
	while (rest > 0) {
		if (!silent) {
			if (size > allocsize) {
				printf("%s: write %.1f%%\r", outputFilename.c_str(), ((size-rest)/(double)size*100.0));
			}
		}
		int32_t bufsize = allocsize;
		if ((uint64_t)bufsize > rest) {
			bufsize = (int32_t)rest;
		}
		rest -= bufsize;
		int i = 0;
		while (i < bufsize) {
			switch (unit) {
			  default:
			  case 1:
				buf[i++]= (uint8_t)data;
				break;
			  case 2:
				if (order == 1) {
					buf[i++] = (uint8_t)(data>>0);
					buf[i++] = (uint8_t)(data>>8);
				} else {
					buf[i++] = (uint8_t)(data >> 8);
					buf[i++] = (uint8_t)(data >> 0);
				}
				break;
			  case 4:
				if (order == 1) {
					buf[i++] = (uint8_t)(data >> 0);
					buf[i++] = (uint8_t)(data >> 8);
					buf[i++] = (uint8_t)(data >> 16);
					buf[i++] = (uint8_t)(data >> 24);
				} else {
					buf[i++] = (uint8_t)(data >> 24);
					buf[i++] = (uint8_t)(data >> 16);
					buf[i++] = (uint8_t)(data >> 8);
					buf[i++] = (uint8_t)(data >> 0);
				}
				break;
			}
			if (stepnum > 0) {
				data += stepnum;
				if (data > endnum) {
					data -= endnum;
					data -= 1;
					data += startnum;
				}
			}
			else {
				data += stepnum;
				if (data < endnum) {
					data += startnum;
					data += 1;
					data -= endnum;
				}
			}
		}
		uint32_t writesize = fwrite(buf, 1, bufsize, fout);
		if (writesize != bufsize) {
			fprintf(stderr, "Failed: Cannot write file [%s].\n", outputFilename.c_str());
			delete[] buf;
			fclose(fout);
			exit(EXIT_FAILURE);
		}
	}
	delete[] buf;
	err = fclose(fout);
	if (err) {
		fprintf(stderr, "Failed: Cannot write file [%s].\n", outputFilename.c_str());
		exit(EXIT_FAILURE);
	}

	if (!silent) {
		if (size > allocsize) {
			printf("%s: write finished.\n", outputFilename.c_str());
		}
	}

	return 0;
}

// =====================================================================
// [EOF]
