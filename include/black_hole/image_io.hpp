#pragma once
// Dependency-free image writers: PPM (P6), BMP (24-bit) and PNG (stored deflate).
// Blender loads PNG and BMP; PPM is handy for quick diffs.

#include "black_hole/cpu_renderer.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace bh {
namespace image_io {

/// Quantise linear float RGB to 8-bit with optional gamma (1.0 = none).
std::vector<std::uint8_t> to_rgb8(const render::Image& img, double gamma = 1.0);

bool write_ppm(const std::string& path, int w, int h, const std::vector<std::uint8_t>& rgb8, std::string& err);
bool write_bmp(const std::string& path, int w, int h, const std::vector<std::uint8_t>& rgb8, std::string& err);
bool write_png(const std::string& path, int w, int h, const std::vector<std::uint8_t>& rgb8, std::string& err);

/// Dispatch by extension (.png / .bmp / .ppm). Unknown extension → error.
bool write_image(const std::string& path, const render::Image& img, double gamma, std::string& err);

/// In-memory PNG encoding (used by tests to check the container is well formed).
std::vector<std::uint8_t> encode_png(int w, int h, const std::vector<std::uint8_t>& rgb8);

std::uint32_t crc32(const std::uint8_t* data, std::size_t len, std::uint32_t crc = 0);
std::uint32_t adler32(const std::uint8_t* data, std::size_t len);

}  // namespace image_io
}  // namespace bh
