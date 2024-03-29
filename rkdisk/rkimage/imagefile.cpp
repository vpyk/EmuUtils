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

#include <string>
#include <fstream>

#include <cstring>

#include "imagefile.h"

using namespace std;


ImageFile::ImageFile(const string& fileName, ImageFileMode mode, int imageSize)
{
    m_mode = mode;
    ios_base::openmode openMode;
    switch (mode) {
    case IFM_READ_ONLY:
        openMode = ios::binary | ios::in;
        break;
    case IFM_READ_WRITE:
        openMode = ios::binary | ios::in | ios::out;
        break;
    case IFM_WRITE_CREATE:
        openMode = ios::binary | ios::out | std::fstream::trunc;
        break;
    }

    m_file.open(fileName, openMode);
    if (!m_file.is_open())
        throw IFE_OPEN_ERROR;

    if (mode != IFM_WRITE_CREATE) {
        // open existing file
        m_file.seekg(0, ios::end);
        m_size = m_file.tellg();
        m_file.seekg(0, ios::beg);
        m_buf = new uint8_t[m_size];
        m_file.read((char*)(m_buf), m_size);
        if (m_file.rdstate()) {
            throw IFE_READ_ERROR;
            m_file.close();
        }
    } else {
        // create new file
        m_buf = new uint8_t[imageSize];
        m_size = imageSize;
        memset(m_buf, 0, m_size);
    }
}


ImageFile::~ImageFile()
{
    if (m_file)
        m_file.close();
}


bool ImageFile::isOpen()
{
    return m_file.is_open();
}


size_t ImageFile::getSize()
{
    return m_size;
}


uint8_t* ImageFile::getData()
{
    return m_buf;
}


uint8_t& ImageFile::operator[](ptrdiff_t idx)
{
    return m_buf[idx];
}

void ImageFile::updateAll()
{
    if (m_mode == IFM_READ_ONLY)
        return;

    m_file.seekg(0, ios::beg);
    m_file.write((char*)(m_buf), m_size);
    if (m_file.rdstate())
        throw IFE_WRITE_ERROR;
}
