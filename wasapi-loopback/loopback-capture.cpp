// loopback-capture.cpp

#include "common.h"
#include <QIODevice>
#include "../wasapiloopback.h"
#include "../soxr/soxr-lsr.h"

class DeleteSrcOnExit {
public:
    DeleteSrcOnExit(SRC_STATE* src) : m_src(src) {}
    ~DeleteSrcOnExit() {
        src_delete(m_src);
    }
private:
    SRC_STATE* m_src;
};

HRESULT LoopbackCapture(
    IMMDevice *pMMDevice,
    HANDLE hStartedEvent,
    HANDLE hStopEvent,
    PUINT32 pnFrames,
    QIODevice* ioDevice,
    QAudioFormat* format
);

DWORD WINAPI LoopbackCaptureThreadFunction(LPVOID pContext) {
    LoopbackCaptureThreadFunctionArguments *pArgs =
        (LoopbackCaptureThreadFunctionArguments*)pContext;

    pArgs->hr = CoInitialize(NULL);
    if (FAILED(pArgs->hr)) {
        ERR(L"CoInitialize failed: hr = 0x%08x", pArgs->hr);
        pArgs->parent->onThreadEnd(QAudio::FatalError);
        return 0;
    }
    CoUninitializeOnExit cuoe;

    pArgs->hr = LoopbackCapture(
        pArgs->pMMDevice,
        pArgs->hStartedEvent,
        pArgs->hStopEvent,
        &pArgs->nFrames,
        pArgs->ioDevice,
        pArgs->format
    );

    pArgs->parent->onThreadEnd(FAILED(pArgs->hr) ? QAudio::FatalError : QAudio::NoError);
    return 0;
}

HRESULT LoopbackCapture(
    IMMDevice *pMMDevice,
    HANDLE hStartedEvent,
    HANDLE hStopEvent,
    PUINT32 pnFrames,
    QIODevice* ioDevice,
    QAudioFormat* format
) {
    HRESULT hr;

    // activate an IAudioClient
    IAudioClient *pAudioClient;
    hr = pMMDevice->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL, NULL,
        (void**)&pAudioClient
    );
    if (FAILED(hr)) {
        ERR(L"IMMDevice::Activate(IAudioClient) failed: hr = 0x%08x", hr);
        return hr;
    }
    ReleaseOnExit releaseAudioClient(pAudioClient);
    
    // get the default device periodicity
    REFERENCE_TIME hnsDefaultDevicePeriod;
    hr = pAudioClient->GetDevicePeriod(&hnsDefaultDevicePeriod, NULL);
    if (FAILED(hr)) {
        ERR(L"IAudioClient::GetDevicePeriod failed: hr = 0x%08x", hr);
        return hr;
    }

    // get the default device format
    WAVEFORMATEX *pmixwfx;
    hr = pAudioClient->GetMixFormat(&pmixwfx);
    if (FAILED(hr)) {
        ERR(L"IAudioClient::GetMixFormat failed: hr = 0x%08x", hr);
        return hr;
    }
    CoTaskMemFreeOnExit freeMixFormat(pmixwfx);

    WAVEFORMATEX *pwfx = pmixwfx;
    bool float32 = false;
    switch (pwfx->wFormatTag) {
        case WAVE_FORMAT_IEEE_FLOAT:
            float32 = true;
            break;
        case WAVE_FORMAT_EXTENSIBLE:
            {
                // naked scope for case-local variable
                PWAVEFORMATEXTENSIBLE pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(pwfx);
                if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat)) {
                    float32 = true;
                }
            }
            break;
    }
    if (pwfx->wBitsPerSample != 32) float32 = false;
    LOG(L"IAudioCaptureClient::GetMixFormat: Channels=%d, SamplesPerSec=%d, Float32=%d",
        (int)pwfx->nChannels, (int)pwfx->nSamplesPerSec, (int)float32);
    WAVEFORMATEX wfx;
    if (!float32) {
        memset(&wfx, 0, sizeof(wfx));
        wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        wfx.nChannels = pwfx->nChannels;
        wfx.nSamplesPerSec = pwfx->nSamplesPerSec;
        wfx.wBitsPerSample = 32;
        wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
        wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
        wfx.cbSize = 0;
        pwfx = &wfx;
    }

    // create a periodic waitable timer
    HANDLE hWakeUp = CreateWaitableTimer(NULL, FALSE, NULL);
    if (NULL == hWakeUp) {
        DWORD dwErr = GetLastError();
        ERR(L"CreateWaitableTimer failed: last error = %u", dwErr);
        return HRESULT_FROM_WIN32(dwErr);
    }
    CloseHandleOnExit closeWakeUp(hWakeUp);

    UINT32 nBlockAlign = pwfx->nBlockAlign;
    *pnFrames = 0;

    int channelCount = format->channelCount();
    QVector<float> bufferIn, bufferOut;
    bufferIn.resize((pwfx->nSamplesPerSec / 10) * channelCount);
    bufferOut.resize((format->sampleRate() / 10) * channelCount + 100);

    SRC_ERROR srcErr = 0;
    SRC_STATE* src = src_new(SRC_SINC_BEST_QUALITY, channelCount, &srcErr);
    if (!src) {
        qCritical() << "src_new failed: " << src_strerror(srcErr);
        return E_FAIL;
    }
    DeleteSrcOnExit deleteSrc(src);

    SRC_DATA srcData = {0};
    srcData.src_ratio = (double)format->sampleRate() / (double)pwfx->nSamplesPerSec;
    srcData.data_in = bufferIn.data();
    srcData.data_out = bufferOut.data();
    srcData.output_frames = 0;
    srcData.output_frames = bufferOut.size() / channelCount;
    srcData.input_frames_used = 0;
    srcData.output_frames_gen = 0;
    srcData.end_of_input = 0;

    // call IAudioClient::Initialize
    // note that AUDCLNT_STREAMFLAGS_LOOPBACK and AUDCLNT_STREAMFLAGS_EVENTCALLBACK
    // do not work together...
    // the "data ready" event never gets set
    // so we're going to do a timer-driven loop
    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        0, 0, pwfx, 0
    );
    if (FAILED(hr)) {
        ERR(L"IAudioClient::Initialize failed: hr = 0x%08x", hr);
        return hr;
    }

    // activate an IAudioCaptureClient
    IAudioCaptureClient *pAudioCaptureClient;
    hr = pAudioClient->GetService(
        __uuidof(IAudioCaptureClient),
        (void**)&pAudioCaptureClient
    );
    if (FAILED(hr)) {
        ERR(L"IAudioClient::GetService(IAudioCaptureClient) failed: hr = 0x%08x", hr);
        return hr;
    }
    ReleaseOnExit releaseAudioCaptureClient(pAudioCaptureClient);
    
    // register with MMCSS
    DWORD nTaskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristics(L"Audio", &nTaskIndex);
    if (NULL == hTask) {
        DWORD dwErr = GetLastError();
        ERR(L"AvSetMmThreadCharacteristics failed: last error = %u", dwErr);
        return HRESULT_FROM_WIN32(dwErr);
    }
    AvRevertMmThreadCharacteristicsOnExit unregisterMmcss(hTask);

    // set the waitable timer
    LARGE_INTEGER liFirstFire;
    liFirstFire.QuadPart = -hnsDefaultDevicePeriod / 2; // negative means relative time
    LONG lTimeBetweenFires = (LONG)hnsDefaultDevicePeriod / 2 / (10 * 1000); // convert to milliseconds
    BOOL bOK = SetWaitableTimer(
        hWakeUp,
        &liFirstFire,
        lTimeBetweenFires,
        NULL, NULL, FALSE
    );
    if (!bOK) {
        DWORD dwErr = GetLastError();
        ERR(L"SetWaitableTimer failed: last error = %u", dwErr);
        return HRESULT_FROM_WIN32(dwErr);
    }
    CancelWaitableTimerOnExit cancelWakeUp(hWakeUp);
    
    // call IAudioClient::Start
    hr = pAudioClient->Start();
    if (FAILED(hr)) {
        ERR(L"IAudioClient::Start failed: hr = 0x%08x", hr);
        return hr;
    }
    AudioClientStopOnExit stopAudioClient(pAudioClient);

    SetEvent(hStartedEvent);
    
    // loopback capture loop
    HANDLE waitArray[2] = { hStopEvent, hWakeUp };
    DWORD dwWaitResult;

    bool bDone = false;
    bool bFirstPacket = true;
    for (UINT32 nPasses = 0; !bDone; nPasses++) {
        // drain data while it is available
        UINT32 nNextPacketSize;
        for (
            hr = pAudioCaptureClient->GetNextPacketSize(&nNextPacketSize);
            SUCCEEDED(hr) && nNextPacketSize > 0;
            hr = pAudioCaptureClient->GetNextPacketSize(&nNextPacketSize)
        ) {
            // get the captured data
            BYTE *pData;
            UINT32 nNumFramesToRead;
            DWORD dwFlags;

            hr = pAudioCaptureClient->GetBuffer(
                &pData,
                &nNumFramesToRead,
                &dwFlags,
                NULL,
                NULL
                );
            if (FAILED(hr)) {
                ERR(L"IAudioCaptureClient::GetBuffer failed on pass %u after %u frames: hr = 0x%08x", nPasses, *pnFrames, hr);
                return hr;
            }

            if (dwFlags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) {
                LOG(L"IAudioCaptureClient::GetBuffer: DATA_DISCONTINUITY on pass %u after %u frames", nPasses, *pnFrames);
            }
            bool silent = (dwFlags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;

            if (0 == nNumFramesToRead) {
                ERR(L"IAudioCaptureClient::GetBuffer said to read 0 frames on pass %u after %u frames", nPasses, *pnFrames);
                return E_UNEXPECTED;
            }

            UINT32 nFrame = 0;
            while (nFrame < nNumFramesToRead) {
                int posIn = 0;
                while (nFrame < nNumFramesToRead && posIn <= bufferIn.size() - channelCount) {
                    if (silent) {
                        for (int i = 0; i < channelCount; ++i) bufferIn[posIn++] = 0;
                    } else {
                        const float* sample = (float*)(pData + nFrame * nBlockAlign);
                        for (int i = 0; i < channelCount; ++i) {
                            bufferIn[posIn++] = sample[qMin(i, (int)(pwfx->nChannels - 1))];
                        }
                    }
                    ++nFrame;
                }
                srcData.input_frames = posIn / channelCount;
                srcData.input_frames_used = 0;
                srcData.output_frames_gen = 0;
                srcErr = src_process(src, &srcData);
                if (srcErr) {
                    qCritical() << "src_process failed: " << src_strerror(srcErr);
                    return E_FAIL;
                }
                Q_ASSERT(srcData.input_frames_used == srcData.input_frames);
                ioDevice->write((const char*)bufferOut.data(), srcData.output_frames_gen * channelCount * sizeof(float));
            }
            *pnFrames += nNumFramesToRead;

            hr = pAudioCaptureClient->ReleaseBuffer(nNumFramesToRead);
            if (FAILED(hr)) {
                ERR(L"IAudioCaptureClient::ReleaseBuffer failed on pass %u after %u frames: hr = 0x%08x", nPasses, *pnFrames, hr);
                return hr;
            }

            bFirstPacket = false;
        }

        if (FAILED(hr)) {
            ERR(L"IAudioCaptureClient::GetNextPacketSize failed on pass %u after %u frames: hr = 0x%08x", nPasses, *pnFrames, hr);
            return hr;
        }

        dwWaitResult = WaitForMultipleObjects(
            ARRAYSIZE(waitArray), waitArray,
            FALSE, INFINITE
        );

        if (WAIT_OBJECT_0 == dwWaitResult) {
            LOG(L"Received stop event after %u passes and %u frames", nPasses, *pnFrames);
            bDone = true;
            continue; // exits loop
        }

        if (WAIT_OBJECT_0 + 1 != dwWaitResult) {
            ERR(L"Unexpected WaitForMultipleObjects return value %u on pass %u after %u frames", dwWaitResult, nPasses, *pnFrames);
            return E_UNEXPECTED;
        }
    } // capture loop

    return S_OK;
}
