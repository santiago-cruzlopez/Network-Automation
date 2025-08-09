#include <DeckLinkAPI.h>
#include <iostream>
#include <atomic>
#include <cstring>

int main() {
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
}