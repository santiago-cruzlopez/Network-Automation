#ifndef DECKLINK_MANAGER_H
#define DECKLINK_MANAGER_H

#include "DeckLinkAPI.h"
#include "CaptureDelegate.h"
#include <string>
#include <atomic>
#include <vector> // Was included before, keep if FrameData struct is used elsewhere

// Forward declaration
class CaptureDelegate;

class DeckLinkManager {
public:
    DeckLinkManager();
    ~DeckLinkManager();

    bool Initialize();
    bool StartCapture();
    void StopCapture();
    void WaitForCaptureToFinish();

    std::atomic<bool> g_do_exit;

    // Getters for configured/detected information
    const std::string& GetInputDeviceName() const { return m_inputDeviceName; }
    int64_t GetInputDeviceSdkIndex() const { return m_inputDeviceSdkIndex; }
    const std::string& GetVideoModeName() const { return m_videoModeName; }
    const std::string& GetPixelFormatName() const { return m_pixelFormatName; }

    // Getters for CaptureDelegate if needed (e.g., for audio config)
    BMDAudioSampleType getAudioSampleType() const { return m_audioSampleType; }
    uint32_t getAudioChannels() const { return m_audioChannels; }
    // IDeckLinkOutput* GetDeckLinkOutputInterface() { return m_deckLinkOutput; } // Not needed for input-only


private:
    IDeckLinkIterator* m_deckLinkIterator;
    IDeckLink* m_deckLinkInputDevice;
    IDeckLinkInput* m_deckLinkInput;
    CaptureDelegate* m_captureDelegate;
    
    BMDDisplayMode          m_selectedDisplayMode;
    BMDPixelFormat          m_pixelFormat;
    uint32_t                m_audioChannels;
    BMDAudioSampleRate      m_audioSampleRate;
    BMDAudioSampleType      m_audioSampleType;

    // Store dynamic information
    std::string             m_inputDeviceName;
    int64_t                 m_inputDeviceSdkIndex; // BMDDeckLinkSubDeviceIndex returns int64_t
    std::string             m_videoModeName;
    std::string             m_pixelFormatName;

    bool SelectInputDevice();
    bool ConfigureInput();
    std::string PixelFormatToStdString(BMDPixelFormat pixelFormat); // Helper function
};

#endif // DECKLINK_MANAGER_H