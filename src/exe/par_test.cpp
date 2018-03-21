/// @author Jakub Semric
/// 2018

#include <iostream>
#include <ostream>
#include <vector>
#include <thread>
#include <future>
#include <ctype.h>
#include <getopt.h>

#include "nfa.hpp"
#include "pcap_reader.hpp"
#include "reduction.hpp"
#include "nfa_error.hpp"

using namespace reduction;
using namespace std;

const int it_max = 10;
const float th_inc = 0.02;
const float th_min = 0.935;

struct ParamData
{
    string fa;
    float pct;
    int iter;
    float threshold;
    float pe, ce, cls_ratio;
    size_t merged;

    ParamData(
        string fa, float pct, int iter, float th, float pe, float ce, float cr,
        size_t merged) :
        fa{fa}, pct{pct}, iter{iter}, threshold{th}, pe{pe}, ce{ce},
        cls_ratio{cr}, merged{merged} {}
};

ostream &operator<<(ostream &os, ParamData p)
{
    return os << p.fa << " " << p.pct << " " << p.iter << " "
        << p.threshold << " " << p.pe << " " << p.ce << " "
        << p.cls_ratio << " " << p.merged;
}

vector<ParamData> foo(
    string fname, float pct, string train_data, vector<string> test_data)
{
    FastNfa target;
    target.read_from_file(fname.c_str());
    vector<ParamData> res;

    for (int iter = 0; iter <= it_max; iter += 1)
    {
        size_t merged = 0;
        FastNfa reduced = target;
        for (float threshold = th_min; threshold < 1; threshold += th_inc)
        {
            // reduce
            FastNfa reduced = target;
            auto ret = reduce(reduced, train_data, pct, threshold, iter);
            merged = ret.second;
            reduced.build();
            // compute error
            auto err = compute_error(target, reduced, test_data);
            ErrorStats aggr(reduced.state_count(), target.state_count());
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
            res.push_back(
                ParamData(fname,pct,iter,threshold,pe,ce,cls_ratio,merged));

            // do not compute with different threshold for no iteration (prune)
            if (iter == 0)
            {
                break;
            }
        }
    }

    return res;
}

int main()
{
    vector<string> automata{
        "min-snort/backdoor.rules.fa", "min-snort/sprobe.fa",
        "min-snort/ex.web.rules"};
    vector<float> pctv{0.16, 0.2, 0.24};

    #if 1
    string train_data = "pcaps/geant.pcap";
    vector<string> test_data{
        "pcaps/geant2.pcap2","pcaps/week2.pcap","pcaps/meter4-1.pcap8"};
    #else
    string train_data = "pcaps/extrasmall.pcap";
    vector<string> test_data{"pcaps/extrasmall.pcap"};
    #endif

    //vector<ParamData> params;
    vector<future<vector<ParamData>>> params;

    for (auto pct : pctv)
    {
        for (auto aut : automata)
        {
            params.push_back(async(foo, aut, pct, train_data, test_data));
        }
    }

    cout << "fa r th pe ce cls_ratio\n";
    for (size_t i = 0; i < params.size(); i++)
    {
        auto p = params[i].get();
        for (auto j : p)
            cout << j << endl;
    }

    return 0;    
}