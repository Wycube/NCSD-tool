# Some utility functions for working with the audio formats used in bcwav and bcstm and some constants
from dataclasses import dataclass
import byteparser


# TODO: Use the context for the initial history samples for DSP ADPCM

PCM8 = 0
PCM16 = 1
DSP_ADPCM = 2
IMA_ADPCM = 3

class ChannelInfo:
    def __init__(self, sample_offset, adpcm_info):
        self.sample_offset = sample_offset
        self.adpcm_info = adpcm_info

def parse_dspadpcm_info(parser : byteparser.ByteParser):
    adpcm_struct = parser.parse_struct(
        [("coefficients", byteparser.ParseType(2, "int", True, 16)),
        ("context", byteparser.ParseType(6)),
        ("loop_context", byteparser.ParseType(6)),
        ("", byteparser.ParseType(2, "padding"))] # Padding
    )
    # print(adpcm_struct)

    return adpcm_struct

def parse_imaadpcm_info(parser : byteparser.ByteParser):
    adpcm_struct = parser.parse_struct(
        [("context", byteparser.ParseType(4)),
        ("loop_context", byteparser.ParseType(4))]
    )
    # print(" {}".format(adpcm_struct))

    return adpcm_struct

def clamp(value):
    if value < -32768:
        return -32768
    if value > 32767:
        return 32767

    return value

def pcm8_to_pcm8(data, channels):
    pass

def pcm16_to_pcm16(data, channels, endianness):
    samples = []

    for c in range(len(channels)):
        channel_start = channels[c].sample_offset
        
        if c == len(channels) - 1:
            channel_end = len(data)
        else:
            channel_end = channels[c + 1].sample_offset
        
        # print("Start: {}  End: {}".format(channel_start / 2, channel_end / 2))
        # print("Samples: {}".format(channel_end // 2 - channel_start // 2))

        samples.append([])

        for i in range(channel_start // 2, channel_end // 2):
            samples[c].append(int.from_bytes(data[i * 2:(i + 1) * 2], endianness, signed=True))

    return samples

# Code adapted from https://wiki.axiodl.com/w/index.php?title=DSP_(File_Format)
def dspadpcm_to_pcm16(data, channels):
    samples = []

    for c in range(len(channels)):
        channel_start = channels[c].sample_offset
        
        if c == len(channels) - 1:
            channel_end = len(data)
        else:
            channel_end = channels[c + 1].sample_offset
        
        # print("Start: {}  End: {}".format(channel_start / 8, channel_end / 8))
        # print("Samples: {}".format((channel_end // 8 - channel_start // 8) * 14))

        hist1 = 0
        hist2 = 0
        samples.append([])

        for i in range(channel_start // 8, channel_end // 8):
            offset = i * 8
            header = data[offset]
            scale = 1 << (header & 0xF)
            coef_index = header >> 4
            coef1 = channels[c].adpcm_info["coefficients"][coef_index * 2]
            coef2 = channels[c].adpcm_info["coefficients"][coef_index * 2 + 1]

            for b in range(1, 8):
                byte = data[offset + b]
                u4_to_s4 = [0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1]

                for n in range(2):
                    nibble = u4_to_s4[(byte >> (4 - n * 4)) & 0xF]
                    sample = clamp(((nibble * scale) << 11) + 1024 + ((coef1 * hist1) + (coef2 * hist2)) >> 11)

                    hist2 = hist1
                    hist1 = sample
                    samples[c].append(sample)

    return samples

def imaadpcm_to_pcm16(data, channels):
    print("IMA ADPCM is not supported!")
    exit(-1)

@dataclass
class WavInfo:
    num_channels: int
    sample_rate: int
    bytes_per_sample: int
    num_samples: int
    sample_data: bytes

def write_wav(wav_info: WavInfo, path: str):
    sample_data_size = wav_info.num_samples * wav_info.num_channels * wav_info.bytes_per_sample
    
    # WAVE header
    wav_data = bytes()
    wav_data += bytes("WAVE", "ascii")

    # fmt header
    wav_data += bytes("fmt ", "ascii")
    wav_data += (16).to_bytes(4, "little")
    wav_data += (1).to_bytes(2, "little")
    wav_data += wav_info.num_channels.to_bytes(2, "little")
    wav_data += wav_info.sample_rate.to_bytes(4, "little")
    wav_data += (wav_info.sample_rate * wav_info.num_channels * wav_info.bytes_per_sample).to_bytes(4, "little")
    wav_data += (wav_info.num_channels * wav_info.bytes_per_sample).to_bytes(2, "little")
    wav_data += (wav_info.bytes_per_sample * 8).to_bytes(2, "little")

    # data header
    wav_data += bytes("data", "ascii")
    wav_data += sample_data_size.to_bytes(4, "little")

    # RIFF header
    out_file = open(path, "wb")
    out_file.write(bytes("RIFF", "ascii"))
    out_file.write((len(wav_data) + sample_data_size).to_bytes(4, "little"))
    out_file.write(wav_data)

    # sample data
    for i in range(wav_info.num_samples):
        for c in range(wav_info.num_channels):
            out_file.write(wav_info.sample_data[c][i].to_bytes(wav_info.bytes_per_sample, "little", signed=True))