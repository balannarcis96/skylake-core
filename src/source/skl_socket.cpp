//!
//! \file skl_socket
//!
//! \brief UNIX - socket utils
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <cstring>
#include <unistd.h>
#include <cerrno>

#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "skl_socket"

static_assert(__is_same(skl::socket_t, decltype(socket(0, 0, 0))));
static_assert(sizeof(skl::ipv4_addr_t) == 4U);
static_assert(sizeof(skl::net_port_t) == 2U);
static_assert(skl::CInvalidSocket == -1);

namespace skl {

socket_t alloc_ipv4_tcp_socket() noexcept {
    return ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

socket_t alloc_ipv4_udp_socket() noexcept {
    return ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

bool set_sock_blocking(socket_t f_socket, bool f_blocking) noexcept {
    const i32 flags = fcntl(f_socket, F_GETFL, 0);
    if (0 > flags) {
        return false;
    }
    if (false == f_blocking) {
        if (0 > fcntl(f_socket, F_SETFL, flags | O_NONBLOCK)) {
            return false;
        }
    } else {
        if (0 > fcntl(f_socket, F_SETFL, flags & ~O_NONBLOCK)) {
            return false;
        }
    }

    return true;
}

bool set_sock_nodelay(socket_t f_socket, bool f_enable) noexcept {
    const auto flag   = i32(f_enable);
    const auto result = setsockopt(f_socket,
                                   IPPROTO_TCP,
                                   TCP_NODELAY,
                                   &flag,
                                   sizeof(flag));

    return result < 0 ? false : true;
}

bool set_udp_sock_broadcast(socket_t f_udp_socket, bool f_enable) noexcept {
    const auto flag   = i32(f_enable);
    const auto result = setsockopt(f_udp_socket,
                                   SOL_SOCKET,
                                   SO_BROADCAST,
                                   &flag,
                                   sizeof(flag));

    return result < 0 ? false : true;
}

bool tcp_connect(socket_t f_tcp_socket, ipv4_addr_t f_addr, net_port_t f_port) noexcept {
    struct sockaddr_in target{};
    target.sin_port        = ::htons(f_port);
    target.sin_family      = AF_INET;
    target.sin_addr.s_addr = le_to_be_u32(f_addr);

    return CInvalidSocket != ::connect(f_tcp_socket, reinterpret_cast<const sockaddr*>(&target), static_cast<int32_t>(sizeof(target)));
}

bool close_socket(socket_t f_socket) noexcept {
    return 0 == ::close(f_socket);
}

bool shutdown_socket(socket_t f_socket) noexcept {
    return 0 == ::shutdown(f_socket, SHUT_RDWR);
}

ipv4_addr_t ipv4_addr_from_str(const char* f_cstring) noexcept {
    struct in_addr ipv4_addr_bin{};
    if (1 == inet_pton(AF_INET, f_cstring, &ipv4_addr_bin)) {
        return be_to_le_u32(ipv4_addr_bin.s_addr);
    }

    return 0;
}

skl_result<ipv4_addr_t> ipv4_addr_from_str_safe(const char* f_cstring) noexcept {
    struct in_addr ipv4_addr_bin{};
    if (1 == inet_pton(AF_INET, f_cstring, &ipv4_addr_bin)) {
        return be_to_le_u32(ipv4_addr_bin.s_addr);
    }

    return skl_fail{};
}

i32 get_last_network_err() noexcept { return errno; }

bool bind_socket(socket_t f_socket, ipv4_addr_t f_addr, net_port_t f_port) noexcept {
    struct sockaddr_in target{};
    memset(&target, 0, sizeof(target));
    target.sin_port        = le_to_be_u16(f_port);
    target.sin_family      = AF_INET;
    target.sin_addr.s_addr = le_to_be_u32(f_addr);
    return 0 == ::bind(f_socket, reinterpret_cast<const sockaddr*>(&target), sizeof(target));
}

i32 udp_send(socket_t f_udp_socket, const byte* f_buffer, u16 f_send_bytes, ipv4_addr_t f_addr, net_port_t f_port) noexcept {
    struct sockaddr_in target{};
    memset(&target, 0, sizeof(target));
    target.sin_port        = le_to_be_u16(f_port);
    target.sin_family      = AF_INET;
    target.sin_addr.s_addr = le_to_be_u32(f_addr);

    return (i32)::sendto(f_udp_socket,
                         f_buffer,
                         f_send_bytes,
                         0,
                         reinterpret_cast<const sockaddr*>(&target),
                         sizeof(sockaddr_in));
}

i32 udp_recv(socket_t f_udp_socket, byte* f_buffer, u16 f_max_bytes, ipv4_addr_t& f_out_addr, net_port_t& f_out_port) noexcept {
    struct sockaddr_in target{};
    socklen_t          len = sizeof(target);

    const auto result = (i32)::recvfrom(f_udp_socket,
                                        f_buffer,
                                        f_max_bytes,
                                        0,
                                        reinterpret_cast<sockaddr*>(&target),
                                        &len);

    if (0 > result) {
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            return 0;
        }
        return result;
    }

    f_out_addr = be_to_le_u32(target.sin_addr.s_addr);
    f_out_port = be_to_le_u16(target.sin_port);

    return result;
}

u16 le_to_be_u16(u16 f_le) noexcept {
    return htons(f_le);
}

u32 le_to_be_u32(u32 f_le) noexcept {
    return htonl(f_le);
}

u16 be_to_le_u16(u16 f_be) noexcept {
    return ntohs(f_be);
}

u32 be_to_le_u32(u32 f_be) noexcept {
    return ntohl(f_be);
}

} // namespace skl
