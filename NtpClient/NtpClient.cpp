/*
    NtpClient.cpp
    Copyright (c) 2024, Amit Gefen

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#include <Windows.h>
#include <cstdint> // For using uint32_t or similar types.

// Functions like WSAStartup, WSACleanup, socket, recv, sendto, etc., are part of the Winsock API.
// Including Ws2_32.lib ensures that the linker resolves references to these functions and includes
// them in the final executable.
#pragma comment(lib, "Ws2_32.lib")


namespace // (Anonymous namespace)
{

    // **** Timestamp class **** 

    // NTP Fixed-Point Timestamp Format.
    // Note: RFC 5905 (http://tools.ietf.org/html/rfc5905).
    class Timestamp final
    {
    public:

        uint32_t seconds_{ 0 }; // Seconds since Jan 1, 1900.
        uint32_t fraction_{ 0 }; // Fractional part of seconds. Integer number of 2^-32 seconds.


        // Reverses the Endianness of the timestamp.
        // Network byte order is big endian, so it needs to be switched before
        // sending or reading.
        void ReverseEndian() {
            ReverseEndianUint32(seconds_);
            ReverseEndianUint32(fraction_);
        }


        // Convert to time_t.
        // Returns the integer part of the timestamp in unix time_t format,
        // which is seconds since Jan 1, 1970.
        [[nodiscard]] time_t ToTimeT() const
        {
            constexpr time_t kSecondsIn24Hours = static_cast<time_t>(60) * 60 * 24, // 60s * 60m * 24h
                kDaysIn70Years = static_cast<time_t>(365) * 70; // 365d * 70y

            // A leap year is a calendar year with an extra day. 17 leap years between 1900 and 1970:
            // 1904, 1908, 1912, 1916, 1920, 1924, 1928, 1932, 1936, 1940, 1944, 1948, 1952, 1956, 1960, 1964, 1968
            const time_t time_since_epoch = seconds_ - kSecondsIn24Hours * (kDaysIn70Years + 17) & UINT32_MAX;

            return time_since_epoch;
        }

    protected:

        // Reverse the endianness of a 32-bit unsigned integer:
        void ReverseEndianUint32(uint32_t& x) {
            x = ((x & 0xFF000000) >> 24) |
                ((x & 0x00FF0000) >> 8) |
                ((x & 0x0000FF00) << 8) |
                ((x & 0x000000FF) << 24);
        }
    };


    // **** NtpMessage class ****

    // A Network Time Protocol Message.
    // According to RFC 5905 (http://tools.ietf.org/html/rfc5905).
    class NtpMessage final
    {
    public:

        // The NTP packet header format, depicted in Figure 8 of RFC 5905, is as follows:
        // 
        //       0                   1                   2                   3
        //       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //      |LI | VN  |Mode |    Stratum     |     Poll      |  Precision   |
        //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //      |                         Root Delay                            |
        //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //      |                         Root Dispersion                       |
        //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //      |                          Reference ID                         |
        //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //      |                                                               |
        //      +                     Reference Timestamp (64)                  +
        //      |                                                               |
        //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //      |                                                               |
        //      +                      Origin Timestamp (64)                    +
        //      |                                                               |
        //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //      |                                                               |
        //      +                      Receive Timestamp (64)                   +
        //      |                                                               |
        //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //      |                                                               |
        //      +                      Transmit Timestamp (64)                  +
        //      |                                                               |
        //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //      |                                                               |
        //      .                                                               .
        //      .                    Extension Field 1 (variable)               .
        //      .                                                               .
        //      |                                                               |
        //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //      |                                                               |
        //      .                                                               .
        //      .                    Extension Field 2 (variable)               .
        //      .                                                               .
        //      |                                                               |
        //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //      |                          Key Identifier                       |
        //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //      |                                                               |
        //      |                            dgst (128)                         |
        //      |                                                               |
        //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //  
        // The NTP packet header consists of several fields, including Leap Indicator (LI), Version Number (VN), Mode,
        // Stratum, Poll, Precision, Root Delay, Root Dispersion, Reference ID, Reference Timestamp, Origin Timestamp,
        // Receive Timestamp, Transmit Timestamp, and Extension Fields.

        uint8_t mode_ : 3;               // (Bit-field: 3 bits) Mode of the message sender. 3 = Client, 4 = Server.
        uint8_t version_ : 3;            // (Bit-field: 3 bits) Protocol version. Should be set to 4.
        uint8_t leap_ : 2;               // (Bit-field: 2 bits) Leap seconds warning. See: RFC section 7.3 (http://tools.ietf.org/html/rfc5905#section-7.3).

        uint8_t stratum_{ 0 };           // Servers between client and physical timekeeper. 1 = Server is Connected to Physical Source. 0 = Unknown.
        uint8_t poll_{ 0 };              // Max Poll Rate. In log2 seconds.
        uint8_t precision_{ 0 };         // Precision of the clock. In log2 seconds.

        // (^^^ All that above: 4 bytes (32 bits) in total ^^^) 

        uint32_t sync_distance_{ 0 };    // (32 bits in total) Round-trip to reference clock. NTP Short Format.
        uint32_t drift_rate_{ 0 };       // (32 bits in total) Dispersion to reference clock. NTP Short Format.

        uint8_t ref_clock_id_[4]{ 0 };   // (32 bits in total) Reference ID. For Stratum 1 devices, a 4-byte string. For other devices, 4-byte IP address.

        Timestamp ref_{};                // (64 bits in total) Reference Timestamp. The time when the system clock was last updated.
        Timestamp orig_{};               // (64 bits in total) Origin Timestamp. Send time of the request. Copied from the request.

        Timestamp rx_{};                 // (64 bits in total) Receive Timestamp. Receive time of the request.
        Timestamp tx_{};                 // (64 bits in total) Transmit Timestamp. Send time of the response. If only a single time is needed, use this one.


        // Constructor:
        NtpMessage() : mode_(0), version_(0), leap_(0) // C++20 supports initializing the bit-fields in the class definition.
        {

        }


        // Reverses the endianness of all timestamps.
        // Network byte order is big endian, so they need to be switched before sending and after reading.
        // Maintaining them in little endian makes them easier to work with locally, though.
        void ReverseEndian()
        {
            ref_.ReverseEndian();
            orig_.ReverseEndian();

            rx_.ReverseEndian();
            tx_.ReverseEndian();
        }


        // Receive an NTPMessage.
        // Return the number of bytes received, 0 on connection gracefully closed.
        int Receive(SOCKET socket)
        {
            // If no error occurs, recv returns the number of bytes received and the buffer pointed to by the buf parameter will contain this data received.
            // If the connection has been gracefully closed, the return value is zero.
            int bytes_received = recv(socket, reinterpret_cast<char*>(this), sizeof(*this), 0); // <-- Receives data from a connected socket or a bound connectionless socket.

            ReverseEndian();

            if (bytes_received == SOCKET_ERROR) {
                [[maybe_unused]] const auto error{ WSAGetLastError() }; // For debug.
                bytes_received = -1; // Set the return value to -1 to indicate an error.
            }

            return bytes_received;
        }


        // Send an NTPMessage.
        // Return the number of bytes sent, 0 on connection gracefully closed, -1 on error.
        int SendTo(const SOCKET socket, sockaddr_in* server_address)
        {
            ReverseEndian();

            // If no error occurs, recv returns the number of bytes received and the buffer pointed to by the buf parameter will contain this data received.
            // If the connection has been gracefully closed, the return value is zero.
            // Otherwise, a value of SOCKET_ERROR is returned, and a specific error code can be retrieved by calling WSAGetLastError.
            int bytes_sent = sendto(socket, reinterpret_cast<const char*>(this), sizeof(*this), 0,
                reinterpret_cast<sockaddr*>(server_address), sizeof(*server_address)); // <-- Sends data to a specific destination.

            ReverseEndian();

            if (bytes_sent == SOCKET_ERROR) {
                [[maybe_unused]] const auto error{ WSAGetLastError() }; // For debug.
                bytes_sent = -1; // Set the return value to -1 to indicate an error.
            }

            return bytes_sent;
        }
    };


    // **** WSA class ****

    // RAII wrapper for WSADATA:
    class WSA final
    {
    public:

        // Constructor:
        WSA()
        {
            // Initiates use of the Winsock DLL by a process.
            // If successful, returns zero.
            // On error, returns one of few error codes.
            // Note: An application can call WSAStartup more than once if it needs to obtain the WSADATA structure information more than once.
            // On each such call, the application can specify any version number (here: 2.2) supported by the Winsock DLL.
            error_ = WSAStartup(MAKEWORD(2, 2), &data_);
        }


        // Destructor:
        ~WSA()
        {
            // Terminates use of the Winsock 2 DLL (Ws2_32.dll).
            // The return value is zero if the operation was successful.
            // On error, the value SOCKET_ERROR is returned, and a specific error number can be retrieved by calling WSAGetLastError().
            // Attention: In multi-threaded environment, WSACleanup terminates Windows Sockets operations for all threads.
            if (WSACleanup() == SOCKET_ERROR) {
                [[maybe_unused]] const auto error{ WSAGetLastError() }; // For debug.
            }
        }


        // Get Data:
        const WSADATA& Data() const { return data_; }


        // Get Error:
        const int Error() const { return error_; }

    private:

        WSADATA data_{};
        int error_{ 0 };
    };

}


namespace ntp_client
{

    // **** GetTime function (Main API) ****
    // 
    // Get time from an NTP server.
    // Return 0 on error.
    time_t GetTime(const char* hostname) // For examole: Google NTP server (time.google.com).
    {
        time_t time_since_epoch{ 0 };
        WSA wsa{};

        if (wsa.Error() == 0) {

            NtpMessage msg = {}; // Initializes the struct to its default values.
            // Important, if you don't set the version/mode, the server will ignore you.
            msg.version_ = 3;
            msg.mode_ = 3; // Client

            // gethostbyname(name) retrieves host information corresponding to a host name from a host database.
            // If no error occurs, gethostbyname returns a pointer to the hostent structure. Otherwise, it returns
            // a null pointer and a specific error number can be retrieved by calling WSAGetLastError().
            if (const auto host_information = gethostbyname(hostname); host_information != nullptr) {
                // (on success:)

                // Converts the (Ipv4) Internet network address into an ASCII string in Internet standard dotted-decimal format:
                const auto ip = inet_ntoa(*reinterpret_cast<struct in_addr*>(*host_information->h_addr_list));

                // Set up the sockaddr_in structure (ref.: https://docs.microsoft.com/en-us/windows/win32/winsock/sockaddr-2):
                sockaddr_in server_address = {}; // Initializes the struct to its default values.
                server_address.sin_family = AF_INET; // The AF_INET address family is the address family for IPv4.
                server_address.sin_addr.s_addr = inet_addr(ip); // Converts a string containing an IPv4 dotted-decimal address into a proper address for the IN_ADDR structure.
                server_address.sin_port = htons(123); // Converts a u_short from host to TCP/IP network byte order (which is big-endian).

                if (const SOCKET socket = ::socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP); socket != INVALID_SOCKET) { // Creates a UDP socket
                    msg.SendTo(socket, &server_address); // <-- SEND

                    NtpMessage response = {}; // Initializes the struct to its default values.
                    response.Receive(socket); // <-- RECEIVE
                    time_since_epoch = response.tx_.ToTimeT();
                }

            } else {
                [[maybe_unused]] const auto error{ WSAGetLastError() }; // For debug.
            }
        }

        return time_since_epoch;
    }

}