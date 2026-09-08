// SPDX-FileCopyrightText: 2026 James Chapman
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "merovingian/canonicaljson/value.hpp"
#include "merovingian/rooms/room_version_policy.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace merovingian::events
{

struct EventIdResult final
{
    std::string event_id{};
    std::string error{};
};

struct EventHashResult final
{
    std::string sha256{};
    std::string error{};
};

[[nodiscard]] auto event_id_is_valid(std::string_view event_id) noexcept -> bool;
[[nodiscard]] auto make_content_hash(canonicaljson::Value const& event) -> EventHashResult;
[[nodiscard]] auto make_reference_hash(canonicaljson::Value const& event, rooms::RoomVersionPolicy const& policy)
    -> EventHashResult;
[[nodiscard]] auto make_reference_hash_event_id(canonicaljson::Value const& event,
                                                rooms::RoomVersionPolicy const& policy) -> EventIdResult;
// Spec: Matrix Server-Server API v1.19 — Room Version 4 § Event IDs
// URL: ../../docs/matrix-v1.19-spec/rooms/v4.md
//
// The event ID is "$" + the reference hash of the REDACTED event, and the
// redaction algorithm is room-version specific, so the caller must supply the
// event's own RoomVersionPolicy. There is no defaulted version: an ID computed
// under the wrong room version does not match the one other servers derive.
[[nodiscard]] auto make_content_hash_id(canonicaljson::Value const& event, rooms::RoomVersionPolicy const& policy)
    -> EventIdResult;

// Spec: Matrix Server-Server API v1.19 — Calculating the Content Hash for an Event
// URL: ../../docs/matrix-v1.19-spec/server-server-api.md#calculating-the-content-hash-for-an-event
//
// Returns true when the event's hashes.sha256 field matches the computed
// SHA-256 content hash. Returns false when the field is absent or incorrect.
[[nodiscard]] auto verify_pdu_content_hash(canonicaljson::Value const& event) -> bool;

} // namespace merovingian::events
