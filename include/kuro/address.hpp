#pragma once

#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>

#include <netinet/in.h>
#include <sys/un.h>

namespace kuro
{
    
class ipv4_address
{
public:
    explicit constexpr ipv4_address(const uint8_t(&bytes)[4]) : ipv4_address(bytes[0], bytes[1], bytes[2], bytes[3]) {} 
    explicit constexpr ipv4_address(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) : 
        ipv4_address((uint32_t(b1) << 24) + (uint32_t(b2) << 16) + (uint32_t(b3) << 8) + uint32_t(b4)) {}
    explicit constexpr ipv4_address(uint32_t addr) : m_addr(addr) {}

    uint32_t to_integer() const
    {
        return m_addr;
    }

    static constexpr ipv4_address loopback()
    {
        return ipv4_address(127, 0, 0, 1);
    }
    static constexpr ipv4_address any()
    {
        return ipv4_address(0, 0, 0, 0);
    }
    static std::optional<ipv4_address> from_string(std::string_view addr)
    {
        uint8_t bytes[4];
        int current_byte = 0;
        int b = 0;

        for (auto c : addr) {
            if (c >= '0' && c <= '9') {
                b = 10*b + (c - '0');
            } else if (c == '.') {
                if (b > 255) {
                    return {};
                }
                bytes[current_byte] = b;
                current_byte++;
                b = 0;
            } else {
                return {};
            }
        }

        if (b > 255) {
            return {};
        }
        bytes[current_byte] = b;
        current_byte++;

        if (current_byte != 4) {
            return {};
        }

        return ipv4_address(bytes);
    }

private:
    uint32_t m_addr;
};

inline std::ostream& operator<<(std::ostream& os, const ipv4_address& addr)
{
    auto addr_int = addr.to_integer();
    auto bytes = reinterpret_cast<uint8_t*>(&addr_int);
    return os << int(bytes[3]) << "." << int(bytes[2]) << "." << int(bytes[1]) << "." << int(bytes[0]);
}

class ipv6_address
{
public:
    constexpr ipv6_address(const uint16_t(&hextets)[8]) : 
        ipv6_address(hextets[0], hextets[1], hextets[2], hextets[3], hextets[4], hextets[5], hextets[6], hextets[7]) {}
    constexpr ipv6_address(uint16_t h1, uint16_t h2, uint16_t h3, uint16_t h4, uint16_t h5, uint16_t h6, uint16_t h7, uint16_t h8) :
        m_addr{h1, h2, h3, h4, h5, h6, h7, h8} {}
    
    static std::optional<ipv6_address> from_string(std::string_view addr)
    {
        uint16_t hextets[8] = {};
        
        auto double_colon = addr.find("::");
        if (double_colon == std::string_view::npos) {
            if (parse_fragment(addr, hextets) != 8) {
                return {};
            } else {
                return ipv6_address(hextets);
            }
        } else {
            if (addr.size() > double_colon + 2 && addr[double_colon + 2] == ':') {
                return {};
            }

            uint16_t fragment_1[8] = {};
            uint16_t fragment_2[8] = {};

            auto f1 = parse_fragment(addr.substr(0, double_colon), fragment_1);
            auto f2 = parse_fragment(addr.substr(double_colon + 2), fragment_2);
            if (f1 == -1 || f2 == -1 || f1 + f2 >= 7) {
                return {};
            }
            for (int i = 0; i < f1; ++i) {
                hextets[i] = fragment_1[i];
            }
            for (int i = 0; i < f2; ++i) {
                hextets[i + (8 - f2)] = fragment_2[i];
            }
            return ipv6_address(hextets);
        }
    }

    constexpr static ipv6_address loopback()
    {
        return ipv6_address(0, 0, 0, 0, 0, 0, 0, 1);
    }
    constexpr static ipv6_address any()
    {
        return ipv6_address(0, 0, 0, 0, 0, 0, 0, 0);
    }

    uint16_t fragment(int i) const
    {
        return m_addr[i];
    }

private:
    static int parse_fragment(std::string_view fragment, uint16_t* out)
    {
        std::string_view cur_str = fragment;
        std::size_t i = 0;

        while (cur_str.length()) {
            auto index = cur_str.find(':', 0);
            auto substr = cur_str.substr(0, index);
            cur_str = (index == std::string_view::npos) ? "" : cur_str.substr(index+1);
            if (substr.length() > 4 || substr.length() == 0) {
                return -1;
            }

            uint16_t hex = 0;
            for (auto c : substr) {
                uint8_t c2 = 0;
                if (c >= '0' && c <= '9') {
                    c2 = c - '0';
                } else if (c >= 'a' && c <= 'f') {
                    c2 = c + 10 - 'a';
                } else if (c >= 'A' && c <= 'F') {
                    c2 = c + 10 - 'A';
                } else { 
                    return -1;
                }
                hex = 16 * hex + c2;
            }
            out[i] = hex;
            i++;
        }

        return i;
    }

    uint16_t m_addr[8];
};

inline std::ostream& operator<<(std::ostream& os, const ipv6_address& addr)
{
    bool in_range = false;
    int start;
    int best_start;
    int max_size = 0;
    for (int i = 0; i < 8; ++i) {
        if (addr.fragment(i) == 0 && !in_range) {
            in_range = true;
            start = i;
        } else if (addr.fragment(i) != 0 && in_range) {
            in_range = false;
            if (i - start > max_size) {
                best_start = start;
                max_size = i - start;
            }
        }
    }

    if (in_range && 8 - start > max_size) {
        best_start = start;
        max_size = 8 - start;
    }

    os << std::hex;
    if (max_size < 2) {
        for (int i = 0; i < 7; ++i) {
            os << addr.fragment(i) << ":";
        }
        os << addr.fragment(7);
    } else {
        for (int i = 0; i < best_start; ++i) {
            os << addr.fragment(i);
            if (i != best_start - 1) {
                os << ":";
            }
        }
        os << "::";
        for (int i = best_start + max_size; i < 8; ++i) {
            os << addr.fragment(i);
            if (i != 7) {
                os << ":";
            }
        }
    }
    return os << std::dec;
}

namespace detail
{

inline sockaddr_in to_sockaddr(ipv4_address addr, uint16_t port)
{
    sockaddr_in s_addr{};
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = htonl(addr.to_integer());
    s_addr.sin_port = htons(port);

    return s_addr;   
}

inline sockaddr_in6 to_sockaddr(ipv6_address addr, uint16_t port)
{
    sockaddr_in6 s_addr{};
    s_addr.sin6_family = AF_INET6;
    s_addr.sin6_port = htons(port);

    for (int i = 0; i < 8; ++i) {
        s_addr.sin6_addr.__in6_u.__u6_addr16[i] = htons(addr.fragment(i));
    }

    return s_addr;
}

inline sockaddr_un to_sockaddr(std::string_view unix_addr)
{
    if (unix_addr.size() > sizeof(sockaddr_un::sun_path)) {
        throw std::runtime_error("Unix address too long");
    }

    sockaddr_un s_addr{};
    s_addr.sun_family = AF_UNIX;
    std::copy(unix_addr.begin(), unix_addr.end(), s_addr.sun_path);

    return s_addr;
}

inline auto from_sockaddr(sockaddr_in addr)
{
    ipv4_address ip(ntohl(addr.sin_addr.s_addr));
    uint16_t port = ntohs(addr.sin_port);
    return std::pair(ip, port);
}

inline auto from_sockaddr(sockaddr_in6 addr)
{
    uint16_t hextets[8];
    for (int i = 0; i < 8; ++i) {
        hextets[i] = ntohs(addr.sin6_addr.__in6_u.__u6_addr16[i]);
    }

    ipv6_address ip(hextets);
    uint16_t port = ntohs(addr.sin6_port);
    return std::pair(ip, port);
}

inline auto from_sockaddr(sockaddr_un addr)
{
    auto len = (addr.sun_path[0] == '\0') ? std::strlen(&addr.sun_path[1]) + 1 : std::strlen(addr.sun_path);
    return std::string(addr.sun_path, len);
}

}

}