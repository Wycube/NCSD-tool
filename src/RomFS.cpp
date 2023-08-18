#include "RomFS.hpp"
#include "Scanner.hpp"
#include <memory>


auto parseLevel3Header(const std::vector<u8> &data, size_t offset) -> Level3Header {
    Scanner scanner(data);
    Level3Header header;

    scanner.seek(offset);
    header.header_length = scanner.readInt<u32>();
    header.dir_hash_offset = scanner.readInt<u32>();
    header.dir_hash_length = scanner.readInt<u32>();
    header.dir_meta_offset = scanner.readInt<u32>();
    header.dir_meta_length = scanner.readInt<u32>();
    header.file_hash_offset = scanner.readInt<u32>();
    header.file_hash_length = scanner.readInt<u32>();
    header.file_meta_offset = scanner.readInt<u32>();
    header.file_meta_length = scanner.readInt<u32>();
    header.file_data_offset = scanner.readInt<u32>();

    return header;
}

auto parseDirectoryMetadata(const std::vector<u8> &data, size_t offset) -> DirectoryMetadata {
    Scanner scanner(data);
    DirectoryMetadata entry;

    scanner.seek(offset);
    entry.parent_offset = scanner.readInt<u32>();
    entry.sibling_offset = scanner.readInt<u32>();
    entry.child_offset = scanner.readInt<u32>();
    entry.first_file_offset = scanner.readInt<u32>();
    entry.same_hash_offset = scanner.readInt<u32>();
    entry.name_length = scanner.readInt<u32>();

    if(entry.name_length > 0) {
        entry.name.reserve(entry.name_length / 2);
        
        for(int i = 0; i < entry.name_length / 2; i++) {
            entry.name.push_back(scanner.readInt<u16>());
        }
    }

    return entry;
}

auto parseFileMetadata(const std::vector<u8> &data, size_t offset) -> FileMetadata {
    Scanner scanner(data);
    FileMetadata entry;

    scanner.seek(offset);
    entry.parent_offset = scanner.readInt<u32>();
    entry.sibling_offset = scanner.readInt<u32>();
    entry.data_offset = scanner.readInt<u64>();
    entry.data_size = scanner.readInt<u64>();
    entry.same_hash_offset = scanner.readInt<u32>();
    entry.name_length = scanner.readInt<u32>();

    if(entry.name_length > 0) {
        entry.name.reserve(entry.name_length / 2);
        
        for(int i = 0; i < entry.name_length / 2; i++) {
            entry.name.push_back(scanner.readInt<u16>());
        }
    }

    return entry;
}

auto parseLevel3(const std::vector<u8> &data, size_t offset) -> Level3 {
    Scanner scanner(data);
    Level3 lvl3;

    scanner.seek(offset);
    lvl3.header = parseLevel3Header(data, offset);
    
    //Directory Hash Table
    scanner.seek(offset + lvl3.header.dir_hash_offset);
    lvl3.dir_hash_table.reserve(lvl3.header.dir_hash_length / 4);
    for(int i = 0; i < lvl3.header.dir_hash_length / 4; i++) {
        lvl3.dir_hash_table.push_back(scanner.readInt<u32>());
    }

    //Directory Metadata Table
    scanner.seek(offset + lvl3.header.dir_meta_offset);
    size_t dir_entry_offset = lvl3.header.dir_meta_offset;
    while(dir_entry_offset < lvl3.header.file_hash_offset - 0x18) {
        lvl3.dir_table.push_back(parseDirectoryMetadata(data, offset + dir_entry_offset));
        dir_entry_offset += 0x18 + lvl3.dir_table.back().name_length;
        
        //Correct for 4-byte alignment
        if(dir_entry_offset & 3) {
            dir_entry_offset += 4 - dir_entry_offset & 3;
        }
    }

    //File Hash Table
    scanner.seek(offset + lvl3.header.file_hash_offset);
    lvl3.file_hash_table.reserve(lvl3.header.file_hash_length / 4);
    for(int i = 0; i < lvl3.header.file_hash_length / 4; i++) {
        lvl3.file_hash_table.push_back(scanner.readInt<u32>());
    }

    //File Metadata Table
    size_t max_file_addr = 0;
    size_t file_entry_offset = lvl3.header.file_meta_offset;
    while(file_entry_offset < lvl3.header.file_data_offset - 0x20) {
        lvl3.file_table.push_back(parseFileMetadata(data, offset + file_entry_offset));
        file_entry_offset += 0x20 + lvl3.file_table.back().name_length;

        //Correct for 4-byte alignment
        if(file_entry_offset & 3) {
            file_entry_offset += 4 - file_entry_offset & 3;
        }

        //Get the max address that is part of a file and use that to determine the size of the file data section
        max_file_addr = std::max(max_file_addr, lvl3.file_table.back().data_offset + lvl3.file_table.back().data_size);
    }

    //File Data
    lvl3.file_data.resize(max_file_addr);
    if(max_file_addr > 0) {
        scanner.seek(offset + lvl3.header.file_data_offset);
        scanner.readBytes(lvl3.file_data.data(), max_file_addr);
    }

    return lvl3;
}

auto parseDirectory(const std::vector<u8> &data, size_t dir_offset, size_t file_offset, size_t offset) -> Directory {
    static_assert(sizeof(char16_t) == 2);
    DirectoryMetadata entry = parseDirectoryMetadata(data, dir_offset + offset);
    Directory dir;

    if(!entry.name.empty()) {
        dir.name = std::u16string(reinterpret_cast<const char16_t*>(entry.name.data()), entry.name.size());
    } else {
        dir.name = u"RomFS";
    }
    
    //Add children directories
    if(entry.child_offset != 0xFFFFFFFF) {
        u32 child_offset = dir_offset + entry.child_offset;
        DirectoryMetadata child_entry = parseDirectoryMetadata(data, child_offset);
        dir.children.push_back(parseDirectory(data, dir_offset, file_offset, entry.child_offset));

        while(child_entry.sibling_offset != 0xFFFFFFFF) {
            dir.children.push_back(parseDirectory(data, dir_offset, file_offset, child_entry.sibling_offset));
            child_offset = dir_offset + child_entry.sibling_offset;
            child_entry = parseDirectoryMetadata(data, child_offset);
        }
    }

    //Add files
    if(entry.first_file_offset != 0xFFFFFFFF) {
        u32 child_file_offset = file_offset + entry.first_file_offset;
        FileMetadata file_entry = parseFileMetadata(data, child_file_offset);
        dir.files.emplace_back(File{std::u16string(reinterpret_cast<const char16_t*>(file_entry.name.data()), file_entry.name.size()), file_entry.data_offset, file_entry.data_size});

        while(file_entry.sibling_offset != 0xFFFFFFFF) {
            child_file_offset = file_offset + file_entry.sibling_offset;
            file_entry = parseFileMetadata(data, child_file_offset);
            dir.files.emplace_back(File{std::u16string(reinterpret_cast<const char16_t*>(file_entry.name.data()), file_entry.name.size()), file_entry.data_offset, file_entry.data_size});
        }
    }

    return dir;
}

auto parseRomFSHeader(const std::vector<u8> &data, size_t offset) -> RomFSHeader {
    Scanner scanner(data);
    RomFSHeader header;

    scanner.seek(offset);
    header.magic = scanner.readInt<u32>();
    header.magic_num = scanner.readInt<u32>();
    header.master_hash_size = scanner.readInt<u32>();
    header.lvl1_offset = scanner.readInt<u64>();
    header.lvl1_hash_size = scanner.readInt<u64>();
    header.lvl1_block_size = scanner.readInt<u32>();
    scanner.skip(4);
    header.lvl2_offset = scanner.readInt<u64>();
    header.lvl2_hash_size = scanner.readInt<u64>();
    header.lvl2_block_size = scanner.readInt<u32>();
    scanner.skip(4);
    header.lvl3_offset = scanner.readInt<u64>();
    header.lvl3_hash_size = scanner.readInt<u64>();
    header.lvl3_block_size = scanner.readInt<u32>();
    scanner.skip(8);
    header.optional_info_size = scanner.readInt<u32>();

    return header;
}

auto parseRomFS(const std::vector<u8> &data, size_t offset) -> RomFS {
    RomFS romfs;
    romfs.header = parseRomFSHeader(data, offset);

    //Check magic 'IVFC'
    if(romfs.header.magic != 0x43465649) {
        printf("RomFS magic does not match! (Expected: 0x43465649, Actual: %08X)\n", romfs.header.magic);
        std::exit(-1);
    }

    //Check magic number 0x10000
    if(romfs.header.magic_num != 0x10000) {
        printf("RomFS magic number does not match! (Expected: 0x10000, Actual: %08X)\n", romfs.header.magic_num);
        std::exit(-1);
    }

    size_t lvl3_offset = offset + 0x1000;
    romfs.level3 = parseLevel3(data, offset + 0x1000);
    romfs.root = parseDirectory(data, lvl3_offset + romfs.level3.header.dir_meta_offset, lvl3_offset + romfs.level3.header.file_meta_offset, 0);

    return romfs;
}