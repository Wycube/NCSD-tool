#pragma once

#include "ExeFS.hpp"
#include "RomFS.hpp"
#include <optional>


struct NCCHHeader {
    u8 signature[0x100];
    u32 magic;
    u32 size;
    u64 partition_id;
    u16 maker_code;
    u16 version;
    u32 some_hash;
    u64 program_id;
    //0x10 bytes reserved
    u8 logo_hash[0x20];
    u8 product_code[0x10];
    u8 exheader_hash[0x20];
    u32 exheader_size;
    //4 bytes reserved
    u8 flags[8];
    u32 plain_offset; //In media units
    u32 plain_size;   //In media units
    u32 logo_offset;  //In media units
    u32 logo_size;    //In media units
    u32 exefs_offset; //In media units
    u32 exefs_size;   //In media units
    u32 exefs_hash_size;
    //4 bytes reserved
    u32 romfs_offset; //In media units
    u32 romfs_size;   //In media units
    u32 romfs_hash_size;
    //4 bytes reserved
    u8 exefs_super_hash[0x20];
    u8 romfs_super_hash[0x20];
};

struct CodeSetInfo {
    u32 address;
    u32 region_size;
    u32 size;
};

struct SystemControlInfo {
    u8 app_title[8];
    //5 bytes reserved
    u8 flag;
    u16 remaster_version;
    CodeSetInfo text_info;
    u32 stack_size;
    CodeSetInfo ro_info;
    //4 bytes reserved
    CodeSetInfo data_info;
    u32 bss_size;
    u32 dependency_list[0x30];
    //SystemInfo
    u64 savedata_size;
    u64 jump_id;
    //0x30 bytes reserved
};

struct AccessControlInfo {
    u64 program_id;
    u32 core_version;
    //TODO: Figure out if this is the right order
    u8 flag1;
    u8 flag2;
    u8 flag0;
    u8 priority;
    u16 resource_limits[0x10];
    //StorageInfo
    u64 extdata_id;
    u64 sys_savedata_ids;
    u64 storage_unique_ids;
    u64 fs_access_and_other;
    u8 service_access_control[0x20][8];
    u8 extended_service_access_control[2][8];
    //0xF bytes reserved
    u8 resource_limit_category;
    
    //ARM11 Kernel Capabilities
    u32 arm11_descriptors[0x1C];
    //0x10 bytes reserved

    //ARM9 Access Control
    u8 arm9_descriptors[0xF];
    u8 arm9_descriptor_version;
};

struct NCCHExtendedHeader {
    SystemControlInfo sci;
    AccessControlInfo aci;
    u8 signature[0x100];
    u8 public_key[0x100];
    AccessControlInfo aci_limits;
};

struct NCCH {
    NCCHHeader header;
    std::optional<NCCHExtendedHeader> exheader;
    std::optional<std::vector<u8>> logo;
    std::optional<std::vector<u8>> plain_region;
    std::optional<ExeFS> exefs;
    std::optional<RomFS> romfs;
};

auto parseNCCHHeader(const std::vector<u8> &data, size_t offset) -> NCCHHeader;
auto parseSystemControlInfo(const std::vector<u8> &data, size_t offset) -> SystemControlInfo;
auto parseAccessControlInfo(const std::vector<u8> &data, size_t offset) -> AccessControlInfo;
auto parseNCCHExtendedHeader(const std::vector<u8> &data, size_t offset) -> NCCHExtendedHeader;
auto parseNCCH(const std::vector<u8> &data, size_t offset) -> NCCH;