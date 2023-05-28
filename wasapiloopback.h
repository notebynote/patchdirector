#ifndef WASAPILOOPBACK_H
#define WASAPILOOPBACK_H

#include <QIODevice>
#include <QAudioOutput>
#include <QStringList>
#include <QMutex>

struct LoopbackCaptureThreadFunctionArguments;

class WasapiLoopback : public QIODevice
{
    Q_OBJECT
public:
    static QStringList deviceNames();

    WasapiLoopback(const QString name, QAudioFormat& format, QObject *parent = Q_NULLPTR);
    virtual ~WasapiLoopback();

    void start(QIODevice *device);
    void stop();

    QAudio::Error error() const;
    QAudio::State state() const;

signals:
    void stateChanged(QAudio::State);

protected:
    virtual qint64 readData(char *data, qint64 len);
    virtual qint64 writeData(const char *data, qint64 len);

private:
    QString deviceName;
    QAudioFormat audioFormat;
    QAudioOutput* audioOutput = Q_NULLPTR;
    LoopbackCaptureThreadFunctionArguments* threadArgs = Q_NULLPTR;
    QAudio::State currentState = QAudio::StoppedState;
    QAudio::Error lastError = QAudio::NoError;
    mutable QMutex stateMutex;

    void setState(QAudio::State newState);

public:
    void onThreadEnd(QAudio::Error result);
};

#endif // WASAPILOOPBACK_H
