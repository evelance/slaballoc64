#include <iostream>
#include <iomanip>
#include <locale>
#include <vector>
#include <random>
#include <algorithm>
#include <time.h>
#include "slaballoc64.hpp"
using namespace std;

int main()
{
    #define OBSZ 64
    const size_t n = 5 * 1024 * 2024;
    const int rounds = 5;
    
    void** ptrs = new void*[n];
    vector<size_t> rindices;
    cout << "Generating random indices..." << endl;
    for (size_t i = 0; i < n; ++i) {
        rindices.push_back(i);
    }
    auto rng = std::default_random_engine {};
    std::shuffle(std::begin(rindices), std::end(rindices), rng);
    cout.imbue(std::locale(""));
    // slaballoc64
    {
        cout << "slaballoc64: 2x random allocation/free of " << n << " objects of size " << OBSZ << endl;
        slaballoc64<OBSZ> alloc;
        for (int round = 0; round < rounds; ++round) {
            clock_t t0 = clock();
            cout << "Round " << (round + 1) << " of " << rounds << "... ";
            cout.flush();
            for (size_t i = 0; i < n; ++i) {
                size_t ri = rindices[i];
                ptrs[ri] = alloc.alloc();
                *(char*)(ptrs[ri]) = 'x';
            }
            for (ssize_t i = n - 1; i >= 0; --i) {
                size_t ri = rindices[i];
                alloc.free(ptrs[ri]);
            }
            // alloc.release();
            for (ssize_t i = n - 1; i >= 0; --i) {
                size_t ri = rindices[i];
                ptrs[ri] = alloc.alloc();
                *(char*)(ptrs[ri]) = 'x';
            }
            for (size_t i = 0; i < n; ++i) {
                size_t ri = rindices[i];
                alloc.free(ptrs[ri]);
            }
            cout << "\b\b\b\b - " << setw(10) << fixed << setprecision(2) << (((clock() - t0) / 1000000.0) * 1000) << "ms" << endl;
        }
    }
    // Malloc
    {
        cout << "Default malloc: 2x random allocation/free of " << n << " objects of size " << OBSZ << endl;
        for (int round = 0; round < rounds; ++round) {
            clock_t t0 = clock();
            cout << "Round " << (round + 1) << " of " << rounds << "... ";
            cout.flush();
            for (size_t i = 0; i < n; ++i) {
                size_t ri = rindices[i];
                ptrs[ri] = malloc(OBSZ);
                *(char*)(ptrs[ri]) = 'x';
            }
            for (ssize_t i = n - 1; i >= 0; --i) {
                size_t ri = rindices[i];
                free(ptrs[ri]);
            }
            for (ssize_t i = n - 1; i >= 0; --i) {
                size_t ri = rindices[i];
                ptrs[ri] = malloc(OBSZ);
                *(char*)(ptrs[ri]) = 'x';
            }
            for (size_t i = 0; i < n; ++i) {
                size_t ri = rindices[i];
                free(ptrs[ri]);
            }
            cout << "\b\b\b\b - " << setw(10) << fixed << setprecision(2) << (((clock() - t0) / 1000000.0) * 1000) << "ms" << endl;
        }
    }
    delete[] ptrs;
    return 0;
}

