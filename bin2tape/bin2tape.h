/*
 *  bin2tape v. 1.0
 *  (c) Viktor Pykhonin <pyk@mail.ru>, 2021-2023
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// https://github.com/vpyk/EmuUtils


#ifndef BIN2TAPE_H
#define BIN2TAPE_H

#include <cstdint>

#include <vector>
#include <string>

#define VERSION "1.03"


static const uint8_t casSignature[8] = {0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74};
static const uint8_t lvtSignature[9] = {0x4C, 0x56, 0x4F, 0x56, 0x2F, 0x32, 0x2E, 0x30, 0x2F}; // "LVOV/2.0/"


enum TapeFileFormat {
    TFF_RK,
    TFF_RKP,
    TFF_RKM,
    TFF_RKU,
    TFF_RK4,
    TFF_RKS,
    TFF_RKO,
    TFF_BRU,
    TFF_CAS,
    TFF_LVT
};


const char* txtFormats[] = {"RK compatible", "RKP (RK compatible)", "RKM", "RKU", "RK4 (RK compatible)", "RKS", "RKO", "BRU", "CAS", "LVT"};


#pragma pack(push, 1)

struct RkHeader {
    uint8_t loadAddrHi;
    uint8_t loadAddrLo;
    uint8_t endAddrHi;
    uint8_t endAddrLo;
};


struct RkFooter {
    uint8_t nullByte1;
    uint8_t nullByte2;
    uint8_t syncByte;
    uint8_t csHi;
    uint8_t csLo;
};


struct RkpFooter {
    uint8_t nullByte;
    uint8_t syncByte;
    uint8_t csHi;
    uint8_t csLo;
};


struct Rk4Footer {
    uint8_t nullBytes[16];
    uint8_t syncByte;
    uint8_t csHi1;
    uint8_t csLo1;
    uint8_t csHi2;
    uint8_t csLo2;
};


struct RkmFooter {
    uint8_t csHi;
    uint8_t csLo;
};


struct RksHeader {
    uint8_t loadAddrLo;
    uint8_t loadAddrHi;
    uint8_t endAddrLo;
    uint8_t endAddrHi;
};


struct RksFooter {
    uint8_t csLo;
    uint8_t csHi;
};


struct BruHeader {
    uint8_t name[8];
    uint8_t loadAddrLo;
    uint8_t loadAddrHi;
    uint8_t lenLo;
    uint8_t lenHi;
    uint8_t attr;
    uint8_t ff[3];
};


struct RkoHeader {
    uint8_t name[8];
    uint8_t nullBytes[64];
    uint8_t syncByte;
    uint8_t loadAddrLo;
    uint8_t loadAddrHi;
    uint8_t lenHi;
    uint8_t lenLo;
    BruHeader bruHeader;
};


struct RkoFooter {
    uint8_t padding[15];
    uint8_t syncByte;
    uint8_t csHi;
    uint8_t csLo;
};


struct CasHeader {
    uint8_t casSignature1[8];
    uint8_t d0[10];
    uint8_t name[6];
    uint8_t padding[8]; // required by Partner etc/
    uint8_t casSignature2[8];
    uint8_t loadAddrLo;
    uint8_t loadAddrHi;
    uint8_t endAddrLo;
    uint8_t endAddrHi;
    uint8_t runAddrLo;
    uint8_t runAddrHi;
};


struct LvtHeader {
    uint8_t lvtSignature[9];
    uint8_t d0;
    uint8_t name[6];
    uint8_t loadAddrLo;
    uint8_t loadAddrHi;
    uint8_t endAddrLo;
    uint8_t endAddrHi;
    uint8_t runAddrLo;
    uint8_t runAddrHi;
};

#pragma pack(pop)


union FileHeader {
    RkHeader rkHeader;
    RksHeader rksHeader;
    BruHeader bruHeader;
    RkoHeader rkoHeader;
    CasHeader casHeader;
    LvtHeader lvtHeader;
};


union FileFooter {
    RkFooter rkFooter;
    RkpFooter rkpFooter;
    Rk4Footer rk4Footer;
    RkmFooter rkmFooter;
    RksFooter rksFooter;
    RkoFooter rkoFooter;
};

#endif // BIN2TAPE_H
