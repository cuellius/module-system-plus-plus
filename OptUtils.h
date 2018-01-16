#pragma once

#include <map>
#include <string>
#include <vector>

class OptUtils
{
public:
	OptUtils(int argc, char **argv);
	bool Has(const std::string &key);
	std::string Get(const std::string &key);
	std::vector<std::string> Leftover();

private:
	std::map<std::string, std::string> m_options;
	std::map<std::string, bool> m_accessed;
};
