#include "StringUtils.h"

std::string &ltrim(std::string &str, const std::string &chars)
{
	int len = (int)str.length();

	for (int i = 0; i < len; ++i)
	{
		if (chars.find(str[i]) == std::string::npos)
		{
			str.erase(0, i);
			break;
		}
	}

	return str;
}

std::string &rtrim(std::string &str, const std::string &chars)
{
	int len = (int)str.length();

	for (int i = len - 1; i >= 0; --i)
	{
		if (chars.find(str[i]) == std::string::npos)
		{
			str.erase(i + 1);
			break;
		}
	}
	
	return str;
}

std::string &trim(std::string &str, const std::string &chars)
{
	return ltrim(rtrim(str, chars), chars);
}

std::string &lower(std::string &str)
{
	std::transform(str.begin(), str.end(), str.begin(), ::tolower);
	return str;
}

std::string &replace(std::string &str, char character, char replacement)
{
	std::replace(str.begin(), str.end(), character, replacement);
	return str;
}

std::string &remove(std::string &str, char character)
{
	str.resize(std::remove(str.begin(), str.end(), character) - str.begin());
	return str;
}

std::string itostr(int number)
{
	char buffer[15];
	_itoa(number, buffer, 10);
	return buffer;
}
