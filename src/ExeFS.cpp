#include "ExeFS.hpp"
#include "Scanner.hpp"


auto parseExeFSHeader(const std::vector<u8> &data, size_t offset) -> ExeFSHeader {
    Scanner scanner(data);
    scanner.seek(offset);
    ExeFSHeader header;
    
    //File Headers
    for(int i = 0; i < 10; i++) {
        scanner.readBytes(header.file_headers[i].name, sizeof(ExeFSFileHeader::name));
        header.file_headers[i].offset = scanner.readInt<u32>();
        header.file_headers[i].size = scanner.readInt<u32>();
    }

    //File Hashes
    for(int i = 0; i < 10; i++) {
        scanner.readBytes(header.file_hashes[i], 32);
    }

    return header;
}

auto parseExeFS(const std::vector<u8> &data, size_t offset) -> ExeFS {
    Scanner scanner(data);
    ExeFS exefs;
    exefs.header = parseExeFSHeader(data, offset);
    
    //Get file data for each file if it exists
    for(int i = 0; i < 10; i++) {
        if(exefs.header.file_headers[i].size > 0) {
            size_t file_size = exefs.header.file_headers[i].size;
            exefs.file_data[i].resize(file_size);
            scanner.seek(offset + exefs.header.file_headers[i].offset + 0x200);
            scanner.readBytes(exefs.file_data[i].data(), file_size);
        }
    }

    return exefs;
}