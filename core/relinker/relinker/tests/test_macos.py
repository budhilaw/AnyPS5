"""Convert the fixture ELF to a Mach-O executable and run it where an x86-64 macOS runtime exists."""

from pathlib import Path
import platform
import struct
import subprocess
import sys
import tempfile



def fixture():
    """A PIE ELF with separate code and data pages (macOS forbids writable code): the entry jumps
    through a relocated pointer in the data page to a function returning 42."""
    image = bytearray(0x2000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, 0x200, 64, 0, 0, 64, 56, 3, 64, 0, 0)
    image[0x200:0x206] = b"\xff\x25" + struct.pack("<i", 0x1300 - 0x206)  # jmp *pointer(%rip)
    image[0x210:0x216] = b"\xb8\x2a\x00\x00\x00\xc3"                 # mov $42, %eax; ret
    struct.pack_into("<QQq", image, 0x1700, 0x1300, 8, 0x210)             # R_X86_64_RELATIVE at 0x1300
    tags = [(5, 0x1600), (10, 1), (6, 0x1620), (11, 24), (7, 0x1700), (8, 24), (9, 24), (0, 0)]
    struct.pack_into("<IIQQQQQQ", image, 64, 1, 5, 0, 0, 0, 0x1000, 0x1000, 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 120, 1, 6, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 176, 2, 6, 0x1400, 0x1400, 0x1400, len(tags) * 16, len(tags) * 16, 8)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x1400 + index * 16, *tag)
    return image


def can_run_x86_64_macho():
    if platform.system() != "Darwin":
        return False
    probe = subprocess.run(["arch", "-x86_64", "/usr/bin/true"], capture_output=True)
    return probe.returncode == 0


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-macho-") as directory:
        work = Path(directory)
        source = work / "fixture.elf"
        output = work / "fixture"
        source.write_bytes(fixture())
        result = subprocess.run([str(relinker), "--macos", str(source), str(output)], capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, (result.stdout, result.stderr)
        image = output.read_bytes()
        assert struct.unpack_from("<I", image, 0)[0] == 0xFEEDFACF, "not a 64-bit Mach-O"
        assert struct.unpack_from("<I", image, 12)[0] == 2, "not an executable"
        if can_run_x86_64_macho():
            executed = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            assert executed.returncode == 42, (executed.returncode, executed.stdout, executed.stderr)
            print("macOS target tests passed (executed under the x86-64 runtime)")
        else:
            print("macOS target tests passed (conversion only)")
        # Guest TLS access without a PT_TLS is refused, as on Windows.
        data = fixture()
        data[0x210:0x21a] = bytes.fromhex("64 48 8b 04 25 00 00 00 00 c3")
        struct.pack_into("<i", data, 0x202, 0x1300 - 0x206)
        source.write_bytes(data)
        result = subprocess.run([str(relinker), "--macos", str(source), str(output)], capture_output=True, text=True, timeout=20)
        assert result.returncode == 2 and "Guest TLS access without a usable PT_TLS" in result.stderr, result.stderr


if __name__ == "__main__":
    main()
