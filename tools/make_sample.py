#!/usr/bin/env python3
"""生成确定性的 BES 黄金测试数据（8 KB）。"""
import sys

def main(path):
    data = bytearray(8192)
    for i in range(len(data)):
        data[i] = (i * 37 + (i >> 3) * 11 + 0x5A) & 0xFF
    # 固定帧头与重复帧，便于检查帧长/独特码
    for off in (0, 256, 512, 1024, 2048, 4096):
        data[off:off+4] = b'\x1A\xCF\xFC\x1D'
    with open(path, 'wb') as f:
        f.write(data)
    print('wrote', path, len(data), 'bytes')

if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'build/golden_sample.dat')
