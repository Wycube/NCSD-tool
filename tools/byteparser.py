# A utility class used for parsing binary file formats
from dataclasses import dataclass


@dataclass
class ParseType:
    size: int
    type: str = "int"
    signed: bool = False
    length: int = 1

class ByteParser:
    def __init__(self, data, byte_order):
        self.data = data
        self.byte_order = byte_order
        self.offset = 0

    def seek(self, offset):
        self.offset = offset

    def from_bytes(self, raw, type):
        match type.type:
            case "int":
                return int.from_bytes(raw, self.byte_order, signed=type.signed)
            case "int_le":
                return int.from_bytes(raw, "little", signed=type.signed)
            case "int_be":
                return int.from_bytes(raw, "big", signed=type.signed)
            case "raw":
                return raw
            case "padding":
                return None

    def parse_value(self, type):
        raw = self.data[self.offset:self.offset + type.size * type.length]
        self.offset += type.size * type.length

        if type.length > 1:
            return [self.from_bytes(raw[i * type.size:(i + 1) * type.size], type) for i in range(type.length)]
        else:
            return self.from_bytes(raw, type)

    def parse_struct(self, members):
        struct = {}

        for member in members:
            value = self.parse_value(member[1])

            if value is not None:
                struct[member[0]] = value

        return struct