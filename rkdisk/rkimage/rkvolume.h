/*
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

#ifndef RKVOLUME_H
#define RKVOLUME_H

#include <list>

#include "volume.h"

struct RkSector {
    uint8_t* ptr;
    uint16_t len;
    bool dirty;
    bool allocated;
};

struct RkFileInfo {
    std::string fileName;
    uint8_t dirTrack;
    uint8_t dirSector;
    int dirOffset;
    uint8_t tList;
    uint8_t sList;
    uint16_t sCount;
    uint8_t attr;
    uint16_t addr;
    int fileSize;
};

class RkVolume : public Volume
{
public:

    struct RkVolumeException {

        enum RkVolumeExceptionType {
            RVET_SECTOR_NOT_FOUND,
            RVET_BAD_DISK_FORMAT,
            RVET_NO_FILESYSTEM,
            RVET_DISK_FULL,
            RVET_DIR_FULL,
            RVET_FILE_NOT_FOUND,
            RVET_FILE_EXISTS
        };

        RkVolumeExceptionType type;
        int track = 0;
        int sector = 0;
    };

    RkVolume(const std::string& fileName, ImageFileMode mode);

    bool isValid() override;

    std::list<RkFileInfo>* getFileList();
    RkFileInfo* getFileInfo(std::string fileName);
    int getFreeBlocks();
    int getFreeDirEntries();

    uint8_t* readFile(std::string fileName, int& size);
    void writeFile(std::string fileName, uint8_t* data, int size, uint16_t addr = 0, uint8_t attr = 0, bool allowOverwrite = false);
    void deleteFile(std::string fileName);
    void setAttributes(std::string fileName, uint8_t attr);
    void format(int directorySize = 4);

    void saveImage();

private:
    RkSector m_sectors[160][5];
    std::list<RkFileInfo> m_fileList;

    int m_freeSectors = 0;
    int m_freeDirEntries = 0;

    bool m_diskRead = false;

    void readDisk();

    void readSectors();
    void readVtoc();
    void readDir();
    void calcSizes();
    void updateSectors();

    void allocateSector(int& track, int& sector);
    void allocateSpecificSector(int track, int sector);
    void freeSector(int track, int sector);
    uint8_t* allocateDirEntry();
};



#endif // RKVOLUME_H
