// NtpClient.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "NtpClient.h"


int main()
{
    // Test NTP client:
    std::cout << "test ntp client (several hosts):\n";
    for (const auto& hostname : { "time.google.com", "time.facebook.com", "time.apple.com" }) {
        std::cout << ntp_client::GetTime(hostname) << " (host: " << hostname << ")\n";
    }
}

