from byteparser import ByteParser, ParseType
import audio
import sys


def main():
    if len(sys.argv) < 2:
        print("usage: bcstm_to_wav.py <input.bcstm> [output.wav]")
        exit(0)

    # Open the input .bcstm file
    input_file = open(sys.argv[1], "rb")
    file_data = input_file.read()
    input_file.close()
    parser = ByteParser(file_data, None)

    # Header
    header_magic = parser.parse_value(ParseType(4, "raw"))
    endianness = parser.parse_value(ParseType(2, "int_le"))
    byte_order = "little" if endianness == 0xFEFF else "big"
    parser.byte_order = byte_order

    if header_magic != b'CSTM':
        print("Invalid header magic! (Expected: b'CSTM', Got: {})".format(header_magic))
        exit(-1)

    header_struct = parser.parse_struct(
        [("header_size", ParseType(2)),
        ("version", ParseType(4)),
        ("file_size", ParseType(4)),
        ("num_blocks", ParseType(2)),
        ("", ParseType(2, "padding")), # Reserved
        ("info_id", ParseType(2)),
        ("", ParseType(2, "padding")), # Padding
        ("info_offset", ParseType(4)),
        ("info_size", ParseType(4)),
        ("seek_id", ParseType(2)),
        ("", ParseType(2, "padding")), # Padding
        ("seek_offset", ParseType(4)),
        ("seek_size", ParseType(4)),
        ("data_id", ParseType(2)),
        ("", ParseType(2, "padding")), # Padding
        ("data_offset", ParseType(4)),
        ("data_size", ParseType(4))]
    )
    # print(header_magic)
    # print("Endianness: 0x{:X}".format(endianness))
    # print(header_struct)

    # Info Block
    parser.seek(header_struct["info_offset"])
    info_struct = parser.parse_struct(
        [("magic", ParseType(4, "raw")),
        ("size", ParseType(4)),
        ("stream_id", ParseType(2)),
        ("", ParseType(2, "padding")), # Padding
        ("stream_offset", ParseType(4)),
        ("track_table_id", ParseType(2)),
        ("", ParseType(2, "padding")), # Padding
        ("track_table_offset", ParseType(4)),
        ("channel_table_id", ParseType(2)),
        ("", ParseType(2, "padding")), # Padding
        ("channel_table_offset", ParseType(4)),
        
        # Stream Info
        ("encoding", ParseType(1)),
        ("loop", ParseType(1)),
        ("channel_count", ParseType(1)),
        ("", ParseType(1, "padding")), # Padding
        ("sample_rate", ParseType(4)),
        ("loop_start", ParseType(4)),
        ("loop_end", ParseType(4)),
        ("block_count", ParseType(4)),
        ("block_size", ParseType(4)),
        ("block_sample_count", ParseType(4)),
        ("last_block_size", ParseType(4)),
        ("last_block_sample_count", ParseType(4)),
        ("last_block_padded_size", ParseType(4)),
        ("seek_data_size", ParseType(4)),
        ("seek_sample_count", ParseType(4)),
        ("samples_id", ParseType(2)),
        ("", ParseType(2, "padding")),
        ("samples_offset", ParseType(4))]
    )

    if info_struct["magic"] != b'INFO':
        print("Invalid info magic! (Expected: b'INFO', Got: {})".format(info_struct["magic"]))
        exit(-1)

    if info_struct["encoding"] != audio.DSP_ADPCM:
        print("Encoding {} not supported!".format(info_struct["encoding"]))
        exit(-1)

    # print(info_struct)

    # Track Info Reference Table
    if info_struct["track_table_id"] == 0x101 and info_struct["track_table_offset"] != 0xFFFFFFFF:
        track_ref_table_start = header_struct["info_offset"] + 8 + info_struct["track_table_offset"]
        parser.seek(track_ref_table_start)
        track_ref_count = parser.parse_value(ParseType(4))
        track_table = []
        channel_byte_tables = []

        for i in range(track_ref_count):
            track_ref = parser.parse_struct(
                [("id", ParseType(2)),
                ("", ParseType(2, "padding")), # Padding
                ("offset", ParseType(4))]
            )
            # print(track_ref)

            # Track Info
            old = parser.offset
            parser.seek(track_ref_table_start + track_ref["offset"])
            track_table.append(parser.parse_struct(
                [("volume", ParseType(1)),
                ("pan", ParseType(1)),
                ("", ParseType(2, "padding")), # Padding
                ("channel_byte_id", ParseType(2)),
                ("", ParseType(2, "padding")), # Padding
                ("channel_byte_offset", ParseType(4))]
            ))
            # print(track_table[-1])

            # Channel Index Byte Table
            parser.seek(parser.offset - 12 + track_table[-1]["channel_byte_offset"])
            count = parser.parse_value(ParseType(4))
            channel_byte_tables.append(parser.parse_value(ParseType(count, "raw")))
            # print(channel_byte_tables[-1])

            # Restore parser offset
            parser.seek(old)
    
    # Channel Info Reference Table
    if info_struct["channel_table_id"] == 0x101 and info_struct["channel_table_offset"] != 0xFFFFFFFF:
        channel_ref_table_start = header_struct["info_offset"] + 8 + info_struct["channel_table_offset"]
        parser.seek(channel_ref_table_start)
        channel_ref_count = parser.parse_value(ParseType(4))
        channel_info_table = []

        for i in range(channel_ref_count):
            channel_ref = parser.parse_struct(
                [("id", ParseType(2)),
                ("", ParseType(2, "padding")), # Padding
                ("offset", ParseType(4))]
            )
            # print(channel_ref)

            # Channel Info
            old = parser.offset
            parser.seek(channel_ref_table_start + channel_ref["offset"])
            adpcm_ref = parser.parse_struct(
                [("id", ParseType(2)),
                ("", ParseType(2, "padding")), # Padding
                ("offset", ParseType(4))]
            )
            # print(adpcm_ref)

            parser.seek(parser.offset + adpcm_ref["offset"] - 8)
            adpcm_data = None
            match info_struct["encoding"]:
                case audio.DSP_ADPCM: adpcm_data = audio.parse_dspadpcm_info(parser)
                case audio.IMA_ADPCM: adpcm_data = audio.parse_imaadpcm_info(parser)

            channel_info_table.append(adpcm_data)

            # Restore parser offset
            parser.seek(old)

    # Seek Block
    parser.seek(header_struct["seek_offset"])
    seek_struct = parser.parse_struct(
        [("magic", ParseType(4, "raw")),
        ("size", ParseType(4))]
    )

    if seek_struct["magic"] != b'SEEK':
        print("Invalid seek magic! (Expected: b'SEEK', Got: {})".format(seek_struct["magic"]))
        exit(-1)

    # print(seek_struct)
    seek_struct["data"] = parser.parse_value(ParseType(seek_struct["size"] - 8, "raw"))

    # Data Block
    parser.seek(header_struct["data_offset"])
    data_struct = parser.parse_struct(
        [("magic", ParseType(4, "raw")),
        ("size", ParseType(4))]
    )

    if data_struct["magic"] != b'DATA':
        print("Invalid data magic! (Expected: b'DATA', Got: {})".format(data_struct["magic"]))
        exit(-1)

    # print(data_struct)
    data_struct["data"] = parser.parse_value(ParseType(data_struct["size"] - 8, "raw"))

    # Sample Data
    # Decode as DSP ADPCM, for now
    offset = info_struct["samples_offset"]
    adpcm_raw = []
    samples = []
    for c in range(info_struct["channel_count"]):
        samples.append([])
        adpcm_raw.append(bytes())
        for i in range(info_struct["block_count"]):
            start = offset + info_struct["block_size"] * c + i * info_struct["block_size"] * info_struct["channel_count"]
            adpcm_raw[c] += data_struct["data"][start:start + info_struct["block_size"]]
    

    for c in range(info_struct["channel_count"]):
        samples[c] = audio.dspadpcm_to_pcm16(adpcm_raw[c], [audio.ChannelInfo(0, channel_info_table[c])])[0]
        # print(c, ":", len(samples[c]))

    num_samples = min([len(channel) for channel in samples])
    wav_file_name = sys.argv[2] if len(sys.argv) > 2 else sys.argv[1].removesuffix(".bcwav") + ".wav"
    wav_info = audio.WavInfo(info_struct["channel_count"], info_struct["sample_rate"], 2, num_samples, samples)
    audio.write_wav(wav_info, wav_file_name)


if __name__ == "__main__":
    main()