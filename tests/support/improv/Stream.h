#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

// Host stand-in for the Arduino Stream used by the vendored Improv library:
// tests queue input bytes and inspect everything the library writes back.
class Stream {
public:
    std::deque<uint8_t> input;
    std::vector<uint8_t> output;
    int available() { return static_cast<int>(input.size()); }
    int read() {
        if (input.empty()) return -1;
        const uint8_t b = input.front(); input.pop_front(); return b;
    }
    size_t write(uint8_t b) { output.push_back(b); return 1; }
    size_t write(const uint8_t* data, size_t size) { output.insert(output.end(), data, data + size); return size; }
};
