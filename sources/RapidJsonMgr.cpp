//
// Created by 27044 on 26-3-17.
//

#include "../headers/RapidJsonMgr.h"

#include <boost/asio/execution/allocator.hpp>


std::string RapidJsonMgr::toJsonString(const rapidjson::Document& doc, bool format)
{
	// 序列化缓冲区
	rapidjson::StringBuffer buffer;
	if (format)
	{
		// 有格式的Json字符串
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);
	}
	else
	{
		// 无格式的Json字符串
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);
	}
	return buffer.GetString();
}

rapidjson::Document RapidJsonMgr::parseJson(const std::string& jsonStr)
{
	// 空文档对象用来存储解析后的字符串
	rapidjson::Document doc;
	if (doc.Parse(jsonStr.c_str()).HasParseError())
	{
		rapidjson::Document errDoc;
		errDoc.SetObject();
		auto& allocator = doc.GetAllocator();
		errDoc.AddMember("parse_error", rapidjson::Value("json parse error", allocator), allocator);
		return errDoc;
	}
	return doc;
}

std::string RapidJsonMgr::toString(const rapidjson::Value& value, const char* key, const std::string& defaultValue)
{
	// 判断是不是对象
	if (!value.IsObject()) {
		return defaultValue;
	}

	// find查找键值 (返回的迭代器指向的是个类似pair的结构)
	auto it = value.FindMember(key);

	// 判断键存不存在
	if (it == value.MemberEnd()) {
		return defaultValue;
	}

	// 判断值类型对不对
	if (!it->value.IsString()) {
		return defaultValue;
	}

	return it->value.GetString();
}

int RapidJsonMgr::toInt(const rapidjson::Value& value, const char* key, int defaultValue)
{
	if (!value.IsObject()) return defaultValue;
	auto it = value.FindMember(key);
	if (it == value.MemberEnd()) return defaultValue;
	if (!it->value.IsInt()) return defaultValue;
	return it->value.GetInt();
}

bool RapidJsonMgr::toBool(const rapidjson::Value& value, const char* key, bool defaultValue)
{
	if (!value.IsObject()) return defaultValue;
	auto it = value.FindMember(key);
	if (it == value.MemberEnd()) return defaultValue;
	if (!it->value.IsBool()) return defaultValue;
	return it->value.GetBool();
}

rapidjson::Document RapidJsonMgr::createDocument()
{
	rapidjson::Document doc;
	doc.SetObject();
	return doc;
}

void RapidJsonMgr::addMember(rapidjson::Document& doc, const char* key, const std::string& value)
{
	auto& allocator = doc.GetAllocator();
	doc.AddMember(rapidjson::Value(key, allocator), rapidjson::Value(value.c_str(), allocator), allocator);
}
