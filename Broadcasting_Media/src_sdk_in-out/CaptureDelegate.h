#ifndef CAPTURE_DELEGATE_H
#define CAPTURE_DELEGATE_H

#include "DeckLinkAPI.h"
#include <atomic>

class DeckLinkManager;

class CaptureDelegate : public IDeckLinkInputCallback {
public:
    explicit CaptureDelegate(DeckLinkManager* manager);
    
    // IDeckLinkInputCallback methods
    virtual HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(BMDVideoInputFormatChangedEvents, IDeckLinkDisplayMode*, BMDDetectedVideoInputFormatFlags);
    virtual HRESULT STDMETHODCALLTYPE VideoInputFrameArrived(IDeckLinkVideoInputFrame*, IDeckLinkAudioInputPacket*);

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

private:
    DeckLinkManager* m_manager;
    std::atomic<ULONG> m_refCount;
    uint64_t m_frameCount;
};

#endif // CAPTURE_DELEGATE_H