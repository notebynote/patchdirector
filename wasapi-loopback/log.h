// log.h

#include <QString>
#include <QDebug>

static QString logFormat(const wchar_t* format, ...)
{
    wchar_t buffer[1001] = L"";
    va_list argptr;
    va_start(argptr, format);
    _vsnwprintf(buffer, 1000, format, argptr);
    va_end(argptr);
    return QString::fromWCharArray(buffer);
}

#define LOG(format, ...) qDebug() << logFormat(format, __VA_ARGS__)
#define ERR(format, ...) qCritical() << logFormat(L"Error: " format, __VA_ARGS__)
