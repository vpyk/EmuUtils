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

# hfe2rdi v. 1.0


import sys
import os


class FmDecoder:

    def __init__(self, bit_len):
        self.step = bit_len // 2
        self.is_data = False
        self.new_track()

    def new_track(self):
        self.track = bytearray()
        self.bit_ofs = 0
        self.data = 0
        self.sync = 0
        self.bit_num = 0

    def decode(self, code):
        for b in code:
            b >>= (self.step - 1)
            for i in range(8 // self.step):
                bit = b & 1
                if self.is_data:
                    self.data = (self.data << 1) & 0xFF
                    self.data |= bit
                    self.bit_num += 1
                else:
                    self.sync = (self.sync << 1) & 0xFF
                    self.sync |= bit
                if self.is_data and self.data == 0x06 and self.sync == 0xFD:
                    self.bit_num = 8
                elif not self.is_data and self.sync == 0x06 and self.data == 0xFD:
                    self.data, self.sync = self.sync, self.data
                    self.is_data = True
                    self.bit_num = 8

                if self.bit_num == 8:
                    self._dump()

                b >>= self.step
                self.is_data = not self.is_data


    def get_track(self):
        self.track = self.track[:3125]
        if len(self.track) < 3125:
            self.track += bytearray((0,) * (3125 - len(self.track)))
        return self.track

    def _dump(self):
        self.track.append(self.data)
        self.bit_num = 0


def main():

    print('hfe2rdi v. 1.0 (c) Viktor Pykhonin, 2023\n')

    if len(sys.argv) != 2:
        print('Usage: hfe2rdi <file.hfe>')
        exit()

    src_file = sys.argv[1]
    dst_file = os.path.splitext(sys.argv[1])[0] + '.rdi'

    print(f'Converting {src_file} -> {dst_file} ...')

    with open(src_file, 'rb') as f:
        hfe = f.read()

    if hfe[:8] != b'HXCPICFE':
        print(f'{src_file}: Not a HFE file!')
        exit(1)

    rev = hfe[8]
    if rev != 0:
        print(f'{src_file}: Unknown file format!')
        exit(1)

    tracks = hfe[9]
    sides = hfe[10]

    if tracks != 80 or sides != 2:
        print(f'{src_file}: Not an RDI file!')
        exit(1)

    bitrate = hfe[12] + (hfe[13] << 8)

    if bitrate == 125:
        bit_len = 2
    elif bitrate == 250:
        bit_len = 4
    else:
        print(f'{src_file}: Unsupported bitrate {bitrate * 1000}!')
        exit(1)

    lut_offset = (hfe[18] + (hfe[19] << 8)) * 512

    out = bytearray()

    decoder = FmDecoder(bit_len)

    with open(dst_file, 'wb') as f:
        for track in range(80):
            track_offs = (hfe[lut_offset + track * 4] + (hfe[lut_offset + track * 4 + 1] << 8)) * 512
            track_len = hfe[lut_offset + track * 4 + 2] + (hfe[lut_offset + track * 4 + 3] << 8)
            if track_len < 6250 * bit_len:
                print(f'{src_file}: Invalid track {track} length!')
                exit(1)

            for side in range(2):
                decoder.new_track()
                for b in range(3125):
                    offs = track_offs + b * bit_len // 256 * 512 + (1 - side) * 256 + b * bit_len % 256
                    code = hfe[offs : offs + bit_len]
                    decoder.decode(code)
                f.write(decoder.get_track())

    print('Done!')


if __name__ == "__main__":
    main()
