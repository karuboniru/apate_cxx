#pragma once

#include <cstddef>
#include <cstring>
#include <iostream>
#include <print>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

template <typename T_> class mmapio {
  constexpr static auto is_array = std::is_unbounded_array_v<T_>;
  using T = std::conditional_t<is_array, std::remove_extent_t<T_>, T_>;

private:
  T *data{static_cast<T *>(MAP_FAILED)};
  size_t m_size{0};
  int fd{-1};
  bool rw{};

public:
  mmapio(const std::string &filename, const bool allow_write = false,
         const size_t count_or_size = 0)
      : mmapio(filename.c_str(), allow_write, count_or_size) {}

  // if the T is unbounded array, then count_or_size
  // refer to count
  // if not, count_or_size refer to size
  // if 0 is passed
  // then the size will be determined by fstat
  // or sizeof T when <T> is not an array
  mmapio(const char *filename, const bool allow_write,
         const size_t count_or_size = 0)
      : rw(allow_write) {
    try {
      auto size = count_or_size * sizeof(T);
      // always set count to 1 if allow_write and T is not an array
      if (allow_write && !is_array && count_or_size == 0) {
        size = sizeof(T);
      } else if (allow_write && !is_array) {
        size = count_or_size;
      }

      if (allow_write)
        fd = ::open(filename, O_RDWR | O_CREAT, 0644);
      else
        fd = ::open(filename, O_RDONLY);

      if (fd == -1) {
        throw std::runtime_error(
            std::format("open failed, {}", std::strerror(errno)));
      }
      struct stat64 file_stat{};
      if (::fstat64(fd, &file_stat) == -1) {
        throw std::runtime_error(
            std::format("fstat failed, {}", std::strerror(errno)));
      }
      if (size) {
        if (file_stat.st_size < size) {
          if (allow_write) {
            if (::ftruncate(fd, size) == -1) {
              std::println(std::cerr, "ftruncate failed, {}",
                           std::strerror(errno));
            }
          } else {
            throw std::runtime_error("file size is smaller than requested, "
                                     "while open in read-only mode");
          }
        }
      } else {
        size = file_stat.st_size;
      }
      if (size == 0) {
        throw std::runtime_error("size is 0");
      }
      m_size = size;
      auto page_mode = allow_write ? (PROT_READ | PROT_WRITE) : PROT_READ;
      auto mmap_mode = allow_write ? MAP_SHARED : MAP_PRIVATE;
      data =
          static_cast<T *>(::mmap(nullptr, size, page_mode, mmap_mode, fd, 0));
      if (data == MAP_FAILED) {
        throw std::runtime_error(
            std::format("mmap failed, errno = {}", std::strerror(errno)));
      }
      // close(fd);
      // fd = -1;
    } catch (std::runtime_error &e) {
      if (data != MAP_FAILED) {
        ::munmap(data, m_size);
      }
      if (fd != -1) {
        close(fd);
      }
      throw e;
    }
  }
  mmapio(const mmapio &) = delete;
  mmapio(mmapio &&other) noexcept
      : data(other.data), m_size(other.m_size), fd(other.fd) {
    other.data = static_cast<T *>(MAP_FAILED);
    other.m_size = 0;
    other.fd = -1;
  }
  mmapio &operator=(const mmapio &) = delete;
  mmapio &operator=(mmapio &&other) noexcept {
    if (this != &other) {
      if (data != MAP_FAILED) {
        ::munmap(data, m_size);
      }
      if (fd != -1) {
        close(fd);
      }
      data = other.data;
      m_size = other.m_size;
      fd = other.fd;
      other.data = static_cast<T *>(MAP_FAILED);
      other.m_size = 0;
      other.fd = -1;
    }
    return *this;
  }
  ~mmapio() {
    if (::munmap(data, m_size) == -1) {
      std::println(std::cerr, "munmap failed, {}", std::strerror(errno));
    }
    if (fd != -1) {
      close(fd);
    }
  }

  bool truncate(long new_size) {
    if (!rw) {
      std::println(std::cerr, "File is not opened in write mode");
      return false;
    }
    if (::ftruncate(fd, new_size) == -1) {
      std::println(std::cerr, "ftruncate failed, {}", std::strerror(errno));
      return false;
    }
    if (::mremap(data, m_size, new_size, 0) == MAP_FAILED) {
      std::println(std::cerr, "mremap failed, {}", std::strerror(errno));
      return false;
    }
    m_size = new_size;
    return true;
  }

  auto &operator[](size_t index) const
    requires(is_array)
  {
    return data[index];
  }
  [[nodiscard]] size_t size() const
    requires(is_array)
  {
    return m_size / sizeof(T);
  }
  T *begin() const
    requires(is_array)
  {
    return data;
  }
  T *end() const
    requires(is_array)
  {
    return data + size();
  }

  auto &operator*() const
    requires(!is_array)
  {
    return *data;
  }
};