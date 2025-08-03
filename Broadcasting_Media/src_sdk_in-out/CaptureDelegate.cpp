#include "CaptureDelegate.h"
#include "DeckLinkManager.h"
#include <iostream>
#include <cstring> 

CaptureDelegate::CaptureDelegate(DeckLinkManager* manager) :
    m_manager(manager), m_refCount(1), m_frameCount(0) {}

HRESULT STDMETHODCALLTYPE CaptureDelegate::VideoInputFrameArrived(
    IDeckLinkVideoInputFrame* videoFrame,
    IDeckLinkAudioInputPacket* audioPacket) {
    
    if (!videoFrame || (m_manager && m_manager->g_do_exit)) {
        return S_OK;
    }

    m_frameCount++;

    void* videoFrameBytes;
    videoFrame->GetBytes(&videoFrameBytes);
    long videoDataSize = videoFrame->GetRowBytes() * videoFrame->GetHeight();

    void* audioFrameBytes = nullptr;
    uint32_t audioPacketSize = 0;
    if (audioPacket) {
        audioPacket->GetBytes(&audioFrameBytes);
        audioPacketSize = audioPacket->GetSampleFrameCount() * (m_manager->getAudioSampleType() == bmdAudioSampleType16bitInteger ? 2 : 4) * m_manager->getAudioChannels();
    }
    
    // Print frame dimensions
    std::cout << "Frame " << m_frameCount << ": Captured frame with shape (" 
              << videoFrame->GetHeight() << ", " << videoFrame->GetWidth() << ")" << std::endl;

    // Create a FrameData object and copy the data for the other thread
    FrameData newFrame;
    newFrame.videoData.resize(videoDataSize);
    memcpy(newFrame.videoData.data(), videoFrameBytes, videoDataSize);

    if (audioPacketSize > 0) {
        newFrame.audioData.resize(audioPacketSize);
        memcpy(newFrame.audioData.data(), audioFrameBytes, audioPacketSize);
    }
    
    m_manager->QueueFrameForPlayout(std::move(newFrame));

    return S_OK;
}

HRESULT STDMETHODCALLTYPE CaptureDelegate::VideoInputFormatChanged(
    BMDVideoInputFormatChangedEvents events, IDeckLinkDisplayMode* mode, BMDDetectedVideoInputFormatFlags flags) {
    std::cout << "Input video format changed." << std::endl;
    return S_OK;
}

// IUnknown implementation (AddRef, Release, QueryInterface)
HRESULT STDMETHODCALLTYPE CaptureDelegate::QueryInterface(REFIID iid, LPVOID *ppv) {
    CFUUIDBytes iunknown = IID_IUnknown;
    CFUUIDBytes inputCallback = IID_IDeckLinkInputCallback;
    if (memcmp(&iid, &iunknown, sizeof(REFIID)) == 0) {
        *ppv = static_cast<IUnknown*>(this);
        AddRef();
        return S_OK;
    } else if (memcmp(&iid, &inputCallback, sizeof(REFIID)) == 0) {
        *ppv = static_cast<IDeckLinkInputCallback*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = NULL; return E_NOINTERFACE;
}
ULONG STDMETHODCALLTYPE CaptureDelegate::AddRef(void) { return ++m_refCount; }
ULONG STDMETHODCALLTYPE CaptureDelegate::Release(void) {
    ULONG newRefValue = --m_refCount;
    if (newRefValue == 0) delete this;
    return newRefValue;
}