
#include <iostream>
#include <fstream>
#include <getopt.h>

#include "nfa.hpp"

using namespace std;
using namespace reduction;

const char *helpstr=
"Lightweight NFA Minimization\n"
"Usage: ./lmin [OPTIONS] FILE1 FILE2 ...\n";


int main(int argc, char **argv) {

    if (argc < 3) {
        cerr << "2 arguments required: input NFA, output NFA\n";
        cerr << helpstr;
        return 1;
    }
    try {
        FastNfa nfa;
        nfa.read_from_file(argv[1]);
        nfa.merge_sl_states();

        ofstream out{argv[2]};
        nfa.print(out);
        out.close();
    }
    catch (exception &e) {
        cerr << "\033[1;31mERROR\033[0m " << e.what() << endl;
        return 1;
    }

    return 0;
}
