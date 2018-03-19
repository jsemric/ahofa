
#include <iostream>
#include <ostream>
#include <exception>
#include <vector>
#include <stdexcept>

#include "nfa.hpp"

namespace reduction
{

using namespace std;

struct ErrorStats
{
    // long data
    vector<size_t> nfa1_data;
    vector<size_t> nfa2_data;
    // packets statistics
    size_t total;
    size_t accepted_reduced;
    size_t accepted_target;
    size_t wrongly_classified;
    size_t correctly_classified;

    ErrorStats(size_t data_reduced_size = 1, size_t data_target_size = 1) :
        nfa1_data(data_reduced_size), nfa2_data(data_target_size), total{0},
        accepted_reduced{0}, accepted_target{0}, wrongly_classified{0},
        correctly_classified{0} {}

    ~ErrorStats() = default;

    void aggregate(const ErrorStats &other_data) {
        total += other_data.total;
        accepted_target += other_data.accepted_target;
        accepted_reduced += other_data.accepted_reduced;
        wrongly_classified += other_data.wrongly_classified;
        correctly_classified += other_data.correctly_classified;
        for (size_t i = 0; i < other_data.nfa1_data.size(); i++) {
            nfa1_data[i] += other_data.nfa1_data[i];
        }
        for (size_t i = 0; i < other_data.nfa2_data.size(); i++) {
            nfa2_data[i] += other_data.nfa2_data[i];
        }
    }
};

class StopWork : public exception {};

class NfaError
{
private:
    const FastNfa &target;
    const FastNfa &reduced;
    vector<string> pcaps;
    bool consistent;

    //mutex mux;
    vector<size_t> fidx_reduced;
    vector<size_t> fidx_target;
    vector<pair<string,ErrorStats>> results;

public:
    NfaError(
        const FastNfa &a1, const FastNfa &a2, const vector<string> &pcaps,
        bool consistent = false);

    vector<pair<string,ErrorStats>> get_result() const { return results;}
    void process_pcaps();

private:
    void compute_error(
        ErrorStats &stats, const unsigned char *payload, unsigned plen) const;
};

}