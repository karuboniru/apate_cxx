#include "mmapio.h"
#include <cstdint>
#include <filesystem>
#include <print>
#include <span>
using byte = unsigned char;

class file_mask {
private:
  static constexpr int maximumMaskLength = 2147483647 / 7; // 2GB/7=约300MB
  static constexpr int maskLengthIndicatorLength =
      4; // 存储面具长度的标记，长度为4个字节，可表示4GB的文件长度

public:
  static int Reveal(const std::filesystem::path &path) {
    auto filesize = std::filesystem::file_size(path);
    mmapio<byte[]> maped_file(path.c_str(), true);
    auto pos_header = filesize - maskLengthIndicatorLength;
    auto maskHeadLength = byte2int(&maped_file[pos_header]);
    std::span<byte, std::dynamic_extent> orig_head;
    if (maskHeadLength <=
        filesize - maskLengthIndicatorLength - maskHeadLength) {
      auto offset = filesize - maskLengthIndicatorLength - maskHeadLength;
      orig_head = std::span<byte, std::dynamic_extent>(&maped_file[offset],
                                                       maskHeadLength);
    } else {
      auto offset = maskHeadLength;
      auto length = filesize - maskLengthIndicatorLength - offset;
      std::println("header length: {}", length);
      orig_head =
          std::span<byte, std::dynamic_extent>(&maped_file[offset], length);
    }
    auto newsize = filesize - maskHeadLength - maskLengthIndicatorLength;

    std::println("I am about to create file of size {} from size {} with "
                 "header size: {}",
                 newsize, filesize, orig_head.size());
    for (size_t i = 0; i < orig_head.size(); i++) {
      maped_file[i] = orig_head[orig_head.size() - 1 - i];
    }
    maped_file.truncate(newsize);
    auto filename_remove_extension = path;
    filename_remove_extension.replace_extension("");
    std::println("{} -> {}", path.c_str(), filename_remove_extension.c_str());
    std::filesystem::rename(path, filename_remove_extension);
    return 0;
  }

  static uint32_t byte2int(const byte *in) {
    // convert in - in+3 to int,
    // assuming Little Endian (should I?)
    return ((uint32_t)in[0]) | ((uint32_t)in[1] << 8) |
           ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
  }
};

int main(int argc, char **argv) {
  if (argc == 1) {
    std::println(std::cerr,
                 "usage: ./main /path/to/file/provided [... more files]");
  }
  for (int i = 1; i < argc; i++) {
    file_mask::Reveal(argv[i]);
  }
  return 0;
}