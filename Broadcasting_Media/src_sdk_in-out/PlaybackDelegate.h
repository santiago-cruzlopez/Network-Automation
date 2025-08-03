#ifndef PLAYBACK_DELEGATE_H
#define PLAYBACK_DELEGATE_H

#include "DeckLinkAPI.h"
#include <atomic>
#include <vector>
#include <mutex>

class DeckLinkManager;

class PlaybackDelegate : public IDeckLinkVideoOutputCallback, public IDeckLinkAudioOutputCallback {
public:
    explicit PlaybackDelegate(DeckLinkManager* manager);
    virtual ~PlaybackDelegate();

    BMDTimeScale getTimeScale() const { return m_timeScale; }

    // IDeckLinkVideoOutputCallback methods
    virtual HRESULT STDMETHODCALLTYPE ScheduledFrameCompleted(IDeckLinkVideoFrame*, BMDOutputFrameCompletionResult);
    virtual HRESULT STDMETHODCALLTYPE ScheduledPlaybackHasStopped();

    // IDeckLinkAudioOutputCallback methods
    virtual HRESULT STDMETHODCALLTYPE RenderAudioSamples(bool preroll);

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    void StartPreroll();

private:
    DeckLinkManager* m_manager;
    IDeckLinkOutput* m_deckLinkOutput;
    std::atomic<ULONG> m_refCount;
    
    uint32_t         m_framesScheduled;
    BMDTimeValue     m_frameDuration;
    BMDTimeScale     m_timeScale;

    std::vector<uint8_t> m_audioBuffer;
    std::mutex           m_audioMutex;

    void ScheduleNextFrame();
};

#endif // PLAYBACK_DELEGATE_H