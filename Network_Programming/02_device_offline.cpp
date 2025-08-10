#include <iostream>
#include <string>
#include <cstdlib>
#include <unistd.h>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <device_ip_or_hostname> <email_address>" << std::endl;
        return 1;
    }

    std::string device = argv[1];
    std::string email = argv[2];
    std::string ping_cmd = "ping -c 1 -W 1 " + device + " > /dev/null 2>&1";
    std::string mail_cmd_prefix = "echo 'Device " + device + " is offline' | mail -s 'Device Offline' " + email;

    while (true) {
        int ping_result = system(ping_cmd.c_str());
        if (ping_result != 0) {
            // Device is offline, send email
            system(mail_cmd_prefix.c_str());
        }
        sleep(60);  // Check every 60 seconds
    }

    return 0;
}