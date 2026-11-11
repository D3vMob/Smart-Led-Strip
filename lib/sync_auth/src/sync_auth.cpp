#include "sync_auth.h"

#include <cstring>

namespace sls {
namespace {

constexpr size_t kBlockSize = 64;

constexpr uint32_t kRoundConstants[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }

class Sha256 {
 public:
  void update(const uint8_t* data, size_t len) {
    total_ += len;
    while (len > 0) {
      size_t take = kBlockSize - used_;
      if (take > len) take = len;
      std::memcpy(buffer_ + used_, data, take);
      used_ += take;
      data += take;
      len -= take;
      if (used_ == kBlockSize) {
        compress(buffer_);
        used_ = 0;
      }
    }
  }

  Sha256Digest finish() {
    uint64_t bit_len = total_ * 8;
    const uint8_t pad = 0x80;
    update(&pad, 1);
    const uint8_t zero = 0;
    while (used_ != kBlockSize - 8) update(&zero, 1);
    uint8_t length[8];
    for (int i = 0; i < 8; ++i) length[i] = static_cast<uint8_t>(bit_len >> (56 - 8 * i));
    update(length, 8);
    Sha256Digest out;
    for (int i = 0; i < 8; ++i)
      for (int j = 0; j < 4; ++j) out[i * 4 + j] = static_cast<uint8_t>(state_[i] >> (24 - 8 * j));
    return out;
  }

 private:
  void compress(const uint8_t* block) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
      w[i] = (uint32_t(block[i * 4]) << 24) | (uint32_t(block[i * 4 + 1]) << 16) | (uint32_t(block[i * 4 + 2]) << 8) |
             uint32_t(block[i * 4 + 3]);
    for (int i = 16; i < 64; ++i) {
      uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
    for (int i = 0; i < 64; ++i) {
      uint32_t t1 = h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + kRoundConstants[i] + w[i];
      uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
  }

  uint32_t state_[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  uint8_t buffer_[kBlockSize];
  size_t used_ = 0;
  uint64_t total_ = 0;
};

}  // namespace

Sha256Digest sha256(const uint8_t* data, size_t len) {
  Sha256 h;
  h.update(data, len);
  return h.finish();
}

Sha256Digest hmac_sha256(const uint8_t* key, size_t key_len, const uint8_t* data, size_t len) {
  uint8_t block[kBlockSize] = {0};
  if (key_len > kBlockSize) {
    Sha256Digest hashed = sha256(key, key_len);
    std::memcpy(block, hashed.data(), hashed.size());
  } else if (key_len > 0) {
    std::memcpy(block, key, key_len);
  }
  uint8_t ipad[kBlockSize], opad[kBlockSize];
  for (size_t i = 0; i < kBlockSize; ++i) {
    ipad[i] = block[i] ^ 0x36;
    opad[i] = block[i] ^ 0x5c;
  }
  Sha256 inner;
  inner.update(ipad, kBlockSize);
  inner.update(data, len);
  Sha256Digest inner_digest = inner.finish();
  Sha256 outer;
  outer.update(opad, kBlockSize);
  outer.update(inner_digest.data(), inner_digest.size());
  return outer.finish();
}

}  // namespace sls

namespace sls {

namespace {

bool is_sync_type(MessageType type) {
  return type == MessageType::MatterSilent || type == MessageType::State || type == MessageType::Heartbeat;
}

// First kSyncTagBytes of the HMAC over the frame encoded with a zeroed tag; false if it cannot be encoded.
bool compute_tag(const Message& message, const GroupSecret& secret, std::array<uint8_t, kSyncTagBytes>& tag) {
  Message copy = message;
  copy.auth.tag.fill(0);
  uint8_t frame[kMaxMessageBytes];
  size_t n = encode(copy, frame, sizeof(frame));
  if (n == 0) return false;
  Sha256Digest mac = hmac_sha256(secret.data(), secret.size(), frame, n);
  for (size_t i = 0; i < kSyncTagBytes; ++i) tag[i] = mac[i];
  return true;
}

}  // namespace

void sign(Message& message, const GroupSecret& secret) {
  if (!is_sync_type(message.type)) return;
  std::array<uint8_t, kSyncTagBytes> tag{};
  if (compute_tag(message, secret, tag)) message.auth.tag = tag;
}

bool verify(const Message& message, const GroupSecret& secret) {
  if (!is_sync_type(message.type)) return false;
  std::array<uint8_t, kSyncTagBytes> expected{};
  if (!compute_tag(message, secret, expected)) return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < kSyncTagBytes; ++i) diff |= static_cast<uint8_t>(expected[i] ^ message.auth.tag[i]);
  return diff == 0;
}

}  // namespace sls
