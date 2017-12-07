/// @author Jakub Semric
/// 2017

#include <cassert>
#include "aux.hpp"

bool is_number(const std::string& s)
{
    auto it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

int hex_to_int(const std::string &str)
{
    int x = 0;
    int m = 1;
    for (int i = str.length()-1; i > 1; i--) {
        int c = tolower(str[i]);
        x += m * (c - (c > '9' ? 'a' - 10 : '0'));
        m *= 16;
    }

    return x;
}

std::string int_to_hex(const unsigned num)
{
    assert (num <= 255);
    char buf[16] = "";
    sprintf(buf, "0x%.2x", num);
    return buf;
}


void print_readable(const unsigned char *payload, unsigned length) {
    for (unsigned i = 0; i < length; i++) {
        if (isprint(payload[i])) {
            printf("%c", payload[i]);
        }
        else {
            printf("\\x%.2x", payload[i]);
        }
    }
    printf("\n");
}
