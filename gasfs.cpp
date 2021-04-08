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

#include "GasFs.h"

namespace GasFs {

// -------------------------------------------------------------

// =====================================================================
// スライスマップの作成
// =====================================================================

int
createMap(GasFs::Global& global, GasFs::Map& map)
{
	// データベースファイルを開く
	std::string filename = global.mSliceFilename+"_000.gfs";
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
	global.mSlice.resize(slices+1);

	// スライスファイルのチェック
	GasFs::Database::SubHeader* subheader = (GasFs::Database::SubHeader*)p;
	p += sizeof(GasFs::Database::SubHeader)*slices;
	for (int i=1; i<=slices; i++) {
		char str[16];
		sprintf(str, "_%03d.gfs", i);
		filename = global.mSliceFilename + std::string(str);
		fin = fopen(filename.c_str(), "rb");
		if (fin == nullptr) {
			fprintf(stderr, "Failed: Cannot open [%s].\n", filename.c_str());
			return -1;
		}
		GasFs::Database::SubHeader b = {0};
		readsize = fread(&b, 1, sizeof(b), fin);
		fclose(fin);
		if (readsize != sizeof(b)) {
			fprintf(stderr, "Failed: Cannot read [%s].\n", filename.c_str());
			return -1;
		}
		if (memcmp(&b, &subheader[i-1], sizeof(b))) {
			fprintf(stderr, "Failed: Slice[%d] SubHeader are different [%s].\n", i, filename.c_str());
			return -1;
		}
		global.mSlice[i].mFiles = (b.mFiles[0]<<0) | (b.mFiles[1]<<8) | (b.mFiles[0]<<16);
		global.mSlice[i].mTotalSize = (b.mTotalSize[0]<<0) | (b.mTotalSize[1]<<8) | (b.mTotalSize[2]<<16) | (b.mTotalSize[3]<<24);
		global.mSlice[i].mCRC = (b.mCRC[0]<<0) | (b.mCRC[1]<<8) | (b.mCRC[2]<<16) | (b.mCRC[3]<<24);
		struct tm lt;
		lt.tm_year = ((b.mDate[0]>>4)*1000)+((b.mDate[0]&0x0f)*100)+((b.mDate[1]>>4)*10)+((b.mDate[1]&0x0f)*1) - 1900;
		lt.tm_mon = ((b.mDate[2]>>4)*10)+((b.mDate[2]&0x0f)*1) - 1;
		lt.tm_mday = ((b.mDate[3]>>4)*10)+((b.mDate[3]&0x0f)*1);
		lt.tm_hour = ((b.mDate[4]>>4)*10)+((b.mDate[4]&0x0f)*1);
		lt.tm_min = ((b.mDate[5]>>4)*10)+((b.mDate[5]&0x0f)*1);
		lt.tm_sec = ((b.mDate[6]>>4)*10)+((b.mDate[6]&0x0f)*1);
		global.mSlice[i].mLastModifiedTime = (uint64_t)mktime(&lt);
	}

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
// CRCの計算
// =====================================================================

uint32_t
GetCRC(uint8_t* buf, uint32_t bufsiz, uint32_t crc)
{
	static uint32_t table[256] = {0};
	const uint32_t magic = 0xedb88320;
	crc ^= 0xffffffff;

	if (table[0] == 0) {
		for (int i=0; i<256; i++) {
			uint32_t t = i;
			for (int j=0; j<8; j++) {
				int b = t & 1;
				t >>= 1;
				if (b) t^= magic;
			}
			table[i] = t;
		}
	}
	while (bufsiz--) {
		crc = table[(crc ^ *(buf++)) & 0xff] ^ (crc >> 8);
	}

	return crc ^ 0xffffffff;
}

// =====================================================================

};

// =====================================================================
// [EOF]
