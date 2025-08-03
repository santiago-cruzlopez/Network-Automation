#include "PlaybackDelegate.h"
#include "DeckLinkManager.h"
#include <iostream>
#include <cstring>

PlaybackDelegate::PlaybackDelegate(DeckLinkManager* manager) :
    m_manager(manager), m_deckLinkOutput(nullptr), m_refCount(1), m_framesScheduled(0),
    m_frameDuration(0), m_timeScale(0) { // Initialize to 0
    m_deckLinkOutput = m_manager->GetDeckLinkOutput();
    if (m_deckLinkOutput) {
        m_deckLinkOutput->AddRef();

        // FIX: Get frame rate and timescale during construction so it's ready for StartStreams()
        IDeckLinkDisplayMode* displayMode = nullptr;
        // Use the hardcoded mode from manager for this lookup
        if (m_deckLinkOutput->GetDisplayMode(bmdModeHD1080i5994, &displayMode) == S_OK) { // 
            displayMode->GetFrameRate(&m_frameDuration, &m_timeScale); // 
            displayMode->Release();
        } else {
            std::cerr << "Error: Could not get display mode for output timing in PlaybackDelegate constructor." << std::endl;
            // Set a fallback timescale to prevent division by zero, though playback may be incorrect
            m_timeScale = 1000;
            m_frameDuration = 0;
        }
    }
}

PlaybackDelegate::~PlaybackDelegate() {
    if (m_deckLinkOutput) {
        m_deckLinkOutput->Release();
    }
}

void PlaybackDelegate::StartPreroll() {
    if (!m_deckLinkOutput) return;

    // Preroll a few frames to fill the pipeline
    for (int i = 0; i < 5; ++i) {
        ScheduleNextFrame();
    }
}

HRESULT STDMETHODCALLTYPE PlaybackDelegate::ScheduledFrameCompleted(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result) {
    if (completedFrame) {
        completedFrame->Release();
    }
    if (m_manager && !m_manager->g_do_exit) {
        ScheduleNextFrame();
    }
    return S_OK;
}

// ScheduleNextFrame, RenderAudioSamples, QueryInterface, AddRef, Release
HRESULT STDMETHODCALLTYPE PlaybackDelegate::ScheduledPlaybackHasStopped() {
    std::cout << "Playback has stopped." << std::endl;
    return S_OK;
}

void PlaybackDelegate::ScheduleNextFrame() {
    if (!m_deckLinkOutput) return;

    FrameData frameToPlay;
    if (m_manager->GetFrameForPlayout(frameToPlay)) {
        IDeckLinkMutableVideoFrame* outputFrame = nullptr;
        long width = 1920; 
        long height = 1080;
        long rowBytes = width * 2; // Manual calculation for bmdFormat8BitYUV

        if (m_deckLinkOutput->CreateVideoFrame(width, height, rowBytes, bmdFormat8BitYUV, bmdFrameFlagDefault, &outputFrame) == S_OK) {
            void* outBufferBytes;
            outputFrame->GetBytes(&outBufferBytes);
            memcpy(outBufferBytes, frameToPlay.videoData.data(), frameToPlay.videoData.size());
            
            m_deckLinkOutput->ScheduleVideoFrame(outputFrame, m_framesScheduled * m_frameDuration, m_frameDuration, m_timeScale);

            std::lock_guard<std::mutex> lock(m_audioMutex);
            m_audioBuffer.insert(m_audioBuffer.end(), frameToPlay.audioData.begin(), frameToPlay.audioData.end());

            m_framesScheduled++;
        } else {
            std::cerr << "Error: Could not create output frame." << std::endl;
        }
    }
}

HRESULT STDMETHODCALLTYPE PlaybackDelegate::RenderAudioSamples(bool preroll) {
    uint32_t samplesToWrite = 0;
    uint32_t samplesWritten = 0;

    std::lock_guard<std::mutex> lock(m_audioMutex);
    
    if (m_audioBuffer.empty()) {
        return S_OK;
    }

    uint32_t bytesPerSampleFrame = m_manager->getAudioChannels() * (m_manager->getAudioSampleType() == bmdAudioSampleType16bitInteger ? 2 : 4);
    if (bytesPerSampleFrame == 0) return E_FAIL;

    samplesToWrite = m_audioBuffer.size() / bytesPerSampleFrame;

    if (m_deckLinkOutput->ScheduleAudioSamples(m_audioBuffer.data(), samplesToWrite, 0, 0, &samplesWritten) == S_OK) {
        if (samplesWritten < samplesToWrite) {
            m_audioBuffer.erase(m_audioBuffer.begin(), m_audioBuffer.begin() + (samplesWritten * bytesPerSampleFrame));
        } else {
            m_audioBuffer.clear();
        }
    }
    
    return S_OK;
}


HRESULT STDMETHODCALLTYPE PlaybackDelegate::QueryInterface(REFIID iid, LPVOID *ppv) {
    CFUUIDBytes iunknown = IID_IUnknown;
    CFUUIDBytes iVideoOutputCallback = IID_IDeckLinkVideoOutputCallback;
    CFUUIDBytes iAudioOutputCallback = IID_IDeckLinkAudioOutputCallback;
    if (memcmp(&iid, &iunknown, sizeof(REFIID)) == 0) {
        *ppv = static_cast<IDeckLinkVideoOutputCallback*>(this); AddRef(); return S_OK;
    } else if (memcmp(&iid, &iVideoOutputCallback, sizeof(REFIID)) == 0) {
        *ppv = static_cast<IDeckLinkVideoOutputCallback*>(this); AddRef(); return S_OK;
    } else if (memcmp(&iid, &iAudioOutputCallback, sizeof(REFIID)) == 0) {
        *ppv = static_cast<IDeckLinkAudioOutputCallback*>(this); AddRef(); return S_OK;
    }
    *ppv = NULL; return E_NOINTERFACE;
}
ULONG STDMETHODCALLTYPE PlaybackDelegate::AddRef(void) { return ++m_refCount; }
ULONG STDMETHODCALLTYPE PlaybackDelegate::Release(void) {
    ULONG newRefValue = --m_refCount;
    if (newRefValue == 0) delete this;
    return newRefValue;
}