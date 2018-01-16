#pragma once

#include <algorithm>
#include <sstream>
#include <string>

std::string &ltrim(std::string &str, const std::string &chars = " \t\n\v\f\r");
std::string &rtrim(std::string &str, const std::string &chars = " \t\n\v\f\r");
std::string &trim(std::string &str, const std::string &chars = " \t\n\v\f\r");
std::string &lower(std::string &str);
std::string &replace(std::string &str, char character, char replacement);
std::string &remove(std::string &str, char character);
std::string itostr(int number);
