/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2013  Martin Bříza <email>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "SafeDataStream.h"

#include <QBuffer>
#include <QtCore/QDebug>

namespace SDDM {

    SafeDataStream::SafeDataStream(QIODevice* stream) : QDataStream(new QByteArray(), QIODevice::ReadWrite),
        m_stream(stream) {
    }

    void SafeDataStream::clear() {
        QBuffer *buf = qobject_cast<QBuffer*>(device());
        if (buf) {
            buf->reset();
            buf->buffer().clear();
        }
    }

    void SafeDataStream::receive() {
        QByteArray header;
        header = m_stream->read(sizeof(uint));
        while (header.length() < sizeof(uint)) {
            m_stream->waitForReadyRead(-1);
            header.append(m_stream->read(sizeof(uint) - header.length()));
        }
        uint len = *(uint *) header.data();
        while (len > 0) {
            QByteArray chunk = m_stream->read(len);
            len -= chunk.length();
            this->writeRawData(chunk.data(), chunk.length());
        }
        device()->reset();
    }

    void SafeDataStream::send() {
        device()->reset();
        QByteArray data = device()->readAll();
        uint len = data.length();
        m_stream->write((char *) &len, sizeof(uint));
        m_stream->write(data.data(), data.length());
    }

}
