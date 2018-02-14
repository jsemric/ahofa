#pragma GCC diagnostic ignored "-Wnarrowing"

#include <iostream>
#include <fstream>
#include <getopt.h>

#include "nfa.hpp"

using namespace std;
using namespace reduction;

const char *helpstr=
"Lightweight NFA Minimization\n"
"Usage: ./lmin [OPTIONS] FILE1 FILE2 ...\n"
"options:\n"
"  -h   : show this help and exit\n"
"  -f   : merge all final states to one final\n"
"  -v   : verbose mode\n";

int main(int argc, char **argv) {

    if (argc < 3) {
        cerr << "2 arguments required: input NFA, output NFA\n";
        cerr << helpstr;
        return 1;
    }

    int opt_cnt = 1;
    bool merge_finals = 0;
    bool verbose = 0;
    int c;
    while ((c = getopt(argc, argv, "hvf")) != -1) {
        opt_cnt++;
        switch (c) {
            // general options
            case 'h':
                cerr << helpstr;
                return 0;
            case 'f':
                merge_finals = true;
                break;
            case 'v':
                verbose = true;
                break;                
            default:
                return 1;
        }
    }

    try {
        FastNfa nfa;
        nfa.read_from_file(argv[opt_cnt]);

        vector<vector<size_t>> state_labels(nfa.state_count());
        unsigned prefix = 0;

        for (unsigned c1 = 0; c1 < 256; c1++) {
            for (unsigned c2 = 0; c2 < 256; c2++) {
                unsigned char word[2] = {c1, c2};
                nfa.parse_word(word, 2,
                    [&state_labels, &prefix](State s)
                    {
                        // insert to a vector only once
                        if (state_labels[s].empty() ||
                            state_labels[s].back() != prefix)
                        {
                            state_labels[s].push_back(prefix);
                        }
                    },
                    [&prefix]() {prefix++;});
            }
        }

        auto state_map = nfa.get_reversed_state_map();
        map<State,State> mapping;
        int cnt = 0;

        // find equivalent states and merge them
        for (size_t i = 0; i < state_labels.size(); i++) {

            if (state_labels[i].empty()) {
                continue;
            }

            for (size_t j = i + 1; j < state_labels.size(); j++) {
                if (state_labels[i] == state_labels[j]) {
                    if (mapping.find(state_map[j]) == mapping.end()) {
                        mapping[state_map[j]] = state_map[i];
                        if (verbose) {
                            cerr << state_map[j] << "->" << state_map[i]
                                << endl;
                        }
                        cnt++;
                    }
                }
            }
        }

        if (verbose) {
            cerr << "Removed: " << cnt << endl;
        }


        // TODO split to rules and merge final states

        if (merge_finals) {
            ;
        }
        else {
            ;
        }


        nfa.merge_states(mapping);

        ofstream out{argv[opt_cnt + 1]};
        nfa.print(out);
        out.close();
    }
    catch (exception &e) {
        cerr << "\033[1;31mERROR\033[0m " << e.what() << endl;
        return 1;
    }

    return 0;
}
