#include "NCSD.hpp"
#include "Scanner.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstdint>

//TODO: Make all the parse* functions take in a scanner and add a method for getting the offset from
//the scanner. The functions don't actually need raw access to the data and they just seek to the offset
//at the start anyways so adding the ability to get the offset so they can utilize when needed but
//ultimately just passing in a scanner and seeking to an offset before-hand would be cleaner and save
//on resources (a little) by not constructing a scanner every single time.

//TODO: wchar_t is only 16-bit on Windows, so rewrite those parts that use it to some other, more platform
//agnostic, way.


struct File {
    std::wstring name;
    size_t offset;
    size_t size;
};

struct Directory {
    std::wstring name;
    std::vector<Directory> children;
    std::vector<File> files;
};

auto parseFileSystem(const std::vector<u8> &data, size_t dir_offset, size_t file_offset, size_t data_offset, size_t offset) -> Directory {
    Scanner scanner(data);
    DirectoryMetadata entry = parseDirectoryMetadata(data, dir_offset + offset);
    Directory dir;

    if(!entry.name.empty()) {
        dir.name = (wchar_t*)entry.name.data();
    } else {
        dir.name = L"root";
    }
    
    //Add children
    if(entry.child_offset != 0xFFFFFFFF) {
        u32 child_offset = dir_offset + entry.child_offset;
        DirectoryMetadata child_entry = parseDirectoryMetadata(data, child_offset);
        dir.children.push_back(parseFileSystem(data, dir_offset, file_offset, data_offset, entry.child_offset));

        while(child_entry.sibling_offset != 0xFFFFFFFF) {
            dir.children.push_back(parseFileSystem(data, dir_offset, file_offset, data_offset, child_entry.sibling_offset));
            child_offset = dir_offset + child_entry.sibling_offset;
            child_entry = parseDirectoryMetadata(data, child_offset);
        }
    }


    //Add files
    if(entry.first_file_offset != 0xFFFFFFFF) {
        u32 child_file_offset = file_offset + entry.first_file_offset;
        FileMetadata file_entry = parseFileMetadata(data, child_file_offset);
        dir.files.emplace_back(File{(wchar_t*)file_entry.name.data(), data_offset + file_entry.data_offset, file_entry.data_size});
        // dumpFile(data, data_offset + file_entry.data_offset, file_entry.data_size, (wchar_t*)file_entry.name);

        // printf("File '%ls' Info:\n", (wchar_t*)file_entry.name);
        // printf("Parent Offset: %X\n", file_entry.parent_offset);
        // printf("Sibling Offset: %X\n", file_entry.sibling_offset);
        // printf("Data Offset: %zX\n", file_entry.data_offset);
        // printf("Data Size: %zX\n", file_entry.data_size);
        // printf("\n");

        while(file_entry.sibling_offset != 0xFFFFFFFF) {
            child_file_offset = file_offset + file_entry.sibling_offset;
            file_entry = parseFileMetadata(data, child_file_offset);
            dir.files.push_back(File{(wchar_t*)file_entry.name.data(), data_offset + file_entry.data_offset, file_entry.data_size});
        }
    }


    return dir;
}

void printDirectory(const Directory &dir, int level = 0) {
    if(level > 0) {
        for(int i = 0; i < level - 1; i++) {
            printf("|");
        }

        printf("-");
    }
    
    printf("%ls\n", dir.name.c_str());

    for(const auto &file : dir.files) {
        for(int i = 0; i < level; i++) {
            printf("|");
        }

        printf("*%ls\n", file.name.c_str());
    }

    for(const auto &child : dir.children) {
        printDirectory(child, level + 1);
    }
}

void dumpFile(const std::vector<u8> &data, size_t offset, size_t size, const std::wstring &name) {
    std::ofstream file(name, std::ios::binary);

    if(!file.is_open()) {
        printf("Failed to dump file %ls!\n", name.c_str());
        return;
    }

    printf("File '%ls' Offset: %zX  Size: %zX\n", name.c_str(), offset, size);
    // printf("File Size  : %zX\n", size);

    file.write(reinterpret_cast<const char*>(&data[offset]), size);
}

void dumpDirectory(const std::vector<u8> &data, const Directory &dir, std::wstring path) {
    //Create directory
    std::wstring new_path = path + L"/" + dir.name;
    std::filesystem::create_directory(new_path);

    //Add files to that directory
    for(const auto &file : dir.files) {
        dumpFile(data, file.offset, file.size, new_path + L"/" + file.name);
    }

    //Recurse for children
    for(const auto &child : dir.children) {
        dumpDirectory(data, child, new_path);
    }
}


int main(int argc, char *argv[]) {
    if(argc < 2) {
        printf("No file provided!\n");
        return -1;
    }

    std::ifstream file(argv[1], std::ios::binary);

    if(!file.is_open()) {
        printf("Failed to open file!\n");
        return -1;
    }

    size_t size = std::filesystem::file_size(argv[1]);
    std::vector<u8> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    file.close();

    Scanner scanner(data);

    //Parse NCSD
    NCSD ncsd = parseNCSD(data, 0);

    printf("Successfully Parsed!\n");
    printf("Magic: %c%c%c%c\n", ncsd.header.magic & 0xFF, ncsd.header.magic >> 8 & 0xFF, ncsd.header.magic >> 16 & 0xFF, ncsd.header.magic >> 24 & 0xFF);
    printf("Size: %u\n", ncsd.header.size);
    printf("Media ID: %zu\n", ncsd.header.media_id);
    printf("FS Type: %zu\n", ncsd.header.fs_type);
    printf("Crypt Type: %zu\n", ncsd.header.crypt_type);

    printf("Partition Table:\n");
    for(int i = 0; i < 8; i++) {
        if(ncsd.partitions[i].has_value()) {
            NCCHHeader &p_header = ncsd.partitions[i]->header;
            printf(" %i:\n", i);
            printf("  Offset: 0x%X\n", ncsd.header.partition_table[i][0] * 0x200);
            printf("  Size  : 0x%X\n", ncsd.header.partition_table[i][1] * 0x200);
            printf("  ExeFS Offset: 0x%X\n", p_header.exefs_offset * 0x200);
            printf("  ExeFS Size  : 0x%X\n", p_header.exefs_size * 0x200);
        }
    }

    //Parse the NCCH Header at Partition 0
    size_t p0_offset = ncsd.header.partition_table[0][0] * 0x200;
    NCCH &p0 = *ncsd.partitions[0];

    printf("Logo Offset: %zX\n", p0_offset + p0.header.logo_offset * 0x200);
    printf("Logo Size: %X\n", p0.header.logo_size * 0x200);

    //scanner RomFS Header
    size_t romfs_offset = p0_offset + p0.header.romfs_offset * 0x200;
    RomFSHeader &romfs_header = p0.romfs.header;
    printf("RomFS offset: %zX\n", p0_offset + p0.header.romfs_offset * 0x200);

    //Magic should equal the string 'IVFC'
    if(romfs_header.magic != 0x43465649) {
        printf("RomFS magic does not match! Value: %08X\n", romfs_header.magic);
        return -1;
    }

    //Magic number should equal 0x10000
    if(romfs_header.magic_num != 0x10000) {
        printf("RomFS magic number does not match! Value: %08X\n", romfs_header.magic_num);
        return -1;
    }

    // printf("Lvl1 Offset: %zX\n", romfs_header.lvl1_offset);
    // printf("Lvl2 Offset: %zX\n", romfs_header.lvl2_offset);
    // printf("Lvl3 Offset: %zX\n", romfs_header.lvl3_offset);

    Level3Header lvl3_header = p0.romfs.level3.header;

    // printf("Lvl3 : 0x%X\n", lvl3_header.header_length);
    // printf("Lvl3 : 0x%X\n", lvl3_header.dir_hash_offset);
    // printf("Lvl3 : 0x%X\n", lvl3_header.dir_hash_length);
    // printf("Lvl3 : 0x%X\n", lvl3_header.dir_meta_offset);
    // printf("Lvl3 : 0x%X\n", lvl3_header.dir_meta_length);
    // printf("Lvl3 : 0x%X\n", lvl3_header.file_hash_offset);
    // printf("Lvl3 : 0x%X\n", lvl3_header.file_hash_length);
    // printf("Lvl3 : 0x%X\n", lvl3_header.file_meta_offset);
    // printf("Lvl3 : 0x%X\n", lvl3_header.file_meta_length);
    // printf("Lvl3 : 0x%X\n", lvl3_header.file_data_offset);

    DirectoryMetadata dir_entry = parseDirectoryMetadata(data, romfs_offset + 0x1000 + lvl3_header.dir_meta_offset);

    // printf("\n");
    // printf("Parent Offset: %X\n", dir_entry.parent_offset);
    // printf("Sibling Offset: %X\n", dir_entry.sibling_offset);
    // printf("Child Offset: %X\n", dir_entry.child_offset);
    // printf("First File Offset: %X\n", dir_entry.first_file_offset);
    // printf("Same Hash Offset: %X\n", dir_entry.same_hash_offset);
    // printf("Name Length: %u\n", dir_entry.name_length);
    // printf("Root Dir Name: %ls\n", (wchar_t*)dir_entry.name);
    // printf("\n");

    DirectoryMetadata child_dir_entry = parseDirectoryMetadata(data, romfs_offset + 0x1000 + lvl3_header.dir_meta_offset + dir_entry.child_offset);

    // printf("Child Directory:\n");
    // printf("Parent Offset: %X\n", child_dir_entry.parent_offset);
    // printf("Sibling Offset: %X\n", child_dir_entry.sibling_offset);
    // printf("Child Offset: %X\n", child_dir_entry.child_offset);
    // printf("First File Offset: %X\n", child_dir_entry.first_file_offset);
    // printf("Same Hash Offset: %X\n", child_dir_entry.same_hash_offset);
    // printf("Name Length: %u\n", child_dir_entry.name_length);
    // printf("Dir Name: %ls\n", (wchar_t*)child_dir_entry.name.data());
    // printf("\n");

    // Directory root = parseFileSystem(data, romfs_offset + 0x1000 + lvl3_header.dir_meta_offset, romfs_offset + 0x1000 + lvl3_header.file_meta_offset, romfs_offset + 0x1000 + lvl3_header.file_data_offset, 0);
    // dumpDirectory(data, root, L".");
    // printDirectory(root);

    // return 0;

    //Parse ExeFS Header
    const size_t exefs_offset = p0_offset + p0.header.exefs_offset * 0x200;
    printf("ExeFS Header Offset: %zX\n", exefs_offset);
    
    // return 0;
    
    // ExeFSHeader exefs_header;
    // scanner.seek(exefs_offset);

    // //File Headers
    // for(int i = 0; i < 10; i++) {
    //     scanner.readBytes(exefs_header.file_headers[i].name, sizeof(FileHeader::name));
    //     exefs_header.file_headers[i].offset = scanner.readInt<u32>();
    //     exefs_header.file_headers[i].size = scanner.readInt<u32>();
    // }

    // //File Hashes
    // for(int i = 0; i < 10; i++) {
    //     scanner.readBytes(exefs_header.file_hashes[i], 32);
    // }

    // printf("Parsed ExeFS Header!\n");
    
    // printf("Files:\n");
    // for(int i = 0; i < 10; i++) {
    //     if(exefs_header.file_headers[i].offset != 0 || exefs_header.file_headers[i].size != 0) {
    //         char name[9];
    //         std::memcpy(name, exefs_header.file_headers[i].name, 8);
    //         name[8] = '\0';

    //         printf(" File %i: %s\n", i, name);
    //         printf("  Offset: 0x%X\n", exefs_header.file_headers[i].offset);
    //         printf("  Size  : 0x%X\n", exefs_header.file_headers[i].size);
    //     }
    // }

    // const size_t graphics_offset_24 = exefs_offset + p0.exefs.header.file_headers[2].offset + 0x200 + 0x2040;
    const size_t graphics_offset_24 = 0x2040;
    // const size_t graphics_offset_48 = exefs_offset + p0.exefs.header.file_headers[2].offset + 0x200 + 0x24C0;
    const size_t graphics_offset_48 = 0x24C0;

    //PPM Stuff below
    std::ofstream image24("28x28.ppm", std::ios::binary);
    std::ofstream image48("48x48.ppm", std::ios::binary);

    image24 << "P6\n";
    image24 << "24 24\n";
    image24 << "255\n";
    
    image48 << "P6\n";
    image48 << "48 48\n";
    image48 << "255\n";

    for(int y = 0; y < 24; y++) {
        for(int x = 0; x < 24; x++) {
            int tilex = x / 8;
            int tiley = y / 8;
            // int tile = (tilex & 1) | ((tiley & 1) << 1) | ((tilex & 2) << 1) | ((tiley & 2) << 2);
            int tile = tilex + tiley * 3;
            // printf("X: %i, Y: %i, Tile: %i\n", tilex, tiley, tile);
            // int x = (i & 1) | ((i & 4) >> 1) | ((i & 0x10) >> 2) | ((i & 0x40) >> 3) | ((i & 0x100) >> 4) | ((i & 0x400) >> 5);
            // int y = ((i & 2) >> 1) | ((i & 8) >> 1) | ((i & 0x20) >> 2) | ((i & 0x80) >> 3) | ((i & 0x200) >> 4);
            int _x = x % 8;
            int _y = y % 8;
            int i = (_x & 1) | ((_y & 1) << 1) | ((_x & 2) << 1) | ((_y & 2) << 2) | ((_x & 4) << 2) | ((_y & 4) << 3) | ((_x & 8) << 3) | ((_y & 8) << 4) | ((_x & 0x10) << 4) | ((_y & 0x10) << 5) | ((_x & 0x20) << 5) | ((_y & 0x20) << 6) | ((_x & 0x40) << 6) | ((_y & 0x40) << 7);
            // printf("X: %i, Y: %i, I: %i\n", x, y, i);
            // int i = x + y * 24;

            u16 color = p0.exefs.file_data[2][graphics_offset_24 + tile * 0x80 + i * 2] | (p0.exefs.file_data[2][graphics_offset_24 + tile * 0x80 + i * 2 + 1] << 8);
            u8 red = (color >> 11) & 0x1F;
            red = static_cast<u8>((float)red * (255.0f / 31.0f));
            u8 green = (color >> 5) & 0x3F;
            green = static_cast<u8>((float)green * (255.0f / 63.0f));
            u8 blue = color & 0x1F;
            blue = static_cast<u8>((float)blue * (255.0f / 31.0f));

            // printf("Red: %i, Green: %i, Blue: %i\n", red, green, blue);

            image24.put(red);
            image24.put(green);
            image24.put(blue);
        }
    }

    for(int y = 0; y < 48; y++) {
        for(int x = 0; x < 48; x++) {
            int tilex = x / 8;
            int tiley = y / 8;
            // int tile = (tilex & 1) | ((tiley & 1) << 1) | ((tilex & 2) << 1) | ((tiley & 2) << 2);
            int tile = tilex + tiley * 6;
            // printf("X: %i, Y: %i, Tile: %i\n", tilex, tiley, tile);
            // int x = (i & 1) | ((i & 4) >> 1) | ((i & 0x10) >> 2) | ((i & 0x40) >> 3) | ((i & 0x100) >> 4) | ((i & 0x400) >> 5);
            // int y = ((i & 2) >> 1) | ((i & 8) >> 1) | ((i & 0x20) >> 2) | ((i & 0x80) >> 3) | ((i & 0x200) >> 4);
            int _x = x % 8;
            int _y = y % 8;
            int i = (_x & 1) | ((_y & 1) << 1) | ((_x & 2) << 1) | ((_y & 2) << 2) | ((_x & 4) << 2) | ((_y & 4) << 3) | ((_x & 8) << 3) | ((_y & 8) << 4) | ((_x & 0x10) << 4) | ((_y & 0x10) << 5) | ((_x & 0x20) << 5) | ((_y & 0x20) << 6) | ((_x & 0x40) << 6) | ((_y & 0x40) << 7);
            // printf("X: %i, Y: %i, I: %i\n", x, y, i);
            // int i = x + y * 24;

            u16 color = p0.exefs.file_data[2][graphics_offset_48 + tile * 0x80 + i * 2] | (p0.exefs.file_data[2][graphics_offset_48 + tile * 0x80 + i * 2 + 1] << 8);
            u8 red = (color >> 11) & 0x1F;
            red = static_cast<u8>((float)red * (255.0f / 31.0f));
            u8 green = (color >> 5) & 0x3F;
            green = static_cast<u8>((float)green * (255.0f / 63.0f));
            u8 blue = color & 0x1F;
            blue = static_cast<u8>((float)blue * (255.0f / 31.0f));

            // printf("Red: %i, Green: %i, Blue: %i\n", red, green, blue);

            image48.put(red);
            image48.put(green);
            image48.put(blue);
        }
    }
}