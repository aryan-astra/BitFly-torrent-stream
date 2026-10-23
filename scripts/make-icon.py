"""Generate the deterministic BitFly B monogram; standard library only."""
from pathlib import Path
import struct
import zlib

glyph = ("11110", "11011", "11011", "11110", "11011", "11011", "11110")

def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

def png(size):
    rows = bytearray()
    for y in range(size):
        rows.append(0)
        for x in range(size):
            samples = [0, 0, 0, 0]
            for sy in range(4):
                for sx in range(4):
                    px, py = (x+(sx+.5)/4)/size, (y+(sy+.5)/4)/size
                    dx, dy = max(abs(px-.5)-.29, 0), max(abs(py-.5)-.29, 0)
                    visible = dx*dx + dy*dy <= .18*.18
                    gx, gy = int((px-.29)/.084), int((py-.206)/.084)
                    ink = .29 <= px < .71 and .206 <= py < .794 and 0 <= gx < 5 and 0 <= gy < 7 and glyph[gy][gx] == "1"
                    color = (240,243,240) if ink else (31,36,33)
                    for i in range(3): samples[i] += color[i] if visible else 0
                    samples[3] += 255 if visible else 0
            rows.extend(round(v/16) for v in samples)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size,size,8,6,0,0,0)) + chunk(b"IDAT", zlib.compress(bytes(rows),9)) + chunk(b"IEND", b"")

sizes = [16, 32, 48, 64, 128, 256]
images = [png(size) for size in sizes]
directory = bytearray(struct.pack("<HHH",0,1,len(sizes)))
offset = 6 + 16*len(sizes)
for size, data in zip(sizes, images):
    directory.extend(struct.pack("<BBBBHHII",size%256,size%256,0,0,1,32,len(data),offset))
    offset += len(data)
target = Path(__file__).resolve().parent.parent / "packaging/BitFly.ico"
target.write_bytes(directory + b"".join(images))
print(target)
