#ifndef DECKLINK_MANAGER_H
#define DECKLINK_MANAGER_H

#include "DeckLinkAPI.h"
#include <string>
#include <atomic>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

// Forward declarations
class CaptureDelegate;
class PlaybackDelegate;

struct FrameData {
    // We use std::vector<uint8_t> to manage memory automatically
    std::vector<uint8_t> videoData;
    std::vector<uint8_t> audioData;
};

class DeckLinkManager {
public:
    DeckLinkManager();
    ~DeckLinkManager();

    bool Initialize();
    bool StartStreams();
    void StopStreams();
    void WaitForStreamsToFinish();

    // Thread-safe methods for the queue
    void QueueFrameForPlayout(FrameData&& frame);
    bool GetFrameForPlayout(FrameData& out_frame);

    // Flag for graceful shutdown
    std::atomic<bool> g_do_exit;

    // Getters for configured/detected information
    const std::string& GetInputDeviceName() const { return m_inputDeviceName; }
    int64_t GetInputDeviceSdkIndex() const { return m_inputDeviceSdkIndex; }
    const std::string& GetOutputDeviceName() const { return m_outputDeviceName; }
    int64_t GetOutputDeviceSdkIndex() const { return m_outputDeviceSdkIndex; }
    const std::string& GetVideoModeName() const { return m_videoModeName; }
    const std::string& GetPixelFormatName() const { return m_pixelFormatName; }

    // Getters needed by delegates
    IDeckLinkOutput* GetDeckLinkOutput() { return m_deckLinkOutput; }
    BMDAudioSampleType getAudioSampleType() const { return m_audioSampleType; }
    uint32_t getAudioChannels() const { return m_audioChannels; }

private:
    IDeckLinkIterator* m_deckLinkIterator;
    IDeckLink* m_deckLinkInputDevice;
    IDeckLinkInput* m_deckLinkInput;
    IDeckLink* m_deckLinkOutputDevice;
    IDeckLinkOutput* m_deckLinkOutput;

    CaptureDelegate* m_captureDelegate;
    PlaybackDelegate* m_playbackDelegate;
    
    // Thread-safe frame queue
    std::queue<FrameData>   m_frameQueue;
    std::mutex              m_queueMutex;
    std::condition_variable m_queueCond;

    // Configuration
    BMDDisplayMode          m_selectedDisplayMode;
    BMDPixelFormat          m_pixelFormat;
    uint32_t                m_audioChannels;
    BMDAudioSampleRate      m_audioSampleRate;
    BMDAudioSampleType      m_audioSampleType;

    // Stored dynamic information
    std::string             m_inputDeviceName;
    int64_t                 m_inputDeviceSdkIndex;
    std::string             m_outputDeviceName;
    int64_t                 m_outputDeviceSdkIndex;
    std::string             m_videoModeName;
    std::string             m_pixelFormatName;

    bool SelectDevices();
    bool ConfigureInput();
    bool ConfigureOutput();
    std::string PixelFormatToStdString(BMDPixelFormat pixelFormat);
};

#endif // DECKLINK_MANAGER_H