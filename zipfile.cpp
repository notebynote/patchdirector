#include "zipfile.h"

#include <QFile>
#include <zip.h>
#include <unzip.h>
#include <ioapi.h>
#include <string.h>

ZipFile::ZipFile()
{
}

ZipFile::~ZipFile()
{
    close();
}

QString ZipFile::name() const
{
    return filename;
}

bool ZipFile::isOpen() const
{
    return !!zip || !!unz;
}

bool ZipFile::create(const QString &name)
{
    close();
    zip = zipOpen64(name.toUtf8().data(), 0);
    if (!zip) return false;
    filename = name;
    dateTime = QDateTime::currentDateTime();
    return true;
}

bool ZipFile::open(const QString &name)
{
    close();
    unz = unzOpen64(name.toUtf8().data());
    if (!unz) return false;
    filename = name;
    return true;
}

void ZipFile::close()
{
    filename.clear();
    if (zip) zipClose(zip, Q_NULLPTR);
    zip = Q_NULLPTR;
    if (unz) unzClose(unz);
    unz = Q_NULLPTR;
}

bool ZipFile::addFile(const QString &name, const QByteArray &data)
{
    if (!zip) return false;
    zip_fileinfo info;
    memset(&info, 0, sizeof(info));
    QTime t = dateTime.time();
    QDate d = dateTime.date();
    info.tmz_date.tm_sec  = t.second();
    info.tmz_date.tm_min  = t.minute();
    info.tmz_date.tm_hour = t.hour();
    info.tmz_date.tm_mday = d.day();
    info.tmz_date.tm_mon  = d.month() - 1;
    info.tmz_date.tm_year = d.year() - 1900;
    int err = zipOpenNewFileInZip4(
         zip, name.toUtf8().data(), &info, Q_NULLPTR, 0, Q_NULLPTR, 0, Q_NULLPTR, Z_DEFLATED, Z_DEFAULT_COMPRESSION,
         0, -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY, Q_NULLPTR, 0, 0, 1 << 11);
    if (err != ZIP_OK) return false;
    err = zipWriteInFileInZip(zip, data.data(), data.size());
    zipCloseFileInZip(zip);
    return err == ZIP_OK;
}

bool ZipFile::addFile(const QString &name, const QString &sourceName)
{
    QFile file(sourceName);
    if (!file.open(QIODevice::ReadOnly)) return false;
    return addFile(name, file.readAll());
}

bool ZipFile::extractFile(const QString &name, QByteArray &data)
{
    data.clear();
    if (!unz) return false;
    if (unzLocateFile(unz, name.toUtf8().data(), 1) != UNZ_OK) return false;
    unz_file_info info;
    memset(&info, 0, sizeof(info));
    if (unzGetCurrentFileInfo(unz, &info, Q_NULLPTR, 0, Q_NULLPTR, 0, Q_NULLPTR, 0) != UNZ_OK) return false;
    if (unzOpenCurrentFile(unz) != UNZ_OK) return false;
    data.resize(info.uncompressed_size);
    int res = unzReadCurrentFile(unz, data.data(), data.size());
    unzCloseCurrentFile(unz);
    if (res != data.size() || !unzeof(unz)) {
        data.clear();
        return false;
    }
    return true;
}

bool ZipFile::extractFile(const QString &name, const QString &destName)
{
    QByteArray data;
    if (!extractFile(name, data)) return false;
    QFile file(destName);
    file.remove();
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    if (file.write(data) != data.size()) return false;
    return true;
}
