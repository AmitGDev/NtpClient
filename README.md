**NTPClient v1.0.0**

Offers a simple and flexible way to retrieve the current time from Network Time Protocol (NTP) servers.

**Author:** Amit Gefen

**License:** MIT License

<br>

**Overview**

NtpClient is a C++ header file designed to make retrieving the current time from NTP servers a breeze on Windows systems. It offers a single, user-friendly function, ntp_client::GetTime, that handles the communication details and delivers the synchronized time as a time_t value.

<br>

**Features**

- Simplicity: Acquiring the current time from NTP servers requires only one function call.
- Efficiency: Leverages UDP communication for swift time synchronization.
- Compatibility: Seamlessly integrates with Windows systems.
- Support: Adheres to the latest NTP version 4 specifications.
- Error Handling: Returns a clear indication (0) of errors for proper handling.

<br>

**Usage**

\- Include NtpClient.hpp (and NtpClient.cpp) in your project:
```cpp
#include "NtpClient.hpp"
```

\- Call ntp_client::GetTime (Pass the desired NTP server hostname as an argument.):

```cpp
time_t current_time = ntp_client::GetTime("time.google.com");
```

The function returns the current time as a time_t value. returns 0 on error.


<br>

**Example Usage**

See the **main.cpp** file for a comprehensive example.

<br>

**Dependencies**

Requires the Winsock library.
Link your project against ws2_32.lib.
