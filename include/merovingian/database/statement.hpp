// SPDX-FileCopyrightText: 2026 James Chapman
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace merovingian::database
{

struct BoundValue final
{
    std::string value{};
    bool sensitive{false};
    // M-09: true when `value` is raw binary (e.g. a BYTEA/BLOB column payload)
    // rather than text. PostgreSQL's parameter path sends every value as a
    // null-terminated C string unless told otherwise, which silently
    // truncates binary payloads at an embedded NUL byte; backends that need
    // byte-exact round-tripping (see postgresql_store.cpp's
    // encode_postgresql_bytea_hex/decode_postgresql_bytea_hex) inspect this
    // flag. SQLite already binds every parameter by explicit length, so it
    // is unaffected and simply ignores the flag.
    bool binary{false};
};

struct PreparedStatement final
{
    std::string name{};
    std::string sql{};
    std::vector<BoundValue> parameters{};
};

struct StatementValidationResult final
{
    bool valid{false};
    std::string reason{};
};

[[nodiscard]] auto statement_name_is_valid(std::string_view name) noexcept -> bool;
[[nodiscard]] auto sql_shape_is_allowed(std::string_view sql) noexcept -> bool;
[[nodiscard]] auto prepared_statement_is_valid(PreparedStatement const& statement) -> StatementValidationResult;
[[nodiscard]] auto redacted_parameter_summary(std::vector<BoundValue> const& parameters) -> std::string;

} // namespace merovingian::database
