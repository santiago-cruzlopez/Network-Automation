#include "DeckLinkManager.h"
#include <iostream>
#include <csignal>

DeckLinkManager* g_deckLinkManager = nullptr;

void signal_handler(int signal) {
    if (g_deckLinkManager) {
        std::cout << "\nInterrupt signal (" << signal << ") received. Shutting down..." << std::endl;
        g_deckLinkManager->g_do_exit = true; 
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, signal_handler); 
    signal(SIGTERM, signal_handler);

    g_deckLinkManager = new DeckLinkManager();

    if (!g_deckLinkManager->Initialize()) {
        std::cerr << "Failed to initialize DeckLink Manager." << std::endl;
        delete g_deckLinkManager;
        return 1;
    }

    std::cout << "\n--- DeckLink Capture & Playout Initialized ---" << std::endl;
    std::cout << "Capturing from: " << g_deckLinkManager->GetInputDeviceName() 
              << " (SDK Sub-device index: " << g_deckLinkManager->GetInputDeviceSdkIndex() << ")" << std::endl;
    std::cout << "Playing out to: " << g_deckLinkManager->GetOutputDeviceName() 
              << " (SDK Sub-device index: " << g_deckLinkManager->GetOutputDeviceSdkIndex() << ")" << std::endl;
    std::cout << "Video Mode: " << g_deckLinkManager->GetVideoModeName() 
              << ", Pixel Format: " << g_deckLinkManager->GetPixelFormatName() << std::endl;
    std::cout << "----------------------------------------------\n" << std::endl;

    if (!g_deckLinkManager->StartStreams()) {
        std::cerr << "Failed to start DeckLink streams." << std::endl;
        g_deckLinkManager->StopStreams(); 
        delete g_deckLinkManager;
        return 1;
    }
    
    g_deckLinkManager->WaitForStreamsToFinish(); 

    std::cout << "Stopping streams..." << std::endl;

    delete g_deckLinkManager;
    g_deckLinkManager = nullptr;

    std::cout << "Application terminated." << std::endl;
    return 0;
}