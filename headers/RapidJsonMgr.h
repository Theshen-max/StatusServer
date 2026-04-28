//
// Created by 27044 on 26-3-17.
//

#ifndef RAPIDJSONWRAPPER_H
#define RAPIDJSONWRAPPER_H

#include <string>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

class RapidJsonMgr
{
public:
	static std::string toJsonString(const rapidjson::Document &doc, bool format = true);

	static rapidjson::Document parseJson(const std::string &jsonStr);

	static std::string toString(const rapidjson::Value& value, const char* key, const std::string& defaultValue = "");

	static int toInt(const rapidjson::Value& value, const char* key, int defaultValue = 0);

	static bool toBool(const rapidjson::Value& value, const char* key, bool defaultValue = false);

	// 创建一个空的Document(顶层)
	static rapidjson::Document createDocument();

	template <typename T>
	static void addMember(rapidjson::Document& doc, const char* key, T&& value);

	// 添加字符串成员
	static void addMember(rapidjson::Document& doc, const char* key, const std::string& value);
};

template<typename T>
void RapidJsonMgr::addMember(rapidjson::Document& doc, const char* key, T&& value)
{
	auto& allocator = doc.GetAllocator();
	doc.AddMember(rapidjson::Value(key, allocator), rapidjson::Value(std::forward<T>(value), allocator), allocator);
}

#endif //RAPIDJSONWRAPPER_H
