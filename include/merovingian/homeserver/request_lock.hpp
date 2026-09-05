// SPDX-FileCopyrightText: 2026 James Chapman
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "merovingian/homeserver/runtime_mutex.hpp"

#include <cstddef>
#include <mutex>

namespace merovingian::homeserver
{

// Publishes the `std::unique_lock` that currently owns `HomeserverRuntime::mutex`
// for this thread's in-flight request, so code deeper in the call stack can
// release that lock around a blocking network call without threading the lock
// object through every intervening signature.
//
// The publication is thread_local, so one request thread never observes
// another's guard. A thread that publishes nothing — the federation worker, a
// test calling a service function directly — leaves the publication null, and
// the default-constructed `RuntimeLockRelease` below then has no guard to
// release.
//
// Nesting is supported: the innermost scope wins and restores its predecessor
// on exit. That matches the one nesting path in the server, where the
// client-server dispatcher releases its own guard before delegating a media
// route to the local router, which then takes (and publishes) its own.
class RequestLockScope final
{
public:
    explicit RequestLockScope(std::unique_lock<RuntimeMutex>& guard) noexcept;
    ~RequestLockScope();

    RequestLockScope(RequestLockScope const&) = delete;
    auto operator=(RequestLockScope const&) -> RequestLockScope& = delete;
    RequestLockScope(RequestLockScope&&) = delete;
    auto operator=(RequestLockScope&&) -> RequestLockScope& = delete;

private:
    std::unique_lock<RuntimeMutex>* previous_{nullptr};
};

// Releases `HomeserverRuntime::mutex` for the lifetime of the scope and
// re-acquires it on exit, including when the guarded call throws.
//
// Holding `runtime.mutex` across outbound HTTP freezes the whole process:
// every client-server request and every inbound federation transaction
// contends on that one mutex, so one slow or unreachable peer stalls all of
// them for the full duration of the timeout — up to `remote_timeout_seconds`
// per destination. Wrap every blocking network call that can run beneath a
// request handler in one of these, and keep runtime state reads and mutations
// outside it.
//
// **It releases every recursion level this thread holds, not just one.**
// `runtime.mutex` is recursive and a self-locking service function called from
// a request handler holds it twice: once via the dispatcher's guard and once
// via its own. Dropping a single level leaves the mutex held for the whole
// blocking call — the defect shipped three times over
// (`create_room` 0.12.1, `leave_room` 0.12.3, `invite_user_by_threepid`
// 0.12.6), each time because the author released the guard they had in hand
// and not the one an outer frame held. Every level is restored on exit, in the
// same count, so outer frames resume owning exactly what they owned before.
//
// Two constructors, one behaviour:
//
//   RuntimeLockRelease{}        // releases via the thread's published guard
//   RuntimeLockRelease{guard}   // releases via a guard in hand
//
// The argument only says which `unique_lock` object should observe the release
// and re-acquire it — the set of levels released is the same either way, so
// choosing the "wrong" one can no longer leave the mutex held.
//
// Doing nothing is always a valid outcome: with no guard published and none
// passed, there is no mutex to act on and the scope neither unlocks nor
// re-locks.
class RuntimeLockRelease final
{
public:
    // Releases this thread's published request lock (see `RequestLockScope`)
    // and any further levels the thread holds.
    RuntimeLockRelease() noexcept;

    // Releases `guard` and any further levels this thread holds. Use this
    // whenever the guard is in hand rather than published: a nested call site
    // has its own guard, and the published one belongs to a different frame.
    explicit RuntimeLockRelease(std::unique_lock<RuntimeMutex>& guard) noexcept;

    // Re-acquires everything the constructor released. A failure to re-acquire
    // leaves the runtime's locking invariant broken with no way to signal it
    // from a destructor, so the resulting exception terminates rather than
    // allowing the caller to continue unsynchronised.
    ~RuntimeLockRelease();

    RuntimeLockRelease(RuntimeLockRelease const&) = delete;
    auto operator=(RuntimeLockRelease const&) -> RuntimeLockRelease& = delete;
    RuntimeLockRelease(RuntimeLockRelease&&) = delete;
    auto operator=(RuntimeLockRelease&&) -> RuntimeLockRelease& = delete;

    // Number of recursion levels released beyond the guard this scope was
    // given. A non-zero value means some outer frame held `runtime.mutex`
    // across this scope's blocking work and would have deadlocked the server
    // before this primitive started draining those levels. Exposed for tests
    // and diagnostics; correctness does not depend on anyone reading it.
    [[nodiscard]] auto outer_levels_released() const noexcept -> std::size_t;

private:
    void release(std::unique_lock<RuntimeMutex>* guard) noexcept;

    std::unique_lock<RuntimeMutex>* released_{nullptr};
    RuntimeMutex* mutex_{nullptr};
    std::size_t outer_levels_{0U};
};

} // namespace merovingian::homeserver
