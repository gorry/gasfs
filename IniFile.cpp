// ◇
// gasfs: GORRY's archive and slice file system
// IniFile: INIファイル読み込み
// Copyright: (C)2020 Hiroaki GOTO as GORRY.
// License: see readme.txt
// =====================================================================

#include "IniFile.h"

// =====================================================================
// INIファイル読み込み
// =====================================================================

/*
■ルール

・"#"で始まる行は、コメントとして行末まで読み飛ばす。
・"["で始まる行は、"]"までの文字列を「セクション名」とする。
・それ以外で始まる行は、"="の左側文字列を「キー名」、右側文字列を
  「バリュー」としてデータベースへ登録する。
  "="の左右にスペースを入れると、それらはキー名末尾／バリュー先頭に含まれる。
・バリューの末尾にあるスペースはカットされる。
・行末は0x0d・0x0aどちらでもよい。また0x1aは改行文字と同じ扱いをする。
・データベースは「セクション名」と「キー名」を指定して、データを読み出す。
・"="の右側を"[[[["にすると、１行１項目によるリストの指定となる。
  リストの末尾は"]]]]"とする。

ファイルの実例は以下の通り。
-------------------------------------------------------------
# コメント

[セクション名]
キー名=バリュー
キー名=[[[[[
    バリュー1
    バリュー2
]]]]
-------------------------------------------------------------

*/

// -------------------------------------------------------------
// INIファイル読み込み
// -------------------------------------------------------------
int
IniFile::load(const char* filename)
{
	std::string section;
	std::string key;
	std::string value;
	ValueList list;

	// INIファイルを開く
	FILE *fin = fopen(filename, "r");
	if (fin == nullptr) {
		return -1;
	}

	// ファイル末尾まで
	int mode = 0;
	while (!feof(fin)) {
		// １行読む
		char buf[4096];
		if (!fgets(buf, sizeof(buf), fin)) {
			break;
		}

		char* p = &buf[0];
		char c;
		while ((c = *(p++)) != '\0') {
			switch (mode) {
			  default:
			  case 0:
				// モード0: 行頭読み取り
				if ((c == 0x0d) || (c == 0x0a) || (c == 0x1a)) {
					// 改行またはEOFなら読み飛ばす
					continue;
				}
				if ((c == ' ') || (c == '\t')) {
					// スペース・タブなら読み飛ばす
					continue;
				}
				if (c == '#') {
					// "#"ならコメント扱い、行末まで読み飛ばす
					goto newline;
				}
				if (c == '[') {
					// "["ならセクション名としてモード1へ遷移
					section.clear();
					mode = 1;
					continue;
				}
				// それ以外ならキー名として追加し、モード2へ遷移
				key.clear();
				key.push_back(c);
				mode = 2;
				continue;

			  case 1:
				// モード1: セクション名読み取り
				if ((c == 0x0d) || (c == 0x0a) || (c == 0x1a)) {
					// 改行またはEOFならセクション名終了、モード0へ遷移
					mode = 0;
					continue;
				}
				if (c == ']') {
					// 改行またはEOFならセクション名終了、行末まで読み飛ばす
					goto newline;
				}
				// それ以外ならセクション名として追加
				section.push_back(c);
				continue;

			  case 2:
				// モード2: キー名読み取り
				if ((c == 0x0d) || (c == 0x0a) || (c == 0x1a)) {
					// 改行またはEOFならセクション名終了、モード0へ遷移
					mode = 0;
					continue;
				}
				if (c == '=') {
					// "="ならモード3へ遷移
					value.clear();
					mode = 3;
					continue;
				}
				// それ以外ならキー名として追加
				key.push_back(c);
				continue;

			  case 3:
				// モード3: バリュー読み取り
				if ((c == 0x0d) || (c == 0x0a) || (c == 0x1a)) {
					cutTailSpace(value);
					// 改行またはEOFならバリュー終了
					// バリューが"[[[["ならリスト開始
					if (!value.compare("[[[[")) {
						list.clear();
						mode = 5;
						continue;
					}
					// キー/バリューをデータベースへ登録後、モード0へ遷移
					std::string sectionkey = (section.empty() ? key : (section + ":" + key));
					list.clear();
					list.push_back(value);
					mIniFileMap.insert(std::make_pair(sectionkey, list));
					mode = 0;
					continue;
				}
				// それ以外ならバリューとして追加
				value.push_back(c);
				continue;

			  case 4:
				// モード4: 行末まで読み飛ばし
				if ((c == 0x0d) || (c == 0x0a) || (c == 0x1a)) {
					// 改行またはEOFなら読み飛ばし終了、モード0へ遷移
					mode = 0;
				}
				continue;

			  case 5:
				// モード5: リスト行頭読み取り
				if ((c == 0x0d) || (c == 0x0a) || (c == 0x1a)) {
					// 改行またはEOFなら読み飛ばす
					continue;
				}
				if ((c == ' ') || (c == '\t')) {
					// スペース・タブなら読み飛ばす
					continue;
				}
				if (c == '#') {
					// "#"ならコメント扱い、行末まで読み飛ばす
					goto newline;
				}
				// それ以外ならバリューとして追加し、モード6へ遷移
				value.clear();
				value.push_back(c);
				mode = 6;
				continue;

			  case 6:
				// モード6: リストバリュー読み取り
				if ((c == 0x0d) || (c == 0x0a) || (c == 0x1a)) {
					cutTailSpace(value);
					// 改行またはEOFならバリュー終了
					// バリューが"]]]]"ならリスト終了、モード0へ遷移
					if (!value.compare("]]]]")) {
						std::string sectionkey = (section.empty() ? key : (section + ":" + key));
						mIniFileMap.insert(std::make_pair(sectionkey, list));
						mode = 0;
						continue;
					}
					// バリューをリストへ登録後、モード5へ遷移
					list.push_back(value);
					mode = 5;
					continue;
				}
				// それ以外ならバリューとして追加
				value.push_back(c);
				continue;


			  newline:
				// 行末まで読み飛ばすモードへ遷移
				mode = 4;
				continue;
			}
		}
	}

	// INIファイルを閉じる
	fclose(fin);
	return 0;
}

// -------------------------------------------------------------
// 末尾のスペースをカットする
// -------------------------------------------------------------
void
IniFile::cutTailSpace(std::string& str)
{
	while (!str.empty()) {
		char c = str[str.size()-1];
		if ((c == ' ') || (c == '\t')) {
			str.pop_back();
			continue;
		}
		return;
	}
}

// -------------------------------------------------------------
// データベースにキーがあるかどうか調べる
// -------------------------------------------------------------
bool
IniFile::hasKey(const std::string& section, const std::string& key)
{
	std::string sectionkey = (section.empty() ? key : (section + ":" + key));
	if (mIniFileMap.end() == mIniFileMap.find(sectionkey)) {
		return false;
	}
	return true;
}

// -------------------------------------------------------------
// データベースのリストサイズを調べる
// -------------------------------------------------------------
size_t
IniFile::getListSize(const std::string& section, const std::string& key)
{
	std::string sectionkey = (section.empty() ? key : (section + ":" + key));
	if (mIniFileMap.end() == mIniFileMap.find(sectionkey)) {
		return false;
	}
	return mIniFileMap[sectionkey].size();
}

// -------------------------------------------------------------
// データベースにリストエントリがあるかどうか調べる
// -------------------------------------------------------------
bool
IniFile::hasListEntry(const std::string& section, const std::string& key, const int n)
{
	std::string sectionkey = (section.empty() ? key : (section + ":" + key));
	if (mIniFileMap.end() == mIniFileMap.find(sectionkey)) {
		return false;
	}
	return (n < mIniFileMap[sectionkey].size());
}

// -------------------------------------------------------------
// データベースからリストを読み出す
// -------------------------------------------------------------
const IniFile::ValueList*
IniFile::getList(const std::string& section, const std::string& key)
{
	std::string sectionkey = (section.empty() ? key : (section + ":" + key));
	if (mIniFileMap.end() == mIniFileMap.find(sectionkey)) {
		return nullptr;
	}
	return &(mIniFileMap[sectionkey]);
}

// -------------------------------------------------------------
// データベースの指定エントリから文字列を読み出す
// -------------------------------------------------------------
const std::string&
IniFile::getListString(const std::string& section, const std::string& key, const int n)
{
	static const std::string empty = "";

	const ValueList* list = getList(section, key);
	if (list == nullptr) {
		return empty;
	}
	if (n >= list->size()) {
		return empty;
	}
	return list->at(n);
}

// -------------------------------------------------------------
// データベースから文字列を読み出す
// -------------------------------------------------------------
const std::string&
IniFile::getString(const std::string& section, const std::string& key)
{
	return getListString(section, key, 0);
}

// -------------------------------------------------------------
// データベースからint値（10進数）を読み出す
// -------------------------------------------------------------
int
IniFile::getInt(const std::string& section, const std::string& key)
{
	std::string value = getListString(section, key, 0);
	return atoi(value.c_str());
}

// -------------------------------------------------------------
// INIファイル書き込み
// -------------------------------------------------------------
int
IniFile::save(const char* filename)
{
	std::string lastsection;
	bool isfirstline = true;

	// INIファイルを開く
	FILE *fout = fopen(filename, "w");
	if (fout == nullptr) {
		return -1;
	}

	// マップ内全部（mIniFileMapはsectionkeyでソート済）
	for (const auto& e: mIniFileMap) {
		const std::string& sectionkey = e.first;
		const ValueList& value = e.second;
		std::string section;
		std::string key;

		// セクション名とキー名を取り出す
		size_t pos = sectionkey.find(":", 0);
		if (std::string::npos == pos) {
			// ":"で区切られていなければキー名のみ
			key = sectionkey;
		} else {
			// ":"で区切られていればセクション名とキー名を取り出し
			section = sectionkey.substr(0, pos);
			key = sectionkey.substr(pos+1);
		}

		// セクション名が変わる場合はそれを出力
		if (lastsection != section) {
			if (!isfirstline) {
				// 最初のセクションでなければ
				fputs("\n", fout);
			}
			fprintf(fout, "[%s]\n", section.c_str());
			lastsection = section;
		}

		// [key]=[value]を出力
		if (value.size() == 1) {
			fprintf(fout, "%s=%s\n", key.c_str(), value[0].c_str());
		} else {
			fprintf(fout, "%s=[[[[\n", key.c_str());
			for (int i=0; i<value.size(); i++) {
				fprintf(fout, "\t%s\n", value[i].c_str());
			}
			fprintf(fout, "\t]]]]\n");
		}
		isfirstline = false;
	}

	// INIファイルを閉じる
	fclose(fout);
	return 0;
}

// -------------------------------------------------------------
// データベースの指定エントリへ文字列を登録する
// -------------------------------------------------------------
void
IniFile::setListString(const std::string& section, const std::string& key, const int n, const std::string& value)
{
	std::string sectionkey = (section.empty() ? key : (section + ":" + key));
	ValueList& list = mIniFileMap[sectionkey];
	list[n] = value;
}

// -------------------------------------------------------------
// データベースの指定エントリへint値（10進数）を登録する
// -------------------------------------------------------------
void
IniFile::setListInt(const std::string& section, const std::string& key, const int n, const int value)
{
	char buf[16];
	sprintf(buf, "%d", value);
	std::string valuestr(buf);
	setListString(section, key, n, valuestr);
}

// -------------------------------------------------------------
// データベースへ文字列を登録する
// -------------------------------------------------------------
void
IniFile::setString(const std::string& section, const std::string& key, const std::string& value)
{
	setListString(section, key, 0, value);
}

// -------------------------------------------------------------
// データベースへint値（10進数）を登録する
// -------------------------------------------------------------
void
IniFile::setInt(const std::string& section, const std::string& key, int value)
{
	setListInt(section, key, 0, value);
}

// =====================================================================
// [EOF]
