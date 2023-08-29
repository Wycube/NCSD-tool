#include "NCSD.hpp"
#include "Scanner.hpp"
#include <fmt/format.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>


enum NCCHSection : u8 {
    ROMFS = 0x1,
    EXEFS = 0x2,
    LOGO  = 0x4,
    PLAIN = 0x8,
    ALL   = 0xF
};

struct ProgramConfig {
    bool print = false;
    u8 partitions = 0;
    u8 sections = 0;
    std::vector<std::string> files;
    std::vector<std::string> dirs;
    std::string file_path;
    std::string dump_dir;
};

auto getFileName(std::string_view path) -> std::string {
    std::string str = std::string(path);
    const size_t index_fwd = str.find_last_of('/');
    const size_t index_bck = str.find_last_of('\\');

    //If there is no foward or back slash then the input string should be the file name
    if(index_fwd == std::string::npos && index_bck == std::string::npos) {
        return str;
    }

    const size_t index_slash = index_fwd == std::string::npos ? index_bck : index_fwd;
    return str.substr(index_slash + 1);
}

void printHelpMessage(const char *name) {
    printf("Usage: %s [options] <file>\n\n", getFileName(name).c_str());
    printf(
    "Options:\n"
    "\t--help     Print this help message\n"
    "\t--version  Print version information\n"
    "\t--print    Print the RomFS filesystem of the partitions\n"
    "\t-a         All, dump all partitions\n"
    "\t-p N       Partition, dump partition N of an NCSD\n"
    "\t-d N       Directory, dump the files in directory named N in the RomFS\n"
    "\t-f N       File, dump the file named N in the RomFS\n"
    "\t-s         Dump all parts of a partition\n"
    "\t-r         Dump the RomFS\n"
    "\t-e         Dump the ExeFS\n"
    "\t-l         Dump the Logo section\n"
    "\t-p         Dump the Plain Region\n"
    );
}

auto parseArgs(int argc, char *argv[]) -> ProgramConfig {
    ProgramConfig config{};

    if(argc < 2) {
        printf("Usage: %s [options] <file>\n\n", getFileName(argv[0]).c_str());
        std::exit(-1);
    }

    for(int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if(arg[0] == '-') {
            //Options
            if(arg == "--help") {
                printHelpMessage(argv[0]);
                std::exit(0);
            } else if(arg == "--version") {
                printf("NCSD Tool, Copyright (c) Wycube 2023\n");
                printf("Version 0.1\n");
                std::exit(0);
            }

            if(arg == "--print") {
                config.print = true;
            } else if(arg == "-a") {
                config.partitions |= 0xFF;
            } else if(arg == "-p") {
                if(i == argc - 1) {
                    printf("Error: No argument provided to option '-p'!\n");
                    std::exit(-1);
                }

                int num = -1;
                try {
                    num = std::stoi(argv[++i]);
                } catch(const std::exception &e) {
                    printf("Error: Invalid argument provided to '-p'!\n");
                    std::exit(-1);
                }

                if(num >= 0 && num <= 7) {
                    config.partitions |= 1 << num;
                }
            } else if(arg == "-d") {
                if(i == argc - 1) {
                    printf("Error: No argument provided to option '-d'!\n");
                    std::exit(-1);
                }

                config.dirs.push_back(argv[++i]);
            } else if(arg == "-f") {
                if(i == argc - 1) {
                    printf("Error: No argument provided to option '-f'!\n");
                    std::exit(-1);
                }

                config.files.push_back(argv[++i]);
            } else if(arg == "-s") {
                config.sections |= ALL;
            } else if(arg == "-r") {
                config.sections |= ROMFS;
            } else if(arg == "-e") {
                config.sections |= EXEFS;
            } else if(arg == "-l") {
                config.sections |= LOGO;
            } else if(arg == "-p") {
                config.sections |= PLAIN;
            } else {
                printf("Warning: Unknown option '%s'\n", argv[i]);
            }
        } else {
            //File path
            if(!config.file_path.empty()) {
                printf("Error: More than one file provided!\n");
                std::exit(-1);
            }

            config.file_path = arg;
            std::string dump_dir = getFileName(config.file_path);
            config.dump_dir = dump_dir.substr(0, dump_dir.find_last_of('.'));
        }
    }

    return config;
}

void printDirectory(const Directory &dir, int level = 0) {
    if(level > 0) {
        fmt::print("{:│>{}}", "├", level);
    }
    fmt::print("{}\n", std::string(dir.name.begin(), dir.name.end()));

    for(const auto &child : dir.children) {
        printDirectory(child, level + 1);
    }

    for(size_t i = 0; i < dir.files.size(); i++) {
        if(i < dir.files.size() - 1) {
            fmt::print("{:│>{}}", "├", level + 1);
        } else {
            fmt::print("{:│>{}}", "└", level + 1);
        }

        fmt::print("{}\n", std::string(dir.files[i].name.begin(), dir.files[i].name.end()));
    }
}

void dumpFile(const File &file, const std::vector<u8> &file_data, const std::u16string &parent) {
    const std::filesystem::path file_path = parent + file.name;
    std::ofstream file_stream(file_path, std::ios::binary);

    if(!file_stream.is_open()) {
        printf("Failed to dump file '%s'\n", file_path.string().c_str());
        return;
    }

    file_stream.write(reinterpret_cast<const char*>(&file_data[file.offset]), file.size);
}

void dumpDirectory(const Directory &dir, const std::vector<u8> &file_data, const std::u16string &parent_path) {
    const std::u16string new_path = parent_path + dir.name + u'/';
    std::filesystem::create_directory(new_path);

    for(const auto &child : dir.children) {
        dumpDirectory(child, file_data, new_path);
    }

    for(const auto &file : dir.files) {
        dumpFile(file, file_data, new_path);
    }
}

auto findFile(const Directory &search_dir, const std::u16string &search_path, const std::u16string &path) -> std::optional<const File*> {
    std::u16string new_search_path = search_path + search_dir.name + u'/';

    for(const auto &file : search_dir.files) {
        if((new_search_path + file.name) == path) {
            return {&file};
        }
    }

    for(const auto &child : search_dir.children) {
        std::optional<const File*> result = findFile(child, new_search_path, path);
        if(result.has_value()) {
            return result;
        }
    }

    return {};
}

auto findDirectory(const Directory &search_dir, const std::u16string &search_path, const std::u16string &path) -> std::optional<const Directory*> {
    std::u16string new_search_path = search_path + search_dir.name;

    if(new_search_path == path) {
        return {&search_dir};
    }

    new_search_path += u'/';

    for(const auto &child : search_dir.children) {
        std::optional<const Directory*> result = findDirectory(child, new_search_path, path);
        if(result.has_value()) {
            return result;
        }
    }

    return {};
}

void dump(const ProgramConfig &config, const NCCH &ncch, int partition = 0) {
    std::string partition_dir = config.dump_dir + '/' + std::to_string(partition) + '/';
    std::filesystem::create_directories(partition_dir);

    //Dump ExeFS
    if(config.sections & EXEFS && ncch.exefs.has_value()) {
        std::string exefs_dir = partition_dir + "ExeFS/";
        std::filesystem::create_directory(exefs_dir);

        for(int i = 0; i < 10; i++) {
            if(ncch.exefs->header.file_headers[i].size > 0) {
                char name[9] = {0};
                std::memcpy(name, ncch.exefs->header.file_headers[i].name, sizeof(ExeFSFileHeader::name));

                std::ofstream file(exefs_dir + name, std::ios::binary);
                file.write(reinterpret_cast<const char*>(ncch.exefs->file_data[i].data()), ncch.exefs->file_data[i].size());
            }
        }
    }

    //Dump Logo
    if(config.sections & LOGO && ncch.logo.has_value()) {
        std::ofstream file(partition_dir + "logo", std::ios::binary);
        file.write(reinterpret_cast<const char*>(ncch.logo->data()), ncch.logo->size());
    }

    //Dump Plain Region
    if(config.sections & PLAIN && ncch.plain_region.has_value()) {
        std::ofstream file(partition_dir + "plain_region", std::ios::binary);
        file.write(reinterpret_cast<const char*>(ncch.plain_region->data()), ncch.plain_region->size());
    }

    //Dump whole RomFS, or the specified files/directories
    if(config.sections & ROMFS && ncch.romfs.has_value()) {
        dumpDirectory(ncch.romfs->root, ncch.romfs->level3.file_data, std::u16string(partition_dir.begin(), partition_dir.end()));
    } else if((!config.files.empty() || !config.dirs.empty()) && ncch.romfs.has_value()) {
        const std::string romfs_dir = partition_dir + "RomFS/";
        for(const auto &file_path : config.files) {
            std::string abs_file_path = std::string("RomFS/") + file_path;
            std::optional<const File*> result = findFile(ncch.romfs->root, u"", std::u16string(abs_file_path.begin(), abs_file_path.end()));
            if(result.has_value()) {
                std::filesystem::path parent_dir = std::filesystem::path(romfs_dir + file_path).parent_path();
                std::filesystem::create_directories(parent_dir);
                dumpFile(*result.value(), ncch.romfs->level3.file_data, parent_dir.u16string() + u'/');
            }
        }

        for(const auto &dir_path : config.dirs) {
            std::string abs_dir_path = std::string("RomFS/") + dir_path;
            std::optional<const Directory*> result = findDirectory(ncch.romfs->root, u"", std::u16string(abs_dir_path.begin(), abs_dir_path.end()));
            if(result.has_value()) {
                std::filesystem::path parent_dir = std::filesystem::path(romfs_dir + dir_path).parent_path();
                std::filesystem::create_directories(parent_dir);
                dumpDirectory(*result.value(), ncch.romfs->level3.file_data, parent_dir.u16string() + u'/');
            }
        }
    }
}

int main(int argc, char *argv[]) {
    ProgramConfig config = parseArgs(argc, argv);
    if(config.file_path.empty()) {
        printf("Error: No file path provided!\n");
        return -1;
    }

    std::ifstream file(config.file_path, std::ios::binary);
    if(!file.is_open()) {
        printf("Error: Failed to open file!\n");
        return -1;
    }

    size_t size = std::filesystem::file_size(config.file_path);
    std::vector<u8> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    file.close();

    //Determine if file is NCSD or an NCCH partition, or neither
    u32 magic = data[0x100] | (data[0x101] << 8) | (data[0x102] << 16) | (data[0x103] << 24);
    std::vector<NCCH> ncchs;

    //Create dump directory
    if(config.sections != 0) {
        std::filesystem::create_directory(config.dump_dir);
    }
    
    if(magic == 0x4453434E) {
        printf("NCSD\n");

        //Print some information about NCSD if necessary
        NCSD ncsd = parseNCSD(data, 0);

        //Add all partitions specified by config
        for(int i = 0; i < 8; i++) {
            if(config.partitions & (1 << i) && ncsd.partitions[i].has_value()) {
                if(config.print && ncsd.partitions[i]->romfs.has_value()) {
                    printf("Partition %i:\n", i);
                    printDirectory(ncsd.partitions[i]->romfs->root);
                }

                dump(config, ncsd.partitions[i].value(), i);
            }
        }
    } else if(magic == 0x4843434E) {
        printf("NCCH\n");
        NCCH ncch = parseNCCH(data, 0);

        if(config.print && ncch.romfs.has_value()) {
            printDirectory(ncch.romfs->root);
        }

        dump(config, ncch);
    } else {
        printf("Error: File is neither an NCSD or NCCH!\n");
        return -1;
    }
}