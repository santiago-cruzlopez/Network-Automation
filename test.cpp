#include <iostream>
#include <cstring>
#include <iomanip>
#include <string>
#include <cmath>
#include <vector>
#include <csignal>
#include <SDL2/SDL.h>
#include <DeckLinkAPI.h>

// This code is just for testing purposes.
// sudo apt install pkgconf libsdl2-dev
// sudo apt install libunistring-dev libaom-dev libdav1d-dev

DeckLinkManager* g_deckLinkManager = nullptr;

void signal_handler(int signal) {
    if (g_deckLinkManager) {
        std::cout << "\nInterrupt signal (" << signal << ") received. Shutting down input..." << std::endl;
        g_deckLinkManager->g_do_exit = true; 
    }
}

using namespace std;
enum EnumTest{TEST1 = 1<<0, TEST2 = 1<<1,};  

int main(void)
{   
    // SDL
    SDL_Init(SDL_INIT_VIDEO);

    double pi = M_PI; 
    std::cout << "Pi: " << pi << std::endl;
    float x,y;

    cout << "Enter x: ";
    cin >> x;

    y = (pow(x,2)/(pow(pi,2)*((pow(x,2))+0.5)))*(1+(pow(x,2)/(pow(pi,2)*((pow(x,2))+0.5))));
    cout << "y: " << std::setprecision(5) << std::fixed << y << endl;

    EnumTest myEnum = TEST1;
    if(myEnum == TEST1){cout << "TEST1 is set to 1" << endl;}
    std::cout << "EnumTest size: " << sizeof(EnumTest) << " bytes" << std::endl;

    // Try to call a DeckLink API function to check linking
    IDeckLinkIterator* iterator = CreateDeckLinkIteratorInstance();
    if (iterator) {
        std::cout << "DeckLink API linked and CreateDeckLinkIteratorInstance() succeeded." << std::endl;
        iterator->Release();
        return 0;
    } else {
        std::cout << "DeckLink API not found or not linked properly." << std::endl;
        return 1;
    }
    
    return 0;
}