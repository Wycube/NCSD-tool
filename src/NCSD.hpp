#pragma once

#include "Types.hpp"
#include "NCCH.hpp"
#include <optional>


struct NCSDHeader {
    u8 signature[0x100];
    u32 magic;
    u32 size;
    u64 media_id;
    u64 fs_type;
    u64 crypt_type;
    u32 partition_table[8][2];
};

//There is also a different NAND header that can possibly be used in place
//when the NCSD is in NAND, but we won't deal with that.
struct NCSDCartHeader {
    u8 exheader_hash[0x20];
    u32 header_size;
    u32 sector_zero_offset;
    u64 partition_flags;
    u64 partition_id_table[8];
    //0x20 bytes reserved
    //14 bytes reserved
    //1 byte used for some kind of anti-piracy check
    //1 byte used for save encrpytion, or something
};

struct NCSD {
    NCSDHeader header;
    NCSDCartHeader cart_header;
    std::optional<NCCH> partitions[8];
};

auto parseNCSDHeader(const std::vector<u8> &data, size_t offset) -> NCSDHeader;
auto parseNCSDCartHeader(const std::vector<u8> &data, size_t offset) -> NCSDCartHeader;
auto parseNCSD(const std::vector<u8> &data, size_t offset) -> NCSD;