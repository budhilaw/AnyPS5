"""Validate fake-signed SELF unwrapping: segment placement, alias coverage and refusals."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_optional_plt import fixture


def wrap(elf, entries, encrypted=False, compressed=False):
    """Build a SELF around the ELF header/program headers of `elf` with the given data entries."""
    phnum = struct.unpack_from("<H", elf, 0x38)[0]
    headers = bytes(elf[:0x40 + phnum * 0x38])
    table = []
    data = bytearray()
    payload_base = 0x20 + len(entries) * 2 * 0x20 + len(headers)
    payload_base = (payload_base + 15) & ~15
    for index, (ph, offset, size) in enumerate(entries):
        digest_offset = payload_base + len(data)
        data += bytes(32)
        props = 0x4 | 0x10000 | ((2 * index + 1) << 20)
        table.append(struct.pack("<QQQQ", props, digest_offset, 32, 32))
        payload_offset = payload_base + len(data)
        data += bytes(elf[offset:offset + size])
        props = 0x4 | 0x800 | (2 << 12) | (ph << 20)
        if encrypted:
            props |= 0x2
        if compressed:
            props |= 0x8
        table.append(struct.pack("<QQQQ", props, payload_offset, size, size))
    body = b"".join(table) + headers
    body += bytes(payload_base - 0x20 - len(body)) + data
    header = struct.pack("<IBBBBIHHQHHI", 0x1D3D154F, 0, 1, 1, 0x12, 0x101, 0x560, 0x510, 0x20 + len(body), len(table), 0x22, 0)
    return header + body


def main():
    relinker = Path(sys.argv[1]).resolve()
    elf = fixture()
    # Loadable segments of the fixture: text at 0x200 and data at 0x300 (see test_optional_plt).
    phs = [struct.unpack_from("<IIQQQQQQ", elf, 0x40 + i * 0x38) for i in range(struct.unpack_from("<H", elf, 0x38)[0])]
    loads = [(i, p[2], p[5]) for i, p in enumerate(phs) if p[0] == 1]
    assert loads, phs
    with tempfile.TemporaryDirectory(prefix="anyps5-self-") as directory:
        source = Path(directory) / "eboot.self"
        output = Path(directory) / "eboot.elf"
        source.write_bytes(wrap(elf, loads))
        result = subprocess.run([str(relinker), "--unself", str(source), str(output)], capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert "unwrapped %d segments" % len(loads) in result.stdout, result.stdout
        unwrapped = output.read_bytes()
        assert unwrapped[:4] == b"\x7fELF" and unwrapped[:0x40 + len(phs) * 0x38] == bytes(elf[:0x40 + len(phs) * 0x38])
        for _, offset, size in loads:
            assert unwrapped[offset:offset + size] == bytes(elf[offset:offset + size]), offset
        # The unwrapped ELF goes through the normal pipeline too.
        relinked = Path(directory) / "eboot.exe"
        result = subprocess.run([str(relinker), "--windows", str(source), str(relinked)], capture_output=True, text=True, timeout=20)
        assert result.returncode == 0 and relinked.exists(), (result.stdout, result.stderr)
        for name, kwargs, error in (("encrypted", {"encrypted": True}, "encrypted"), ("compressed", {"compressed": True}, "compressed")):
            source.write_bytes(wrap(elf, loads, **kwargs))
            result = subprocess.run([str(relinker), "--unself", str(source), str(output)], capture_output=True, text=True, timeout=20)
            assert result.returncode == 2 and error in result.stderr, (name, result.stderr)
        result = subprocess.run([str(relinker), "--unself", str(Path(directory) / "plain.elf"), str(output)], capture_output=True, text=True, timeout=20)
        assert result.returncode != 0, result
    print("SELF unwrap integration tests passed")


if __name__ == "__main__":
    main()
