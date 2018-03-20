/// @author Jakub Semric
/// 2018

#include <iostream>
#include <ostream>
#include <sstream>
#include <exception>
#include <vector>
#include <map>
#include <stdexcept>
#include <ctype.h>
#include <getopt.h>

#include "nfa.hpp"
#include "pcap_reader.hpp"
#include "reduction.hpp"
#include "nfa_error.hpp"

using namespace reduction;
using namespace std;

int main(int argc, char **argv)
{
    FastNfa target;
    string fname = "min-snort/backdoor.rules.fa";
    if (argc > 1) fname = argv[1];
    target.read_from_file(fname.c_str());

    #if 0
    string train_data = "pcaps/gean.pcap";
    vector<string> test_data{
        "pcaps/geant2.pcap2","pcaps/week2.pcap","pcaps/meter4-1.pcap8"};
    #else
    string train_data = "pcaps/extrasmall.pcap";
    vector<string> test_data{"pcaps/extrasmall.pcap"};
    #endif
    float pct = 0.16;
    cout << "i" << " " << "th" << " " << "pe" << " " << "ce"
         << " " << "cls_ratio" << endl;
    for (int iter = 0; iter < 6; iter += 1)    
    {
        // 0-10 = 11 iterations
        size_t merged = 0;
        FastNfa reduced = target;
        for (float threshold = 0.995; threshold < 1; threshold += 0.02)
        {
            // reduce
            FastNfa reduced = target;
            auto ret = reduce(reduced, train_data, pct, threshold, iter);
            merged = ret.second;
            reduced.build();
            // compute error
            auto err = compute_error(target, reduced, test_data);

            // accumulate results
            for (auto i : err)
            {
                aggr.aggregate(i.second);
            }

            size_t wrong_acceptances = aggr.accepted_reduced -
                aggr.accepted_target;
            float pe = wrong_acceptances * 1.0 / aggr.total;
            float ce = aggr.wrongly_classified * 1.0 / aggr.total;
            float cls_ratio = aggr.correctly_classified * 1.0 /
                (aggr.correctly_classified + aggr.wrongly_classified);

            cout << iter << " " << threshold << " " << pe << " " << ce
                 << " " << cls_ratio << " " << merged << endl;

            // do not compute with different threshold for no iteration (prune)
            if (iter == 0)
            {
                break;
            }
        }
    }

    return 0;    
}