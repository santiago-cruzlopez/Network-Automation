#include <iostream>
#include <vector>
#include <string>
#include <cstring>  // Added for memcmp
#include <DeckLinkAPI.h>

// Input callback class
class InputCallback : public IDeckLinkInputCallback {
public:
    InputCallback(IDeckLinkOutput* output) : m_refCount(1), m_output(output), m_frameCount(0) {}

    HRESULT QueryInterface(REFIID iid, LPVOID* ppv) override {
        if (memcmp(&iid, &IID_IDeckLinkInputCallback, sizeof(iid)) == 0) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG AddRef() override { return __sync_fetch_and_add(&m_refCount, 1) + 1; }
    ULONG Release() override {
        ULONG newRefCount = __sync_fetch_and_sub(&m_refCount, 1) - 1;
        if (newRefCount == 0) { delete this; return 0; }
        return newRefCount;
    }

    HRESULT VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame, IDeckLinkAudioInputPacket* audioPacket) override {
        if (!videoFrame) return S_OK;

        long width = videoFrame->GetWidth();
        long height = videoFrame->GetHeight();
        std::cout << "Frame " << m_frameCount++ << ": Captured frame with shape (" << height << ", " << width << ", YUV)" << std::endl;

        BMDTimeScale timeScale = 1000000;
        BMDTimeValue frameTime, frameDuration;
        videoFrame->GetHardwareReferenceTimestamp(timeScale, &frameTime, &frameDuration);

        BMDTimeValue displayTime = frameTime + 2 * frameDuration;
        HRESULT result = m_output->ScheduleVideoFrame(videoFrame, displayTime, frameDuration, timeScale);
        if (result != S_OK) std::cerr << "Failed to schedule video frame" << std::endl;

        if (audioPacket) {
            void* audioBuffer;
            audioPacket->GetBytes(&audioBuffer);
            int sampleFrameCount = audioPacket->GetSampleFrameCount();
            uint32_t samplesWritten;
            result = m_output->ScheduleAudioSamples(audioBuffer, sampleFrameCount, displayTime, timeScale, &samplesWritten);
            if (result != S_OK) std::cerr << "Failed to schedule audio samples" << std::endl;
        }
        return S_OK;
    }

    HRESULT VideoInputFormatChanged(BMDVideoInputFormatChangedEvents, IDeckLinkDisplayMode*, BMDDetectedVideoInputFormatFlags) override {
        return S_OK;
    }

private:
    int m_refCount;
    IDeckLinkOutput* m_output;
    uint64_t m_frameCount;
};

int main() {
    HRESULT result;

    IDeckLinkIterator* deckLinkIterator = CreateDeckLinkIteratorInstance();
    if (!deckLinkIterator) {
        std::cerr << "Failed to create DeckLink iterator" << std::endl;
        return 1;
    }

    IDeckLink* inputDevice = nullptr;
    IDeckLink* outputDevice = nullptr;
    IDeckLinkProfileAttributes* attributes = nullptr;  // Updated to IDeckLinkProfileAttributes
    std::vector<IDeckLink*> devices;

    IDeckLink* deckLink = nullptr;
    while (deckLinkIterator->Next(&deckLink) == S_OK) {
        devices.push_back(deckLink);
        deckLink->AddRef();
        if (deckLink->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&attributes) == S_OK) {  // Updated IID
            int64_t subDeviceIndex;
            if (attributes->GetInt(BMDDeckLinkSubDeviceIndex, &subDeviceIndex) == S_OK) {
                if (subDeviceIndex == 2) { inputDevice = deckLink; inputDevice->AddRef(); }
                else if (subDeviceIndex == 0) { outputDevice = deckLink; outputDevice->AddRef(); }
            }
            attributes->Release();
        }
    }
    deckLinkIterator->Release();

    if (!inputDevice || !outputDevice) {
        std::cerr << "Failed to find input (sub-device 2) or output (sub-device 0)" << std::endl;
        for (auto device : devices) device->Release();
        return 1;
    }

    IDeckLinkInput* deckLinkInput = nullptr;
    IDeckLinkOutput* deckLinkOutput = nullptr;

    if (inputDevice->QueryInterface(IID_IDeckLinkInput, (void**)&deckLinkInput) != S_OK ||
        outputDevice->QueryInterface(IID_IDeckLinkOutput, (void**)&deckLinkOutput) != S_OK) {
        std::cerr << "Failed to get input/output interface" << std::endl;
        if (deckLinkInput) deckLinkInput->Release();
        if (outputDevice) outputDevice->Release();
        inputDevice->Release();
        for (auto device : devices) device->Release();
        return 1;
    }

    result = deckLinkInput->EnableVideoInput(bmdModeHD1080i5994, bmdFormat8BitYUV, bmdVideoInputFlagDefault);
    result |= deckLinkInput->EnableAudioInput(bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger, 4);
    result |= deckLinkOutput->EnableVideoOutput(bmdModeHD1080i5994, bmdVideoOutputFlagDefault);
    result |= deckLinkOutput->EnableAudioOutput(bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger, 4, bmdAudioOutputStreamContinuous);
    if (result != S_OK) {
        std::cerr << "Failed to enable input/output" << std::endl;
        deckLinkInput->Release();
        deckLinkOutput->Release();
        inputDevice->Release();
        outputDevice->Release();
        for (auto device : devices) device->Release();
        return 1;
    }

    InputCallback* inputCallback = new InputCallback(deckLinkOutput);
    deckLinkInput->SetCallback(inputCallback);

    result = deckLinkOutput->StartScheduledPlayback(0, 1000000, 1.0);
    result |= deckLinkInput->StartStreams();
    if (result != S_OK) {
        std::cerr << "Failed to start playback/streams" << std::endl;
        BMDTimeValue actualStopTime;
        deckLinkOutput->StopScheduledPlayback(0, &actualStopTime, 1000000);  // Updated call
        delete inputCallback;
        deckLinkInput->SetCallback(nullptr);
        deckLinkOutput->DisableVideoOutput();
        deckLinkOutput->DisableAudioOutput();
        deckLinkInput->DisableVideoInput();
        deckLinkInput->DisableAudioInput();
        deckLinkInput->Release();
        deckLinkOutput->Release();
        inputDevice->Release();
        outputDevice->Release();
        for (auto device : devices) device->Release();
        return 1;
    }

    std::cout << "Running loop-through. Press Enter to stop..." << std::endl;
    std::cin.get();

    deckLinkInput->StopStreams();
    BMDTimeValue actualStopTime;
    deckLinkOutput->StopScheduledPlayback(0, &actualStopTime, 1000000);  // Updated call
    delete inputCallback;
    deckLinkInput->SetCallback(nullptr);
    deckLinkOutput->DisableVideoOutput();
    deckLinkOutput->DisableAudioOutput();
    deckLinkInput->DisableVideoInput();
    deckLinkInput->DisableAudioInput();
    deckLinkInput->Release();
    deckLinkOutput->Release();
    inputDevice->Release();
    outputDevice->Release();
    for (auto device : devices) device->Release();

    return 0;
}