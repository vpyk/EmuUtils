#  Rdi HFE tools
#  (c) Viktor Pykhonin <pyk@mail.ru>, 2023
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

# https://github.com/vpyk/EmuUtils

# rdi2hfe v. 1.01


import sys
import os


def fm_encode_block(src, sync):
    fm = bytearray()

    for b, s in zip(src, sync):
        w = 0
        for i in range(8):
            w <<= 2
            if b & 1:
                w |= 3
            else:
                w |= 1
            b = b >> 1
        if s:
            w &= 0xEFFF
        fm.append(w & 0xFF)
        fm.append(w >> 8)

    if len(fm) < 256:
        fm += bytearray((0x55,) * (256 - len(fm)))

    return fm


def find_syncrobytes(track, len_fix = False):
    s_bytes = list((False,) * 3125)

    n_sect = 0
    cur_pos = 0
    while cur_pos < 3125 and n_sect < 5:
        # find first 06 within next 100 bytes
        pos = track.find(b'\x06\x06\x06', cur_pos, cur_pos + 100)
        if pos == -1:
            break
        s_bytes[pos] = True
        cur_pos = pos + 1

        # find subsequient 06s
        while cur_pos < 3125 and track[cur_pos] == 0x06:
            s_bytes[cur_pos] = True
            cur_pos += 1

        # find EA D3 within next 10 bytes
        pos = track.find(b'\xEA\xD3', cur_pos, cur_pos + 10)
        if pos == -1:
            break
        cur_pos = pos + 7

        # find first 06 within next 20 bytes
        pos = track.find(b'\x06\x06\x06', cur_pos, cur_pos + 20)
        if pos == -1:
            break
        s_bytes[pos] = True
        cur_pos = pos + 1

        # find subsequient 06s
        while cur_pos < 3125 and track[cur_pos] == 0x06:
            s_bytes[cur_pos] = True
            cur_pos += 1

        # find DD F3 within next 20 bytes
        pos = track.find(b'\xDD\xF3', cur_pos, cur_pos + 20)
        if pos == -1:
            break
        cur_pos = pos + 2
        n_sect += 1
        if not len_fix:
            cur_pos += 530
        else:
            size = int.from_bytes(track[cur_pos : cur_pos + 2], 'little')
            cur_pos = cur_pos + size + 30

    return s_bytes, n_sect


def main():

    print('rdi2hfe v. 1.01 (c) Viktor Pykhonin, 2023\n')

    if len(sys.argv) != 2:
        print('Usage: rdi2hfe <file.rdi>')
        exit()

    src_file = sys.argv[1]
    dst_file = os.path.splitext(sys.argv[1])[0] + '.hfe'

    print(f'Converting {src_file} -> {dst_file} ...')

    with open(src_file, 'rb') as f:
        rdi = f.read()
    
    header = bytearray()
                         
    header += b'HXCPICFE'                      # signature
    header += int.to_bytes(0, 1, 'little')     # revision
    header += int.to_bytes(80, 1, 'little')    # tracks
    header += int.to_bytes(2, 1, 'little')     # sides
    header += int.to_bytes(2, 1, 'little')     # FM encoding
    header += int.to_bytes(125, 2, 'little')   # bitrate 125000
    header += int.to_bytes(300, 2, 'little')   # RPM
    header += int.to_bytes(7, 1, 'little')     # interface (GENERIC SHUGGART DD)
    header += int.to_bytes(1, 1, 'little')     # dnu?
    header += int.to_bytes(1, 2, 'little')     # LUT offset in 512 blocks: 0x200

    with open(dst_file, 'wb') as f:
        success = True
        
        f.write(header)
        f.write(bytearray((0xFF,) * (512 - len(header))))

        ofs = 2
        for i in range(80):
            f.write(int.to_bytes(ofs, 2, 'little'))
            f.write(int.to_bytes(12504, 2, 'little'))
            ofs += 25
        f.write(bytearray((0xFF,) * (512 - 80 * 4)))
    
        for track in range(80):
            side0 = rdi[track * 6250 : track * 6250 + 3125]
            side1 = rdi[track * 6250 + 3125 : track * 6250 + 6250]
    
            sync0, n_sect0 = find_syncrobytes(side0)
            if n_sect0 != 5:
                print(f'Track {track + 1} side 0, trying different method...')
                sync0, n_sect0 = find_syncrobytes(side0, True)
            if n_sect0 != 5:
                print(f'Warning: track {track + 1} side 0 contains {n_sect0} sector(s) instead of 5!')
                success = False
    
            sync1, n_sect1 = find_syncrobytes(side1)
            if n_sect1 != 5:
                print(f'Track {track + 1} side 1, trying different method...')
                sync0, n_sect1 = find_syncrobytes(side1, True)
            if n_sect1 != 5:
                print(f'Warning: track {track + 1} side 1 contains {n_sect1} sector(s) instead of 5!')
                success = False

            bytes_left = 3125
            offset = 0
    
            while bytes_left:
                chunk_s0 = side0[offset : offset + min(128, bytes_left)]
                chunk_s1 = side1[offset : offset + min(128, bytes_left)]
    
                sync_s0 = sync0[offset : offset + min(128, bytes_left)]
                sync_s1 = sync1[offset : offset + min(128, bytes_left)]
    
                fm = fm_encode_block(chunk_s1, sync_s1)
                f.write(fm)

                fm = fm_encode_block(chunk_s0, sync_s0)
                f.write(fm)
    
                offset += 128
                bytes_left -= min(128, bytes_left)

        print()
        if success:
            print('Done!')
        else:
            print('Converted with warnings, the resulting file may be unreadable!')


if __name__ == "__main__":
    main()
