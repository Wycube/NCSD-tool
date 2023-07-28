#include "NCSD.hpp"
#include "NCCH.hpp"
#include "Scanner.hpp"

auto parseNCSDHeader(const std::vector<u8> &data, size_t offset) -> NCSDHeader {
    Scanner scanner(data);
    NCSDHeader header;

    scanner.seek(offset);
    scanner.readBytes(header.signature, sizeof(NCSDHeader::signature));
    header.magic = scanner.readInt<u32>();
    header.size = scanner.readInt<u32>();
    header.media_id = scanner.readInt<u64>();
    header.fs_type = scanner.readInt<u64>();
    header.crypt_type = scanner.readInt<u64>();

    for(int i = 0; i < 8; i++) {
        header.partition_table[i][0] = scanner.readInt<u32>();
        header.partition_table[i][1] = scanner.readInt<u32>();
    }

    return header;
}

auto parseNCSDCartHeader(const std::vector<u8> &data, size_t offset) -> NCSDCartHeader {
    Scanner scanner(data);
    NCSDCartHeader header;

    scanner.seek(offset);
    scanner.readBytes(header.exheader_hash, sizeof(NCSDCartHeader::exheader_hash));
    header.header_size = scanner.readInt<u32>();
    header.sector_zero_offset = scanner.readInt<u32>();
    header.partition_flags = scanner.readInt<u64>();

    for(int i = 0; i < 8; i++) {
        header.partition_id_table[i] = scanner.readInt<u64>();
    }

    return header;
}

auto parseNCSD(const std::vector<u8> &data, size_t offset) -> NCSD {
    NCSD ncsd;
    ncsd.header = parseNCSDHeader(data, offset);

    //Check magic 'NCSD'
    if(ncsd.header.magic != 0x4453434E) {
        printf("NCSD header magic doesn't match! (Expected: 0x4453434E, Actual: %08X)\n", ncsd.header.magic);
        std::exit(-1);
    }

    //Cart Header Section
    ncsd.cart_header = parseNCSDCartHeader(data, offset + 0x160);

    //NCCH Partitions
    for(int i = 0; i < 8; i++) {
        //Determine if a partition exists by a non-zero size
        if(ncsd.header.partition_table[i][1] != 0) {
            ncsd.partitions[i] = parseNCCH(data, ncsd.header.partition_table[i][0] * 0x200);
        }
    }

    return ncsd;
}