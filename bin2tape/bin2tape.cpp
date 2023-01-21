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


#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>

#include <cstring>
#include <assert.h>

#include "bin2tape.h"


using namespace std;

void usage(string& moduleName)
{
            cout << "Usage: " << moduleName << " [options] input_file.bin [output_file]" << endl << endl <<
            "options are:" << endl << endl <<
            "  -t format" << endl <<
            "    output file format, available formats are:" << endl << endl <<
            "      rk  (RK86 and clones, default)" << endl <<
            "      rkr (Radio-86RK)," << endl <<
            "      rkp (Partner)" << endl <<
            "      rka (Apogey)" << endl <<
            "      rkm (Mikrosha)" << endl <<
            "      rk8 (Mikro-80)" << endl <<
            "      rku (UT-88)" << endl <<
            "      rk4 (Electronika KR-04)" << endl <<
            "      rkl (Palmira)" << endl <<
            "      rke (Eureka)" << endl <<
            "      rks (Specialist w/o name)" << endl <<
            "      rko (Orion, tape)" << endl <<
            "      bru, ord (Orion, disk)" << endl <<
            "      lvt (Lvov)" << endl <<
            "      cas (Partner, Apogey, Pk8000, Lvov and others)" << endl << endl <<
            "  -a addr" << endl <<
            "    load address (hex), default = 0000 (0100 for .com input files)" << endl << endl <<
            "  -r run_addr" << endl <<
            "    run address for cas and lvt formats (hex), default = load address" << endl << endl <<
            "  -n filename" << endl <<
            "    internal file name (for BRU, RKO, RKS, CAS), default is based on input file name" << endl << endl <<
            "  -n-" << endl <<
            "    no internal file name" << endl << endl <<
            "output_file - output file name, default is based on input file name" << endl;
}


bool loadFile(string& fileName, vector<uint8_t>& body)
{
    ifstream f(fileName, ofstream::binary);
    if (f.fail())
        return false;

    f.seekg(0, ios_base::end);

    int size = f.tellg();

    if (size > 0x10000) {
        f.close();
        return false;
    }

    f.seekg(ios_base::beg);
    body.resize(size);
    f.read((char*)(body.data()), size);
    if (f.fail())
        return false;

    f.close();

    return true;
}


const uint8_t* makeIntName(string baseName, int len)
{
    // cut off the extention if any
    baseName = baseName.substr(0, baseName.find_first_of('.'));

    static uint8_t intName[8];

    int i = 0;
    while (i < len) {
        char ch = baseName[i];

        if (!ch)
            break;

        if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == ' ')
            ch = ch >= 'a' && ch <= 'z' ? ch - 0x20 : ch;
        else
            ch = '-';

        intName[i] = uint8_t(ch);

        ++i;
    }

    while (i < len)
        intName[i++] = 0x20;

    return intName;
}


uint16_t addToRkCs(uint16_t baseCs, const uint8_t* data, int len, bool lastChunk = false)
{
    if (lastChunk)
        --len;

    for (int i = 0; i < len; i++) {
        baseCs += data[i];
        baseCs += (data[i] << 8);
    }

    if (lastChunk)
        baseCs = (baseCs & 0xff00) | ((baseCs + data[len]) & 0xff);

    return baseCs;
}


uint16_t calcRkCs(vector<uint8_t>& data)
{
    return addToRkCs(0, data.data(), data.size(), true);
}


uint16_t calcRkmCs(vector<uint8_t>& data)
{
    uint16_t cs = 0;
    for (uint16_t i = 0; i < data.size(); i++)
        cs ^= i & 1 ? data.data()[i] << 8 : data.data()[i];
    return cs;
}


uint16_t calcRkuCs(vector<uint8_t>& data)
{
    uint16_t cs = 0;
    for (uint16_t i = 0; i < data.size(); i++)
        cs += data.data()[i];
    return cs;
}


bool convert(vector<uint8_t> body, TapeFileFormat format, uint16_t loadAddr, uint16_t startAddr, string& outputFile, const uint8_t* intFileName)
{
    int endAddr = loadAddr + body.size() - 1;

    int headerSize = 0;
    int footerSize = 0;

    FileHeader header;
    FileFooter footer;

    const char* headerPtr = (const char*)&header;
    const char* footerPtr = (const char*)&footer;

    uint16_t cs;
    int paddingSize;

    switch (format) {
    case TFF_RK:
    case TFF_RKP:
    case TFF_RKM:
    case TFF_RKU:
    case TFF_RK4:
        headerSize = sizeof(RkHeader);
        header.rkHeader.loadAddrHi = loadAddr >> 8;
        header.rkHeader.loadAddrLo = loadAddr & 0xFF;
        header.rkHeader.endAddrHi = endAddr >> 8;
        header.rkHeader.endAddrLo = endAddr & 0xFF;

        switch (format) {
        case TFF_RKM:
            cs = calcRkmCs(body);
            break;
        case TFF_RKU:
            cs = calcRkuCs(body);
            break;
        default:
            cs = calcRkCs(body);
        }

        if (format == TFF_RK || format == TFF_RKU) {
            footerSize = sizeof(RkFooter);
            footer.rkFooter.nullByte1 = 0;
            footer.rkFooter.nullByte2 = 0;
            footer.rkFooter.syncByte = 0xE6;
            footer.rkFooter.csHi = cs >> 8;
            footer.rkFooter.csLo = cs & 0xFF;
        } else if (format == TFF_RKP) {
            footerSize = sizeof(RkpFooter);
            footer.rkpFooter.nullByte = 0;
            footer.rkpFooter.syncByte = 0xE6;
            footer.rkpFooter.csHi = cs >> 8;
            footer.rkpFooter.csLo = cs & 0xFF;
        } else if (format == TFF_RK4) {
            footerSize = sizeof(Rk4Footer);
            memset(footer.rk4Footer.nullBytes, 0, sizeof(footer.rk4Footer.nullBytes));
            footer.rk4Footer.syncByte = 0xE6;
            footer.rk4Footer.csHi1= cs >> 8;
            footer.rk4Footer.csLo1 = cs & 0xFF;
            footer.rk4Footer.csHi2= cs >> 8;
            footer.rk4Footer.csLo2 = cs & 0xFF;
        } else if (format == TFF_RKM) {
            footerSize = sizeof(RkmFooter);
            footer.rkmFooter.csHi = cs >> 8;
            footer.rkmFooter.csLo = cs & 0xFF;
        }
        break;
    case TFF_RKS:
        cs = calcRkCs(body);

        headerSize = sizeof(RksHeader);
        header.rksHeader.loadAddrHi = loadAddr >> 8;
        header.rksHeader.loadAddrLo = loadAddr & 0xFF;
        header.rksHeader.endAddrHi = endAddr >> 8;
        header.rksHeader.endAddrLo = endAddr & 0xFF;

        footerSize = sizeof(RksFooter);
        footer.rksFooter.csHi = cs >> 8;
        footer.rksFooter.csLo = cs & 0xFF;
        break;
    case TFF_BRU:
        headerSize = sizeof(BruHeader);
        memcpy(&header.bruHeader.name, intFileName, 8);
        header.bruHeader.loadAddrHi = loadAddr >> 8;
        header.bruHeader.loadAddrLo = loadAddr & 0xFF;
        header.bruHeader.lenHi = (endAddr - loadAddr + 1) >> 8;
        header.bruHeader.lenLo = (endAddr - loadAddr + 1) & 0xFF;
        header.bruHeader.attr = 0;
        memset(header.bruHeader.ff, 0xFF, sizeof(header.bruHeader.ff));
        footerSize = 0;
        break;
    case TFF_RKO:
        headerSize = sizeof(RkoHeader);

        memcpy(&header.rkoHeader.name, intFileName, 8);
        header.rkoHeader.loadAddrHi = loadAddr >> 8;
        header.rkoHeader.loadAddrLo = loadAddr & 0xFF;
        header.rkoHeader.lenHi = (endAddr - loadAddr + 1 + 16) >> 8;
        header.rkoHeader.lenLo = (endAddr - loadAddr + 1 + 16) & 0xFF;
        header.rkoHeader.syncByte = 0xE6;
        memset(header.rkoHeader.nullBytes, 0, sizeof(header.rkoHeader.nullBytes));

        header.rkoHeader.bruHeader.loadAddrHi = loadAddr >> 8;
        header.rkoHeader.bruHeader.loadAddrLo = loadAddr & 0xFF;
        header.rkoHeader.bruHeader.lenHi = (endAddr - loadAddr + 1) >> 8;
        header.rkoHeader.bruHeader.lenLo = (endAddr - loadAddr + 1) & 0xFF;
        header.rkoHeader.bruHeader.attr = 0;
        memset(header.rkoHeader.bruHeader.ff, 0xFF, sizeof(header.rkoHeader.bruHeader.ff));
        memcpy(&header.rkoHeader.bruHeader.name, intFileName, 8);

        memset(footer.rkoFooter.padding, 0, sizeof(footer.rkoFooter.padding));
        footer.rkoFooter.syncByte = 0xE6;

        cs = addToRkCs(0, (uint8_t*)(&header.rkoHeader.bruHeader), sizeof(BruHeader), false);
        cs = addToRkCs(cs, body.data(), body.size(), false);
        cs = addToRkCs(cs, footer.rkoFooter.padding, 3, true);

        footer.rkoFooter.csHi = cs >> 8;
        footer.rkoFooter.csLo = cs & 0xFF;
        paddingSize = (-headerSize - body.size()) & 0x0F;
        footerSize = paddingSize + 3 /* syncByte + csHi + csLo */;
        footerPtr += (sizeof(footer.rkoFooter.padding) - paddingSize);
        break;
    case TFF_CAS:
        headerSize = sizeof(CasHeader);
        memcpy(&header.casHeader.name, intFileName, 6);
        header.casHeader.loadAddrHi = loadAddr >> 8;
        header.casHeader.loadAddrLo = loadAddr & 0xFF;
        header.casHeader.endAddrHi = endAddr >> 8;
        header.casHeader.endAddrLo = endAddr & 0xFF;
        header.casHeader.runAddrHi = startAddr >> 8;
        header.casHeader.runAddrLo = startAddr & 0xFF;
        memset(header.casHeader.d0, 0xD0, sizeof(header.casHeader.d0));
        memset(header.casHeader.padding, 0, sizeof(header.casHeader.padding));
        memcpy(header.casHeader.casSignature1, casSignature, sizeof(header.casHeader.casSignature1));
        memcpy(header.casHeader.casSignature2, casSignature, sizeof(header.casHeader.casSignature2));
        footerSize = 0;
        break;
    case TFF_LVT:
        headerSize = sizeof(LvtHeader);
        memcpy(&header.lvtHeader.name, intFileName, 6);
        header.lvtHeader.loadAddrHi = loadAddr >> 8;
        header.lvtHeader.loadAddrLo = loadAddr & 0xFF;
        header.lvtHeader.endAddrHi = endAddr >> 8;
        header.lvtHeader.endAddrLo = endAddr & 0xFF;
        header.lvtHeader.runAddrHi = startAddr >> 8;
        header.lvtHeader.runAddrLo = startAddr & 0xFF;
        header.lvtHeader.d0 = 0xD0;
        memcpy(header.lvtHeader.lvtSignature, lvtSignature, sizeof(header.lvtHeader.lvtSignature));
        footerSize = 0;
        break;
    }

    ofstream f(outputFile, ofstream::binary);
    if (f.fail())
        return false;
    f.write(headerPtr, headerSize);
    f.write((const char*)(body.data()), body.size());
    f.write(footerPtr, footerSize);
    if (f.fail()) {
        f.close();
        return false;
    }
    f.close();

    return true;
}


int main(int argc, const char** argv)
{
    static_assert(sizeof(RkFooter) == 5, "Packed structs required!");

    cout << "bin2tape v. " VERSION " (c) Viktor Pykhonin, 2021-2023" << endl << endl;
    string moduleName = argv[0];
    moduleName = moduleName.substr(moduleName.find_last_of("/\\:") + 1);

    TapeFileFormat format  = TFF_RK;
    string ext = "rk";
    uint16_t loadAddr = 0;
    uint16_t runAddr = 0;
    string intFileName;
    string inputFileName;
    string outputFileName;

    bool loadAddrSpecified = false;
    bool runAddrSpecified = false;
    bool intFileSpecified = false;
    bool inputFileSpecified = false;
    bool outputFileSpecified = false;

    // parse command line

    if (argc < 2) {
        usage(moduleName);
        return 1;
    }

    int i = 1;
    string option, value;
    while (i < argc) {
        option = argv[i];

        if (option == "-t") {
            ++i;
            if (i > argc) {
                usage(moduleName);
                return 1;
            }
            value = argv[i];
            ext = value;

            if (value == "rk" || value == "rkr" || value == "rka" || value == "rk8" || value == "rke" || value == "rkl")
                format = TFF_RK;
            else if (value == "rkm")
                format = TFF_RKM;
            else if (value == "rku")
                format = TFF_RKU;
            else if (value == "rks")
                format = TFF_RKS;
            else if (value == "rko")
                format = TFF_RKO;
            else if (value == "bru" || value == "ord")
                format = TFF_BRU;
            else if (value == "rkp")
                format = TFF_RKP;
            else if (value == "rk4")
                format = TFF_RK4;
            else if (value == "cas")
                format = TFF_CAS;
            else if (value == "lvt")
                format = TFF_LVT;
            else {
                cout << "Invalid format specification!" << endl << endl;
                usage(moduleName);
                return 1;
            }
        } else if (option == "-a") {
            ++i;
            if (i > argc) {
                usage(moduleName);
                return 1;
            }
            value = argv[i];

            char* numEnd;
            loadAddr = strtoul(value.c_str(), &numEnd, 16);
            loadAddrSpecified = true;
            if (*numEnd) {
                cout << "Invalid load address!" << endl << endl;
                usage(moduleName);
                return 1;
            }
        } else if (option == "-r") {
            ++i;
            if (i > argc) {
                usage(moduleName);
                return 1;
            }
            value = argv[i];

            char* numEnd;
            runAddr = strtoul(value.c_str(), &numEnd, 16);
            runAddrSpecified = true;
            if (*numEnd) {
                cout << "Invalid run address!" << endl << endl;
                usage(moduleName);
                return 1;
            }
        } else if (option == "-n") {
            ++i;
            if (i > argc) {
                usage(moduleName);
                return 1;
            }
            value = argv[i];

            intFileName = value;
            intFileSpecified = true;
        } else if (option == "-n-") {
            intFileSpecified = true;
        } else {
            if (option[0] == '-') {
                cout << "Invalid option:" << option << endl << endl;
                usage(moduleName);
                return 1;
            }

            if (!inputFileSpecified) {
                inputFileName = option;
                inputFileSpecified = true;
            } else if (!outputFileSpecified) {
                outputFileName = option;
                outputFileSpecified = true;
            } else {
                usage(moduleName);
                return 1;
            }
        }

        ++i;
    }

    if (!inputFileSpecified) {
        cout << "No input file name specified!" << endl << endl;
        usage(moduleName);
        return 1;
    }

    string inputFileNameWoPath = inputFileName.substr(inputFileName.find_last_of("/\\:") + 1);

    if (!intFileSpecified)
        intFileName = inputFileNameWoPath;

    string inputFileExt = inputFileName.substr(inputFileName.find_last_of('.') + 1);
    if (!loadAddrSpecified && (inputFileExt == "com" || inputFileExt == "COM"))
        loadAddr = 0x100;

    if (!runAddrSpecified)
        runAddr = loadAddr;

    if (!outputFileSpecified)
        outputFileName = inputFileNameWoPath.substr(0, inputFileNameWoPath.find_last_of('.')) + "." + ext;

    const uint8_t* intFileNameBuf = nullptr;
    int intFileNameLen = 0;

    if (format == TFF_BRU || format == TFF_RKO)
        intFileNameLen = 8;
    else if (format == TFF_CAS || format == TFF_LVT)
        intFileNameLen = 6;

    if (intFileNameLen)
        intFileNameBuf = makeIntName(intFileName, intFileNameLen);

    vector<uint8_t> body;
    if (!loadFile(inputFileName, body)) {
        cout << "Input file error" << endl;
        return 1;
    }

    cout << "Processing " << inputFileName << ":" << endl;
    cout << "\tFormat:\t\t" << txtFormats[format] << endl;
    cout << "\tLoad address:\t" << setfill('0') << setw(4) << uppercase << hex << loadAddr << endl;
    cout << "\tEnd address:\t" << setfill('0') << setw(4) << uppercase << hex << loadAddr + body.size() - 1 << endl;
    if (format == TFF_CAS || format == TFF_LVT)
        cout << "\tRun address:\t" << setfill('0') << setw(4) << uppercase << hex << runAddr << endl;
    if (format == TFF_CAS || format == TFF_LVT || format == TFF_BRU || format == TFF_RKO) {
        cout << "\tInt. file name:\t";
        for (int i = 0; i < intFileNameLen; i++)
            cout << char(intFileNameBuf[i]);
        cout << endl;
    }

    cout << endl;

    cout << "Writing " << outputFileName << " ... ";

    if (!convert(body, format, loadAddr, runAddr, outputFileName, intFileNameBuf)) {
        cout << "error!" << endl;
        return 1;
    }

    cout << "done." << endl;

    return 0;
}
