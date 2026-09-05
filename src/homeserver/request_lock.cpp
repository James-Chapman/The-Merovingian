// SPDX-FileCopyrightText: 2026 James Chapman
// SPDX-License-Identifier: GPL-3.0-or-later

#include "merovingian/homeserver/request_lock.hpp"

namespace merovingian::homeserver
{
namespace
{

    // The guard owning runtime.mutex for this thread's in-flight request, or
    // null when this thread is not inside a request handler. Only ever read and
    // written by its own thread, so it needs no synchronisation of its own.
    thread_local auto* current_request_guard = static_cast<std::unique_lock<RuntimeMutex>*>(nullptr);

} // namespace

RequestLockScope::RequestLockScope(std::unique_lock<RuntimeMutex>& guard) noexcept
    : previous_{current_request_guard}
{
    current_request_guard = &guard;
}

RequestLockScope::~RequestLockScope()
{
    current_request_guard = previous_;
}

RuntimeLockRelease::RuntimeLockRelease() noexcept
{
    release(current_request_guard);
}

RuntimeLockRelease::RuntimeLockRelease(std::unique_lock<RuntimeMutex>& guard) noexcept
{
    release(&guard);
}

void RuntimeLockRelease::release(std::unique_lock<RuntimeMutex>* guard) noexcept
{
    if (guard == nullptr)
    {
        // No request handler on the stack and no guard in hand: there is no
        // mutex to act on. This is the federation worker and the direct-call
        // test path.
        return;
    }
    mutex_ = guard->mutex();
    if (mutex_ == nullptr)
    {
        return;
    }
    // Release the guard the caller named first, so that guard observes the
    // release (`owns_lock()` reports false for the duration) and is the last
    // thing re-acquired on exit.
    if (guard->owns_lock())
    {
        released_ = guard;
        released_->unlock();
    }
    // Every level still held after that belongs to an outer frame: a
    // dispatcher's guard, or a self-locking caller's. `runtime.mutex` is
    // recursive, so leaving one in place keeps the whole server serialised
    // behind whatever blocking work this scope was opened for. Drain them and
    // count them; the destructor restores exactly this many.
    while (mutex_->held_by_current_thread())
    {
        mutex_->unlock();
        ++outer_levels_;
    }
}

RuntimeLockRelease::~RuntimeLockRelease()
{
    // Outer levels first, then the caller's own guard, so the recursion depth
    // is rebuilt in the order it was taken. On one recursive mutex the order
    // is immaterial to correctness; it keeps the intent readable.
    for (auto level = std::size_t{0U}; level < outer_levels_; ++level)
    {
        mutex_->lock();
    }
    if (released_ != nullptr)
    {
        released_->lock();
    }
}

auto RuntimeLockRelease::outer_levels_released() const noexcept -> std::size_t
{
    return outer_levels_;
}

} // namespace merovingian::homeserver
