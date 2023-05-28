#ifndef ZIPFILE_H
#define ZIPFILE_H

#include <QString>
#include <QByteArray>
#include <QDateTime>

class ZipFile
{
public:
    ZipFile();
    ~ZipFile();

    QString name() const;

    bool isOpen() const;
    bool create(const QString& name);
    bool open(const QString& name);
    void close();

    bool addFile(const QString& name, const QByteArray& data);
    bool addFile(const QString& name, const QString& sourceName);

    bool extractFile(const QString& name, QByteArray& data);
    bool extractFile(const QString& name, const QString& destName);

private:
    QString filename;
    QDateTime dateTime;
    void* zip = Q_NULLPTR;
    void* unz = Q_NULLPTR;
};

#endif // ZIPFILE_H
