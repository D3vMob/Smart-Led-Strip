#include "group_store.h"

#include <Preferences.h>

#include <cstring>

#include "pairing.h"

namespace {

constexpr const char* kNamespace = "group";
constexpr const char* kKey = "members";
constexpr const char* kRoleKey = "role";
constexpr const char* kMotionNamespace = "motion";
constexpr size_t kMacBytes = sizeof(sls::Mac);

}  // namespace

std::vector<sls::Mac> load_group_members() {
  std::vector<sls::Mac> members;
  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) return members;
  const size_t length = preferences.getBytesLength(kKey);
  if (length > 0 && length % kMacBytes == 0 && length / kMacBytes <= sls::kMaxMembers) {
    uint8_t buffer[sls::kMaxMembers * kMacBytes];
    if (preferences.getBytes(kKey, buffer, sizeof(buffer)) == length) {
      for (size_t offset = 0; offset < length; offset += kMacBytes) {
        sls::Mac mac;
        std::memcpy(mac.data(), buffer + offset, kMacBytes);
        members.push_back(mac);
      }
    }
  }
  preferences.end();
  return members;
}

void save_group_members(const std::vector<sls::Mac>& members) {
  uint8_t buffer[sls::kMaxMembers * kMacBytes];
  size_t length = 0;
  for (const sls::Mac& mac : members) {
    if (length + kMacBytes > sizeof(buffer)) break;
    std::memcpy(buffer + length, mac.data(), kMacBytes);
    length += kMacBytes;
  }
  Preferences preferences;
  if (preferences.begin(kNamespace, false)) {
    preferences.putBytes(kKey, buffer, length);
    preferences.end();
  }
}

sls::SyncRole load_sync_role() {
  sls::SyncRole role = sls::SyncRole::Undecided;
  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) return role;
  const uint8_t stored = preferences.getUChar(kRoleKey, static_cast<uint8_t>(sls::SyncRole::Undecided));
  if (stored <= static_cast<uint8_t>(sls::SyncRole::Member)) role = static_cast<sls::SyncRole>(stored);
  preferences.end();
  return role;
}

void save_sync_role(sls::SyncRole role) {
  Preferences preferences;
  if (preferences.begin(kNamespace, false)) {
    preferences.putUChar(kRoleKey, static_cast<uint8_t>(role));
    preferences.end();
  }
}

void clear_motion_master_keys() {
  Preferences preferences;
  if (preferences.begin(kMotionNamespace, false)) {
    preferences.clear();
    preferences.end();
  }
}
