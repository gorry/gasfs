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

#define GASFS_VERSION "20210115a"
#define GASFS_MARK "GFS1"

namespace GasFs {

// -------------------------------------------------------------
// アーカイブデータ構造
// -------------------------------------------------------------

struct Slice {
	bool mNoAddFreeFile;
	int mFiles;
	int64_t mRest;
	uint64_t mLastModifiedTime;
	std::string mFilename;
};

struct Global {
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
	uint8_t mDummy[4];
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
createMap(GasFs::Map& map, GasFs::Global& global);



// -------------------------------------------------------------

};

// =====================================================================
// [EOF]
