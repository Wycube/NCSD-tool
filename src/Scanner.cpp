#include "Scanner.hpp"


Scanner::Scanner(const std::vector<u8> &data) : data(data), read_index(0) { }

void Scanner::seek(size_t index) {
    read_index = index;
}

void Scanner::skip(size_t count) {
    read_index += count;
}

void Scanner::readBytes(u8 *out, size_t count) {
    std::memcpy(out, &data[read_index], count);
    read_index += count;
}