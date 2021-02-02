// ◇
// gasfs: GORRY's archive and slice file system
// IniFile: INIファイル読み込み
// Copyright: (C)2021 Hiroaki GOTO as GORRY.
// License: see readme.txt
// =====================================================================

#if !defined(__INIFILE_H__)
#define __INIFILE_H__

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <vector>
#include <string>
#include <map>

// =====================================================================
// INIファイル読み込み
// =====================================================================


class IniFile
{
public:		// struct, enum
	typedef std::vector<std::string> ValueList;

public:		// function
	IniFile() {}
	virtual ~IniFile() {}

	int load(const char* filename);
	int load(const std::string& filename) { return load(filename.c_str()); }
	void cutTailSpace(std::string& str);
	bool hasKey(const std::string& section, const std::string& key);
	bool hasListEntry(const std::string& section, const std::string& key, const int n);
	const ValueList* getList(const std::string& section, const std::string& key);
	size_t getListSize(const std::string& section, const std::string& key);
	const std::string& getListString(const std::string& section, const std::string& key, const int n);
	const std::string& getString(const std::string& section, const std::string& key);
	int getListInt(const std::string& section, const std::string& key, const int n);
	int getInt(const std::string& section, const std::string& key);

	int save(const char* filename);
	int save(const std::string& filename) { return save(filename.c_str()); }
	void setList(const std::string& section, const std::string& key, const ValueList& list);
	void setListString(const std::string& section, const std::string& key, int n, const std::string& value);
	void setString(const std::string& section, const std::string& key, const std::string& value);
	void setListInt(const std::string& section, const std::string& key, const int n, const int value);
	void setInt(const std::string& section, const std::string& key, const int value);

private:	// function

public:		// var

private:	// var
	std::map<const std::string, ValueList> mIniFileMap;

};


#endif  // __INIFILE_H__
// =====================================================================
// [EOF]
