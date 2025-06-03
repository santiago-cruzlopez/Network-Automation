#include "CaptureDelegate.h"
#include "DeckLinkManager.h" 
#include <iostream>
#include <cstring>

CaptureDelegate::CaptureDelegate(DeckLinkManager* manager, IDeckLinkInput* deckLinkInput) :
    m_manager(manager), m_deckLinkInput(deckLinkInput), m_refCount(1), m_frameCount(0) {
    if (m_deckLinkInput)
        m_deckLinkInput->AddRef();
}

CaptureDelegate::~CaptureDelegate() {
    if (m_deckLinkInput)
        m_deckLinkInput->Release();
}

HRESULT STDMETHODCALLTYPE CaptureDelegate::VideoInputFormatChanged(
    BMDVideoInputFormatChangedEvents notificationEvents,
    IDeckLinkDisplayMode* newDisplayMode,
    BMDDetectedVideoInputFormatFlags detectedSignalFlags) {
    // Refer to IDeckLinkInputCallback::VideoInputFormatChanged method
    char* modeName = nullptr;
    if (newDisplayMode->GetName((const char**)&modeName) == S_OK) { // GetName is a method of IDeckLinkDisplayMode
        std::cout << "Input format changed to: " << modeName << std::endl;
        free(modeName); 
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CaptureDelegate::VideoInputFrameArrived(
    IDeckLinkVideoInputFrame* videoFrame,
    IDeckLinkAudioInputPacket* audioPacket) { // Method of IDeckLinkInputCallback
    
    if (m_manager && m_manager->g_do_exit) {
        return S_OK; 
    }

    m_frameCount++;

    if (videoFrame) {
        BMDTimecodeFormat timecodeFormat = bmdTimecodeRP188Any; //
        IDeckLinkTimecode* timecode = nullptr;
        if (videoFrame->GetTimecode(timecodeFormat, &timecode) == S_OK && timecode) { // GetTimecode is method of IDeckLinkVideoFrame
            char* timecodeStr = nullptr;
            if (timecode->GetString((const char**)&timecodeStr) == S_OK) { // GetString is method of IDeckLinkTimecode
                std::cout << "Input: Received frame " << m_frameCount << ", Timecode: " << timecodeStr << std::endl;
                free(timecodeStr); 
            }
            timecode->Release();
        } else {
            std::cout << "Input: Received frame " << m_frameCount << " (No valid timecode detected)" << std::endl;
        }
    } else {
        std::cout << "Input: Received NULL video frame (Frame Count: " << m_frameCount << ")" << std::endl;
    }
    
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CaptureDelegate::QueryInterface(REFIID iid, LPVOID *ppv) {
    // Standard COM QueryInterface implementation
    CFUUIDBytes iunknown = IID_IUnknown; // IID_IUnknown is part of the COM model
    CFUUIDBytes inputCallback = IID_IDeckLinkInputCallback; // Defined in DeckLinkAPI.h

    if (memcmp(&iid, &iunknown, sizeof(REFIID)) == 0) { // memcmp requires <cstring>
        *ppv = static_cast<IUnknown*>(this);
        AddRef();
        return S_OK;
    } else if (memcmp(&iid, &inputCallback, sizeof(REFIID)) == 0) { // memcmp requires <cstring>
        *ppv = static_cast<IDeckLinkInputCallback*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE CaptureDelegate::AddRef(void) {
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE CaptureDelegate::Release(void) {
    ULONG newRefValue = --m_refCount;
    if (newRefValue == 0) {
        delete this;
    }
    return newRefValue;
}