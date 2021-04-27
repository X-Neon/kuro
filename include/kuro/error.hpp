#pragma once

#include <system_error>

namespace kuro::detail
{

inline void check_error(int ret_val)
{
    if (ret_val == -1) {
        throw std::system_error(errno, std::system_category());
    }
}

inline int check_fd(int fd)
{
    check_error(fd);
    return fd;
}

}