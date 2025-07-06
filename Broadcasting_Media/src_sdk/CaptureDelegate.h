#ifndef CAPTURE_DELEGATE_H
#define CAPTURE_DELEGATE_H

#include "DeckLinkAPI.h"
#include <atomic>

// Forward declaration
class DeckLinkManager;

class CaptureDelegate : public IDeckLinkInputCallback {
public:
    CaptureDelegate(DeckLinkManager* manager, IDeckLinkInput* deckLinkInput);
    virtual ~CaptureDelegate();

    // IDeckLinkInputCallback methods
    virtual HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(BMDVideoInputFormatChangedEvents, IDeckLinkDisplayMode*, BMDDetectedVideoInputFormatFlags);
    virtual HRESULT STDMETHODCALLTYPE VideoInputFrameArrived(IDeckLinkVideoInputFrame*, IDeckLinkAudioInputPacket*);

    // IUnknown methods
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

private:
    DeckLinkManager* m_manager;
    IDeckLinkInput* m_deckLinkInput; 
    std::atomic<ULONG> m_refCount;
    uint64_t        m_frameCount; // Changed to uint64_t for potentially long captures
};

#endif // CAPTURE_DELEGATE_H