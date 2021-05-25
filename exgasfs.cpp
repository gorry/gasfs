// ◇
// gasfs: GORRY's archive and slice file system
// ExGasFs: gasfsファイルからの抽出
// Copyright: (C)2021 Hiroaki GOTO as GORRY.
// License: see readme.txt
// =====================================================================

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include <locale.h>
#include <wchar.h>
#include <direct.h>

#include "IniFile.h"
#include "WStrUtil.h"
#include "GasFs.h"

// -------------------------------------------------------------

bool gVerbose;


// =====================================================================
// ヘルプ表示
// =====================================================================

void showHelp()
{
	printf("%s", 
	   "ExGasFs: GORRY's Archive and Slice File System: Version " GASFS_VERSION " GORRY.\n"
	   "Usage:\n"
	   "  exgasfs [input_000.gfs] [filters...]\n"
	   "                        Load [input_000.gfs] and find matching files by [filters].\n"
	   "                        If [filters] is empty, find all files.\n"
	   "Option:\n"
	   "  --extract [dir]       Extract found files to specified directory.\n"
	   "                        If this option is omitted, Extract No files.\n"
	   "  --slice [num]         Load only [num] slice.\n"
	   "  --list [list.gfi]     Output list file.\n"
	   "  --skipcheckcrc        Skip CRC check.\n"
	   "  --verbose             Output verbose log.\n"
	   "  --help                Show this.\n"
	);
}

// =====================================================================
// スライスマップのエクスポート
// =====================================================================

bool
exportMapList(const GasFs::Global& global, const std::string& outputFilename, const GasFs::Map& map, const std::string& extractDir)
{
	FILE* fout = fopen(outputFilename.c_str(), "w");
	if (fout == nullptr) {
		fprintf(stderr, "Failed: Cannot export to [%s].\n", outputFilename.c_str());
		return false;
	}

	int slices = global.mSlices;
	std::wstring wmsg(L"# ◇ASCII, LF\n");
	std::string msg = WStrUtil::wstr2str(wmsg);
	fprintf(fout, msg.c_str());
	fprintf(fout, "[Global]\n");
	fprintf(fout, "Slices=%d\n", global.mSlices);
	fprintf(fout, "MaxSliceSize=%d\n", global.mMaxSliceSize);
	fprintf(fout, "\n");

	fprintf(fout, "[Input]\n");
	fprintf(fout, "PathList=[[[[\n");
	if (!extractDir.empty()) {
		const std::wstring wdir = WStrUtil::str2wstr(extractDir);
		const std::wstring wnewpath = WStrUtil::pathAddPath(wdir, L"*.*");
		const std::string newpath = WStrUtil::wstr2str(wnewpath);
		fprintf(fout, "\t%s\n", newpath.c_str());
	}
	fprintf(fout, "]]]]\n");
	fprintf(fout, "\n");

	for (int i=1; i<=slices; i++) {
		fprintf(fout, "[%03d]\n", i);
		fprintf(fout, "PathList=[[[[\n");
		for (const auto& e: map) {
			const std::string& path = e.first;
			const GasFs::Entry& entry = e.second;
			if (entry.mSlice == i) {
				fprintf(fout, "\t%s\n", path.c_str());
			}
		}
		fprintf(fout, "\t****\n");
		fprintf(fout, "]]]]\n");
		fprintf(fout, "\n");
	}

	fprintf(fout, "# EOF\n");
	fclose(fout);

	return true;
}

// =====================================================================
// Pathのディレクトリ部の作成
// =====================================================================

bool
createDirOfPath(const std::string& path)
{
	// pathからディレクトリ部を抽出
	const std::wstring wpath = WStrUtil::str2wstr(path);
	const WStrUtil::pathPair p = WStrUtil::pathSplitPath(wpath);
	std::wstring wdir = p.first;
	if (wdir.empty()) {
		// ディレクトリ部が空なら終了
		return true;
	}

	// ディレクトリ部の調査
	wdir = WStrUtil::pathRemoveSlash(wdir);
	const std::string dir = WStrUtil::wstr2str(wdir);
	struct _stat stat;
	int ret = _stat(dir.c_str(), &stat);
	if (ret == 0) {
		if (stat.st_mode & _S_IFDIR) {
			// 存在しているのがディレクトリなら終了
			return true;
		}
		// 存在しているのがディレクトリ以外ならエラー
		fprintf(stderr, "Failed: [%s] is not directory.\n", dir.c_str());
		return false;
	}
	if (errno == ENOENT) {
		// ディレクトリが存在しない場合は親ディレクトリを調べる
		bool b = createDirOfPath(dir);
		if (b) {
			// 親ディレクトリが存在しているならディレクトリを作る
			ret = _mkdir(dir.c_str());
			if (ret == 0) {
				// ディレクトリ作成が成功なら終了
				return true;
			}
			fprintf(stderr, "Failed: Cannot create directory [%s].\n", dir.c_str());
			return false;
		}
		return false;
	}

	// statできなければエラー
	fprintf(stderr, "Failed: Cannot stat [%s].\n", dir.c_str());
	return false;
}

// =====================================================================
// メイン
// =====================================================================

int
wmain(int argc, wchar_t** argv, wchar_t** envp)
{
	bool extract = false;
	bool list = false;
	std::string listFilename;
	std::string inputFilename;
	std::string extractDir;
	std::wstring winputFilename;
	std::wstring wextractDir;
	std::vector<std::string> extractFiles;
	int extractSlice = 0;
	GasFs::Global global = {0};

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
		if (arg == "--extract") {
			if (i + 1 >= argc) {
				fprintf(stderr, "Failed: Specify --extract param.\n");
				exit(EXIT_FAILURE);
			}
			std::wstring wfilename(argv[i+1]);
			wfilename = WStrUtil::pathBackslash2Slash(wfilename);
			wextractDir = WStrUtil::pathAddSlash(wfilename);
			extractDir = WStrUtil::wstr2str(wextractDir);
			i++;
			extract = true;
			continue;
		}
		if (arg == "--list") {
			if (i + 1 >= argc) {
				fprintf(stderr, "Failed: Specify --list param.\n");
				exit(EXIT_FAILURE);
			}
			std::wstring wfilename(argv[i+1]);
			wfilename = WStrUtil::pathBackslash2Slash(wfilename);
			listFilename = WStrUtil::wstr2str(wfilename);
			i++;
			list = true;
			continue;
		}
		if (arg == "--slice") {
			if (i + 1 >= argc) {
				fprintf(stderr, "Failed: Specify --slice param.\n");
				exit(EXIT_FAILURE);
			}
			extractSlice = atoi(arg.c_str());
			if (extractSlice < 1) {
			if (i + 1 >= argc) {
				fprintf(stderr, "Failed: --Slice param > 0.\n");
				exit(EXIT_FAILURE);
			}
			}
			continue;
		}
		if (arg == "--skipcheckcrc") {
			global.mSkipCheckCRC = true;
			continue;
		}
		if (arg == "--verbose") {
			gVerbose = true;
			continue;
		}
		if (!inputFilename.empty()) {
			extractFiles.push_back(arg);
			continue;
		}
		{
			winputFilename = WStrUtil::pathBackslash2Slash(warg);
			// 入力に"_000.gfs"が付いていたら削る
			size_t len = winputFilename.size();
			if (winputFilename.substr(len-8) == L"_000.gfs") {
				winputFilename.erase(len-8, 8);
			}
			inputFilename = WStrUtil::wstr2str(winputFilename);
		}
	}

	// 入力がなければ終了
	if (inputFilename.empty()) {
		fprintf(stderr, "Failed: Specify [input_000.gfs].\n");
		exit(EXIT_FAILURE);
	}

#if defined(_WINDOWS)
	if (gVerbose) {
		printf("Locale = [%s]\n", env);
	}
#endif

	if (gVerbose) {
		if (extractFiles.size()) {
			printf("Filter: [%s]", extractFiles[0].c_str());
			for (int i=1; i<(int)extractFiles.size(); i++) {
				printf(", [%s]\n", extractFiles[i].c_str());
			}
			printf("\n");
		}
	}

	// データベースを読み込む
	GasFs::Map map;
	global.mSliceFilename = inputFilename;
	int slices = createMap(global, map);
	if (slices < 0) {
		exit(EXIT_FAILURE);
	}
	if (extractSlice > slices) {
		fprintf(stderr, "Failed: --Slice param > %d.\n", slices);
		exit(EXIT_FAILURE);
	}

	// データベースを読んでいく
	for (int i=1; i<=slices; i++) {
		if (extractSlice) {
			if (i != extractSlice) continue;
		}
		printf("Slice [%03d]\n", i);

		// 読み込み元スライスを開く
		char filename[_MAX_PATH];
		sprintf(filename, "%s_%03d.gfs", inputFilename.c_str(), i);
		FILE* fin = fopen(filename, "rb");
		if (fin == nullptr) {
			fprintf(stderr, "Failed: Cannot open file [%s]\n", filename);
			exit(EXIT_FAILURE);
		}

		// スライスのCRCチェック
		if (!global.mSkipCheckCRC) {
			fseek(fin, sizeof(GasFs::Database::SubHeader), SEEK_SET);
			uint8_t buf[1024*64];
			uint32_t datacrc = 0;
			while (!0) {
				uint32_t readsize = sizeof(buf);
				readsize = fread(buf, 1, readsize, fin);
				if (readsize == 0) break;
				datacrc = GasFs::GetCRC(buf, readsize, datacrc);
			}
			uint32_t crc = global.mSlice[i].mCRC;
			if (crc != datacrc) {
				fprintf(stderr, "Failed: Slice[%d] CRC error(header=%08x, data=%08x) [%s].\n", i, crc, datacrc, filename);
				exit(EXIT_FAILURE);
			}
		}

		int files = 0;
		int64_t totalSize = 0;
		for (const auto& e: map) {
			const std::string& path = e.first;
			const GasFs::Entry& entry = e.second;
			if (entry.mSlice == i) {
				if (!extractFiles.empty()) {
					bool found = false;
					for (int j=0; j<(int)extractFiles.size(); j++) {
						const std::string& f = extractFiles[j];
						if (!memcmp(f.c_str(), path.c_str(), f.size())) {
							found = true;
						}
					}
					if (!found) continue;
				}
				if (gVerbose) {
					printf("  %10" PRIu64 " %s\n", entry.mSize, path.c_str());
				}
				if (extract) {
					// 書き込み先を開く
					const std::wstring wpath = WStrUtil::str2wstr(path);
					const std::wstring wnewpath = WStrUtil::pathAddPath(wextractDir, wpath);
					const std::string newpath = WStrUtil::wstr2str(wnewpath);
					FILE* fout = fopen(newpath.c_str(), "wb");
					if (fout == nullptr) {
						// 書き込み先が開けない場合、
						// 書き込み先のディレクトリを作ってみる
						bool b = createDirOfPath(newpath);
						if (!b) {
							exit(EXIT_FAILURE);
						}
						fout = fopen(newpath.c_str(), "wb");
						if (fout == nullptr) {
							fprintf(stderr, "Failed: Cannot create file [%s]\n", newpath.c_str());
							exit(EXIT_FAILURE);
						}
					}

					// 読み込み元スライスをシーク
					fpos_t pos = entry.mOffset + sizeof(GasFs::Database::SubHeader);
					fsetpos(fin, &pos);

					// bufsizeずつスライスから書き写す
					uint64_t rest = entry.mSize;
					const int bufsize = 1024*1024*16;
					uint8_t* buf = new uint8_t[bufsize];
					while (rest > 0) {
						uint64_t size = bufsize;
						if (size > rest) {
							size = rest;
						}
						size_t readsize = fread(buf, 1, (size_t)size, fin);
						if (readsize == 0) {
							break;
						}
						size_t wrotesize = fwrite(buf, 1, readsize, fout);
						if (wrotesize != readsize) {
							fprintf(stderr, "Failed: Cannot write [%s].\n", newpath.c_str());
							exit(EXIT_FAILURE);
						}
						rest -= readsize;
					}
					delete[] buf;

					// 書き写し終了
					int ret = fclose(fout);
					if (ret) {
						fprintf(stderr, "Failed: Cannot write [%s].\n", newpath.c_str());
						exit(EXIT_FAILURE);
					}
				}
				files++;
				totalSize += entry.mSize;
			}
		}

		fclose(fin);

		if (files) {
			printf("%d files, %" PRIi64 "MBytes\n", files, totalSize/1024/1024);
		}
	}

	// スライスリストをエクスポート
	if (list) {
		exportMapList(global, listFilename, map, extractDir);
	}

	printf("Read [%s.000] with %d slices, %zu files archived.\n", inputFilename.c_str(), slices, map.size());

	return 0;
}

// =====================================================================
// [EOF]
