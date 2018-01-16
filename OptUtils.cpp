#include "OptUtils.h"
#include "StringUtils.h"

OptUtils::OptUtils(int argc, char **argv)
{
	std::string prev_option;

	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];
		std::string cur_option = trim(arg);

		if (cur_option[0] == '-')
		{
			prev_option = cur_option;
			m_options[cur_option] = ".";
		}
		else if (!prev_option.empty())
		{
			m_options[prev_option] = cur_option;
			prev_option.clear();
		}
		else
		{
			m_options[cur_option] = ".";
		}
	}
}

bool OptUtils::Has(const std::string &key)
{
	m_accessed[key] = true;

	return !m_options[key].empty();
}

std::string OptUtils::Get(const std::string &key)
{
	m_accessed[key] = true;

	if (!m_options[key].empty() && m_options[key] != ".")
		return m_options[key];
	
	return "";
}

std::vector<std::string> OptUtils::Leftover()
{
	std::vector<std::string> leftover;

	for (auto it = m_options.begin(); it != m_options.end(); ++it)
	{
		if (m_accessed.find(it->first) == m_accessed.end())
			leftover.push_back(it->first);
	}

	return leftover;
}
