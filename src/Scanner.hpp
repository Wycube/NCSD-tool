#pragma once

#include "Types.hpp"
#include <vector>
#include <cstring>


class Scanner {
public:

    explicit Scanner(const std::vector<u8> &data);

    auto index() -> size_t;
    void seek(size_t index);
    void skip(size_t count);
    void readBytes(u8 *out, size_t count);    
    template<typename T>
    auto readInt() -> T {
        static_assert(std::is_integral_v<T>);

        T value = 0;
        for(int i = 0; i < sizeof(T); i++) {
            value |= static_cast<T>(data[read_index + i]) << (8 * i);
        }
        read_index += sizeof(T);

        return value;
    }

private:

    const std::vector<u8> &data;
    size_t read_index;
};