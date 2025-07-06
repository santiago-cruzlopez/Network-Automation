#include "DeckLinkManager.h"
#include "CaptureDelegate.h"
#include "PlaybackDelegate.h"
#include <iostream>

const int TARGET_INPUT_SUBDEVICE_INDEX = 2; // Physical SDI Port 2
const int TARGET_OUTPUT_SUBDEVICE_INDEX = 0; // Physical SDI Port 1

DeckLinkManager::DeckLinkManager() :
    m_deckLinkIterator(nullptr),
    m_deckLinkInputDevice(nullptr), m_deckLinkInput(nullptr),
    m_deckLinkOutputDevice(nullptr), m_deckLinkOutput(nullptr),
    m_captureDelegate(nullptr), m_playbackDelegate(nullptr),
    m_selectedDisplayMode(bmdModeHD1080i5994),
    m_pixelFormat(bmdFormat8BitYUV),
    m_audioChannels(2), 
    m_audioSampleRate(bmdAudioSampleRate48kHz),
    m_audioSampleType(bmdAudioSampleType16bitInteger),
    m_inputDeviceSdkIndex(-1),
    m_outputDeviceSdkIndex(-1),
    g_do_exit(false) {}

DeckLinkManager::~DeckLinkManager() {
    g_do_exit = true;
    m_queueCond.notify_all();

    StopStreams();

    if (m_captureDelegate) m_captureDelegate->Release();
    if (m_playbackDelegate) m_playbackDelegate->Release();
    if (m_deckLinkInput) m_deckLinkInput->Release();
    if (m_deckLinkInputDevice) m_deckLinkInputDevice->Release();
    if (m_deckLinkOutput) m_deckLinkOutput->Release();
    if (m_deckLinkOutputDevice) m_deckLinkOutputDevice->Release();
    if (m_deckLinkIterator) m_deckLinkIterator->Release();
}

bool DeckLinkManager::Initialize() {
    HRESULT result;

    m_deckLinkIterator = CreateDeckLinkIteratorInstance();
    if (!m_deckLinkIterator) {
        std::cerr << "Error: Could not create DeckLink iterator." << std::endl;
        return false;
    }

    if (!SelectDevices()) return false;
    if (!ConfigureInput()) return false;
    if (!ConfigureOutput()) return false;

    m_captureDelegate = new CaptureDelegate(this);
    result = m_deckLinkInput->SetCallback(m_captureDelegate);
    if (result != S_OK) {
        std::cerr << "Error: Could not set input callback." << std::endl;
        return false;
    }

    m_playbackDelegate = new PlaybackDelegate(this);
    result = m_deckLinkOutput->SetScheduledFrameCompletionCallback(m_playbackDelegate);
    if (result != S_OK) {
        std::cerr << "Error: Could not set video output callback." << std::endl;
        return false;
    }
    result = m_deckLinkOutput->SetAudioCallback(m_playbackDelegate);
    if (result != S_OK) {
        std::cerr << "Error: Could not set audio output callback." << std::endl;
        return false;
    }

    m_videoModeName = "1080i59.94"; 
    m_pixelFormatName = PixelFormatToStdString(m_pixelFormat);

    std::cout << "DeckLinkManager initialized successfully." << std::endl;
    return true;
}

bool DeckLinkManager::SelectDevices() {
    IDeckLink* deckLink = nullptr;
    HRESULT result;

    std::cout << "Searching for DeckLink devices..." << std::endl;

    while (m_deckLinkIterator->Next(&deckLink) == S_OK) {
        char* deviceNameStr = nullptr;
        result = deckLink->GetDisplayName((const char**)&deviceNameStr);
        if (result != S_OK) {
            deckLink->Release();
            continue;
        }

        IDeckLinkProfileAttributes* deckLinkAttributes = nullptr;
        result = deckLink->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&deckLinkAttributes);
        if (result == S_OK) {
            int64_t subDeviceIndex = -1;
            deckLinkAttributes->GetInt(BMDDeckLinkSubDeviceIndex, &subDeviceIndex);

            if (subDeviceIndex == TARGET_INPUT_SUBDEVICE_INDEX && !m_deckLinkInputDevice) {
                m_deckLinkInputDevice = deckLink;
                m_deckLinkInputDevice->AddRef();
                m_inputDeviceName = deviceNameStr;
                m_inputDeviceSdkIndex = subDeviceIndex;
            } else if (subDeviceIndex == TARGET_OUTPUT_SUBDEVICE_INDEX && !m_deckLinkOutputDevice) {
                m_deckLinkOutputDevice = deckLink;
                m_deckLinkOutputDevice->AddRef();
                m_outputDeviceName = deviceNameStr;
                m_outputDeviceSdkIndex = subDeviceIndex;
            }
            deckLinkAttributes->Release();
        }
        free(deviceNameStr);
        deckLink->Release();
    }

    if (!m_deckLinkInputDevice) {
        std::cerr << "Error: Could not find target INPUT device (SDK Index " << TARGET_INPUT_SUBDEVICE_INDEX << ")." << std::endl;
        return false;
    }
     if (!m_deckLinkOutputDevice) {
        std::cerr << "Error: Could not find target OUTPUT device (SDK Index " << TARGET_OUTPUT_SUBDEVICE_INDEX << ")." << std::endl;
        return false;
    }

    return true;
}

bool DeckLinkManager::ConfigureInput() {
    HRESULT result;
    result = m_deckLinkInputDevice->QueryInterface(IID_IDeckLinkInput, (void**)&m_deckLinkInput);
    if (result != S_OK) { std::cerr << "Error: Could not get IDeckLinkInput." << std::endl; return false; }

    bool isModeSupported = false;
    result = m_deckLinkInput->DoesSupportVideoMode(bmdVideoConnectionSDI, m_selectedDisplayMode, m_pixelFormat, bmdNoVideoInputConversion, bmdSupportedVideoModeDefault, nullptr, &isModeSupported);
    if (result != S_OK || !isModeSupported) { std::cerr << "Error: Input video mode not supported." << std::endl; return false; }

    result = m_deckLinkInput->EnableVideoInput(m_selectedDisplayMode, m_pixelFormat, bmdVideoInputFlagDefault);
    if (result != S_OK) { std::cerr << "Error: Could not enable video input." << std::endl; return false; }

    result = m_deckLinkInput->EnableAudioInput(m_audioSampleRate, m_audioSampleType, m_audioChannels);
    if (result != S_OK) { std::cout << "Warning: Could not enable audio input as configured." << std::endl; }

    return true;
}

bool DeckLinkManager::ConfigureOutput() {
    HRESULT result;
    result = m_deckLinkOutputDevice->QueryInterface(IID_IDeckLinkOutput, (void**)&m_deckLinkOutput);
    if (result != S_OK) { std::cerr << "Error: Could not get IDeckLinkOutput." << std::endl; return false; }

    bool isModeSupported = false;
    result = m_deckLinkOutput->DoesSupportVideoMode(
        bmdVideoConnectionSDI,
        m_selectedDisplayMode,
        m_pixelFormat,
        bmdNoVideoOutputConversion,     
        bmdSupportedVideoModeDefault,  
        nullptr,                       
        &isModeSupported               
    );

    if (result != S_OK || !isModeSupported) {
        std::cerr << "Error: Output video mode not supported." << std::endl;
        return false;
    }

    result = m_deckLinkOutput->EnableVideoOutput(m_selectedDisplayMode, bmdVideoOutputFlagDefault);
    if (result != S_OK) { std::cerr << "Error: Could not enable video output." << std::endl; return false; }

    result = m_deckLinkOutput->EnableAudioOutput(m_audioSampleRate, m_audioSampleType, m_audioChannels, bmdAudioOutputStreamTimestamped);
    if (result != S_OK) { std::cerr << "Error: Could not enable audio output." << std::endl; return false; }
    
    return true;
}

bool DeckLinkManager::StartStreams() {
    HRESULT result;
    
    // Start playback delegate's preroll process
    m_playbackDelegate->StartPreroll();

    result = m_deckLinkOutput->StartScheduledPlayback(0, m_playbackDelegate->getTimeScale(), 1.0);
    if (result != S_OK) {
        std::cerr << "Error: Could not start scheduled playback." << std::endl;
        return false;
    }

    result = m_deckLinkInput->StartStreams();
    if (result != S_OK) {
        std::cerr << "Error: Could not start input streams." << std::endl;
        return false;
    }
    
    return true;
}

void DeckLinkManager::StopStreams() {
    if (m_deckLinkInput) m_deckLinkInput->StopStreams();
    if (m_deckLinkOutput) m_deckLinkOutput->StopScheduledPlayback(0, nullptr, 0);
}

void DeckLinkManager::WaitForStreamsToFinish() {
    std::cout << "Capture and Playout running... Press Enter to stop." << std::endl;
    std::cin.get();
    g_do_exit = true;
    m_queueCond.notify_all();
}

void DeckLinkManager::QueueFrameForPlayout(FrameData&& frame) {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_frameQueue.push(std::move(frame));
    }
    m_queueCond.notify_one();
}

bool DeckLinkManager::GetFrameForPlayout(FrameData& out_frame) {
    std::unique_lock<std::mutex> lock(m_queueMutex);
    m_queueCond.wait(lock, [this]{ return !m_frameQueue.empty() || g_do_exit; });

    if (g_do_exit && m_frameQueue.empty()) {
        return false;
    }

    out_frame = std::move(m_frameQueue.front());
    m_frameQueue.pop();
    return true;
}

std::string DeckLinkManager::PixelFormatToStdString(BMDPixelFormat pixelFormat) {
    switch (pixelFormat) {
        case bmdFormat8BitYUV:  return "8-bit YUV 4:2:2 ('2vuy')";
        case bmdFormat10BitYUV: return "10-bit YUV 4:2:2 ('v210')";
        case bmdFormat8BitARGB: return "8-bit ARGB 4:4:4:4";
        case bmdFormat8BitBGRA: return "8-bit BGRA 4:4:4:4";
        default: return "Unknown Pixel Format";
    }
}