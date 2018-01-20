#include <iostream>
#include <array>
#include "nfa.hpp"
#include "pcap_reader.hpp"

#define RAND (((double) rand()) / (RAND_MAX))

using namespace std;
using namespace reduction;

#define N 256
array<array<int,N>,N> mc;
array<array<double,N>,N> mcd;

int get_max(int next) {
    assert(next < N);
    int max = 0;
    for (int i = 0; i < N; i++) {
        max = mc[next][max] < mc[next][i] ? i : max;
    }
    return max;
}

void print_r(int c) {
    if (isprint(c) || 1) {
        printf("%c", c);
    }
    else {
        printf("0x%.2x ", c);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        return 1;
    }
    int cnt = 0;
    FastNfa nfa;
    nfa.read_from_file(argv[2]);
    pcapreader::process_payload(
        argv[1], [&](const unsigned char*payload, unsigned len)
        {
            for (unsigned i = 0; i + 1 < len; i++) {
                mc[payload[i] % N][payload[i+1] % N]++;
                cnt += payload[i] > payload[i+1];
            }
        });

/*
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N-1; j++)
            cout << mc[i][j] << " ";
        cout << mc[i][N-1] << endl;
    }

    for (int i = 32; i < N; i++) {
        int next = i;
        int cnt = 5;
        while (cnt--) {
            print_r(next);
            next = get_max(next);
        }
        print_r(next);
        cout << "\n";
    }
*/

    for (int i = 0; i < N; i++) {
        int max = 0;
        for (int j = 0; j < N-1; j++) {
            if ( max < mc[i][j]) max = mc[i][j];
        }

        for (int j = 0; j < N-1; j++) {
            mcd[i][j] = mc[i][j]*1.0/max;
        }
    }

    int row, col;
    vector<size_t> data(nfa.state_count());
    for (int i = 0; i < 3; i++) {
        row = random() % 256;
        unsigned char str[100];
        unsigned len = 0;
        for (int j = 0; j < 100; j++) {
            double rnd = RAND;
            double sum = 0;
            for (col = 0; sum < rnd && col < 256; col++) {
                sum += mcd[row][col];
            }
            // mcd[row][col] is the result
            // append symbol
            str[len++] += col;
            cout << col << " ";
            // change state
            row = col;
        }
        cout << endl;
        vector<bool> bm(nfa.state_count());
        nfa.parse_word(str, len, [&bm](State s){ bm[s] = 1; });
        for (size_t i = 0; i < data.size(); i++) {
            data[i] += bm[i];
        }
    }
    /*
    for (size_t i = 0; i < data.size(); i++)
        cout << data[i] << endl;
*/
    return 0;
}
