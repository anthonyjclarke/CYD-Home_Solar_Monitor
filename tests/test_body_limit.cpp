// BodyLimit (audit F4): bodies at or under the limit are kept intact; the first
// block that would pass it is refused whole, and nothing is accepted after.
#include "body_limit.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

static size_t feed(BodyLimit& b, const std::string& s) {
    return b.accept(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

int main() {
    {   // Known-length body exactly at the limit, in uneven blocks.
        BodyLimit b(10);
        assert(feed(b, "{\"st") == 4 && feed(b, "ate\"") == 4 && feed(b, ":1") == 2);
        assert(!b.overflow() && b.body() == "{\"state\":1");
    }
    {   // One byte over: the offending block is refused, earlier bytes kept.
        BodyLimit b(10);
        assert(feed(b, "0123456") == 7);
        assert(feed(b, "7890") == 0);
        assert(b.overflow() && b.body() == "0123456");
        assert(feed(b, "") == 0 && feed(b, "x") == 0);  // Stays refused, even for empty or small blocks.
        assert(b.body().size() == 7);
    }
    {   // Chunked or close-delimited body far larger than the limit: memory never exceeds it.
        BodyLimit b(16384);
        const std::vector<uint8_t> block(1460, 'x');
        size_t kept = 0, blocks = 0;
        while (blocks < 1000 && b.accept(block.data(), block.size()) == block.size()) { kept += block.size(); ++blocks; }
        assert(b.overflow() && kept == 11 * 1460 && b.body().size() == kept && b.body().size() <= 16384);
    }
    {   // Single oversized block (unknown length, first read) and an empty body.
        BodyLimit big(8);
        assert(feed(big, "123456789") == 0 && big.overflow() && big.body().empty());
        BodyLimit empty(8);
        assert(!empty.overflow() && empty.body().empty());
    }
    {   // A zero-byte write at the limit is fine; the next real byte is not.
        BodyLimit b(3);
        assert(feed(b, "abc") == 3 && feed(b, "") == 0 && !b.overflow());
        assert(feed(b, "d") == 0 && b.overflow());
    }
    std::cout << "Body limit: exact, over-by-one, chunked flood, oversized first block, empty and sticky refusal passed.\n";
}
