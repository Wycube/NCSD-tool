#include "NCCH.hpp"
#include "Scanner.hpp"


auto parseNCCHHeader(const std::vector<u8> &data, size_t offset) -> NCCHHeader {
    Scanner scanner(data);
    NCCHHeader header;

    scanner.seek(offset);
    scanner.readBytes(header.signature, sizeof(NCCHHeader::signature));
    header.magic = scanner.readInt<u32>();
    header.size = scanner.readInt<u32>();
    header.partition_id = scanner.readInt<u64>();
    header.maker_code = scanner.readInt<u16>();
    header.version = scanner.readInt<u16>();
    header.some_hash = scanner.readInt<u32>();
    header.program_id = scanner.readInt<u64>();
    scanner.skip(0x10);
    scanner.readBytes(header.logo_hash, sizeof(NCCHHeader::logo_hash));
    scanner.readBytes(header.product_code, sizeof(NCCHHeader::product_code));
    scanner.readBytes(header.exheader_hash, sizeof(NCCHHeader::exheader_hash));
    header.exheader_size = scanner.readInt<u32>();
    scanner.skip(4);
    scanner.readBytes(header.flags, sizeof(NCCHHeader::flags));
    header.plain_offset = scanner.readInt<u32>();
    header.plain_size = scanner.readInt<u32>();
    header.logo_offset = scanner.readInt<u32>();
    header.logo_size = scanner.readInt<u32>();
    header.exefs_offset = scanner.readInt<u32>();
    header.exefs_size = scanner.readInt<u32>();
    header.exefs_hash_size = scanner.readInt<u32>();
    scanner.skip(4);
    header.romfs_offset = scanner.readInt<u32>();
    header.romfs_size = scanner.readInt<u32>();
    header.romfs_hash_size = scanner.readInt<u32>();
    scanner.skip(4);
    scanner.readBytes(header.exefs_super_hash, sizeof(NCCHHeader::exefs_super_hash));
    scanner.readBytes(header.romfs_super_hash, sizeof(NCCHHeader::romfs_super_hash));

    return header;
}

auto parseSystemControlInfo(const std::vector<u8> &data, size_t offset) -> SystemControlInfo {
    Scanner scanner(data);
    SystemControlInfo sci;

    scanner.seek(offset);
    scanner.readBytes(sci.app_title, sizeof(SystemControlInfo::app_title));
    scanner.skip(5);
    sci.flag = scanner.readInt<u8>();
    sci.remaster_version = scanner.readInt<u16>();
    sci.text_info.address = scanner.readInt<u32>();
    sci.text_info.region_size = scanner.readInt<u32>();
    sci.text_info.size = scanner.readInt<u32>();
    sci.stack_size = scanner.readInt<u32>();
    sci.ro_info.address = scanner.readInt<u32>();
    sci.ro_info.region_size = scanner.readInt<u32>();
    sci.ro_info.size = scanner.readInt<u32>();
    scanner.skip(4);
    sci.data_info.address = scanner.readInt<u32>();
    sci.data_info.region_size = scanner.readInt<u32>();
    sci.data_info.size = scanner.readInt<u32>();
    sci.bss_size = scanner.readInt<u32>();
    
    for(int i = 0; i < sizeof(SystemControlInfo::dependency_list) / sizeof(u32); i++) {
        sci.dependency_list[i] = scanner.readInt<u32>();
    }

    sci.savedata_size = scanner.readInt<u64>();
    sci.jump_id = scanner.readInt<u64>();

    return sci;
}

auto parseAccessControlInfo(const std::vector<u8> &data, size_t offset) -> AccessControlInfo {
    Scanner scanner(data);
    AccessControlInfo aci;

    scanner.seek(offset);
    aci.program_id = scanner.readInt<u64>();
    aci.core_version = scanner.readInt<u32>();
    aci.flag1 = scanner.readInt<u8>();
    aci.flag2 = scanner.readInt<u8>();
    aci.flag0 = scanner.readInt<u8>();
    aci.priority = scanner.readInt<u8>();

    for(int i = 0; i < sizeof(AccessControlInfo::resource_limits) / sizeof(u16); i++) {
        aci.resource_limits[i] = scanner.readInt<u16>();
    }

    aci.extdata_id = scanner.readInt<u64>();
    aci.sys_savedata_ids = scanner.readInt<u64>();
    aci.storage_unique_ids = scanner.readInt<u64>();
    aci.fs_access_and_other = scanner.readInt<u64>();
    scanner.readBytes(reinterpret_cast<u8*>(aci.service_access_control), sizeof(AccessControlInfo::service_access_control));
    scanner.readBytes(reinterpret_cast<u8*>(aci.extended_service_access_control), sizeof(AccessControlInfo::extended_service_access_control));
    scanner.skip(0xF);
    aci.resource_limit_category = scanner.readInt<u8>();

    for(int i = 0; i < sizeof(AccessControlInfo::arm11_descriptors) / sizeof(u32); i++) {
        aci.arm11_descriptors[i] = scanner.readInt<u32>();
    }

    scanner.skip(0x10);
    scanner.readBytes(aci.arm9_descriptors, sizeof(AccessControlInfo::arm9_descriptors));
    aci.arm9_descriptor_version = scanner.readInt<u8>();

    return aci;
}

auto parseNCCHExtendedHeader(const std::vector<u8> &data, size_t offset) -> NCCHExtendedHeader {
    Scanner scanner(data);
    NCCHExtendedHeader exheader;

    scanner.seek(offset);
    exheader.sci = parseSystemControlInfo(data, 0);
    exheader.aci = parseAccessControlInfo(data, 0x200);
    scanner.readBytes(exheader.signature, sizeof(NCCHExtendedHeader::signature));
    scanner.readBytes(exheader.public_key, sizeof(NCCHExtendedHeader::public_key));
    exheader.aci_limits = parseAccessControlInfo(data, 0x600);

    return exheader;
}

auto parseNCCH(const std::vector<u8> &data, size_t offset) -> NCCH {
    Scanner scanner(data);
    NCCH ncch;
    ncch.header = parseNCCHHeader(data, offset);

    //Check magic 'NCCH'
    if(ncch.header.magic != 0x4843434E) {
        printf("NCCH header magic doesn't match! (Expected: 0x4843434E, Actual: %08X)\n", ncch.header.magic);
        std::exit(-1);
    }

    //Check for Extended Header
    if(ncch.header.exheader_size > 0) {
        ncch.exheader = parseNCCHExtendedHeader(data, offset + 0x200);
    }

    //Check for Logo
    if(ncch.header.logo_size > 0) {
        ncch.logo = std::vector<u8>(ncch.header.logo_size * 0x200);
        scanner.seek(offset + ncch.header.logo_offset * 0x200);
        scanner.readBytes(ncch.logo->data(), ncch.header.logo_size * 0x200);
    }

    //Check for Plain Region
    if(ncch.header.plain_size > 0) {
        ncch.plain_region = std::vector<u8>(ncch.header.plain_size * 0x200);
        scanner.seek(offset + ncch.header.plain_offset * 0x200);
        scanner.readBytes(ncch.plain_region->data(), ncch.header.plain_size * 0x200);
    }

    //Check for ExeFS
    if(ncch.header.exefs_size > 0) {
        ncch.exefs = parseExeFS(data, offset + ncch.header.exefs_offset * 0x200);
    }

    //Check for RomFS
    if(ncch.header.romfs_size > 0) {
        scanner.seek(offset + ncch.header.romfs_offset * 0x200);
        ncch.romfs = parseRomFS(data, offset + ncch.header.romfs_offset * 0x200);
    }

    return ncch;
}