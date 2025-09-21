#include <iostream>
#include <vector>
#include <unordered_map>

class ParseArg
{
	std::string trim(const std::string& s) {
		auto start = s.begin();
		while (start != s.end() and std::isspace(*start)) ++start;
		auto end = s.end();
		do { --end; } while(std::distance(start, end) >= 0 and std::isspace(*end));
		return std::string(start, end+1);
	}
	public:
	ParseArg() = default;
	ParseArg(int argc, char* argv[]) { parse(argc, argv); }
	void parse(int argc, char* argv[]) {
		if (argc <= 0) return;
		std::string opt{};
		std::string val{};
		for (int ii = 0 ; ii < argc ; ++ii)
		{
			std::string temp(trim(argv[ii]));
			if (ii == 0)
			{
				m_app = std::move(temp);
				continue;
			}

			if (temp[0] == '-')
			{
				if (temp.size() < 2)
					return;

				if (temp[1] == '-') // long option
				{
					if (temp.size() == 2)
						continue;
					size_t pos = temp.find('=');
					if (pos != std::string::npos)
						m_options.emplace(temp.substr(0, pos), temp.substr(pos+1));
					else
						opt = temp;
				}
				else
				{
					m_options.emplace(temp, "");
				}
			}
			else
			{
				val = temp;
				if (!opt.empty())
				{
					m_options.emplace(opt, val);
					opt.clear();
				}
			}
		}
	}
	void print()
	{
		for (const auto& [k, v] : m_options)
		{
			std::cout << "[" << k << "," << v << "], ";
		}
		std::cout << std::endl;
	}
	std::unordered_map<std::string, std::string>	m_options;
	std::string	m_app;
};

int main(int argc, char* argv[])
{
	ParseArg pa(argc, argv);
	pa.print();
}
