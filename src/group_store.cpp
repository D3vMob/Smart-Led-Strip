#include "group_store.h"

#include <Preferences.h>

#include <cstring>

#include "pairing.h"

namespace {

constexpr const char* kNamespace = "group";
constexpr const char* kKey = "members";
constexpr const char* kRoleKey = "role";
constexpr const char* kCoordinatorKey = "coord";
constexpr const char* kSecretKey = "secret";
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

std::optional<sls::GroupSecret> load_group_secret() {
  std::optional<sls::GroupSecret> secret;
  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) return secret;
  sls::GroupSecret buffer;
  if (preferences.getBytesLength(kSecretKey) == buffer.size() &&
      preferences.getBytes(kSecretKey, buffer.data(), buffer.size()) == buffer.size()) {
    secret = buffer;
  }
  preferences.end();
  return secret;
}

void save_group_secret(const sls::GroupSecret& secret) {
  Preferences preferences;
  if (preferences.begin(kNamespace, false)) {
    preferences.putBytes(kSecretKey, secret.data(), secret.size());
    preferences.end();
  }
}

sls::SyncRecord load_sync_record() {
  sls::SyncRecord record;
  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) return record;
  const uint8_t stored = preferences.getUChar(kRoleKey, static_cast<uint8_t>(sls::SyncRole::Undecided));
  if (stored <= static_cast<uint8_t>(sls::SyncRole::Member)) record.role = static_cast<sls::SyncRole>(stored);
  sls::Mac coordinator;
  if (preferences.getBytesLength(kCoordinatorKey) == kMacBytes &&
      preferences.getBytes(kCoordinatorKey, coordinator.data(), kMacBytes) == kMacBytes) {
    record.coordinator = coordinator;
  }
  preferences.end();
  return record;
}

void save_sync_record(const sls::SyncRecord& record) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) return;
  preferences.putUChar(kRoleKey, static_cast<uint8_t>(record.role));
  if (record.coordinator) {
    preferences.putBytes(kCoordinatorKey, record.coordinator->data(), kMacBytes);
  } else {
    preferences.remove(kCoordinatorKey);
  }
  preferences.end();
}

void clear_motion_master_keys() {
  Preferences preferences;
  if (preferences.begin(kMotionNamespace, false)) {
    preferences.clear();
    preferences.end();
  }
}
