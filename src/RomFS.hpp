#pragma once

#include "Types.hpp"
#include <vector>
#include <string>


struct RomFSHeader {
    u32 magic;
    u32 magic_num;
    u32 master_hash_size;
    u64 lvl1_offset;
    u64 lvl1_hash_size;
    u32 lvl1_block_size;
    //4 bytes reserved
    u64 lvl2_offset;
    u64 lvl2_hash_size;
    u32 lvl2_block_size;
    //4 bytes reserved
    u64 lvl3_offset;
    u64 lvl3_hash_size;
    u32 lvl3_block_size;
    //4 bytes reserved
    //4 bytes reserved
    u32 optional_info_size;
};

//All offsets and lengths below are in bytes unless otherwise specified

struct Level3Header {
    u32 header_length;
    u32 dir_hash_offset;
    u32 dir_hash_length;
    u32 dir_meta_offset;
    u32 dir_meta_length;
    u32 file_hash_offset;
    u32 file_hash_length;
    u32 file_meta_offset;
    u32 file_meta_length;
    u32 file_data_offset;
};

struct DirectoryMetadata {
    u32 parent_offset;
    u32 sibling_offset;
    u32 child_offset;
    u32 first_file_offset;
    u32 same_hash_offset;
    u32 name_length;
    std::vector<u16> name; //In units of 16-bits (seems to be UTF-16 or UCS-2)
};

struct FileMetadata {
    u32 parent_offset;
    u32 sibling_offset;
    u64 data_offset;
    u64 data_size;
    u32 same_hash_offset;
    u32 name_length;
    std::vector<u16> name; //In units of 16-bits (seems to be UTF-16 or UCS-2)
};

struct Level3 {
    Level3Header header;
    std::vector<u32> dir_hash_table;
    std::vector<DirectoryMetadata> dir_table;
    std::vector<u32> file_hash_table;
    std::vector<FileMetadata> file_table;
    std::vector<u8> file_data;
};

struct File {
    std::u16string name;
    size_t offset;
    size_t size;
};

struct Directory {
    std::u16string name;
    std::vector<Directory> children;
    std::vector<File> files;
};

struct RomFS {
    RomFSHeader header;
    Level3 level3;
    Directory root;
};

auto parseLevel3Header(const std::vector<u8> &data, size_t offset) -> Level3Header;
auto parseDirectoryMetadata(const std::vector<u8> &data, size_t offest) -> DirectoryMetadata;
auto parseFileMetadata(const std::vector<u8> &data, size_t offset) -> FileMetadata;
auto parseLevel3(const std::vector<u8> &data, size_t offset) -> Level3;
auto parseDirectory(const std::vector<u8> &data, size_t dir_offset, size_t file_offset, size_t offset) -> Directory;
auto parseRomFSHeader(const std::vector<u8> &data, size_t offset) -> RomFSHeader;
auto parseRomFS(const std::vector<u8> &data, size_t offset) -> RomFS;