#include "../include/PasswordHasher.h"

#include <array>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
constexpr std::array<std::uint32_t, 64> kSha256K = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
    0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
    0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
    0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
    0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
    0xc67178f2U};

inline std::uint32_t rotr(std::uint32_t x, std::uint32_t n)
{
    return (x >> n) | (x << (32U - n));
}

std::string bytesToHex(const std::vector<std::uint8_t>& bytes)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (std::uint8_t b : bytes)
    {
        oss << std::setw(2) << static_cast<unsigned int>(b);
    }
    return oss.str();
}

std::vector<std::uint8_t> hexToBytes(const std::string& hex)
{
    if (hex.size() % 2 != 0)
    {
        return {};
    }

    std::vector<std::uint8_t> out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2)
    {
        unsigned int value = 0;
        std::istringstream iss(hex.substr(i, 2));
        iss >> std::hex >> value;
        if (iss.fail())
        {
            return {};
        }
        out.push_back(static_cast<std::uint8_t>(value));
    }
    return out;
}

std::vector<std::uint8_t> sha256Bytes(const std::vector<std::uint8_t>& input)
{
    std::vector<std::uint8_t> data = input;
    const std::uint64_t bitLen = static_cast<std::uint64_t>(data.size()) * 8ULL;

    data.push_back(0x80U);
    while ((data.size() % 64U) != 56U)
    {
        data.push_back(0x00U);
    }

    for (int i = 7; i >= 0; --i)
    {
        data.push_back(static_cast<std::uint8_t>((bitLen >> (i * 8)) & 0xFFULL));
    }

    std::array<std::uint32_t, 8> h = {
        0x6a09e667U,
        0xbb67ae85U,
        0x3c6ef372U,
        0xa54ff53aU,
        0x510e527fU,
        0x9b05688cU,
        0x1f83d9abU,
        0x5be0cd19U};

    std::array<std::uint32_t, 64> w{};
    for (std::size_t chunk = 0; chunk < data.size(); chunk += 64)
    {
        for (std::size_t i = 0; i < 16; ++i)
        {
            const std::size_t idx = chunk + i * 4;
            w[i] = (static_cast<std::uint32_t>(data[idx]) << 24) |
                   (static_cast<std::uint32_t>(data[idx + 1]) << 16) |
                   (static_cast<std::uint32_t>(data[idx + 2]) << 8) |
                   static_cast<std::uint32_t>(data[idx + 3]);
        }

        for (std::size_t i = 16; i < 64; ++i)
        {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = h[0];
        std::uint32_t b = h[1];
        std::uint32_t c = h[2];
        std::uint32_t d = h[3];
        std::uint32_t e = h[4];
        std::uint32_t f = h[5];
        std::uint32_t g = h[6];
        std::uint32_t hh = h[7];

        for (std::size_t i = 0; i < 64; ++i)
        {
            const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ ((~e) & g);
            const std::uint32_t temp1 = hh + s1 + ch + kSha256K[i] + w[i];
            const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = s0 + maj;

            hh = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += hh;
    }

    std::vector<std::uint8_t> digest;
    digest.reserve(32);
    for (std::uint32_t v : h)
    {
        digest.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFU));
        digest.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFU));
        digest.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFU));
        digest.push_back(static_cast<std::uint8_t>(v & 0xFFU));
    }
    return digest;
}

std::string slowHashHex(const std::string& password, const std::string& saltHex)
{
    std::vector<std::uint8_t> salt = hexToBytes(saltHex);
    if (salt.empty())
    {
        return {};
    }

    std::vector<std::uint8_t> payload;
    payload.reserve(salt.size() + password.size());
    payload.insert(payload.end(), salt.begin(), salt.end());
    payload.insert(payload.end(), password.begin(), password.end());

    std::vector<std::uint8_t> digest = sha256Bytes(payload);

    constexpr int kIterations = 12000;
    for (int i = 0; i < kIterations; ++i)
    {
        std::vector<std::uint8_t> round;
        round.reserve(digest.size() + salt.size());
        round.insert(round.end(), digest.begin(), digest.end());
        round.insert(round.end(), salt.begin(), salt.end());
        digest = sha256Bytes(round);
    }

    return bytesToHex(digest);
}

bool constantTimeEquals(const std::string& a, const std::string& b)
{
    if (a.size() != b.size())
    {
        return false;
    }

    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return diff == 0;
}
} // namespace

std::string generateSaltHex(std::size_t numBytes)
{
    if (numBytes == 0)
    {
        return {};
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);

    std::vector<std::uint8_t> bytes;
    bytes.reserve(numBytes);
    for (std::size_t i = 0; i < numBytes; ++i)
    {
        bytes.push_back(static_cast<std::uint8_t>(dist(gen)));
    }
    return bytesToHex(bytes);
}

std::string hashPassword(const std::string& password, const std::string& saltHex)
{
    return slowHashHex(password, saltHex);
}

bool verifyPassword(const std::string& password, const std::string& saltHex, const std::string& expectedHashHex)
{
    const std::string actual = slowHashHex(password, saltHex);
    if (actual.empty())
    {
        return false;
    }
    return constantTimeEquals(actual, expectedHashHex);
}
