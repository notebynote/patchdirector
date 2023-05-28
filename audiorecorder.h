#ifndef AUDIORECORDER_H
#define AUDIORECORDER_H

#include <QWidget>
#include <QIODevice>
#include <QAudioInput>
#include <QAudioOutput>
#include <QTimer>
#include <QVector>
#include <QStringList>
#include <QMutex>

#ifdef Q_OS_WIN
class WasapiLoopback;
#endif

class AudioRecorder : public QIODevice
{
    Q_OBJECT
public:
    enum State {
        Recording,
        Playing,
        Stopped
    };

#ifdef Q_OS_OSX
    static QStringList systemOutputDeviceNames();
    static QString defaultOutputDeviceName();
    static void setDefaultOutputDeviceName(const QString& name);
#endif

    static QStringList inputDeviceNames();
    static QString inputDeviceName();
    static void setInputDeviceName(const QString& name);

    static int MP3Bitrate();
    static void setMP3Bitrate(int bitrate);

    explicit AudioRecorder(QWidget *parent = 0);

    State state() const;
    bool hasWatermark() const;

    void record(bool watermark = false);
    void play();
    void stop();
    void clear();

    int position() const;
    int maxPosition() const;
    void setPosition(int pos);

    bool saveMP3(const QString& name);

signals:
    void notify();

public slots:

protected:
    virtual qint64 readData(char *data, qint64 len);
    virtual qint64 writeData(const char *data, qint64 len);

private:
    QWidget* parent = Q_NULLPTR;
    QAudioFormat format;
    QAudioInput* audioInput = Q_NULLPTR;
#ifdef Q_OS_WIN
    WasapiLoopback* audioLoopback = Q_NULLPTR;
#else
    QObject* audioLoopback = Q_NULLPTR;
#endif
    QAudioOutput* audioOutput = Q_NULLPTR;
    QTimer* timer = Q_NULLPTR;
    mutable QMutex ioMutex;
    QVector<float> buffer;
    int bufferPos = 0;
    int bufferEnd = 0;
    QVector<qint16> watermark;
    bool watermarkEnabled = false;

    void postProcess();
    void normalize();
    void addWatermark();
    void fade();

private slots:
    void inputStateChanged(QAudio::State state);
    void outputStateChanged(QAudio::State state);
    void internalNotify();
};

#endif // AUDIORECORDER_H
