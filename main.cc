#include <iostream>
#include "libregex.hh"

int main(int argc, const char* const* argv)
{
    if(argc != 2)
    {
        std::cout
        << "regex-opt " VERSION " — Copyright © 1992, 2006 Bisqwit (http://iki.fi/bisqwit/)\n"
           "This program is distributed under the terms of the General Public License.\n"
           "\033[0;38;5;103musage:\033[0m \033[0;38;5;65mregex-opt\033[0m \033[0;38;5;101m<regexp>\033[0m\n";;
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
        std::cout << "\033[0;38;5;103mError: \033[0m" << s << std::endl;
        return -1;
    }
    catch(const std::string& s)
    {
        std::cout << "\033[0;38;5;103mError: \033[0m" << s.c_str() << std::endl;
        return -1;
    }
    return 0;
}
