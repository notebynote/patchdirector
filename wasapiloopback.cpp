#include "wasapiloopback.h"

#include "wasapi-loopback/common.h"

QStringList WasapiLoopback::deviceNames()
{
    QStringList names;
    list_devices(names);
    return names;
}

WasapiLoopback::WasapiLoopback(const QString name, QAudioFormat &format, QObject *parent) :
    QIODevice(parent), deviceName(name), audioFormat(format)
{}

WasapiLoopback::~WasapiLoopback()
{
    stop();
}

void WasapiLoopback::start(QIODevice *device)
{
    if (!device) return;
    stop();
    if (state() != QAudio::StoppedState) return;
    lastError = QAudio::NoError;

    QAudioDeviceInfo outputDevice = QAudioDeviceInfo::defaultOutputDevice();
    bool found = false;
    if (!deviceName.isEmpty()) {
        foreach (const QAudioDeviceInfo& dev, QAudioDeviceInfo::availableDevices(QAudio::AudioOutput)) {
            if (dev.deviceName() == deviceName) {
                outputDevice = dev;
                found = true;
                break;
            }
        }
        if (!found) {
            foreach (const QAudioDeviceInfo& dev, QAudioDeviceInfo::availableDevices(QAudio::AudioOutput)) {
                QString name = dev.deviceName();
                if (name == deviceName.left(name.length())) {
                    outputDevice = dev;
                    found = true;
                    break;
                }
            }
        }
        if (!found) qWarning() << "WasapiLoopback::start: output device not found";
    }
    audioOutput = new QAudioOutput(outputDevice, audioFormat, this);
    audioOutput->setBufferSize(audioFormat.bytesForDuration(200000));
    open(ReadWrite);
    audioOutput->start(this);

    bool error = false;

    threadArgs = new LoopbackCaptureThreadFunctionArguments;
    threadArgs->pMMDevice = NULL;
    threadArgs->hStartedEvent = NULL;
    threadArgs->hStopEvent = NULL;
    threadArgs->hr = E_UNEXPECTED;
    threadArgs->hThread = NULL;
    threadArgs->ioDevice = device;
    threadArgs->format = &audioFormat;
    threadArgs->parent = this;

    setState(QAudio::ActiveState);
    if (!threadArgs) return;

    if (!deviceName.isEmpty()) {
        if (FAILED(get_specific_device(deviceName.toStdWString().c_str(), &threadArgs->pMMDevice))) {
            threadArgs->pMMDevice = NULL;
        }
    }
    if (!threadArgs->pMMDevice) {
        if (FAILED(get_default_device(&threadArgs->pMMDevice))) {
            threadArgs->pMMDevice = NULL;
            error = true;
        }
    }
    threadArgs->hStartedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == threadArgs->hStartedEvent) {
        ERR(L"CreateEvent failed: last error is %u", GetLastError());
        error = true;
    }
    threadArgs->hStopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == threadArgs->hStopEvent) {
        ERR(L"CreateEvent failed: last error is %u", GetLastError());
        error = true;
    }
    if (!error) {
        threadArgs->hThread = CreateThread(
            NULL, 0,
            LoopbackCaptureThreadFunction, threadArgs,
            0, NULL
        );
        if (NULL == threadArgs->hThread) {
            ERR(L"CreateThread failed: last error is %u", GetLastError());
            error = true;
        }
    }
    if (error) {
        lastError = QAudio::FatalError;
        setState(QAudio::StoppedState);
    }
}

void WasapiLoopback::stop()
{
    if (audioOutput) {
        audioOutput->stop();
        audioOutput->deleteLater();
        audioOutput = Q_NULLPTR;
        close();
    }
    if (threadArgs) {
        if (threadArgs->hThread) {
            if (!SetEvent(threadArgs->hStopEvent)) {
                ERR(L"SetEvent failed: last error is %d", GetLastError());
            }
            DWORD dwWaitResult = WaitForSingleObject(threadArgs->hThread, INFINITE);
            if (WAIT_OBJECT_0 != dwWaitResult) {
                ERR(L"WaitForSingleObject returned unexpected result 0x%08x, last error is %d", dwWaitResult, GetLastError());
            }
        }
        if (threadArgs->pMMDevice) threadArgs->pMMDevice->Release();
        CloseHandle(threadArgs->hStartedEvent);
        CloseHandle(threadArgs->hStopEvent);
        CloseHandle(threadArgs->hThread);
        delete threadArgs;
        threadArgs = Q_NULLPTR;
    }
    setState(QAudio::StoppedState);
}

QAudio::Error WasapiLoopback::error() const
{
    QMutexLocker locker(&stateMutex);
    return lastError;
}

QAudio::State WasapiLoopback::state() const
{
    QMutexLocker locker(&stateMutex);
    return currentState;
}

qint64 WasapiLoopback::readData(char *data, qint64 len)
{
    memset(data, 0, len);
    return len;
}

qint64 WasapiLoopback::writeData(const char *data, qint64 len)
{
    return -1;
}

void WasapiLoopback::setState(QAudio::State newState)
{
    QMutexLocker locker(&stateMutex);
    if (newState != currentState) {
        currentState = newState;
        locker.unlock();
        emit stateChanged(currentState);
    }
}

void WasapiLoopback::onThreadEnd(QAudio::Error result)
{
    {
        QMutexLocker locker(&stateMutex);
        lastError = result;
    }
    setState(QAudio::StoppedState);
}
