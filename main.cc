#include <iostream>

#include "libregex.hh"

int main(int argc, const char* const* argv)
{
    if(argc != 2)
    {
        std::cout
        << "regex-opt " VERSION " - Copyright (C) 1992,2006 Bisqwit (http://iki.fi/bisqwit/)\n"
           "This program is distributed under the terms of the General Public License.\n"
           "\n"
           "Usage: regex-opt <regexp>\n";;
        return 0;
    }
    try {
        std::string regex = argv[1];

        unsigned pos=0;

        regexopt_choices tree = RegexOptParse(regex, pos);

        DumpRegexOptTree(std::cout, tree);
    }
    catch(const char *s)
    {
        std::cout << "Error: " << s << std::endl;
        return -1;
    }
    catch(const std::string& s)
    {
        std::cout << "Error: " << s.c_str() << std::endl;
        return -1;
    }
    return 0;
}
