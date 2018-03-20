#pragma GCC diagnostic ignored "-Wunused-parameter"

#include <iostream>
#include <vector>
#include <mutex>
#include <thread>

#include "pcap_reader.hpp"

using namespace std;
using namespace pcapreader;

const int NW = 2;
const int MAX = 2000;
vector<size_t> plens(MAX);
mutex mux;

void sum_up(const vector<size_t> &v)
{
	mux.lock();
    for (size_t i = 0; i < plens.size(); i++)
    {
        plens[i] += v[i];
    }
	mux.unlock();
}

void process_pcaps(vector<string> pcaps)
{
	for (auto p : pcaps)
	{
        vector<size_t> v(MAX);
		pcapreader::process_payload(p.c_str(),
            [&v](const unsigned char *payload, unsigned len) { v[len]++; });
	    sum_up(v);
	}
}

int main(int argc, char **argv)
{
    vector<vector<string>> v(NW);

    for (int i = 1; i < argc; i++)
        v[i % NW].push_back(argv[i]);

    vector<thread> threads;
    for (int i = 0; i < NW; i++)
        threads.push_back(thread{[&v, i]()
                {
                    process_pcaps(v[i]);
                }
            });

    for (int i = 0; i < NW; i++)
        if (threads[i].joinable())
            threads[i].join();

    size_t total = 0;
    for (size_t i = 0; i < plens.size(); i++)
    {
        total += plens[i];
        //cout << i << " " << plens[i] << endl;
    }
    cout << total << endl;

	return 0;
}