#include "DeckLinkManager.h"
#include <iostream>
#include <csignal>

DeckLinkManager* g_deckLinkManager = nullptr;

void signal_handler(int signal) {
    if (g_deckLinkManager) {
        std::cout << "\nInterrupt signal (" << signal << ") received. Shutting down input..." << std::endl;
        g_deckLinkManager->g_do_exit = true; 
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, signal_handler); 
    signal(SIGTERM, signal_handler);

    g_deckLinkManager = new DeckLinkManager();

    if (!g_deckLinkManager->Initialize()) {
        std::cerr << "Failed to initialize DeckLink Manager for input." << std::endl;
        delete g_deckLinkManager;
        return 1;
    }

    // Now that Initialize() is done, we can get the dynamic info
    std::cout << "SDI Input Test Application Initialized." << std::endl;
    std::cout << "Capturing from: " << g_deckLinkManager->GetInputDeviceName() 
              << " (SDK Sub-device index: " << g_deckLinkManager->GetInputDeviceSdkIndex() << ")" << std::endl;
    std::cout << "Video Mode: " << g_deckLinkManager->GetVideoModeName() 
              << ", Pixel Format: " << g_deckLinkManager->GetPixelFormatName() << std::endl;


    if (!g_deckLinkManager->StartCapture()) {
        std::cerr << "Failed to start DeckLink capture stream." << std::endl;
        g_deckLinkManager->StopCapture(); 
        delete g_deckLinkManager;
        return 1;
    }
    
    g_deckLinkManager->WaitForCaptureToFinish(); 

    std::cout << "Stopping capture stream..." << std::endl;
    g_deckLinkManager->StopCapture();

    delete g_deckLinkManager;
    g_deckLinkManager = nullptr;

    std::cout << "Input test application terminated." << std::endl;
    return 0;
}