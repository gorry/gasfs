// ◇
// gasfs: GORRY's archive and slice file system
// MkGasFs: gasfsファイルの作成
// Copyright: (C)2021 Hiroaki GOTO as GORRY.
// License: see readme.txt
// =====================================================================

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include <locale.h>
#include <wchar.h>
#include <time.h>
#include <dirent.h>

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
	   "MkGasFs: GORRY's Archive and Slice File System: Version " GASFS_VERSION " GORRY.\n"
	   "Usage:\n"
	   "  mkgasfs [input.gfi]   Load [input.gfi] and make file system.\n"
	   "Option:\n"
	   "  --output [filename]   Set output file [filename.000-999].\n"
	   "                        If this option is omitted, use [input.000-999] instead.\n"
	   "  --basedir [dir]       Set base directory of input files.\n"
	   "                        Not effect to input.gfi and output.\n"
	   "  --list [list.gfi]     Output list file.\n"
	   "  --verbose             Output verbose log.\n"
	   "  --force               Force (ignore file modified time) make file system.\n"
	   "  --help                Show this.\n"
	);
}

// =====================================================================
// パスマップの作成
// =====================================================================

bool
addPathToMap(GasFs::Global& global, GasFs::Map& map, int depth, int slice, const std::string& path)
{
	if (gVerbose) {
		for (int d=0; d<depth; d++) {
			printf(" ");
		}
		printf("[%d: %s]\n", depth, path.c_str());
	}

	const std::wstring wpath = WStrUtil::str2wstr(path);
	const WStrUtil::pathPair p = WStrUtil::pathSplitPath(wpath);
	const std::wstring& wdir = p.first;
	const std::wstring& wfilename = p.second;

	const std::wstring wbasedir = WStrUtil::str2wstr(global.mBaseDir);
	const std::wstring winputPath = WStrUtil::pathAddPath(wbasedir, wpath);
	const std::string inputPath = WStrUtil::wstr2str(winputPath);
	DIR* dirp;
	dirp = opendir(inputPath.c_str());
	if (dirp == nullptr) {
		fprintf(stderr, "Failed: Folder not found [%s].\n", inputPath.c_str());
		return false;
	}

	struct dirent* ent;
	while ((ent = readdir(dirp)) != NULL) {
		// "."は常に無視
		if (!strcmp(ent->d_name, ".")) {
			continue;
		}
		// ".."は常に無視
		if (!strcmp(ent->d_name, "..")) {
			continue;
		}
		const std::string name(ent->d_name);
		const std::wstring wname = WStrUtil::str2wstr(name);
		std::wstring wnewpath = WStrUtil::pathAddPath(wdir, wname);
		if (ent->d_type & DT_DIR) {
			// 通常ディレクトリ
			wnewpath = WStrUtil::pathAddSlash(wnewpath);
			const std::string newpath = WStrUtil::wstr2str(wnewpath);
			bool ret = addPathToMap(global, map, depth+1, slice, newpath);
			if (!ret) {
				return ret;
			}
			continue;
		}
		if (ent->d_type & DT_REG) {
			const std::string newpath = WStrUtil::wstr2str(wnewpath);
			const std::wstring wbasedir = WStrUtil::str2wstr(global.mBaseDir);
			const std::wstring wnewpathWithBasedir = WStrUtil::pathAddPath(wbasedir, wnewpath);
			const std::string newpathWithBasedir = WStrUtil::wstr2str(wnewpathWithBasedir);

			// 元ファイルがあればそれをスライスに追加
			struct _stat s;
			int st = _stat(newpathWithBasedir.c_str(), &s);
			if (st != 0) {
				fprintf(stderr, "Failed: Not found [%s].\n", newpathWithBasedir.c_str());
				return false;
			}

			// 元ファイルの情報を得る
			uint64_t filesize = s.st_size;
			uint64_t offset = 0;
			uint64_t lastmodifiedtime = s.st_mtime;

			// パスマップに追加
			GasFs::Entry entry;
			entry.mSlice = slice;
			entry.mOffset = offset;
			entry.mSize = filesize;
			entry.mLastModifiedTime = lastmodifiedtime;
			map.insert(std::make_pair(newpath, entry));

			if (gVerbose) {
				printf("  time=%" PRIu64 ", size=%10" PRIu64 ": %s\n", lastmodifiedtime, filesize, newpath.c_str());
			}
		}
	}

	return true;
}

// =====================================================================
// 入力パスマップからスライスマップの情報を埋める
// =====================================================================

bool
FillSliceMapInfoFromInputPathMap(GasFs::Global& global, GasFs::Map& mapSlice, GasFs::Map& mapInputPath)
{
	int slices = global.mSlices;
	int maxSliceSize = global.mMaxSliceSize;

	// スライスマップを列挙して入力パスマップから情報を移す
	for (int i=1; i<=slices; i++) {
		uint64_t lastModifiedTime = 0;
		global.mSlice[i].mFiles = 0;
		global.mSlice[i].mRest = (int64_t)maxSliceSize*1024*1024 - sizeof(GasFs::Database::SubHeader);

		if (gVerbose) {
			printf("Slice %03d: Specified Files...\n", i);
		}

		// スライスに必ず入れるファイルを追加
		for (auto& e : mapSlice) {
			const std::string& pathSlice = e.first;
			GasFs::Entry& entrySlice = e.second;
			int slice = entrySlice.mSlice;
			if (i != slice) {
				continue;
			}

			// 入力パスマップにスライスマップのファイル情報があるか確認
			// ない場合、すでに他のスライスで指定されていて削除された可能性が高い
			GasFs::Map::iterator it = mapInputPath.find(pathSlice);
			if (it == mapInputPath.end()) {
				fprintf(stderr, "Failed: Not found (already selected by other slice?) entry [%s] from [Input] PathList.\n", pathSlice.c_str());
				return false;
			}

			// スライス容量を計算
			global.mSlice[slice].mRest -= (int64_t)entrySlice.mSize;
			if (lastModifiedTime < entrySlice.mLastModifiedTime) {
				lastModifiedTime = entrySlice.mLastModifiedTime;
			}

			// 入力パスマップから削除
			if (gVerbose) {
				printf(" [%4" PRIi64 "MB] %s\n", global.mSlice[slice].mRest/1024/1024, pathSlice.c_str());
			}
			mapInputPath.erase(it);
			global.mSlice[i].mFiles++;
		}

		// ファイル全体の最終更新時刻で一番新しいものを持っておく
		if (global.mSlice[i].mLastModifiedTime < lastModifiedTime) {
			global.mSlice[i].mLastModifiedTime = lastModifiedTime;
		}

		// スライス容量チェック
		if (global.mSlice[i].mRest < 0) {
			fprintf(stderr, "Failed: Not enough size (%" PRIi64 "MB) at Slice %03d\n", -global.mSlice[i].mRest/1024/1024, i);
			return false;
		}
		if (gVerbose) {
			printf(" -> Slice %03d: Store %d files, rest=%" PRIi64 "MB\n", i, global.mSlice[i].mFiles, global.mSlice[i].mRest/1024/1024);
			if (global.mSlice[i].mNoAddFreeFile) {
				printf(" -> Specified '****'(No Add Free Files)\n");
			}
		}
	}

	if (gVerbose) {
		printf("\nAdd Free %d files to rest Slice...\n", mapInputPath.size());
	}

	// 入力パスマップを列挙してスライスマップへ情報を移す
	int toslice = 1;
	for (auto& e: mapInputPath) {
		const std::string& pathInput = e.first;
		GasFs::Entry& entryInput = e.second;

		// ファイル容量分の空きがあるスライスを探す
		int64_t filesize = (int64_t)entryInput.mSize;
		int i;
		for (i=0; i<slices; i++) {
			if (global.mSlice[toslice].mRest >= filesize) {
				if (!global.mSlice[toslice].mNoAddFreeFile) {
					// 空いてて追加禁止属性のないスライスを見つけた
					break;
				}
			}
			toslice++;
			if (toslice > slices) {
				toslice = 1;
			}
		}
		if (i >= slices) {
			fprintf(stderr, "Failed: Not enough slices (%d) at file [%s]", slices, pathInput.c_str());
			return false;
		}

		// スライスの情報を更新
		global.mSlice[toslice].mFiles++;
		global.mSlice[toslice].mRest -= filesize;
		if (global.mSlice[toslice].mLastModifiedTime < entryInput.mLastModifiedTime) {
			global.mSlice[toslice].mLastModifiedTime = entryInput.mLastModifiedTime;
		}

		// スライスマップに追加
		GasFs::Entry entry;
		entry.mSlice = toslice;
		entry.mOffset = 0;
		entry.mSize = (uint64_t)filesize;
		entry.mLastModifiedTime = entryInput.mLastModifiedTime;
		mapSlice.insert(std::make_pair(pathInput, entry));

		if (gVerbose) {
			printf(" to Slice %03d [%4" PRIi64 "MB]: %s\n", toslice, global.mSlice[toslice].mRest/1024/1024, pathInput.c_str());
		}
	}

	if (gVerbose) {
		printf("\n");
		for (int i=1; i<=slices; i++) {
			printf("Slice %03d: Store %d files, rest=%" PRIi64 "MB\n", i, global.mSlice[i].mFiles, global.mSlice[i].mRest/1024/1024);
		}
	}

	return true;
}

// =====================================================================
// スライスマップからスライスファイルを作成する
// =====================================================================

bool
MakeSliceFileFromSliceMap(GasFs::Global& global, GasFs::Map& mapSlice)
{
	int slices = global.mSlices;
	int maxSliceSize = global.mMaxSliceSize;
	const std::string& sliceFilename = global.mSliceFilename;

	for (int i=1; i<=slices; i++) {
		uint32_t crc = 0;
		int64_t totalSize = 0;
		bool skip = false;

		// スライスのファイル名を決定
		char slicePath[_MAX_PATH];
		sprintf(slicePath, "%s_%03d.gfs", sliceFilename.c_str(), i);
		if (gVerbose) {
			printf("Output Slice %03d file [%s] ... ", i, slicePath);
		}

		// スライスの更新確認
		uint64_t lastmodifiedtime = 0;
		struct _stat s;
		int st = _stat(slicePath, &s);
		if (st == 0) {
			lastmodifiedtime = s.st_mtime;
		}
		if (!global.mForce) {
			if (st == 0) {
				// スライスに入れるファイル全部の最終更新時刻がスライスより古いときはスキップ
				if (lastmodifiedtime > global.mSlice[i].mLastModifiedTime) {
					if (gVerbose) {
						printf("Skip modifying [%s]: Slice time(%" PRIu64 ") > Files time(%" PRIu64 ").\n", slicePath, lastmodifiedtime, global.mSlice[i].mLastModifiedTime);
					}
					skip = true;
				} else {
					if (gVerbose) {
						printf("modifying [%s]: Slice time(%" PRIu64 ") <= Files time(%" PRIu64 ")... ", slicePath, lastmodifiedtime, global.mSlice[i].mLastModifiedTime);
					}
				}
			} else {
				if (gVerbose) {
					printf("creating [%s] ... ", slicePath);
				}
			}
		} else {
			if (gVerbose) {
				printf("creating [%s]: by --force option ... ", slicePath);
			}
		}

		// スライスサブヘッダが同一か確認
		if (skip) {
			GasFs::Database::SubHeader b = {0};
			if (gVerbose) {
				printf("Check Slice SubHeader [%s]\n", slicePath);
			}
			skip = false;
			FILE *fin = fopen(slicePath, "rb");
			if (fin != nullptr) {
				fread(&b, 1, sizeof(b), fin);
				fclose(fin);
				if (!memcmp(&(b.mMark[0]), GASFS_SUBMARK, 4)) {
					if (gVerbose) {
						printf("Skip modifying [%s]: Slice SubHeader OK.\n", slicePath);
					}
					skip = true;

					global.mSlice[i].mFiles = (b.mFiles[0]<<0) | (b.mFiles[1]<<8) | (b.mFiles[2]<<16);
					global.mSlice[i].mTotalSize = (b.mTotalSize[0]<<0) | (b.mTotalSize[1]<<8) | (b.mTotalSize[2]<<16) | (b.mTotalSize[3]<<24);
					global.mSlice[i].mCRC = (b.mCRC[0]<<0) | (b.mCRC[1]<<8) | (b.mCRC[2]<<16) | (b.mCRC[3]<<24);
					struct tm lt;
					lt.tm_year = ((b.mDate[0]>>4)*1000)+((b.mDate[0]&0x0f)*100)+((b.mDate[1]>>4)*10)+((b.mDate[1]&0x0f)*1) - 1900;
					lt.tm_mon = ((b.mDate[2]>>4)*10)+((b.mDate[2]&0x0f)*1) - 1;
					lt.tm_mday = ((b.mDate[3]>>4)*10)+((b.mDate[3]&0x0f)*1);
					lt.tm_hour = ((b.mDate[4]>>4)*10)+((b.mDate[4]&0x0f)*1);
					lt.tm_min = ((b.mDate[5]>>4)*10)+((b.mDate[5]&0x0f)*1);
					lt.tm_sec = ((b.mDate[6]>>4)*10)+((b.mDate[6]&0x0f)*1);
					global.mSlice[i].mLastModifiedTime = (uint64_t)_mkgmtime(&lt);
				}
			}
		}

		// スライスを開く
		FILE *fout = nullptr;
		if (!skip) {
			fout = fopen(slicePath, "wb");
			if (fout == nullptr) {
				fprintf(stderr, "Failed: Cannot open slice [%s].\n", slicePath);
				return false;
			}

			// サブヘッダの分を書く
			GasFs::Database::SubHeader b = {0};
			size_t wrotesize = fwrite(&b, 1, sizeof(b), fout);
			if (wrotesize != sizeof(b)) {
				fprintf(stderr, "Failed: Cannot write slice [%s].\n", slicePath);
				return false;
			}
		}

		// スライスマップを列挙
		for (auto& e: mapSlice) {
			const std::string& path = e.first;
			GasFs::Entry& entry = e.second;
			if (entry.mSlice != i) {
				continue;
			}

			// 入力を開く
			const std::wstring wpath = WStrUtil::str2wstr(path);
			const std::wstring wbasedir = WStrUtil::str2wstr(global.mBaseDir);
			const std::wstring wpathWithBasedir = WStrUtil::pathAddPath(wbasedir, wpath);
			const std::string inputPath = WStrUtil::wstr2str(wpathWithBasedir);
			FILE *fin = nullptr;
			if (!skip) {
				fin = fopen(inputPath.c_str(), "rb");
				if (fin == nullptr) {
					fprintf(stderr, "Failed: Cannot open input [%s].\n", inputPath.c_str());
					fclose(fout);
					return false;
				}
			}

			// スライスのオフセットを記録
			entry.mOffset = (size_t)totalSize;

			if (!skip) {
				// bufsizeずつスライスに書き写す
				uint64_t rest = entry.mSize;
				const int bufsize = 1024*1024*16;
				uint8_t* buf = new uint8_t[bufsize];
				while (rest > 0) {
					size_t readsize = fread(buf, 1, bufsize, fin);
					if (readsize == 0) {
						break;
					}
					crc = GasFs::GetCRC(buf, readsize, crc);
					size_t wrotesize = fwrite(buf, 1, readsize, fout);
					if (wrotesize != readsize) {
						fclose(fin);
						fprintf(stderr, "Failed: Cannot write slice [%s].\n", slicePath);
						return false;
					}
					rest -= readsize;
				}
				delete[] buf;
				fclose(fin);
			}

			totalSize += (int64_t)entry.mSize;
		}
		if (!skip) {
			global.mSlice[i].mTotalSize = (uint64_t)totalSize;
			global.mSlice[i].mCRC = crc;
		}
		if (gVerbose) {
			printf("%" PRIi64 "MB\n", totalSize/1024/1024);
		}

		// スライスサブヘッダを記録
		if (!skip) {
			GasFs::Database::SubHeader b = {0};

			struct tm lt = *(gmtime((const time_t*)&(global.mSlice[i].mLastModifiedTime)));
			char str[16];
			sprintf(str, "0x%04d%02d", lt.tm_year+1900, lt.tm_mon+1);
			uint32_t date1 = strtoul(str, nullptr, 0);
			sprintf(str, "0x%02d%02d%02d%02d", lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
			uint32_t date2 = strtoul(str, nullptr, 0);

			size_t files = global.mSlice[i].mFiles;
			memcpy(&(b.mMark[0]), GASFS_SUBMARK, 4);
			if (gVerbose) {
				printf("slice=%d, files=%zu, date=%06x%08x\n", i, files, date1, date2);
			}
			b.mSliceNo[0] = i;
			b.mFiles[0] = (files>>0)&0xff;
			b.mFiles[1] = (files>>8)&0xff;
			b.mFiles[2] = (files>>16)&0xff;
			b.mTotalSize[0] = (totalSize>>0)&0xff;
			b.mTotalSize[1] = (totalSize>>8)&0xff;
			b.mTotalSize[2] = (totalSize>>16)&0xff;
			b.mTotalSize[3] = (totalSize>>24)&0xff;
			b.mCRC[0] = (crc>>0)&0xff;
			b.mCRC[1] = (crc>>8)&0xff;
			b.mCRC[2] = (crc>>16)&0xff;
			b.mCRC[3] = (crc>>24)&0xff;
			b.mDate[0] = (date1>>16)&0xff;  // 2021/04/07 18:45:01なら"20 21 04 07 18 45 01"のバイト列になる
			b.mDate[1] = (date1>> 8)&0xff;
			b.mDate[2] = (date1>> 0)&0xff;
			b.mDate[3] = (date2>>24)&0xff;
			b.mDate[4] = (date2>>16)&0xff;
			b.mDate[5] = (date2>> 8)&0xff;
			b.mDate[6] = (date2>> 0)&0xff;
			size_t writeSize = sizeof(b);
			fseek(fout, 0, SEEK_SET);
			size_t wroteSize = fwrite(&b, 1, writeSize, fout);
			if (wroteSize != writeSize) {
				fprintf(stderr, "Failed: Cannot write slice [%s].\n", slicePath);
				fclose(fout);
				return false;
			}
		}

		// スライスを閉じる
		if (!skip) {
			int err = fclose(fout);
			if (err) {
				fprintf(stderr, "Failed: Cannot write slice [%s].\n", slicePath);
				return false;
			}
			int st = _stat(slicePath, &s);
			if (st == 0) {
				lastmodifiedtime = s.st_mtime;
			}
		}
		if (global.mLastModifiedTime < lastmodifiedtime) {
			global.mLastModifiedTime = lastmodifiedtime;
		}
	}

	return true;
}

// =====================================================================
// スライスマップからスライスデータベースを作成する
// =====================================================================

bool
MakeSliceDatabaseFileFromSliceMap(const GasFs::Global& global, const GasFs::Map& mapSlice)
{
	int slices = global.mSlices;
	int maxSliceSize = global.mMaxSliceSize;
	const std::string& sliceFilename = global.mSliceFilename;

	// データベースを開く
	char dbPath[_MAX_PATH];
	sprintf(dbPath, "%s_000.gfs", sliceFilename.c_str());
	if (gVerbose) {
		printf("Output Slice Database file [%s]\n", dbPath);
	}
	struct _stat s;
	int st = _stat(dbPath, &s);
	if (st == 0) {
		uint64_t lastmodifiedtime = s.st_mtime;
		if (lastmodifiedtime > global.mLastModifiedTime) {
			// 全てのスライスの最終更新時刻がデータベースより古かったら更新しない
			if (gVerbose) {
				printf("Skip modifying [%s]: Database time(%" PRIu64 ") > Slices time(%" PRIu64 ").\n", dbPath, lastmodifiedtime, global.mLastModifiedTime);
			}
			return true;
		}
	}
	FILE *fout = fopen(dbPath, "wb");
	if (fout == nullptr) {
		fprintf(stderr, "Failed: Cannot open slice [%s].\n", dbPath);
		return false;
	}

	// データベースヘッダを書き出す
	{
		GasFs::Database::Header b = {0};

		struct tm lt = *(gmtime((const time_t*)&(global.mLastModifiedTime)));
		char str[16];
		sprintf(str, "0x%04d%02d", lt.tm_year+1900, lt.tm_mon+1);
		uint32_t date1 = strtoul(str, nullptr, 0);
		sprintf(str, "0x%02d%02d%02d%02d", lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
		uint32_t date2 = strtoul(str, nullptr, 0);

		size_t entries = mapSlice.size();
		memcpy(&(b.mMark[0]), GASFS_MARK, 4);
		if (gVerbose) {
			printf("slices=%d, entries=%zu, maxSliceSize=%d\n", slices, entries, maxSliceSize);
		}
		b.mSlices[0] = slices;
		b.mEntries[0] = (entries>>0)&0xff;
		b.mEntries[1] = (entries>>8)&0xff;
		b.mEntries[2] = (entries>>16)&0xff;
		b.mMaxSliceSize[0] = (maxSliceSize>>0)&0xff;
		b.mMaxSliceSize[1] = (maxSliceSize>>8)&0xff;
		b.mMaxSliceSize[2] = (maxSliceSize>>16)&0xff;
		b.mMaxSliceSize[3] = (maxSliceSize>>24)&0xff;
		b.mDate[0] = (date1>>16)&0xff;  // 2021/04/07 18:45:01なら"20 21 04 07 18 45 01"のバイト列になる
		b.mDate[1] = (date1>> 8)&0xff;
		b.mDate[2] = (date1>> 0)&0xff;
		b.mDate[3] = (date2>>24)&0xff;
		b.mDate[4] = (date2>>16)&0xff;
		b.mDate[5] = (date2>> 8)&0xff;
		b.mDate[6] = (date2>> 0)&0xff;
		size_t writeSize = sizeof(b);
		size_t wroteSize = fwrite(&b, 1, writeSize, fout);
		if (wroteSize != writeSize) {
			fprintf(stderr, "Failed: Cannot write slice [%s].\n", dbPath);
			fclose(fout);
			return false;
		}
	}

	// スライスリストを書き出す
	for (int i=1; i<=slices; i++) {
		GasFs::Database::SubHeader b = {0};
		struct tm lt = *(gmtime((const time_t*)&(global.mSlice[i].mLastModifiedTime)));
		char str[16];
		sprintf(str, "0x%04d%02d", lt.tm_year+1900, lt.tm_mon+1);
		uint32_t date1 = strtoul(str, nullptr, 0);
		sprintf(str, "0x%02d%02d%02d%02d", lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
		uint32_t date2 = strtoul(str, nullptr, 0);

		size_t files = global.mSlice[i].mFiles;
		uint64_t totalSize = global.mSlice[i].mTotalSize;
		uint32_t crc = global.mSlice[i].mCRC;
		memcpy(&(b.mMark[0]), GASFS_SUBMARK, 4);
		if (gVerbose) {
			printf("slice=%d, files=%zu, date=%06x%08x\n", i, files, date1, date2);
		}
		b.mSliceNo[0] = i;
		b.mFiles[0] = (files>>0)&0xff;
		b.mFiles[1] = (files>>8)&0xff;
		b.mFiles[2] = (files>>16)&0xff;
		b.mTotalSize[0] = (totalSize>>0)&0xff;
		b.mTotalSize[1] = (totalSize>>8)&0xff;
		b.mTotalSize[2] = (totalSize>>16)&0xff;
		b.mTotalSize[3] = (totalSize>>24)&0xff;
		b.mCRC[0] = (crc>>0)&0xff;
		b.mCRC[1] = (crc>>8)&0xff;
		b.mCRC[2] = (crc>>16)&0xff;
		b.mCRC[3] = (crc>>24)&0xff;
		b.mDate[0] = (date1>>16)&0xff;  // 2021/04/07 18:45:01なら"20 21 04 07 18 45 01"のバイト列になる
		b.mDate[1] = (date1>> 8)&0xff;
		b.mDate[2] = (date1>> 0)&0xff;
		b.mDate[3] = (date2>>24)&0xff;
		b.mDate[4] = (date2>>16)&0xff;
		b.mDate[5] = (date2>> 8)&0xff;
		b.mDate[6] = (date2>> 0)&0xff;
		size_t writeSize = sizeof(b);
		size_t wroteSize = fwrite(&b, 1, writeSize, fout);
		if (wroteSize != writeSize) {
			fprintf(stderr, "Failed: Cannot write slice [%s].\n", dbPath);
			fclose(fout);
			return false;
		}
	}

	// データベースエントリを書き出す
	std::vector<char> pathBuf;
	size_t pathOfs = 0;
	for (const auto& e: mapSlice) {
		const std::string& path = e.first;
		const GasFs::Entry& entry = e.second;
		GasFs::Database::Entry b;
		if (gVerbose) {
			printf("slice=%d, pathOfs=%zu, offset=%" PRIu64 ", size=%" PRIu64 ": %s\n", entry.mSlice, pathOfs, entry.mOffset, entry.mSize, path.c_str());
		}
		b.mSlice[0] = entry.mSlice;
		b.mPathOfs[0] = (pathOfs>>0)&0xff;
		b.mPathOfs[1] = (pathOfs>>8)&0xff;
		b.mPathOfs[2] = (pathOfs>>16)&0xff;
		size_t len = path.size();
		size_t ofs = pathBuf.size();
		pathBuf.resize(ofs+len+1);
		memcpy(&pathBuf[ofs], &path[0], len+1);  // '\0'を含む
		pathOfs += len+1;
		b.mOffset[0] = (entry.mOffset>>0)&0xff;
		b.mOffset[1] = (entry.mOffset>>8)&0xff;
		b.mOffset[2] = (entry.mOffset>>16)&0xff;
		b.mOffset[3] = (entry.mOffset>>24)&0xff;
		b.mOffset[4] = (entry.mOffset>>32)&0xff;
		b.mOffset[5] = (entry.mOffset>>40)&0xff;
		b.mSize[0] = (entry.mSize>>0)&0xff;
		b.mSize[1] = (entry.mSize>>8)&0xff;
		b.mSize[2] = (entry.mSize>>16)&0xff;
		b.mSize[3] = (entry.mSize>>24)&0xff;
		b.mSize[4] = (entry.mSize>>32)&0xff;
		b.mSize[5] = (entry.mSize>>40)&0xff;
		size_t writeSize = sizeof(b);
		size_t wroteSize = fwrite(&b, 1, writeSize, fout);
		if (wroteSize != writeSize) {
			fprintf(stderr, "Failed: Cannot write slice [%s].\n", dbPath);
			fclose(fout);
			return false;
		}
	}

	// Pathリストをデータベースに書き出す
	{
		size_t writeSize = pathBuf.size();
		size_t wroteSize = fwrite(pathBuf.data(), 1, writeSize, fout);
		if (wroteSize != writeSize) {
			fprintf(stderr, "Failed: Cannot write slice [%s].\n", dbPath);
			fclose(fout);
			return false;
		}
	}

	// 終了
	int err = fclose(fout);
	if (err) {
		fprintf(stderr, "Failed: Cannot write slice [%s].\n", dbPath);
		return false;
	}

	return true;
}


// =====================================================================
// スライスマップのエクスポート
// =====================================================================

bool
exportMapList(const GasFs::Global& global, const std::string& outputFilename, const GasFs::Map& map, const IniFile::ValueList* valueList)
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
	for (int i=0; i<(int)valueList->size(); i++) {
		fprintf(fout, "\t%s\n", valueList->at(i).c_str());
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
		fprintf(fout, "]]]]\n");
		fprintf(fout, "\n");
	}

	fprintf(fout, "# EOF\n");
	fclose(fout);

	return true;
}

// =====================================================================
// メイン
// =====================================================================

int
wmain(int argc, wchar_t** argv, wchar_t** envp)
{
	GasFs::Global global = {0};
	bool ret;
	bool list = false;
	std::string listFilename;
	std::string inputFilename;
	std::string outputFilename;
	std::string basedir;
	std::wstring winputFilename;
	std::wstring wbasedir;

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
		if (arg == "--basedir") {
			if (i+1 >= argc) {
				fprintf(stderr, "Failed: Specify --basedir param.\n");
				exit(EXIT_FAILURE);
			}
			std::wstring wfilename(argv[i+1]);
			wfilename = WStrUtil::pathBackslash2Slash(wfilename);
			wbasedir = WStrUtil::pathAddSlash(wfilename);
			global.mBaseDir = WStrUtil::wstr2str(wbasedir);
			i++;
			continue;
		}
		if (arg == "--output") {
			if (i + 1 >= argc) {
				fprintf(stderr, "Failed: Specify --output param.\n");
				exit(EXIT_FAILURE);
			}
			std::wstring wfilename(argv[i+1]);
			wfilename = WStrUtil::pathBackslash2Slash(wfilename);
			outputFilename = WStrUtil::wstr2str(wfilename);
			i++;
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
		if (arg == "--verbose") {
			gVerbose = true;
			continue;
		}
		if (arg == "--force") {
			global.mForce = true;
			continue;
		}
		if (!inputFilename.empty()) {
			fprintf(stderr, "Failed: Specify only one [input.gfi].\n");
			exit(EXIT_FAILURE);
		}
		{
			std::wstring wfilename(argv[i]);
			winputFilename = WStrUtil::pathBackslash2Slash(wfilename);
			// 入力に".gfi"が付いていたら削る
			size_t len = winputFilename.size();
			if (winputFilename.substr(len-4) == L".gfi") {
				winputFilename.erase(len-4, 4);
			}
			inputFilename = WStrUtil::wstr2str(winputFilename);
		}
	}

	// 入力がなければ終了
	if (inputFilename.empty()) {
		fprintf(stderr, "Failed: Specify [input.gfi].\n");
		exit(EXIT_FAILURE);
	}

#if defined(_WINDOWS)
	if (gVerbose) {
		printf("Locale = [%s]\n", env);
	}
#endif

	// 出力がなければ入力のファイル名部を使い回す
	if (outputFilename.empty()) {
		winputFilename = WStrUtil::str2wstr(inputFilename);
		const WStrUtil::pathPair p = WStrUtil::pathSplitPath(winputFilename);
		const std::wstring& wfilename = p.second;
		outputFilename = WStrUtil::wstr2str(wfilename);
	}

	// GFIファイルを読む
	IniFile inputGFI;
	std::string inputPath = WStrUtil::wstr2str(winputFilename) + ".gfi";
	if (gVerbose) {
		printf("\n* Load GFI file [%s]\n", inputPath.c_str());
	}
	int err = inputGFI.load(inputPath);
	if (err < 0) {
		fprintf(stderr, "Failed: Cannot Open [%s].\n", inputPath.c_str());
		exit(EXIT_FAILURE);
	}
	global.mGFIFilename = inputPath;

	// グローバル情報をゲット
	if (gVerbose) {
		printf("\n* Read information from [%s]\n", inputPath.c_str());
	}
	int slices = inputGFI.getInt("Global", "Slices");
	int maxSliceSize = inputGFI.getInt("Global", "MaxSliceSize");

	// すでにスライスデータベースが存在していれば読む
	// 構造が異なる場合は--forceが付いているものとして扱う
	global.mSliceFilename = outputFilename;
	char dbPath[_MAX_PATH];
	sprintf(dbPath, "%s_000.gfs", global.mSliceFilename.c_str());
	GasFs::Map mapOldSlice;
	FILE* fin = fopen(dbPath, "rb");
	if (fin) {
		GasFs::Database::Header b = {0};
		fread(&b, 1, sizeof(b), fin);
		fclose(fin);
		if (!memcmp(&(b.mMark[0]), GASFS_MARK, 4)) {
			int ret = GasFs::createMap(global, mapOldSlice);
			if (ret >= 0) {
				do {
					if (global.mSlices != slices) {
						printf("treat as --force option: old Slice database [%s] slices(%d) is not equal to new slices(%d).\n", dbPath, global.mSlices, slices);
						global.mForce = true;
						break;
					}
					if (global.mMaxSliceSize != maxSliceSize) {
						printf("treat as --force option: old Slice database [%s] max slice size(%d) is not equal to new max slice size(%d).\n", dbPath, global.mMaxSliceSize, maxSliceSize);
						global.mForce = true;
						break;
					}
				} while (0);
			}
		}
	}

	// グローバル情報を更新
	global.mSlices = slices;
	global.mMaxSliceSize = maxSliceSize;
	global.mLastModifiedTime = 0;
	global.mSlice.resize(slices+1);

	// GFIファイルが新しい場合は--forceが付いているものとして扱う
	{
		uint64_t lmdInput = 0;
		uint64_t lmdOutput = 0;
		struct _stat s;
		int st = _stat(global.mGFIFilename.c_str(), &s);
		if (st == 0) {
			lmdInput = s.st_mtime;
		}
		st = _stat(dbPath, &s);
		if (st == 0) {
			lmdOutput = s.st_mtime;
		}
		if (lmdInput > lmdOutput) {
			global.mForce = true;
			if (gVerbose) {
				printf("treat as --force option: Slice file [%s] time(%" PRIu64 ") < GFI file time(%" PRIu64 ")\n", dbPath, lmdOutput, lmdInput);
			}
		}
	}

	// [Input]ファイルリストから入力パスマップを作る
	GasFs::Map mapInputPath;
	const IniFile::ValueList* inputPathList = inputGFI.getList("Input", "PathList");
	if (inputPathList) {
		if (gVerbose) {
			printf("\n* Create Input PathMap\n");
		}
		size_t listSize = inputPathList->size();
		for (int j=0; j<(int)listSize; j++) {
			const std::string& path = inputPathList->at(j);
			ret = addPathToMap(global, mapInputPath, 1, 0, path);
			if (!ret) {
				exit(EXIT_FAILURE);
			}
		}
	}

	// スライスファイルリストからスライスマップを作る
	if (gVerbose) {
		printf("\n* Create SliceMap\n");
	}
	GasFs::Map mapSlice;
	for (int i=1; i<=slices; i++) {
		char buf[16];
		sprintf(buf, "%03d", i);
		const IniFile::ValueList* slicePathList = inputGFI.getList(buf, "PathList");
		if (slicePathList) {
			if (gVerbose) {
				printf("[%s]\n", buf);
			}
			size_t listSize = slicePathList->size();
			for (int j=0; j<(int)listSize; j++) {
				const std::string& path = slicePathList->at(j);
				if (path.compare("****")) {
					ret = addPathToMap(global, mapSlice, 1, i, path);
					if (!ret) {
						exit(EXIT_FAILURE);
					}
				} else {
					global.mSlice[i].mNoAddFreeFile = true;
				}
			}
		}
	}

	// 入力パスマップからスライスマップの情報を埋める
	if (gVerbose) {
		printf("\n* Fill SliceMap\n");
	}
	ret = FillSliceMapInfoFromInputPathMap(global, mapSlice, mapInputPath);
	if (!ret) {
		exit(EXIT_FAILURE);
	}

	// 現在のスライスデータベースと内容が食い違う場合は--forceが付いているものとして扱う
	if (!global.mForce) {
		if (gVerbose) {
			printf("\n* Compare SliceMap\n");
		}
		do {
			if (mapSlice.size() != mapOldSlice.size()) {
				global.mForce = true;
				if (gVerbose) {
					printf("treat as --force option: old Slice file [%s] files(%d) != new Slice files(%d)\n", dbPath, mapOldSlice.size(), mapSlice.size());
				}
				break;
			}
			GasFs::Map::iterator it1 = mapSlice.begin();
			GasFs::Map::iterator it2 = mapOldSlice.begin();
			while (!0) {
				if ((it1 == mapSlice.end()) || (it2 == mapOldSlice.end())) {
					break;
				}
				const std::string& path1 = it1->first;
				const GasFs::Entry& entry1 = it1->second;
				const std::string& path2 = it2->first;
				const GasFs::Entry& entry2 = it2->second;
				if (path1 != path2) {
					global.mForce = true;
					if (gVerbose) {
						printf("treat as --force option: different entry: old File [%s], new File [%s]\n", path1.c_str(), path2.c_str());
					}
					break;
				}
				if (entry1.mSlice != entry2.mSlice) {
					global.mForce = true;
					if (gVerbose) {
						printf("treat as --force option: old File [%s] Slice(%d) != new File [%s] Slice(%d)\n", path2.c_str(), entry2.mSlice, path1.c_str(), entry1.mSlice);
					}
					break;
				}
				it1++;
				it2++;
			}
			if (!global.mForce) {
				if (gVerbose) {
					printf("all old Slice file [%s] files(%d) == new Slice files(%d)\n", dbPath, mapOldSlice.size(), mapSlice.size());
				}
			}
		} while (0);
	}

	// スライスマップからスライスファイルを作る
	if (gVerbose) {
		printf("\n* Make Slice File\n");
	}
	ret = MakeSliceFileFromSliceMap(global, mapSlice);
	if (!ret) {
		exit(EXIT_FAILURE);
	}

	// スライスマップからスライスデータベースファイルを作る
	if (gVerbose) {
		printf("\n* Make Slice Database\n");
	}
	ret = MakeSliceDatabaseFileFromSliceMap(global, mapSlice);
	if (!ret) {
		exit(EXIT_FAILURE);
	}

	// スライスリストをエクスポート
	if (list) {
		if (gVerbose) {
			printf("\n* Export Slice List to [%s]\n", listFilename.c_str());
		}
		exportMapList(global, listFilename, mapSlice, inputPathList);
	}

	printf("Output [%s_*.gfs] with %d slices, %zu files archived.\n", outputFilename.c_str(), slices, mapSlice.size());

	return 0;
}

// =====================================================================
// [EOF]
