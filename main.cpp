#include <iostream>
#include <vector>
#include <string>
//#include <SDL2/SDL.h>

    enum EnumTest
{
    TEST1 = 1<<0,
    TEST2 = 1<<1,
    TEST3 = 1<<2,
    TEST4 = 1<<3,
};  

int main()
{   
    bool red = true;
    bool green = false;
    EnumTest myEnum = TEST1;

    if(myEnum == TEST1 && red == true)
    {
        std::cout << "TEST1 is set to 1" << std::endl;
    }
    else if(green == false)
    {
        std::cout << "TEST1 is not set" << std::endl;
    }

    std::cout << "EnumTest size: " << sizeof(EnumTest) << " bytes" << std::endl;
    
    return 0;
}