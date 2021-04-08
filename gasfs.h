// ◇
// gasfs: GORRY's archive and slice file system
// GasFs: gasfsファイルの構造
// Copyright: (C)2021 Hiroaki GOTO as GORRY.
// License: see readme.txt
// =====================================================================


#include <stdint.h>
#include <string>
#include <map>
#include <vector>

#define GASFS_VERSION "20210407a"
#define GASFS_MARK "GFS2"
#define GASFS_SUBMARK "gFS2"

namespace GasFs {

// -------------------------------------------------------------
// アーカイブデータ構造
// -------------------------------------------------------------

struct Slice {
	bool mNoAddFreeFile;
	int mFiles;
	int64_t mRest;
	uint64_t mLastModifiedTime;
	uint64_t mTotalSize;
	uint32_t mCRC;
	std::string mFilename;
};

struct Global {
	int mEntries;
	int mSlices;
	int mMaxSliceSize;
	bool mForce;
	uint64_t mLastModifiedTime;
	std::string mGFIFilename;
	std::string mSliceFilename;
	std::string mBaseDir;
	std::vector<Slice> mSlice;
};

struct Entry {
	int mSlice;
	uint64_t mOffset;
	uint64_t mSize;
	uint64_t mLastModifiedTime;
};

typedef std::map<const std::string, Entry> Map;

namespace Database {

struct Header {
	uint8_t mMark[3];
	uint8_t mVersion[1];
	uint8_t mSlices[1];
	uint8_t mEntries[3];
	uint8_t mMaxSliceSize[4];
	uint8_t mDate[7];
	uint8_t mDummy[13];
};

struct SubHeader {
	uint8_t mMark[3];
	uint8_t mVersion[1];
	uint8_t mSliceNo[1];
	uint8_t mFiles[3];
	uint8_t mTotalSize[4];
	uint8_t mCRC[4];
	uint8_t mDate[7];
	uint8_t mDummy[9];
};

struct Entry {
	uint8_t mSlice[1];
	uint8_t mPathOfs[3];
	uint8_t mOffset[6];
	uint8_t mSize[6];
};

};

// -------------------------------------------------------------

int
createMap(GasFs::Global& global, GasFs::Map& map);

uint32_t
GetCRC(uint8_t* buf, uint32_t bufsiz, uint32_t crc=0);


// -------------------------------------------------------------

};

// =====================================================================
// [EOF]
