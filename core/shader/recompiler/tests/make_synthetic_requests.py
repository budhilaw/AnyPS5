import base64
import struct
import sys

CACHE_HEADER = 'anyps5-shader-requests 1'
REQUEST_SIGNATURE = 0x41505335
REQUEST_VERSION = 2

CODE_ADDRESS = 0x20000
TABLE_ADDRESS = 0x10000
LIGHT_STRIDE = 0x18c
PROJECTOR_OFFSET = 0xb8
LIGHT_COUNT = 4

SAMPLE_LZ_2D = 0xf09c0f08
SAMPLE_LZ_CUBE = 0xf09c0f18
SAMPLE_L_2D = 0xf0900f08
SAMPLE_L_3D = 0xf0900f10
SAMPLE_L_2D_ARRAY = 0xf0900f28

NULL_TEXTURE = [0] * 8
TEXTURE_2D = [0x00000020, 56 << 20, 0, 0x90000fac, 0, 0, 0, 0]
TEXTURE_CUBE = [0x00000010, 56 << 20, 0, 0xb0000fac, 0, 0, 0, 0]
TEXTURE_3D = [0x00000040, 56 << 20, 0, 0xa0000fac, 0, 0, 0, 0]
OUTPUT_BUFFER = [0x30000, 0, 16, 0x00000fac]


class Writer:
    def __init__(self):
        self.data = bytearray()

    def write_u8(self, value):
        self.data += struct.pack('<B', value)

    def write_bool(self, value):
        self.write_u8(1 if value else 0)

    def write_u32(self, value):
        self.data += struct.pack('<I', value & 0xffffffff)

    def write_u64(self, value):
        self.data += struct.pack('<Q', value)

    def write_bytes(self, value):
        self.write_u64(len(value))
        self.data += bytes(value)

    def write_u32_span(self, values):
        self.write_u64(len(values))
        for value in values:
            self.write_u32(value)


def write_shader_binary(writer, code):
    writer.write_u8(0)
    writer.write_u64(CODE_ADDRESS)
    writer.write_u32_span(code)
    writer.write_u64(0)
    writer.write_bytes(b'')


def write_guest_context(writer, user_data, memory):
    writer.write_u32(32)
    writer.write_u32(0)
    writer.write_u32_span(user_data)
    writer.write_bool(True)
    for value in (32, 1, 1):
        writer.write_u32(value)
    writer.write_u32(0)
    for value in (False, False, False):
        writer.write_bool(value)
    writer.write_bool(False)
    writer.write_u32(1)
    writer.write_bool(False)
    writer.write_bool(False)
    writer.write_u64(len(memory))
    for address, data in memory:
        writer.write_u64(address)
        writer.write_bytes(data)


def write_spirv_target(writer):
    writer.write_u32(0x00401000)
    writer.write_u32(0x00010300)
    writer.write_u32(32)
    writer.write_u32(0)
    writer.write_u32_span([])
    writer.write_u64(0)
    writer.write_bool(False)
    for value in (1024, 1024, 64):
        writer.write_u32(value)
    writer.write_u32(1024)
    writer.write_u32(65536)
    writer.write_bool(False)
    writer.write_bool(False)
    writer.write_u32(0xffffffff)


def write_binding_layout(writer):
    for value in (0, 0, 0, 128):
        writer.write_u32(value)


def compute_request(code, user_data, memory):
    writer = Writer()
    writer.write_u32(REQUEST_SIGNATURE)
    writer.write_u32(REQUEST_VERSION)
    write_shader_binary(writer, code)
    write_guest_context(writer, user_data, memory)
    write_spirv_target(writer)
    write_binding_layout(writer)
    writer.write_bool(False)
    writer.write_bool(True)
    return base64.b64encode(bytes(writer.data)).decode('ascii')


def sample_texture(sample, texture):
    user_data = texture + [0, 0, 0, 0] + OUTPUT_BUFFER
    code = [0x7e0002f0, 0x7e0202f0, 0x7e0402f2, 0x7e0602f4, sample, 0x00400400, 0xe0780000, 0x80030400, 0xbf810000]
    return compute_request(code, user_data, [])


def sample_light_table(sample, projectors):
    table = bytearray(LIGHT_COUNT * LIGHT_STRIDE)
    for light, projector in enumerate(projectors):
        struct.pack_into('<8I', table, light * LIGHT_STRIDE + PROJECTOR_OFFSET, *projector)
    user_data = [TABLE_ADDRESS, 0, LIGHT_COUNT * LIGHT_STRIDE, 0x00000fac, 0, 0, 0, 0, 0, 0, 0, 0] + OUTPUT_BUFFER
    code = [0x9309ff08, LIGHT_STRIDE, 0xf4200600, 0x12000000, 0xf42c0400, 0x12000000 | PROJECTOR_OFFSET,
            0x7e000218, 0x7e0202f0, 0x7e0402f2, sample, 0x00240400, 0xe0780000, 0x80030400, 0xbf810000]
    return compute_request(code, user_data, [(TABLE_ADDRESS, table)])


def main():
    if len(sys.argv) != 2:
        sys.exit('usage: make_synthetic_requests.py <SyntheticRequests.txt>')
    requests = [
        sample_texture(SAMPLE_L_2D, TEXTURE_2D),
        sample_texture(SAMPLE_L_2D_ARRAY, TEXTURE_2D),
        sample_texture(SAMPLE_L_3D, TEXTURE_2D),
        sample_texture(SAMPLE_LZ_2D, TEXTURE_CUBE),
        sample_light_table(SAMPLE_LZ_2D, [TEXTURE_CUBE, TEXTURE_CUBE, TEXTURE_2D, TEXTURE_CUBE]),
        sample_light_table(SAMPLE_LZ_CUBE, [TEXTURE_CUBE, TEXTURE_CUBE, TEXTURE_2D, TEXTURE_CUBE]),
        sample_light_table(SAMPLE_LZ_2D, [TEXTURE_3D, TEXTURE_CUBE, NULL_TEXTURE, TEXTURE_CUBE]),
    ]
    with open(sys.argv[1], 'w', newline='\n') as output:
        output.write(CACHE_HEADER + '\n')
        for request in requests:
            output.write(request + '\n')


if __name__ == '__main__':
    main()
