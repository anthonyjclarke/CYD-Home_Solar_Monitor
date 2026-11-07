#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

// Keeps at most `limit` bytes of an HTTP response body (audit F4). Portable so
// host tests cover it; ha_source.cpp wraps it in an Arduino Stream for
// HTTPClient::writeToStream(), which decodes chunked bodies and stops reading
// (closing the connection) as soon as accept() takes fewer bytes than offered.
// An oversized reply of unknown length is therefore never buffered whole.
class BodyLimit {
public:
    explicit BodyLimit(size_t limit) : limit_(limit) {}
    size_t accept(const uint8_t* data, size_t size) {
        if (overflow_ || size > limit_ - body_.size()) { overflow_ = true; return 0; }
        body_.append(reinterpret_cast<const char*>(data), size);
        return size;
    }
    bool overflow() const { return overflow_; }
    const std::string& body() const { return body_; }

private:
    const size_t limit_;
    std::string body_;
    bool overflow_ = false;
};
