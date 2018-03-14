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

using namespace reduction;
using namespace std;


int main()
{
    FastNfa target;
    target  .read_from_file("min-snort/backdoor.rules.fa");

    string train_data = "pcaps/geant.pcap";
    vector<string> test_data{
        "pcaps/geant2.pcap2","pcaps/week2.pcap","pcaps/meter4-1.pcap8"};
    int threads = 1;
    float pct = 0.16;

    for (float threshold = 0.9; threshold < 1; threshold += 0.2)
    {
        FastNfa nfa = target;
        // 4 thresholds
        for (int count = 10000; count < 50000; count += 10000)
        {
            // 4 counts
            for (int iter = 4; iter < 20; iter += 4)
            {
                // 4 iterations
                nfold_merge(nfa, train_data, pct, threshold, count, iter);
                //compute_error(target, nfa, test_data);
            }
        }
    }

    return 0;    
}