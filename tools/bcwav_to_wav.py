from byteparser import ByteParser, ParseType
import audio
import sys


def main():
    if len(sys.argv) < 2:
        print("usage: bcwave_to_wav.py <input.bcwav> [output.wav]")
        exit(0)

    # Open the input .bcwav file
    input_file = open(sys.argv[1], "rb")
    file_data = input_file.read()
    input_file.close()
    parser = ByteParser(file_data, None)

    # Header
    header_magic = parser.parse_value(ParseType(4, "raw"))
    endianness = parser.parse_value(ParseType(2, "int_le"))
    byte_order = "little" if endianness == 0xFEFF else "big"
    parser.byte_order = byte_order

    if header_magic != b'CWAV':
        print("Invalid header magic! (Expected: b'CWAV', Got: {})".format(header_magic))
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
        ("encoding", ParseType(1)),
        ("loop", ParseType(1)),
        ("", ParseType(2, "padding")), # Padding
        ("sample_rate", ParseType(4)),
        ("loop_start", ParseType(4)),
        ("loop_end", ParseType(4)),
        ("", ParseType(4, "padding")), # Reserved
        ("ref_count", ParseType(4))]
    )

    if info_struct["magic"] != b'INFO':
        print("Invalid info magic! (Expected: b'INFO', Got: {})".format(info_struct["magic"]))
        exit(-1)

    if info_struct["encoding"] == audio.IMA_ADPCM:
        print("Encoding {} not supported!".format(info_struct["encoding"]))
        exit(-1)

    # print(info_struct)

    # Ref Table
    channels = []
    for i in range(info_struct["ref_count"]):
        ref_struct = parser.parse_struct(
            [("id", ParseType(2)),
            ("", ParseType(2, "padding")), # Padding
            ("offset", ParseType(4))]
        )
        # print("Channel {}: {}".format(i, ref_struct))

        old = parser.offset
        parser.seek(parser.offset + ref_struct["offset"] - 4 - 8 * (i + 1))
        channel_struct = parser.parse_struct(
            [("samples_id", ParseType(2)),
            ("", ParseType(2, "padding")), # Padding
            ("samples_offset", ParseType(4)),
            ("adpcm_id", ParseType(2)),
            ("", ParseType(2, "padding")), # Padding
            ("adpcm_offset", ParseType(4)),
            ("", ParseType(4, "padding"))] # Reserved
        )
        # print(" {}".format(channel_struct))

        parser.seek(parser.offset + channel_struct["adpcm_offset"] - 20)
        adpcm_data = None
        match info_struct["encoding"]:
            case audio.DSP_ADPCM: adpcm_data = audio.parse_dspadpcm_info(parser)
            case audio.IMA_ADPCM: adpcm_data = audio.parse_imaadpcm_info(parser)

        channels.append(audio.ChannelInfo(channel_struct["samples_offset"], adpcm_data))
        parser.seek(old)

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
    data = file_data[parser.offset:parser.offset + data_struct["size"] - 8]

    # Convert DSP ADPCM to PCM16
    bytes_per_sample = 2
    match info_struct["encoding"]:
        case audio.PCM8: samples = audio.pcm8_to_pcm8(data, channels); bytes_per_sample = 1
        case audio.PCM16: samples = audio.pcm16_to_pcm16(data, channels, byte_order)
        case audio.DSP_ADPCM: samples = audio.dspadpcm_to_pcm16(data, channels)
        case audio.IMA_ADPCM: samples = audio.imaadpcm_to_pcm16(data, channels) # Currently unimplemented

    # for c in range(len(channels)):
    #     print("Channel {} size: {}".format(c, len(samples[c])))

    # One channel might have more samples because of padding used to align the next channel to 0x20 bytes
    # So taking the minimum of the channels eliminates the samples from padding
    num_samples = min([len(channel) for channel in samples])
    wav_file_name = sys.argv[2] if len(sys.argv) > 2 else sys.argv[1].removesuffix(".bcwav") + ".wav"
    wav_info = audio.WavInfo(len(channels), info_struct["sample_rate"], bytes_per_sample, num_samples, samples)
    audio.write_wav(wav_info, wav_file_name)

if __name__ == "__main__":
    main()