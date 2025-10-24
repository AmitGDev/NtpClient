### NTPClient v2.0.0

<br>

⚠️ **Note:** This release is not backward compatible with v1. If you require v1, please refer to [here](https://github.com/AmitGDev/NtpClient/tree/version-1).

<br>

Offers a simple and flexible way to retrieve the current timestamp from Network Time Protocol (NTP) servers.

**Author:** Amit Gefen

**License:** MIT License

<br>

**Overview**

NtpClient is a C++23 code designed to make retrieving the current timestamp from NTP servers a breeze on Windows systems. It offers a single, user-friendly function, GetNtpTimestamp(), that handles the communication details and delivers the NtpTimestamp values.

<br>

**Features**

- Simplicity: Acquiring the current timestamp from NTP servers requires only one function call.
- Efficiency: Leverages UDP communication for swift timestamp synchronization.
- Compatibility: Seamlessly integrates with Windows systems.
- Support: Adheres to the latest NTP version 4 specifications.
- Error Handling: Returns a clear indication of errors for proper handling.

<br>

**Usage**

\- Include NtpClient.hpp (and NtpClient.cpp) in your project:
```cpp
#include "NtpClient.hpp"
```

\- Call GetNtpTimestamp (Pass the desired NTP server hostname as an argument.):

```cpp
auto ntp_timestamp = GetNtpTimestamp("time.google.com");
```

The function returns the current NTP timestamp as a NtpTimestamp object. Or an error.


<br>

**Example Usage**

See the **main.cpp** file for a comprehensive example.

<br>

**Dependencies**

Requires the Winsock library.
Link your project against ws2_32.lib.

<br>
--
<br>

### V2.0.0 Key Improvements:

**C++23 Features**

- **`std::expected<T, E>`** - Modern error handling without exceptions, providing type-safe success/failure returns
- **`std::byteswap()`** - Efficient byte order conversion replacing manual bit manipulation
- **`std::to_underlying()`** - Safe enum-to-integer conversions for error codes
- **`std::bit_cast`** - Type-safe reinterpretation for pointer casts (replaces `reinterpret_cast`)

**C++20 Features**

- **`std::endian`** - Compile-time endianness detection, eliminating unnecessary runtime byte swapping on big-endian systems
- **Designated initializers** - Clear, readable struct initialization (`addrinfo hints{.ai_family = AF_INET, ...}`)
- **Enhanced `constexpr`** - Compile-time evaluation for constructors and utility methods

**Error Handling**

- **Custom error category system** - `ErrorCategory` class implementing `std::error_category`
- **Strongly-typed error enum** - `NtpError` enum class with descriptive error codes:
  - `kSuccess` - Operation completed successfully
  - `kWsaInitFailed` - Winsock initialization failed
  - `kInvalidHostname` - Invalid or too-long hostname
  - `kHostResolutionFailed` - DNS resolution failed
  - `kSocketCreationFailed` - Could not create UDP socket
  - `kTimeoutFailed` - Failed to configure socket timeouts
  - `kSendFailed` - Failed to send NTP request
  - `kReceiveFailed` - Failed to receive NTP response
  - `kInvalidResponse` - Response validation failed
- **Consistent error propagation** - All functions return `std::expected` for uniform error handling
- **System error integration** - Winsock errors wrapped in `std::error_code` for consistency

**RAII & Resource Management**

- **`Socket` class** - RAII wrapper with move semantics (non-copyable) ensuring automatic cleanup
- **`WinsockScope` class** - Guarantees WSA initialization/cleanup in exception-safe manner
- **`std::unique_ptr` with custom deleter** - Automatic `freeaddrinfo()` cleanup
- **Move semantics** - Proper implementation throughout with `std::exchange()`

**API & Design Improvements**

- Separation of Concerns:
  - Timestamp conversion logic removed from core NTP protocol class
  - Returns raw `NtpTimestamp` structure (seconds + fraction fields)
  - Client code controls time interpretation and format conversion
  - Clear boundary between protocol handling and application logic

- Networking Modernization:
  - **Deprecated API removal** - `gethostbyname()` → modern `getaddrinfo()`
  - **Winsock2 headers** - `<winsock2.h>` and `<ws2tcpip.h>` with proper include order
  - **IPv6-ready architecture** - Uses `addrinfo` structures (currently configured for IPv4)
  - **Proper address resolution** - Full DNS resolution with hint-based filtering


- Bit Field Replacement:
  - Eliminated unreliable compiler-dependent bit-fields
  - Manual byte manipulation with explicit masks and shifts
  - Predictable memory layout across compilers and platforms
  - Separate getters/setters (`SetMode()`, `mode()`, `SetVersion()`, `version()`, `SetLeapIndicator()`, `leap_indicator()`)

**Robustness & Safety**

- Timeouts & Reliability:
  - Socket timeouts - Configurable send/receive timeouts (`SO_RCVTIMEO`, `SO_SNDTIMEO`)
  - Default timeout - Prevents indefinite blocking on network issues
  - Timeout configuration API - `Socket::SetTimeout()` method

- Validation & Error Detection:
  - **`NtpMessage::IsValid()`** - Comprehensive response validation:
    - Server mode check (mode == 4)
    - Leap indicator alarm condition check (LI != 3)
    - Stratum range validation (1-15)
    - Non-zero transmit timestamp verification
  - Hostname validation - Length checks against maximum DNS hostname length (253 characters)
  - Response size verification - Ensures complete NTP packet received
  - Address family validation - Confirms IPv4 address resolution

- Type Safety:
  - `[[nodiscard]]` attributes - Compiler warnings when ignoring return values
  - `noexcept` specifications - Explicit exception guarantees for performance optimization
  - `constexpr` methods - Compile-time safety where possible
  - Const-correctness - Proper const qualifiers throughout

**Code Organization & Maintainability**

- Constants & Configuration:
  - Named constants for all magic numbers
  - Type-safe configuration values
  - Centralized constant definitions

- Namespace Structure:
  - Primary namespace
  - Anonymous namespace - Internal implementation details (classes, helpers)
  - Clear public API - Single public function `GetNtpTimestamp()`

- Endianness Handling:
  - `SwapEndiansIfNLE()` method - "Swap Endians If Native [is] Little Endian"
  - Zero runtime cost on big-endian systems - `if constexpr` eliminates code path
  - Eliminates the v1 `ReverseEndian()` naming ambiguity

- Performance Optimizations
  - **Compile-time endianness detection** - Zero runtime overhead with `std::endian::native`
  - **Compiler intrinsics** - `std::byteswap()` generates optimal assembly
  - **Move semantics** - Prevents unnecessary copies in resource management
  - **`constexpr` evaluation** - Compile-time computation where applicable
  - **Reduced branching** - Cleaner control flow with `std::expected`

<br>
--
<br>

### Breaking Changes from V1:

<br>

| Aspect | V1 | V2 |
|--------|----|----|
| **Function name** | `GetTime()` | `GetNtpTimestamp()` |
| **Parameter type** | `const char*` | `const std::string&` |
| **Return type** | `time_t` (0 on error) | `std::expected<NtpTimestamp, std::error_code>` |
| **Namespace** | `ntp_client` | `amitgdev::ntp_client` |
