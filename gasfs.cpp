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

#include "GasFs.h"

namespace GasFs {

// -------------------------------------------------------------

// =====================================================================
// スライスマップの作成
// =====================================================================

int
createMap(GasFs::Map& map, GasFs::Global& global)
{
	// データベースファイルを開く
	const std::string filename = global.mSliceFilename+"_000.gfs";
	FILE* fin = fopen(filename.c_str(), "rb");
	if (fin == nullptr) {
		fprintf(stderr, "Failed: Cannot open [%s].\n", filename.c_str());
		return -1;
	}
	fseek(fin, 0, SEEK_END);
	fpos_t pos;
	fgetpos(fin, &pos);
	size_t filesize = (size_t)pos;
	fseek(fin, 0, SEEK_SET);
	uint8_t* buf = new uint8_t[filesize];
	if (buf == nullptr) {
		fprintf(stderr, "Failed: Memory exhaused at reading [%s].\n", filename.c_str());
		fclose(fin);
		return -1;
	}
	size_t readsize = fread(buf, 1, filesize, fin);
	fclose(fin);
	if (readsize != filesize) {
		fprintf(stderr, "Failed: Cannot read [%s].\n", filename.c_str());
		return -1;
	}
	uint8_t* p = buf;

	// データベースファイルのチェック
	GasFs::Database::Header* header = (GasFs::Database::Header*)p;
	p += sizeof(GasFs::Database::Header);
	if (memcmp(&(header->mMark[0]), GASFS_MARK, 4)) {
		fprintf(stderr, "Failed: Not GasFs file [%s].\n", filename.c_str());
		fclose(fin);
		return -1;
	}
	int slices = header->mSlices[0];
	int entries = (header->mEntries[0]<<0) | (header->mEntries[1]<<8) |(header->mEntries[2]<<16);
	int maxSliceSize = (header->mMaxSliceSize[0]<<0) | (header->mMaxSliceSize[1]<<8) | (header->mMaxSliceSize[2]<<16) | (header->mMaxSliceSize[3]<<24);

	// データベースエントリを読む
	GasFs::Database::Entry* ent = (GasFs::Database::Entry*)p;
	p += sizeof(GasFs::Database::Entry)*entries;
	for (int i=0; i<entries; i++) {
		GasFs::Database::Entry* e = &ent[i];
		GasFs::Entry entry;
		entry.mSlice = e->mSlice[0];
		size_t pathofs = (e->mPathOfs[0]<<0) | (e->mPathOfs[1]<<8) | (e->mPathOfs[2]<<16);
		const std::string path((char*)(p+pathofs));
		entry.mOffset = (e->mOffset[0]<<0) | (e->mOffset[1]<<8) | (e->mOffset[2]<<16) | (e->mOffset[3]<<24) | ((uint64_t)e->mOffset[4]<<32) | ((uint64_t)e->mOffset[5]<<40);
		entry.mSize = (e->mSize[0]<<0) | (e->mSize[1]<<8) | (e->mSize[2]<<16) | (e->mSize[3]<<24) | ((uint64_t)e->mSize[4]<<32) | ((uint64_t)e->mSize[5]<<40);
		map.insert(std::make_pair(path, entry));
	}

	global.mSlices = slices;
	global.mMaxSliceSize = maxSliceSize;
	return slices;
}

// =====================================================================

};

// =====================================================================
// [EOF]
