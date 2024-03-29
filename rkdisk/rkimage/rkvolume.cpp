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

#include <cstdlib>
#include <cstring>
#include <cassert>

#include <string>
#include <algorithm>

#include "rkvolume.h"

using namespace std;


RkVolume::RkVolume(const std::string& fileName, ImageFileMode mode) : Volume(fileName, mode, mode == IFM_WRITE_CREATE ? 500000 : 0)
{
}


bool RkVolume::isValid()
{
    if (!m_image)
        return false;

    if (m_image->getSize() != 500000)
        return false;

    // check file structure here
    int pos = 0;
    const int bytesToAnalize = 32;

    uint8_t* imageData = m_image->getData();

    while (pos < bytesToAnalize && imageData[pos] != 0x06)
        pos++;
    while (pos < bytesToAnalize && imageData[pos] == 0x06)
        pos++;
    while (pos < bytesToAnalize - 1 && imageData[pos] != 0xEA && imageData[pos + 1] != 0xD3)
        pos++;

    return pos < bytesToAnalize - 2;
}


void RkVolume::readDisk()
{
    if (m_diskRead)
        return;

    readSectors();
    readVtoc();
    readDir();

    m_diskRead = true;
}


void RkVolume::readSectors()
{
    for (int t = 0; t < 160; t++) {
        uint8_t* trackData = m_image->getData() + t * 3125;

        int pos = 0;
        int nSectorsFound = 0;
        for (int i = 0; i < 5; i++)
            m_sectors[t][i].dirty = true;

        while (pos < 3125 && nSectorsFound < 5) {
            // find syncrobyte
            while (pos < 3125 && trackData[pos] != 0x06)
                pos++;

            // find address mark
            while (pos < 3125 - 3 && trackData[pos] != 0xEA && trackData[pos + 1] != 0xD3)
                pos++;
            pos += 2;

            if (pos >= 3125 - 2)
                throw RkVolumeException {RkVolumeException::RVET_BAD_DISK_FORMAT};

            int nTrack = trackData[pos++];
            int nSect = trackData[pos++];
            if (nTrack != t)
                throw RkVolumeException {RkVolumeException::RVET_BAD_DISK_FORMAT};

            // find synchrobyte
            while (pos < 3125 && trackData[pos] != 0x06)
                pos++;

            // find data mark
            while (pos < 3125 - 3 && trackData[pos] != 0xDD && trackData[pos + 1] != 0xF3)
                pos++;
            pos += 2;

            if (pos >= 3125 - 519)
                throw RkVolumeException {RkVolumeException::RVET_BAD_DISK_FORMAT};

            int sectLen = trackData[pos] + (trackData[pos + 1] << 8);
            pos += 3;

            m_sectors[nTrack][nSect].ptr = trackData + pos;
            m_sectors[nTrack][nSect].len = sectLen;
            m_sectors[nTrack][nSect].dirty = false;

            nSectorsFound++;

            pos += 530;
        }
        bool allSect = true;
        for (int i = 0; i < 5; i++)
            allSect = allSect || m_sectors[t][i].dirty;
        if (!allSect)
            throw RkVolumeException {RkVolumeException::RVET_BAD_DISK_FORMAT}; // there are missings sectors on the track
    }
}


void RkVolume::readVtoc()
{
    int allocated = 0;

    uint8_t* vtocPtr = m_sectors[32][0].ptr;
    if ((vtocPtr[32] & 3) != 3)
        throw RkVolumeException {RkVolumeException::RVET_NO_FILESYSTEM}; // there are missings sectors on the track

    for (int t = 0; t < 160; t++) {
        int bt = vtocPtr[t];
        for (int s = 0; s < 5; s++) {
            m_sectors[t][s].allocated = bt & 1;
            allocated += bt & 1;
            bt >>= 1;
        }
    }

    m_freeSectors = 800 - allocated;
}


void RkVolume::readDir()
{
    m_fileList.clear();
    m_freeDirEntries = 0;

    int dirTrack = 32;
    int dirSector = 1;

    int dirSectors = 0;
    int dirEntriesUsed = 0;

    do {
        uint8_t* sectorData = m_sectors[dirTrack][dirSector].ptr;

        int pos = 7;

        dirSectors++;

        while (pos < 512 - 21 && sectorData[pos]) {
            if (sectorData[pos] == 0xFF) {
                pos += 21;
                continue;
            }

            RkFileInfo fileInfo;

            fileInfo.dirTrack = dirTrack;
            fileInfo.dirSector = dirSector;
            fileInfo.dirOffset = pos;

            if (!sectorData[pos + 9])
                fileInfo.fileName.append((char*)(sectorData + pos));
             else
                fileInfo.fileName.append((char*)(sectorData + pos), 10);

            pos += 11;

            if (sectorData[pos])
                fileInfo.fileName.push_back('.');

            if (!sectorData[pos + 2])
                fileInfo.fileName.append((char*)(sectorData + pos));
             else
                fileInfo.fileName.append((char*)(sectorData + pos), 3);

            pos += 3;

            fileInfo.tList = sectorData[pos++];
            fileInfo.sList = sectorData[pos++];

            fileInfo.addr = sectorData[pos] + (sectorData[pos + 1] << 8);
            pos += 2;

            fileInfo.sCount = sectorData[pos] + (sectorData[pos + 1] << 8);
            pos += 2;

            fileInfo.attr = sectorData[pos++];

            m_fileList.push_back(fileInfo);

            dirEntriesUsed++;
        }

        dirTrack = sectorData[0];
        dirSector = sectorData[1];

    } while (dirTrack || dirSector);

    m_freeDirEntries = dirSectors * 24 - dirEntriesUsed;

    m_fileList.sort([](const auto& x, const auto& y) {return x.fileName < y.fileName;});

    calcSizes();
}


void RkVolume::calcSizes()
{
    for (auto& fi: m_fileList) {
        int t = fi.tList;
        int s = fi.sList;

        int len = 0;

        do {
            uint8_t* ptr = m_sectors[t][s].ptr;
            int sectorSize = m_sectors[t][s].len;

            t = ptr[0];
            s = ptr[1];

            if (t >= 160 || s >= 5)
                throw RkVolumeException {RkVolumeException::RVET_SECTOR_NOT_FOUND, t, s};

            int pos = 2;
            while (pos <= sectorSize - 2) {
                int nextTrack = ptr[pos++];
                int nextSector = ptr[pos++];

                if (nextTrack >= 160 || nextSector >= 5)
                    throw RkVolumeException {RkVolumeException::RVET_SECTOR_NOT_FOUND, nextTrack, nextSector};

                if (nextTrack || nextSector)
                    len += m_sectors[nextTrack][nextSector].len;
                else
                    break;
            }
        } while (t || s);
        fi.fileSize = len;
    }
}


std::list<RkFileInfo>* RkVolume::getFileList()
{
    readDisk();
    return &m_fileList;
}


int RkVolume::getFreeBlocks()
{
    readDisk();
    return m_freeSectors;
}


int RkVolume::getFreeDirEntries()
{
    readDisk();
    return m_freeDirEntries;
}


uint8_t* RkVolume::readFile(std::string fileName, int& len)
{
    readDisk();

    std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::toupper);

    auto fi = find_if(m_fileList.begin(), m_fileList.end(), [fileName](const auto& x) {return fileName == x.fileName;});
    if (fi != m_fileList.end()) {
        len = fi->fileSize;

        int left = fi->fileSize;
        int bufPos = 0;
        uint8_t* buf = new uint8_t[left];

        int t = fi->tList;
        int s = fi->sList;

        if (t >= 160 || s >= 5)
            throw RkVolumeException {RkVolumeException::RVET_SECTOR_NOT_FOUND, t, s};

        do {
            uint8_t* ptr = m_sectors[t][s].ptr;
            int sectorSize = m_sectors[t][s].len;

            t = ptr[0];
            s = ptr[1];

            if (t >= 160 || s >= 5)
                throw RkVolumeException {RkVolumeException::RVET_SECTOR_NOT_FOUND, t, s};

            int tslistPos = 2;
            while (tslistPos <= sectorSize - 2) {
                int nextTrack = ptr[tslistPos++];
                int nextSector = ptr[tslistPos++];

                if (nextTrack >= 160 || nextSector >= 5)
                    throw RkVolumeException {RkVolumeException::RVET_SECTOR_NOT_FOUND, nextTrack, nextSector};

                if (nextTrack || nextSector) {
                    int toRead = m_sectors[nextTrack][nextSector].len;
                    if (toRead <= left) {
                        memcpy(buf + bufPos, m_sectors[nextTrack][nextSector].ptr, toRead);
                        bufPos += toRead;
                        left -= toRead;
                    }
                } else
                    break;
            }
        } while (t || s);
        return buf;
    } else
        throw RkVolumeException {RkVolumeException::RVET_FILE_NOT_FOUND};

    return nullptr;
}


void RkVolume::allocateSector(int& track, int& sector)
{
    if (!m_freeSectors)
        throw RkVolumeException {RkVolumeException::RVET_DISK_FULL};

    for (int t = 0; t < 160; t++)
        for (int s = 0; s < 5; s++)
            if (!m_sectors[t][s].allocated) {
                memset(m_sectors[t][s].ptr, 0, 512);
                track = t;
                sector = s;
                m_sectors[t][s].dirty = true;
                m_sectors[t][s].allocated = true;
                m_sectors[32][0].ptr[t] |= (1 << s);
                m_sectors[32][0].dirty = true;
                --m_freeSectors;
                return;
            }

    throw RkVolumeException {RkVolumeException::RVET_DISK_FULL};
}


void RkVolume::allocateSpecificSector(int track, int sector)
{
    if (!m_sectors[track][sector].allocated)
        --m_freeSectors;

    memset(m_sectors[track][sector].ptr, 0, 512);
    m_sectors[track][sector].dirty = true;
    m_sectors[track][sector].allocated = true;
    m_sectors[32][0].ptr[track] |= (1 << sector);
    m_sectors[32][0].dirty = true;
}


void RkVolume::freeSector(int track, int sector)
{
    if (m_sectors[track][sector].allocated) {
        m_sectors[track][sector].allocated = false;
        m_sectors[track][sector].dirty = true;
        m_sectors[32][0].ptr[track] &= ~(1 << sector);
        m_sectors[32][0].dirty = true;
        ++m_freeSectors;
    }
}


uint8_t* RkVolume::allocateDirEntry()
{
    int track = 32;
    int sector = 1;

    uint8_t* sectorData = m_sectors[track][sector].ptr;

    do {
        int pos = 7;

        while (pos < 512 - 21) {
            if (sectorData[pos] == 0 || sectorData[pos] == 0xFF) {
                m_sectors[track][sector].dirty = true;
                return sectorData + pos;
            }
            pos += 21;
        }

        track = sectorData[0];
        sector = sectorData[1];

        if (track >= 160 || sector >= 5)
            throw RkVolumeException {RkVolumeException::RVET_SECTOR_NOT_FOUND, track, sector};

        sectorData = m_sectors[track][sector].ptr;

    } while (track || sector);

    throw RkVolumeException {RkVolumeException::RVET_DIR_FULL};
}


void RkVolume::writeFile(string fileName, uint8_t* data, int size, uint16_t addr, uint8_t attr, bool allowOverwrite)
{
    readDisk();

    std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::toupper);

    size_t periodPos = fileName.find_last_of('.');
    string sExt = periodPos != string::npos ? fileName.substr(periodPos + 1, 3) : "";
    if (periodPos > 10)
        periodPos = 10;
    string sBaseName = fileName.substr(0, periodPos);

    if (find_if(m_fileList.begin(), m_fileList.end(), [fileName](const auto& x) {return fileName == x.fileName;}) != m_fileList.end()) {
        if (allowOverwrite)
            deleteFile(fileName);
        else
            throw RkVolumeException {RkVolumeException::RVET_FILE_EXISTS};
    }

    int sectorsNeeded = (size + 511) / 512;
    if (sectorsNeeded == 0)
        sectorsNeeded = 1;
    sectorsNeeded += (sectorsNeeded + 253) / 254;

    if (sectorsNeeded > m_freeSectors)
        // no free space
        throw RkVolumeException {RkVolumeException::RVET_DISK_FULL};

    uint8_t* dir = allocateDirEntry();

    strncpy(reinterpret_cast<char*>(dir), sBaseName.c_str(), 10);
    dir[10] = 0;
    dir += 11;

    strncpy(reinterpret_cast<char*>(dir), sExt.c_str(), 3);
    dir += 3;

    int tslistTrack, tslistSector;

    allocateSector(tslistTrack, tslistSector);

    *dir++ = tslistTrack;
    *dir++ = tslistSector;

    // starting address
    *dir++ = addr & 0xFF;
    *dir++ = addr >> 8;

    *dir++ = sectorsNeeded % 256;
    *dir++ = sectorsNeeded / 256;

    // attr
    *dir = attr;

    int left = size;

    uint8_t* tslistPtr = m_sectors[tslistTrack][tslistSector].ptr;
    tslistPtr[0] = 0;
    tslistPtr[1] = 0;

    int tslistPos = 2;

    int track, sector;
    while (left > 0) {
        allocateSector(track, sector);

        uint8_t* ptr = m_sectors[track][sector].ptr;

        int bytesToCopy = left > 512 ? 512 : left;
        memcpy(reinterpret_cast<char*>(ptr), data, bytesToCopy);
        m_sectors[track][sector].len = bytesToCopy;
        if (bytesToCopy < 512)
            memset(ptr + bytesToCopy, 0, 512 - bytesToCopy + 2); // + CS: 2 bytes
        data += bytesToCopy;
        left -= bytesToCopy;

        tslistPtr[tslistPos++] = track;
        tslistPtr[tslistPos++] = sector;

        if (tslistPos == 254) {
            tslistPtr[254] = 0;
            tslistPtr[255] = 0;

            allocateSector(tslistTrack, tslistSector);
            tslistPtr[0] = tslistTrack;
            tslistPtr[1] = tslistSector;
            uint8_t* tslistPtr = m_sectors[tslistTrack][tslistSector].ptr;
            tslistPtr[0] = 0;
            tslistPtr[1] = 0;
            tslistPos = 2;
        }
    }
    tslistPtr[tslistPos] = 0;
    tslistPtr[tslistPos + 1] = 0;

    updateSectors();
    readDir();
}


RkFileInfo* RkVolume::getFileInfo(std::string fileName)
{
    readDisk();

    std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::toupper);

    auto fi = find_if(m_fileList.begin(), m_fileList.end(), [fileName](const auto& x) {return fileName == x.fileName;});
    if (fi != m_fileList.end()) {
        return &*fi;
    } else
        throw RkVolumeException {RkVolumeException::RVET_FILE_NOT_FOUND};
}


void RkVolume::deleteFile(std::string fileName)
{
    readDisk();

    std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::toupper);

    auto fi = find_if(m_fileList.begin(), m_fileList.end(), [fileName](const auto& x) {return fileName == x.fileName;});
    if (fi == m_fileList.end())
        throw RkVolumeException {RkVolumeException::RVET_FILE_NOT_FOUND};

    uint8_t* dir = m_sectors[fi->dirTrack][fi->dirSector].ptr + fi->dirOffset;

    dir[10] = dir[0];
    dir[0] = 0xFF;

    int t = fi->tList;
    int s = fi->sList;

    if (t >= 160 || s >= 5)
        throw RkVolumeException {RkVolumeException::RVET_SECTOR_NOT_FOUND, t, s};

    do {
        uint8_t* ptr = m_sectors[t][s].ptr;
        int sectorSize = m_sectors[t][s].len;

        if (t >= 160 || s >= 5)
            throw RkVolumeException {RkVolumeException::RVET_SECTOR_NOT_FOUND, t, s};

        int tslistPos = 2;
        while (tslistPos <= sectorSize - 2) {
            int nextT = ptr[tslistPos++];
            int nextS = ptr[tslistPos++];

            if (nextT >= 160 || nextS >= 5)
                throw RkVolumeException {RkVolumeException::RVET_SECTOR_NOT_FOUND, nextT, nextS};

            if (nextT || nextS)
                freeSector(nextT, nextS);
            else
                break;
        }
        freeSector(t, s);

        t = ptr[0];
        s = ptr[1];
    } while (t || s);

    updateSectors();
    readDir();
}


void RkVolume::setAttributes(std::string fileName, uint8_t attr)
{
    readDisk();

    std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::toupper);

    auto fi = find_if(m_fileList.begin(), m_fileList.end(), [fileName](const auto& x) {return fileName == x.fileName;});
    if (fi != m_fileList.end()) {
        fi->attr = attr;
        RkSector* sector = &m_sectors[fi->dirTrack][fi->dirSector];
        sector->ptr[fi->dirOffset + 20] = attr;
        sector->dirty = true;;
    } else
        throw RkVolumeException {RkVolumeException::RVET_FILE_NOT_FOUND};

    updateSectors();
    //readDir();
}


void RkVolume::format(int directorySize)
{
    if (m_image->getSize() != 500000)
        throw RkVolumeException {RkVolumeException::RVET_BAD_DISK_FORMAT};

    const int c_sectorNums[5] = {0, 3, 1, 4, 2};

    for (int tr = 0; tr < 160; tr++) {
        uint8_t* track = m_image->getData() + tr * 3125;
        memset(track, 0, 586 * 5);
        memset(track + 586 * 5, 0xFF, 3125 - 586 * 5);

        for (int s = 0; s < 5; s++) {

            uint8_t* ptr = track + 586 * s;

            // syncrobytes * 5
            memset(ptr, 0x06, 5);
            ptr += 5;

            // null bytes * 5
            ptr += 5;

            *ptr++ = 0xEA;
            *ptr++ = 0xD3;
            *ptr++ = tr;
            *ptr++ = c_sectorNums[s];

            *ptr++ = tr + c_sectorNums[s]; // CS

            // null bytes * 5
            ptr += 5;

            // syncrobytes * 5
            memset(ptr, 0x06, 5);
            ptr += 5;

            // null bytes * 5
            ptr += 5;

            *ptr++ = 0xDD;
            *ptr++ = 0xF3;

            *ptr++ = 0x00;
            *ptr++ = 0x02;

            if (tr == 0x20 && s == 0) {
                ptr -= 2;
                *ptr++ = 0xA0;
                *ptr++ = 0x00;
                ptr[0x20] = 0x1f;
                ptr[0xA0] = 0x1f;
            }

            // the rest bytes are 0x00
        }
    }

    readSectors();

    allocateSpecificSector(32, 0);
    m_sectors[32][0].len = 160;

    for (int i = 1; i <= directorySize; i++) {
        int t = 32 + i / 5;
        int s = i % 5;
        allocateSpecificSector(t, s);
        if (i != directorySize) {
            m_sectors[t][s].ptr[0] = 32 + (i + 1) / 5;
            m_sectors[t][s].ptr[1] = (i + 1) % 5;
        }
    }

    updateSectors();
}


void RkVolume::updateSectors()
{
    for (int t = 0; t < 160; t++)
        for(int s = 0; s < 5; s++)
            if (m_sectors[t][s].dirty) {
                int len = m_sectors[t][s].len;
                uint8_t* ptr = m_sectors[t][s].ptr;
                uint16_t cs = 0;
                ptr[-3] = m_sectors[t][s].len & 0xFF;
                ptr[-2] = m_sectors[t][s].len >> 8;
                for (int i = 0; i < len; i++)
                    cs += ptr[i];
                ptr[len] = cs & 0xFF;
                ptr[len + 1] = cs >> 8;
            }
}


void RkVolume::saveImage()
{
    m_image->updateAll();
}
