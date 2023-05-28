#include "audiorecorder.h"

#include <QMessageBox>
#include <QCoreApplication>
#include <QSettings>
#include <QFile>
#include <QDebug>
#include <cmath>
#ifdef Q_OS_OSX
    #include <CoreAudio/AudioHardware.h>
#endif
#include <lame/lame.h>
#ifdef Q_OS_WIN
    #include "wasapiloopback.h"
#endif

class BiquadFilter
{
public:
    double a1 = 0, a2 = 0;
    double b0 = 0, b1 = 0, b2 = 0;
    double x1 = 0, x2 = 0;
    double y1 = 0, y2 = 0;

    void reset() { x1 = 0; x2 = 0; y1 = 0; y2 = 0; }

    double apply(double x)
    {
        double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return y;
    }
};

#ifdef Q_OS_OSX

static AudioDeviceID origDefaultOutputDeviceID = kAudioObjectUnknown;

static QVector<AudioDeviceID> systemOutputDeviceIDs()
{
    QVector<AudioDeviceID> devs;
    AudioObjectPropertyAddress propertyAddress = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    UInt32 size = 0;
    OSStatus result = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &size);
    if (result || !size) return devs;
    QVector<AudioDeviceID> ids;
    ids.resize(size / sizeof(AudioDeviceID));
    size = ids.count() * sizeof(AudioDeviceID);
    result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &size, ids.data());
    if (result) return devs;
    propertyAddress.mSelector = kAudioDevicePropertyStreams;
    propertyAddress.mScope = kAudioDevicePropertyScopeOutput;
    foreach (AudioDeviceID id, ids) {
            size = 0;
            result = AudioObjectGetPropertyDataSize(id, &propertyAddress, 0, NULL, &size);
            if (!result && size > 0) devs.append(id);
    }
    return devs;
}

static QString systemDeviceName(AudioDeviceID id)
{
    AudioObjectPropertyAddress propertyAddress = {
        kAudioObjectPropertyName,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    CFStringRef name = NULL;
    UInt32 size = sizeof(name);
    OSStatus result = AudioObjectGetPropertyData(id, &propertyAddress, 0, NULL, &size, &name);
    if (result) return "";
    QString s = QString::fromCFString(name);
    CFRelease(name);
    return s;
}

static AudioDeviceID systemOutputDeviceID(const QString& name)
{
    if (name.isEmpty()) return kAudioObjectUnknown;
    foreach (AudioDeviceID id, systemOutputDeviceIDs()) {
        if (name == systemDeviceName(id)) return id;
    }
    return kAudioObjectUnknown;
}

static AudioDeviceID defaultOutputDeviceID()
{
    AudioObjectPropertyAddress propertyAddress = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    AudioDeviceID id = kAudioObjectUnknown;
    UInt32 size = sizeof(id);
    OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &size, &id);
    if (result) return kAudioObjectUnknown;
    return id;
}

static bool setDefaultOutputDeviceID(AudioDeviceID id)
{
    if (id == kAudioObjectUnknown) return false;
    AudioObjectPropertyAddress propertyAddress = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    OSStatus result = AudioObjectSetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, sizeof(id), &id);
    return (!result);
}


QStringList AudioRecorder::systemOutputDeviceNames()
{
    QStringList names;
    foreach (AudioDeviceID id, systemOutputDeviceIDs()) {
        QString name = systemDeviceName(id);
        if (!name.isEmpty()) names.append(name);
    }
    return names;
}

QString AudioRecorder::defaultOutputDeviceName()
{
    QSettings settings;
    return settings.value("RecorderDefaultOutputDevice").toString();
}

void AudioRecorder::setDefaultOutputDeviceName(const QString &name)
{
    QSettings settings;
    settings.setValue("RecorderDefaultOutputDevice", name);
}

#endif

#ifdef Q_OS_WIN

static const char loopbackSuffix[] = " (Loopback)";

QStringList loopbackDeviceNames()
{
    QStringList names = WasapiLoopback::deviceNames();
    for (auto &name : names) {
        name += loopbackSuffix;
    }
    return names;
}

static bool getWasapiDeviceName(const QString& loopbackName, QString& wasapiName)
{
    if (loopbackName.right(sizeof(loopbackSuffix) - 1) != loopbackSuffix) return false;
    wasapiName = loopbackName.left(loopbackName.length() - (sizeof(loopbackSuffix) - 1));
    foreach (const QString& name, WasapiLoopback::deviceNames()) {
        if (name == wasapiName) return true;
    }
    wasapiName.clear();
    return false;
}

#endif

QStringList AudioRecorder::inputDeviceNames()
{
    QStringList list;
    foreach (const QAudioDeviceInfo& info, QAudioDeviceInfo::availableDevices(QAudio::AudioInput)) {
        list.append(info.deviceName());
    }
    list.sort(Qt::CaseInsensitive);
#ifdef Q_OS_WIN
    QStringList list2 = loopbackDeviceNames();
    list2.sort(Qt::CaseInsensitive);
    list = list2 + list;
#endif
    return list;
}

QString AudioRecorder::inputDeviceName()
{
    QSettings settings;
    return settings.value("RecorderInputDevice").toString();
}

void AudioRecorder::setInputDeviceName(const QString &name)
{
    QSettings settings;
    settings.setValue("RecorderInputDevice", name);
}

int AudioRecorder::MP3Bitrate()
{
    QSettings settings;
    int bitrate = settings.value("RecorderMP3Bitrate").toInt();
    if (bitrate <= 0) bitrate = 128;
    return bitrate;
}

void AudioRecorder::setMP3Bitrate(int bitrate)
{
    QSettings settings;
    return settings.setValue("RecorderMP3Bitrate", bitrate);
}

AudioRecorder::AudioRecorder(QWidget *parent) : QIODevice(parent), parent(parent)
{
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleSize(32);
    format.setSampleType(QAudioFormat::Float);
    format.setCodec("audio/pcm");
    buffer.resize(format.sampleRate() * format.channelCount() * 60);
}

AudioRecorder::State AudioRecorder::state() const
{
    if (audioInput || audioLoopback) return Recording;
    if (audioOutput) return Playing;
    return Stopped;
}

bool AudioRecorder::hasWatermark() const
{
    return watermarkEnabled;
}

void AudioRecorder::record(bool watermark)
{
    stop();
    QAudioDeviceInfo inputDevice = QAudioDeviceInfo::defaultInputDevice();
    QString name = inputDeviceName();
    bool input = false;
    if (!name.isEmpty()) {
        foreach (const QAudioDeviceInfo& dev, QAudioDeviceInfo::availableDevices(QAudio::AudioInput)) {
            if (dev.deviceName() == name) {
                inputDevice = dev;
                input = true;
                break;
            }
        }
    }
    watermarkEnabled = watermark;
#ifdef Q_OS_WIN
    QString wasapiName;
    if (name.isEmpty() || getWasapiDeviceName(name, wasapiName) || !input) {
        audioLoopback = new WasapiLoopback(wasapiName, format, this);
        connect(audioLoopback, SIGNAL(stateChanged(QAudio::State)), SLOT(inputStateChanged(QAudio::State)));
        open(ReadWrite);
        bufferPos = 0;
        bufferEnd = buffer.size();
        audioLoopback->start(this);
        timer = new QTimer(this);
        connect(timer, SIGNAL(timeout()), SLOT(internalNotify()));
        timer->start(100);
        return;
    }
#endif
#ifdef Q_OS_OSX
    origDefaultOutputDeviceID = defaultOutputDeviceID();
    setDefaultOutputDeviceID(systemOutputDeviceID(defaultOutputDeviceName()));
#endif
    audioInput = new QAudioInput(inputDevice, format, this);
    audioInput->setBufferSize(format.bytesForDuration(100000));
    audioInput->setNotifyInterval(100);
    connect(audioInput, SIGNAL(stateChanged(QAudio::State)), SLOT(inputStateChanged(QAudio::State)));
    connect(audioInput, SIGNAL(notify()), SLOT(internalNotify()));
    open(ReadWrite);
    bufferPos = 0;
    bufferEnd = buffer.size();
    audioInput->start(this);
}

void AudioRecorder::play()
{
    stop();
    if (bufferPos >= bufferEnd) return;
    audioOutput = new QAudioOutput(format, this);
    audioOutput->setBufferSize(format.bytesForDuration(100000));
    audioOutput->setNotifyInterval(100);
    connect(audioOutput, SIGNAL(stateChanged(QAudio::State)), SLOT(outputStateChanged(QAudio::State)));
    connect(audioOutput, SIGNAL(notify()), SLOT(internalNotify()));
    open(ReadWrite);
    audioOutput->start(this);
}

void AudioRecorder::stop()
{
    if (!audioInput && !audioLoopback && !audioOutput) return;
    if (timer) {
        timer->stop();
        timer->deleteLater();
        timer = Q_NULLPTR;
    }
    if (audioInput) {
        QAudioInput* ai = audioInput;
        audioInput = Q_NULLPTR;
        bufferEnd = bufferPos;
        bufferPos = 0;
        postProcess();
        ai->disconnect();
        ai->stop();
        ai->deleteLater();
#ifdef Q_OS_OSX
        setDefaultOutputDeviceID(origDefaultOutputDeviceID);
#endif
    }
#ifdef Q_OS_WIN
    if (audioLoopback) {
        WasapiLoopback* al = audioLoopback;
        audioLoopback = Q_NULLPTR;
        bufferEnd = bufferPos;
        bufferPos = 0;
        postProcess();
        al->disconnect();
        al->stop();
        al->deleteLater();
    }
#endif
    if (audioOutput) {
        QAudioOutput* ao = audioOutput;
        audioOutput = Q_NULLPTR;
        if (bufferPos >= bufferEnd) bufferPos = 0;
        ao->disconnect();
        ao->stop();
        ao->deleteLater();
    }
    close();
    emit notify();
}

void AudioRecorder::clear()
{
    stop();
    bufferPos = 0;
    bufferEnd = 0;
    watermarkEnabled = false;
}

int AudioRecorder::position() const
{
    QMutexLocker locker(&ioMutex);
    return (int)((qint64)bufferPos * 1000 / (format.sampleRate() * format.channelCount()));
}

int AudioRecorder::maxPosition() const
{
    return (int)((qint64)bufferEnd * 1000 / (format.sampleRate() * format.channelCount()));
}

void AudioRecorder::setPosition(int pos)
{
    QMutexLocker locker(&ioMutex);
    if (audioInput || audioLoopback) return;
    int i = (int)((qint64)pos * format.sampleRate() * format.channelCount() / 1000);
    if (i > bufferEnd) i = bufferEnd;
    bufferPos = i;
}

bool AudioRecorder::saveMP3(const QString &name)
{
    lame_global_flags* lame = lame_init();
    if (!lame) return false;
    lame_set_in_samplerate(lame, format.sampleRate());
    lame_set_num_channels(lame, 2);
    //lame_set_quality(lame, 2);
    lame_set_mode(lame, JOINT_STEREO);
    lame_set_brate(lame, MP3Bitrate());
    lame_init_params(lame);
    QVector<float> left, right;
    left.resize(format.sampleRate());
    right.resize(left.size());
    QByteArray mp3Buf;
    mp3Buf.resize(1.25 * left.size() + 7200);
    QFile file(name);
    file.remove();
    bool ok = file.open(QIODevice::WriteOnly | QIODevice::Truncate);
    int pos = 0;
    while (ok && pos < bufferEnd) {
        int inSize = (bufferEnd - pos) / 2;
        if (inSize <= 0) break;
        if (inSize > left.size()) inSize = left.size();
        for (int i = 0; i < inSize; ++i) {
            left[i] = buffer[pos++] * 32768;
            right[i] = buffer[pos++] * 32768;
        }
        int outSize = lame_encode_buffer_float(
            lame, left.data(), right.data(), inSize, (unsigned char*)mp3Buf.data(), mp3Buf.size());
        ok = outSize >= 0;
        if (ok && file.write(mp3Buf.data(), outSize) != outSize) ok = false;
    }
    if (ok) {
        int outSize = lame_encode_flush(lame, (unsigned char*)mp3Buf.data(), mp3Buf.size());
        ok = outSize > 0;
        if (ok && file.write(mp3Buf.data(), outSize) != outSize) ok = false;
    }
    lame_close(lame);
    return ok;
}

qint64 AudioRecorder::readData(char *data, qint64 len)
{
    QMutexLocker locker(&ioMutex);
    if (!audioOutput || bufferPos >= bufferEnd) return 0; //-1;
    float* floatData = (float*)data;
    int floatLen = (int)(len / 4);
    if (floatLen > bufferEnd - bufferPos) floatLen = bufferEnd - bufferPos;
    qCopy(buffer.begin() + bufferPos, buffer.begin() + bufferPos + floatLen, floatData);
    bufferPos += floatLen;
    return floatLen * 4;
}

qint64 AudioRecorder::writeData(const char *data, qint64 len)
{
    QMutexLocker locker(&ioMutex);
    if (!audioInput && !audioLoopback || bufferPos >= bufferEnd) return len;
    const float* floatData = (const float*)data;
    int floatLen = (int)(len / 4);
    if (bufferPos == 0) {
        int i = 0;
        for (i = 0; i < floatLen; ++i) {
            if (std::abs(floatData[i]) > 0.0001) break;
        }
        if (i >= floatLen) return len;
        floatData += i;
        floatLen -= i;
    }
    if (floatLen > bufferEnd - bufferPos) floatLen = bufferEnd - bufferPos;
    qCopy(floatData, floatData + floatLen, buffer.begin() + bufferPos);
    bufferPos += floatLen;
    return len;
}

void AudioRecorder::postProcess()
{
    normalize();
    if (watermarkEnabled) addWatermark();
    fade();
}

void AudioRecorder::normalize()
{
    BiquadFilter preFilter;
    preFilter.a1 = -1.66365511325602;
    preFilter.a2 = 0.7125954280732254;
    preFilter.b0 = 1.530841230050348;
    preFilter.b1 = -2.650979995154729;
    preFilter.b2 = 1.169079079921587;

    BiquadFilter weightFilter;
    weightFilter.a1 = -1.989169673629796;
    weightFilter.a2 = 0.9891990357870394;
    weightFilter.b0 = 0.9995600645425144;
    weightFilter.b1 = -1.999120129085029;
    weightFilter.b2 = 0.9995600645425144;

    int channels = format.channelCount();
    int window = format.sampleRate() * 3;
    int samples = bufferEnd / channels;
    if (samples < 1) return;
    if (window > samples) window = samples;
    double peak = 0;
    QVector<double> volBuf;
    volBuf.fill(0, window);
    int volBufPos = 0;
    double maxVol = 0;
    double curVol = 0;
    int pos = 0;
    for (int i = 0; i < samples; ++i) {
        double d = 0;
        for (int j = 0; j < channels; ++j) {
            double s = buffer[pos++];
            peak = std::max(peak, std::abs(s));
            d += s;
        }
        d /= channels;
        d = weightFilter.apply(preFilter.apply(d));
        d = d * d;
        curVol += d - volBuf[volBufPos];
        if (curVol > maxVol) maxVol = curVol;
        volBuf[volBufPos++] = d;
        if (volBufPos >= volBuf.size()) volBufPos = 0;
    }
    maxVol = std::sqrt(maxVol / window);
    if (maxVol < 1e-9 || peak < 1e-9) return;
    double vol = std::min(0.071 / maxVol, 0.9 / peak);
    //qDebug() << maxVol << " " << peak << " -> " << vol << " (" << 0.9 / peak << ")";
    if (vol > 32) vol = 32;
    for (int i = 0; i < bufferEnd; ++i)  buffer[i] *= vol;
}

void AudioRecorder::addWatermark()
{
    if (watermark.isEmpty()) {
        QFile file(":/res/watermark.pcm");
        if (!file.open(QIODevice::ReadOnly)) return;
        watermark.resize(file.size() / sizeof(qint16));
        file.read((char*)watermark.data(), watermark.size() * sizeof(qint16));
        file.close();
        if (watermark.isEmpty()) return;
    }
    int pos = format.sampleRate() * format.channelCount() / 2;
    while (pos < bufferEnd) {
        for (int i = 0; i < watermark.size() && pos < bufferEnd; ++i) {
            float wm = watermark[i] / 32768.f;
            for (int j = 0; j < format.channelCount() && pos < bufferEnd; ++j) {
                float f = buffer[pos] + wm;
                if (f > 1.) f = 1.;
                if (f < -1.) f = -1.;
                buffer[pos++] = f;
            }
        }
    }
}

void AudioRecorder::fade()
{
    int fadein = format.sampleRate() * format.channelCount() / 100;
    if (fadein > bufferEnd) fadein = bufferEnd;
    for (int i = 0; i < fadein; ++i) buffer[i] *= (float)i / fadein;
    int fadeout = format.sampleRate() * format.channelCount() * 2;
    if (fadeout > bufferEnd) fadeout = bufferEnd;
    for (int i = fadeout; i > 0; --i) buffer[bufferEnd - i] *= (float)i / fadeout;
}

void AudioRecorder::inputStateChanged(QAudio::State state)
{
    if (!audioInput && !audioLoopback) return;
    if (state == QAudio::IdleState) {
        if (bufferPos >= bufferEnd) stop();
    } else if (state == QAudio::StoppedState) {
        if (audioInput && audioInput->error() != QAudio::NoError
#ifdef Q_OS_WIN
            || audioLoopback && audioLoopback->error() != QAudio::NoError
#endif
        ) {
            QMessageBox::critical(parent, QCoreApplication::applicationName(),
                tr("An error occured during audio recording."));
        }
        stop();
    }
}

void AudioRecorder::outputStateChanged(QAudio::State state)
{
    if (!audioOutput) return;
    if (state == QAudio::IdleState) {
        stop();
    } else if (state == QAudio::StoppedState) {
        if (bufferPos < bufferEnd && audioOutput->error() != QAudio::NoError) {
            QMessageBox::critical(parent, QCoreApplication::applicationName(),
                tr("An error occured during audio playback."));
        }
        stop();
    }
}

void AudioRecorder::internalNotify()
{
    if (audioInput || audioLoopback) {
        if (bufferPos >= bufferEnd) stop();
    }
    emit notify();
}
