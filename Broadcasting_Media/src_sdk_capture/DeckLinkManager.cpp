#include "DeckLinkManager.h"
#include <iostream>

const int TARGET_INPUT_SUBDEVICE_INDEX = 0; // Changed as per your ffmpeg output likely pointing to index 0

DeckLinkManager::DeckLinkManager() :
    m_deckLinkIterator(nullptr),
    m_deckLinkInputDevice(nullptr), m_deckLinkInput(nullptr),
    m_captureDelegate(nullptr),
    m_selectedDisplayMode(bmdModeHD1080i5994), 
    m_pixelFormat(bmdFormat8BitYUV),          
    m_audioChannels(4), 
    m_audioSampleRate(bmdAudioSampleRate48kHz), 
    m_audioSampleType(bmdAudioSampleType16bitInteger), 
    m_inputDeviceSdkIndex(-1), // Initialize
    g_do_exit(false) {}

DeckLinkManager::~DeckLinkManager() {
    StopCapture(); 

    if (m_captureDelegate) {
        m_captureDelegate->Release();
        m_captureDelegate = nullptr;
    }
    if (m_deckLinkInput) {
        m_deckLinkInput->Release();
        m_deckLinkInput = nullptr;
    }
    if (m_deckLinkInputDevice) {
        m_deckLinkInputDevice->Release();
        m_deckLinkInputDevice = nullptr;
    }
    if (m_deckLinkIterator) {
        m_deckLinkIterator->Release();
        m_deckLinkIterator = nullptr;
    }
}

// Helper function to convert BMDPixelFormat to std::string
std::string DeckLinkManager::PixelFormatToStdString(BMDPixelFormat pixelFormat) {
    switch (pixelFormat) {
        case bmdFormat8BitYUV:  return "8-bit YUV 4:2:2 ('2vuy')";    
        case bmdFormat10BitYUV: return "10-bit YUV 4:2:2 ('v210')";   
        case bmdFormat8BitARGB: return "8-bit ARGB 4:4:4:4";          
        case bmdFormat8BitBGRA: return "8-bit BGRA 4:4:4:4"; 
        case bmdFormat10BitRGB: return "10-bit RGB 4:4:4 ('r210')";  
        // Add other formats as needed from SDK manual (Section 3.4)
        default:                return "Unknown Pixel Format";
    }
}

bool DeckLinkManager::Initialize() {
    HRESULT result;

    m_deckLinkIterator = CreateDeckLinkIteratorInstance(); //
    if (!m_deckLinkIterator) {
        std::cerr << "Error: Could not create DeckLink iterator." << std::endl;
        return false;
    }

    if (!SelectInputDevice()) return false; // This will populate m_inputDeviceName and m_inputDeviceSdkIndex
    if (!ConfigureInput()) return false;  // This will populate m_videoModeName and m_pixelFormatName

    m_captureDelegate = new CaptureDelegate(this, m_deckLinkInput);
    result = m_deckLinkInput->SetCallback(m_captureDelegate); //
    if (result != S_OK) {
        std::cerr << "Error: Could not set input callback. HRESULT: 0x" << std::hex << result << std::endl;
        return false;
    }

    std::cout << "DeckLinkManager initialized for input successfully." << std::endl;
    return true;
}

bool DeckLinkManager::SelectInputDevice() {
    IDeckLink* deckLink = nullptr;
    int currentIndex = 0;
    HRESULT result;

    std::cout << "Searching for DeckLink input device..." << std::endl;

    while (m_deckLinkIterator->Next(&deckLink) == S_OK) { //
        char* deviceNameStr = nullptr;
        result = deckLink->GetDisplayName((const char**)&deviceNameStr); //
        if (result == S_OK) {
            std::cout << "Found device: " << deviceNameStr << " (System Index: " << currentIndex << ")" << std::endl;
            
            IDeckLinkProfileAttributes* deckLinkAttributes = nullptr;
            result = deckLink->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&deckLinkAttributes); //
            if (result == S_OK) {
                int64_t subDeviceIndex = -1; 
                deckLinkAttributes->GetInt(BMDDeckLinkSubDeviceIndex, &subDeviceIndex); //
                std::cout << "  Sub-device API index: " << subDeviceIndex << std::endl;

                if (subDeviceIndex == TARGET_INPUT_SUBDEVICE_INDEX) {
                    m_deckLinkInputDevice = deckLink;
                    m_deckLinkInputDevice->AddRef(); 
                    m_inputDeviceName = deviceNameStr; // Store the name
                    m_inputDeviceSdkIndex = subDeviceIndex; // Store the SDK index
                    std::cout << "  Selected as INPUT device: " << m_inputDeviceName 
                              << " (SDK Index: " << m_inputDeviceSdkIndex << ")" << std::endl;
                    deckLinkAttributes->Release();
                    free(deviceNameStr);
                    break; 
                }
                deckLinkAttributes->Release();
            }
            free(deviceNameStr);
        }
        deckLink->Release(); 
        currentIndex++;
    }

    if (!m_deckLinkInputDevice) {
        std::cerr << "Error: Could not find target INPUT DeckLink device (Sub-device API index " << TARGET_INPUT_SUBDEVICE_INDEX << ")." << std::endl;
        return false;
    }
    // m_inputDeviceName and m_inputDeviceSdkIndex are now set
    return true;
}

bool DeckLinkManager::ConfigureInput() {
    HRESULT result;
    result = m_deckLinkInputDevice->QueryInterface(IID_IDeckLinkInput, (void**)&m_deckLinkInput); //
    if (result != S_OK) {
        std::cerr << "Error: Could not get IDeckLinkInput interface. HRESULT: 0x" << std::hex << result << std::endl;
        return false;
    }

    bool isModeSupported = false;
    result = m_deckLinkInput->DoesSupportVideoMode(
        bmdVideoConnectionSDI,
        m_selectedDisplayMode,
        m_pixelFormat,
        bmdNoVideoInputConversion,      // From BMDVideoInputConversionMode enum 
        bmdSupportedVideoModeDefault,   // From BMDSupportedVideoModeFlags enum 
        nullptr,                        
        &isModeSupported               
    ); //

    if (result != S_OK || !isModeSupported) {
        std::cerr << "Error: Selected video mode or pixel format not supported on input device. HRESULT: 0x" << std::hex << result << std::endl;
        return false;
    }

    // Get and store the video mode name
    IDeckLinkDisplayMode* displayModeObj = nullptr;
    result = m_deckLinkInput->GetDisplayMode(m_selectedDisplayMode, &displayModeObj); //
    if (result == S_OK && displayModeObj) {
        char* modeNameStr = nullptr;
        if (displayModeObj->GetName((const char**)&modeNameStr) == S_OK) { //
            m_videoModeName = modeNameStr;
            free(modeNameStr);
        } else {
            m_videoModeName = "Unknown Mode";
        }
        displayModeObj->Release();
    } else {
        m_videoModeName = "Unknown Mode (Error fetching)";
    }

    // Store the pixel format name
    m_pixelFormatName = PixelFormatToStdString(m_pixelFormat);


    result = m_deckLinkInput->EnableVideoInput(m_selectedDisplayMode, m_pixelFormat, bmdVideoInputFlagDefault); //
    if (result != S_OK) {
        std::cerr << "Error: Could not enable video input. HRESULT: 0x" << std::hex << result << std::endl;
        return false;
    }

    result = m_deckLinkInput->EnableAudioInput(m_audioSampleRate, m_audioSampleType, m_audioChannels); //
    if (result != S_OK) {
        std::cout << "Warning: Could not enable audio input fully as configured. HRESULT: 0x" << std::hex << result << std::endl;
    }
    std::cout << "Input device configured with Mode: " << m_videoModeName << ", Pixel Format: " << m_pixelFormatName << std::endl;
    return true;
}

bool DeckLinkManager::StartCapture() {
    HRESULT result;
    result = m_deckLinkInput->StartStreams(); //
    if (result != S_OK) {
        std::cerr << "Error: Could not start input streams. HRESULT: 0x" << std::hex << result << std::endl;
        return false;
    }
    std::cout << "Input stream started. Waiting for frames..." << std::endl;
    return true;
}

void DeckLinkManager::StopCapture() {
    if (m_deckLinkInput) {
        m_deckLinkInput->StopStreams(); //
        m_deckLinkInput->DisableAudioInput(); //
        m_deckLinkInput->DisableVideoInput(); //
    }
    std::cout << "Input stream stopped." << std::endl;
}

void DeckLinkManager::WaitForCaptureToFinish() {
    std::cout << "Capturing... Press Enter to stop." << std::endl;
    std::cin.get();
    g_do_exit = true; 
}