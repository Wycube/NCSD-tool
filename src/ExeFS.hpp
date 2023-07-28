#pragma once

#include "Types.hpp"
#include <vector>


struct ExeFSFileHeader {
    u8 name[8];
    u32 offset; //In bytes, from the end of the header
    u32 size;   //In bytes
};

struct ExeFSHeader {
    ExeFSFileHeader file_headers[10];
    //0x20 reserved bytes
    u8 file_hashes[10][32];
};

struct ExeFS {
    ExeFSHeader header;
    std::vector<u8> file_data[10];
};

auto parseExeFSHeader(const std::vector<u8> &data, size_t offset) -> ExeFSHeader;
auto parseExeFS(const std::vector<u8> &data, size_t offset) -> ExeFS;