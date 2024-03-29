/*
 *  rkdisk
 *  (c) Viktor Pykhonin <pyk@mail.ru>, 2024
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

#include "rkimage/rkvolume.h"


#define VERSION "1.02"


using namespace std;

void usage(const string& moduleName)
{
            cout << "Usage: " << moduleName << " <command> [<options>...] <image_file.rdi> [<rk_file>] [<target_file>]" << endl << endl <<
                    "Commands:" << endl << endl <<
                    "    a   Add file to image" << endl <<
                    "        options:" << endl <<
                    "            -a addr - starting Address (hex), default = 0000" << endl <<
                    "            -o      - Overwrite file if exists" << endl <<
                    "            -r      - set \"Read only\" attribute" << endl <<
                    "            -h      - set \"Hidden\" attribute" << endl <<
                    "    x   eXtract file from image" << endl <<
                    "    d   Delete file from image" << endl <<
                    "    l   List files in image" << endl <<
                    "        options:" << endl <<
                    "            -b - Brief listing" << endl <<
                    "    f   Format or create new empty image" << endl <<
                    "        options:" << endl <<
                    "            -y      - don't ask to confirm" << endl <<
                    "            -s size - directory Size in sectors (default 4)" << endl <<
                    "    t   set file aTtributes" << endl <<
                    "        options:" << endl <<
                    "            -r      - set \"Read only\" attribute" << endl <<
                    "            -h      - set \"Hidden\" attribute" << endl <<
                    endl;
}


string makeRkDosFileName(const string& rkFileName)
{
    // cut off the extention if any
    int periodPos = rkFileName.find_last_of('.');

    string rkDosName = rkFileName.substr(0, min(periodPos, 10));
    string ext = rkFileName.substr(periodPos + 1, 3);

    if (!ext.empty()) {
        rkDosName.append(".");
        rkDosName.append(ext);
    }

    for (char& ch: rkDosName) {
        if (!(ch >= '0' && ch <= '9') && !(ch >= 'A' && ch <= 'Z') && !(ch >= 'a' && ch <= 'z') && ch != ' ' && ch != '.')
            ch = '_';
    }

    return rkDosName;
}


void listFiles(const string& imageFileName, bool briefListing)
{
    RkVolume vol(imageFileName, IFM_READ_ONLY);

    auto fileList = vol.getFileList();

    if (!briefListing) {
        cout << "Name          " << "\t" << "Addr" << "\t" << "Blocks" << "\t" << "  Bytes" << "\t" << "  Attr" << endl;
        cout << "----          " << "\t" << "----" << "\t" << "------" << "\t" << "  -----" << "\t" << "  ----" << endl;

        for (const auto& fi: *fileList) {
            string attr = fi.attr & 0x80 ? "R" : "";
            if (fi.attr & 0x40)
                attr += "H";
            cout << left << setw(14) << setfill(' ') << fi.fileName << "\t"
                 << right << setw(4) << setfill('0') << hex << fi.addr << "\t"
                 << setw(6) << setfill(' ') << dec << fi.sCount << "\t"
                 << setw(7) << setfill(' ') << dec << fi.fileSize << "\t"
                 << setw(6) << attr << endl;
        }
    } else {
        int i = 0;
        for (const auto& fi: *fileList) {
            cout << left << setw(14) << setfill(' ') << fi.fileName << "\t";
            if (++i % 5 == 0)
                cout << endl;
        }
        cout << endl;
    }
    cout << endl << fileList->size() << " file(s) total" << endl;
    int freeBlocks = vol.getFreeBlocks();
    int freeDirEntries = vol.getFreeDirEntries();
    cout << endl << freeBlocks << " block(s) (" << freeBlocks * 512 << " bytes) free" << endl;
    cout << freeDirEntries << " directory entries free" << endl;
}


void deleteFile(const string& imageFileName, const string& rkFileName)
{
    RkVolume vol(imageFileName, IFM_READ_WRITE);
    vol.deleteFile(rkFileName);
    vol.saveImage();
}


void setAttributes(const string& imageFileName, const string& rkFileName, bool readOnly, bool hidden)
{
    RkVolume vol(imageFileName, IFM_READ_WRITE);
    uint8_t attr = (readOnly ? 0x80 : 0) | (hidden ? 0x40 : 0);
    vol.setAttributes(rkFileName, attr);
    vol.saveImage();
}


bool addFile(const string& imageFileName, const string& rkFileName, uint16_t addr, bool readOnly, bool hidden, bool allowOverwrite)
{
    ifstream rkFile(rkFileName, ios::binary);
    if (!rkFile.is_open()) {
        cout << "error opening file " << rkFileName << endl;
        return false;
    }

    rkFile.seekg(0, ios::end);
    int size = rkFile.tellg();
    rkFile.seekg(0, ios::beg);
    uint8_t* buf = new uint8_t[size];
    rkFile.read((char*)buf, size);

    if (rkFile.rdstate()) {
        cout << "error reading file " << rkFileName << endl;
        delete[] buf;
        return false;
    }

    rkFile.close();

    RkVolume vol(imageFileName, IFM_READ_WRITE);

    uint8_t attr = (readOnly ? 0x80 : 0) | (hidden ? 0x40 : 0);

    vol.writeFile(rkFileName, buf, size, addr, attr, allowOverwrite);
    vol.saveImage();

    return true;
}


bool extractFile(const string& imageFileName, const string& rkFileName, const string& targetFileName)
{
    ofstream rkFile(targetFileName, ios::binary | std::fstream::trunc);
    if (!rkFile.is_open()) {
        cout << "error opening file " << rkFileName << endl;
        return false;
    }

    RkVolume vol(imageFileName, IFM_READ_ONLY);

    int size = 0;
    uint8_t* buf = vol.readFile(rkFileName, size);

    rkFile.write(reinterpret_cast<char*>(buf), size);
    if (rkFile.rdstate()) {
        cout << "error writing file " << rkFileName << endl;
        delete[] buf;
        return false;
    }
    rkFile.close();

    delete[] buf;

    return true;
}


void formatImage(const string& imageFileName, int directorySize)
{
    RkVolume vol(imageFileName, IFM_WRITE_CREATE);
    vol.format(directorySize);
    vol.saveImage();
}


int main(int argc, const char** argv)
{
    cout << "rkdisk v. " VERSION " (c) Viktor Pykhonin, 2024" << endl << endl;
    string moduleName = argv[0];
    moduleName = moduleName.substr(moduleName.find_last_of("/\\:") + 1);

    string imageFileName;
    string rkFileName;
    string targetFileName;

    bool allowOverwrite = false;
    bool briefListing = false;
    bool readOnly = false;
    bool hidden = false;
    bool noConfirmation = false;
    uint16_t startingAddr = 0;
    int directorySize = 4;

    // parse command line

    if (argc < 2) {
        usage(moduleName);
        return 1;
    }

    string command = argv[1];
    int i = 2;

    string option, value;
    while (i < argc) {
        option = argv[i];

        if (option == "-o") {
            if (i > argc || command != "a") {
                usage(moduleName);
                return 1;
            }
            allowOverwrite = true;
        } else if (option == "-a") {
            ++i;
            if (i > argc || command != "a") {
                usage(moduleName);
                return 1;
            }
            value = argv[i];

            char* numEnd;
            startingAddr = strtoul(value.c_str(), &numEnd, 16);
            if (*numEnd) {
                cout << "Invalid starting address!" << endl << endl;
                usage(moduleName);
                return 1;
            }
        } else if (option == "-s") {
            ++i;
            if (i > argc || command != "f") {
                usage(moduleName);
                return 1;
            }
            value = argv[i];

            char* numEnd;
            directorySize = strtoul(value.c_str(), &numEnd, 10);
            if (*numEnd || directorySize < 1 || directorySize > 99) {
                cout << "Invalid directory size!" << endl << endl;
                usage(moduleName);
                return 1;
            }
        } else if (option == "-b") {
            if (i > argc || command != "l") {
                usage(moduleName);
                return 1;
            }
            briefListing = true;
        } else if (option == "-y") {
            if (i > argc || command != "f") {
                usage(moduleName);
                return 1;
            }
            noConfirmation = true;
        } else if (option == "-r") {
            if (i > argc || (command != "a" && command != "t")) {
                usage(moduleName);
                return 1;
            }
            readOnly = true;
        } else if (option == "-h") {
            if (i > argc || (command != "a" && command != "t")) {
                usage(moduleName);
                return 1;
            }
            hidden = true;
        } else {
            if (option[0] == '-') {
                cout << "Invalid option:" << option << endl << endl;
                usage(moduleName);
                return 1;
            }

            if (imageFileName.empty()) {
                imageFileName = option;
            } else if (rkFileName.empty()) {
                rkFileName = option;
            } else if (targetFileName.empty()) {
                targetFileName = option;
            } else {
                usage(moduleName);
                return 1;
            }
        }

        ++i;
    }

    if (command != "a" && command != "x" && command != "d" && command != "l" && command != "f" && command != "t") {
        cout << "Unknown comamnd \"" << command << "\"" << endl << endl;
        usage(moduleName);
        return 1;
    }

    if (imageFileName.empty()) {
        cout << "No image file name specified!" << endl << endl;
        usage(moduleName);
        return 1;
    }

    try {

        if (command == "l") {
            if (!rkFileName.empty()) {
                cout << "Extra file name specified!" << endl << endl;
                usage(moduleName);
                return 1;
            }
            cout << "Directory content for image " << imageFileName << ":" << endl << endl;
            listFiles(imageFileName, briefListing);
            return 0;
        } else if (command == "f") {
            if (!targetFileName.empty()) {
                cout << "Extra file name specified!" << endl << endl;
                usage(moduleName);
                return 1;
            }
            if (!noConfirmation) {
                cout << "Format image " << imageFileName << "? [y/N] ";
                char ch;
                cin.get(ch);
                if (ch != 'y' && ch != 'Y')
                    return 1;
            }

            cout << "Formatting image " << imageFileName << ", " << directorySize << " sector(s) directory ... ";
            formatImage(imageFileName, directorySize);
            cout << "done." << endl;
            return 0;
        }

        if (rkFileName.empty()) {
            cout << "No rk file name specified!" << endl << endl;
            usage(moduleName);
            return 1;
        }

        if (command == "x") {
            if (targetFileName.empty())
                targetFileName = rkFileName;
            cout << "Extracting file " << rkFileName << " from image " << imageFileName << " to " << targetFileName << " ... ";
            if (!extractFile(imageFileName, rkFileName, targetFileName))
                return 1;
        } else if (command == "a") {
            if (!targetFileName.empty()) {
                cout << "Extra file name specified!" << endl << endl;
                usage(moduleName);
                return 1;
            }
            string rkFileNameWoPath = rkFileName.substr(rkFileName.find_last_of("/\\:") + 1);
            string newRkFileName = makeRkDosFileName(rkFileNameWoPath);
            if (rkFileNameWoPath != newRkFileName)
                cout << "New rk file name: " << newRkFileName << endl;
            cout << "Adding file " << rkFileNameWoPath << " to image " << imageFileName << " ... ";
            addFile(imageFileName, newRkFileName, startingAddr, readOnly, hidden, allowOverwrite);
        } else if (command == "d") {
            if (!targetFileName.empty()) {
                cout << "Extra file name specified!" << endl << endl;
                usage(moduleName);
                return 1;
            }
            cout << "Deleting file " << rkFileName << " from image " << imageFileName << " ... ";
            deleteFile(imageFileName, rkFileName);
        } else if (command == "t") {
            if (!targetFileName.empty()) {
                cout << "Extra file name specified!" << endl << endl;
                usage(moduleName);
                return 1;
            }
            cout << "Setting file attributes " << rkFileName << " from image " << imageFileName << " ... ";
            setAttributes(imageFileName, rkFileName, readOnly, hidden);
        }

        cout << "done." << endl;

        return 0;
    }

    catch (RkVolume::RkVolumeException& e) {
        cout << "image error: ";
        switch (e.type) {
        case RkVolume::RkVolumeException::RVET_SECTOR_NOT_FOUND:
            cout << "sector not found! Track " << e.track << ", sector " << e.sector << "." << endl;
            break;
        case RkVolume::RkVolumeException::RVET_DISK_FULL:
            cout << "insufficient disk space!" << endl;
            break;
        case RkVolume::RkVolumeException::RVET_DIR_FULL:
            cout << "No more dir entries!" << endl;
            break;
        case RkVolume::RkVolumeException::RVET_BAD_DISK_FORMAT:
            cout << "bad disk image!" << endl;
            break;
        case RkVolume::RkVolumeException::RVET_NO_FILESYSTEM:
            cout << "no filesyetem on image!" << endl;
            break;
        case RkVolume::RkVolumeException::RVET_FILE_NOT_FOUND:
            cout << "file not found!" << endl;
            break;
        case RkVolume::RkVolumeException::RVET_FILE_EXISTS:
            cout << "file already exists!" << endl;
            break;
        default:
            cout << "unknown error!" << endl;
        }
    }

    catch (ImageFileException& e) {
        cout << endl << "Disk error: ";
        switch (e) {
        case IFE_OPEN_ERROR:
            cout << "file open error!" << endl;
            break;
        case IFE_READ_ERROR:
            cout << "file read error!" << endl;
            break;
        case IFE_WRITE_ERROR:
            cout << "file write error!" << endl;
            break;
        }
    }

    return 1;
}
