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

#ifndef IMAGEFILE_H
#define IMAGEFILE_H

#include <string>
#include <fstream>

enum ImageFileMode {
    IFM_READ_ONLY,
    IFM_READ_WRITE,
    IFM_WRITE_CREATE
};

enum ImageFileException {
    IFE_OPEN_ERROR,
    IFE_READ_ERROR,
    IFE_WRITE_ERROR
};

class ImageFile
{
public:
    ImageFile(const std::string& fileName, ImageFileMode mode, int imageSize = 0);
    ~ImageFile();

    bool isOpen();
    size_t getSize();
    uint8_t* getData();
    void updateAll();
    uint8_t& operator[](std::ptrdiff_t idx);

private:
    std::fstream m_file;
    size_t m_size = 0;
    uint8_t* m_buf = nullptr;
    ImageFileMode m_mode;
};

#endif // IMAGEFILE_H
