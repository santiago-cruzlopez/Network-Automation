#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <string>
#include <DeckLinkAPI.h>

class CaptureCallback : public IDeckLinkInputCallback {
public:
    virtual HRESULT STDMETHODCALLTYPE VideoInputFrameArrived(
        IDeckLinkVideoInputFrame *videoFrame,
        IDeckLinkAudioInputPacket *audioPacket) {
        if (videoFrame) {
            long width, height;
            videoFrame->GetWidth(&width);
            videoFrame->GetHeight(&height);
            std::cout << "Captured frame with shape (" << height << ", " << width << ", 3)" << std::endl;
            
            // Process the video frame here
            
            IDeckLinkMutableVideoFrame *outputFrame = NULL;
            hr = deckLinkOutput->CreateVideoFrame(
                width, height, width * 3, bmdFormat8BitYUV, bmdFrameFlagDefault, &outputFrame);
            if (SUCCEEDED(hr) && outputFrame) {
                // Copy the captured frame data to the output frame
                void *frameBytes;
                videoFrame->GetBytes(&frameBytes);
                void *outputBytes;
                outputFrame->GetBytes(&outputBytes);
                memcpy(outputBytes, frameBytes, width * height * 3);
                
                // Schedule the frame for playback
                BMDTimeValue displayTime = 0;
                BMDTimeScale timeScale = 10000000;
                deckLinkOutput->ScheduleVideoFrame(outputFrame, displayTime, 1, timeScale);
                outputFrame->Release();
            }
        }
        if (audioPacket) {
            // Process the audio packet here
            void *audioBytes;
            audioPacket->GetBytes(&audioBytes);
            uint32_t sampleFrameCount;
            audioPacket->GetSampleFrameCount(&sampleFrameCount);
            
            // Schedule the audio for playback
            deckLinkOutput->WriteAudioSamplesSync(audioBytes, sampleFrameCount, NULL);
        }
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(
        BMDVideoInputFormatChangedEvents notificationEvents,
        IDeckLinkDisplayMode *newDisplayMode,
        BMDDetectedVideoInputFormatFlags detectedSignalFlags) {
        return S_OK;
    }

    virtual ULONG STDMETHODCALLTYPE AddRef() { return 1; }
    virtual ULONG STDMETHODCALLTYPE Release() { return 1; }
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppv) {
        if (iid == IID_IDeckLinkInputCallback) {
            *ppv = static_cast<IDeckLinkInputCallback*>(this);
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    }

private:
    IDeckLinkOutput *deckLinkOutput;
    HRESULT hr;
};

int main() {
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize COM library." << std::endl;
        return 1;
    }

    IDeckLinkIterator *deckLinkIterator = NULL;
    hr = CoCreateInstance(
        CLSID_CDeckLinkIterator,
        NULL,
        CLSCTX_ALL,
        IID_IDeckLinkIterator,
        (void**)&deckLinkIterator);
    if (FAILED(hr)) {
        std::cerr << "Failed to create DeckLink iterator." << std::endl;
        CoUninitialize();
        return 1;
    }

    IDeckLink *deckLink = NULL;
    while (deckLinkIterator->Next(&deckLink) == S_OK) {
        char *modelName = NULL;
        hr = deckLink->GetModelName(&modelName);
        if (SUCCEEDED(hr) && modelName && std::string(modelName) == "DeckLink Duo 2") {
            break;
        }
        if (modelName) {
            free(modelName);
        }
        deckLink->Release();
        deckLink = NULL;
    }
    deckLinkIterator->Release();
    if (!deckLink) {
        std::cerr << "DeckLink Duo 2 not found." << std::endl;
        CoUninitialize();
        return 1;
    }

    IDeckLinkInput *deckLinkInput = NULL;
    hr = deckLink->QueryInterface(IID_IDeckLinkInput, (void**)&deckLinkInput);
    if (FAILED(hr)) {
        std::cerr << "Failed to get DeckLinkInput interface." << std::endl;
        deckLink->Release();
        CoUninitialize();
        return 1;
    }

    BMDDisplayMode displayMode = bmdDisplayMode1080i5994;
    BMDPixelFormat pixelFormat = bmdFormat8BitYUV;
    bool isSupported = false;
    hr = deckLinkInput->DoesSupportVideoMode(
        bmdVideoConnectionSDISource,
        displayMode,
        pixelFormat,
        bmdVideoInputConversionNone,
        NULL,
        &isSupported);
    if (FAILED(hr) || !isSupported) {
        std::cerr << "Video mode not supported on input." << std::endl;
        deckLinkInput->Release();
        deckLink->Release();
        CoUninitialize();
        return 1;
    }

    hr = deckLinkInput->EnableVideoInput(
        displayMode,
        pixelFormat,
        bmdVideoInputEnableFormatDetection);
    if (FAILED(hr)) {
        std::cerr << "Failed to enable video input." << std::endl;
        deckLinkInput->Release();
        deckLink->Release();
        CoUninitialize();
        return 1;
    }

    BMDAudioSampleRate sampleRate = bmdAudioSampleRate48kHz;
    BMDAudioSampleType sampleType = bmdAudioSampleType16bit;
    uint32_t channelCount = 4;
    hr = deckLinkInput->EnableAudioInput(sampleRate, sampleType, channelCount);
    if (FAILED(hr)) {
        std::cerr << "Failed to enable audio input." << std::endl;
        deckLinkInput->DisableVideoInput();
        deckLinkInput->Release();
        deckLink->Release();
        CoUninitialize();
        return 1;
    }

    IDeckLinkOutput *deckLinkOutput = NULL;
    hr = deckLink->QueryInterface(IID_IDeckLinkOutput, (void**)&deckLinkOutput);
    if (FAILED(hr)) {
        std::cerr << "Failed to get DeckLinkOutput interface." << std::endl;
        deckLinkInput->DisableAudioInput();
        deckLinkInput->DisableVideoInput();
        deckLinkInput->Release();
        deckLink->Release();
        CoUninitialize();
        return 1;
    }

    hr = deckLinkOutput->DoesSupportVideoMode(
        bmdVideoConnectionSDI,
        displayMode,
        pixelFormat,
        bmdVideoOutputConversionNone,
        NULL,
        NULL);
    if (FAILED(hr)) {
        std::cerr << "Video mode not supported on output." << std::endl;
        deckLinkOutput->Release();
        deckLinkInput->DisableAudioInput();
        deckLinkInput->DisableVideoInput();
        deckLinkInput->Release();
        deckLink->Release();
        CoUninitialize();
        return 1;
    }

    hr = deckLinkOutput->EnableVideoOutput(displayMode, bmdVideoOutputEnableFormatDetection);
    if (FAILED(hr)) {
        std::cerr << "Failed to enable video output." << std::endl;
        deckLinkOutput->Release();
        deckLinkInput->DisableAudioInput();
        deckLinkInput->DisableVideoInput();
        deckLinkInput->Release();
        deckLink->Release();
        CoUninitialize();
        return 1;
    }

    hr = deckLinkOutput->EnableAudioOutput(sampleRate, sampleType, channelCount, bmdAudioOutputStreamTypeNormal);
    if (FAILED(hr)) {
        std::cerr << "Failed to enable audio output." << std::endl;
        deckLinkOutput->DisableVideoOutput();
        deckLinkOutput->Release();
        deckLinkInput->DisableAudioInput();
        deckLinkInput->DisableVideoInput();
        deckLinkInput->Release();
        deckLink->Release();
        CoUninitialize();
        return 1;
    }

    CaptureCallback *captureCallback = new CaptureCallback();
    captureCallback->deckLinkOutput = deckLinkOutput;
    hr = deckLinkInput->SetCallback(captureCallback);
    if (FAILED(hr)) {
        std::cerr << "Failed to set capture callback." << std::endl;
        delete captureCallback;
        deckLinkOutput->DisableAudioOutput();
        deckLinkOutput->DisableVideoOutput();
        deckLinkOutput->Release();
        deckLinkInput->DisableAudioInput();
        deckLinkInput->DisableVideoInput();
        deckLinkInput->Release();
        deckLink->Release();
        CoUninitialize();
        return 1;
    }

    hr = deckLinkInput->StartStreams();
    if (FAILED(hr)) {
        std::cerr << "Failed to start capture." << std::endl;
        delete captureCallback;
        deckLinkOutput->DisableAudioOutput();
        deckLinkOutput->DisableVideoOutput();
        deckLinkOutput->Release();
        deckLinkInput->DisableAudioInput();
        deckLinkInput->DisableVideoInput();
        deckLinkInput->Release();
        deckLink->Release();
        CoUninitialize();
        return 1;
    }

    hr = deckLinkOutput->StartScheduledPlayback(NULL, NULL, 1.0);
    if (FAILED(hr)) {
        std::cerr << "Failed to start playout." << std::endl;
        deckLinkInput->StopStreams();
        delete captureCallback;
        deckLinkOutput->DisableAudioOutput();
        deckLinkOutput->DisableVideoOutput();
        deckLinkOutput->Release();
        deckLinkInput->DisableAudioInput();
        deckLinkInput->DisableVideoInput();
        deckLinkInput->Release();
        deckLink->Release();
        CoUninitialize();
        return 1;
    }

    // Keep the application running
    std::cout << "Press Enter to quit..." << std::endl;
    std::cin.get();

    // Cleanup
    deckLinkInput->StopStreams();
    deckLinkOutput->StopScheduledPlayback();
    delete captureCallback;
    deckLinkOutput->DisableAudioOutput();
    deckLinkOutput->DisableVideoOutput();
    deckLinkOutput->Release();
    deckLinkInput->DisableAudioInput();
    deckLinkInput->DisableVideoInput();
    deckLinkInput->Release();
    deckLink->Release();
    CoUninitialize();

    return 0;
}