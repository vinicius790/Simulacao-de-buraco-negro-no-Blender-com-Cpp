#include "black_hole/image_io.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>

namespace bh {
namespace image_io {
namespace {

void put_u32_be(std::vector<std::uint8_t>& v, std::uint32_t x) {
    v.push_back(static_cast<std::uint8_t>(x >> 24));
    v.push_back(static_cast<std::uint8_t>(x >> 16));
    v.push_back(static_cast<std::uint8_t>(x >> 8));
    v.push_back(static_cast<std::uint8_t>(x));
}

void put_u32_le(std::vector<std::uint8_t>& v, std::uint32_t x) {
    v.push_back(static_cast<std::uint8_t>(x));
    v.push_back(static_cast<std::uint8_t>(x >> 8));
    v.push_back(static_cast<std::uint8_t>(x >> 16));
    v.push_back(static_cast<std::uint8_t>(x >> 24));
}

void put_u16_le(std::vector<std::uint8_t>& v, std::uint16_t x) {
    v.push_back(static_cast<std::uint8_t>(x));
    v.push_back(static_cast<std::uint8_t>(x >> 8));
}

void png_chunk(std::vector<std::uint8_t>& out, const char type[4], const std::vector<std::uint8_t>& data) {
    put_u32_be(out, static_cast<std::uint32_t>(data.size()));
    std::vector<std::uint8_t> body(type, type + 4);
    body.insert(body.end(), data.begin(), data.end());
    out.insert(out.end(), body.begin(), body.end());
    put_u32_be(out, crc32(body.data(), body.size()));
}

bool write_bytes(const std::string& path, const std::vector<std::uint8_t>& bytes, std::string& err) {
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        err = "cannot write: " + path;
        return false;
    }
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!f) {
        err = "write failed: " + path;
        return false;
    }
    return true;
}

}  // namespace

std::uint32_t crc32(const std::uint8_t* data, std::size_t len, std::uint32_t crc) {
    // Magic static: initialisation is thread-safe (C++11), unlike a lazily
    // filled table guarded by a plain bool.
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t n = 0; n < 256; ++n) {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            t[n] = c;
        }
        return t;
    }();
    std::uint32_t c = crc ^ 0xFFFFFFFFu;
    for (std::size_t i = 0; i < len; ++i) c = table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

std::uint32_t adler32(const std::uint8_t* data, std::size_t len) {
    std::uint32_t a = 1, b = 0;
    for (std::size_t i = 0; i < len; ++i) {
        a = (a + data[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

std::vector<std::uint8_t> to_rgb8(const render::Image& img, double gamma) {
    std::vector<std::uint8_t> out(static_cast<std::size_t>(img.width) * img.height * 3);
    const double inv_gamma = (gamma > 0.0) ? 1.0 / gamma : 1.0;
    for (std::size_t i = 0; i < out.size(); ++i) {
        double v = std::clamp(static_cast<double>(img.rgb[i]), 0.0, 1.0);
        if (gamma != 1.0) v = std::pow(v, inv_gamma);
        out[i] = static_cast<std::uint8_t>(std::lround(v * 255.0));
    }
    return out;
}

bool write_ppm(const std::string& path, int w, int h, const std::vector<std::uint8_t>& rgb8, std::string& err) {
    std::vector<std::uint8_t> bytes;
    const std::string header = "P6\n" + std::to_string(w) + " " + std::to_string(h) + "\n255\n";
    bytes.insert(bytes.end(), header.begin(), header.end());
    bytes.insert(bytes.end(), rgb8.begin(), rgb8.end());
    return write_bytes(path, bytes, err);
}

bool write_bmp(const std::string& path, int w, int h, const std::vector<std::uint8_t>& rgb8, std::string& err) {
    const std::uint32_t row_bytes = (static_cast<std::uint32_t>(w) * 3 + 3) & ~3u;
    const std::uint32_t pixel_bytes = row_bytes * static_cast<std::uint32_t>(h);
    std::vector<std::uint8_t> b;
    b.reserve(54 + pixel_bytes);
    b.push_back('B');
    b.push_back('M');
    put_u32_le(b, 54 + pixel_bytes);
    put_u32_le(b, 0);
    put_u32_le(b, 54);
    put_u32_le(b, 40);
    put_u32_le(b, static_cast<std::uint32_t>(w));
    put_u32_le(b, static_cast<std::uint32_t>(h));
    put_u16_le(b, 1);
    put_u16_le(b, 24);
    put_u32_le(b, 0);
    put_u32_le(b, pixel_bytes);
    put_u32_le(b, 2835);
    put_u32_le(b, 2835);
    put_u32_le(b, 0);
    put_u32_le(b, 0);
    for (int y = h - 1; y >= 0; --y) {  // bottom-up
        for (int x = 0; x < w; ++x) {
            const std::size_t i = (static_cast<std::size_t>(y) * w + x) * 3;
            b.push_back(rgb8[i + 2]);
            b.push_back(rgb8[i + 1]);
            b.push_back(rgb8[i + 0]);
        }
        for (std::uint32_t p = static_cast<std::uint32_t>(w) * 3; p < row_bytes; ++p) b.push_back(0);
    }
    return write_bytes(path, b, err);
}

std::vector<std::uint8_t> encode_png(int w, int h, const std::vector<std::uint8_t>& rgb8) {
    std::vector<std::uint8_t> out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

    std::vector<std::uint8_t> ihdr;
    put_u32_be(ihdr, static_cast<std::uint32_t>(w));
    put_u32_be(ihdr, static_cast<std::uint32_t>(h));
    ihdr.push_back(8);  // bit depth
    ihdr.push_back(2);  // colour type RGB
    ihdr.push_back(0);
    ihdr.push_back(0);
    ihdr.push_back(0);
    png_chunk(out, "IHDR", ihdr);

    // Raw scanlines with filter byte 0.
    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(h) * (static_cast<std::size_t>(w) * 3 + 1));
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        const std::size_t off = static_cast<std::size_t>(y) * w * 3;
        raw.insert(raw.end(), rgb8.begin() + static_cast<std::ptrdiff_t>(off),
                   rgb8.begin() + static_cast<std::ptrdiff_t>(off + static_cast<std::size_t>(w) * 3));
    }

    // zlib stream with stored (uncompressed) deflate blocks.
    std::vector<std::uint8_t> z;
    z.push_back(0x78);
    z.push_back(0x01);
    std::size_t pos = 0;
    while (pos < raw.size() || raw.empty()) {
        const std::size_t n = std::min<std::size_t>(65535, raw.size() - pos);
        const bool last = (pos + n >= raw.size());
        z.push_back(last ? 1 : 0);
        z.push_back(static_cast<std::uint8_t>(n & 0xFF));
        z.push_back(static_cast<std::uint8_t>(n >> 8));
        z.push_back(static_cast<std::uint8_t>(~n & 0xFF));
        z.push_back(static_cast<std::uint8_t>((~n >> 8) & 0xFF));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos), raw.begin() + static_cast<std::ptrdiff_t>(pos + n));
        pos += n;
        if (raw.empty()) break;
    }
    put_u32_be(z, adler32(raw.data(), raw.size()));
    png_chunk(out, "IDAT", z);
    png_chunk(out, "IEND", {});
    return out;
}

bool write_png(const std::string& path, int w, int h, const std::vector<std::uint8_t>& rgb8, std::string& err) {
    return write_bytes(path, encode_png(w, h, rgb8), err);
}

bool write_image(const std::string& path, const render::Image& img, double gamma, std::string& err) {
    const auto rgb8 = to_rgb8(img, gamma);
    const std::size_t dot = path.find_last_of('.');
    std::string ext = (dot == std::string::npos) ? "" : path.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext == "png") return write_png(path, img.width, img.height, rgb8, err);
    if (ext == "bmp") return write_bmp(path, img.width, img.height, rgb8, err);
    if (ext == "ppm") return write_ppm(path, img.width, img.height, rgb8, err);
    err = "unsupported image extension (use .png, .bmp or .ppm): " + path;
    return false;
}

}  // namespace image_io
}  // namespace bh
