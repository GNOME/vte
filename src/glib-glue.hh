/*
 * Copyright © 2019 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cassert>

#include <functional>
#include <memory>

#include <glib.h>

namespace vte::glib {

template<typename T>
using free_ptr = std::unique_ptr<T, decltype(&g_free)>;

template<typename T>
free_ptr<T>
take_free_ptr(T* ptr)
{
        return {ptr, &g_free};
}

using string_ptr = free_ptr<char>;

inline string_ptr
take_string(char* str)
{
        return take_free_ptr(str);
}

class Error {
public:
        Error() = default;
        ~Error() { reset(); }

        Error(Error const&) = delete;
        Error(Error&&) = delete;
        Error& operator=(Error const&) = delete;
        Error& operator=(Error&&) = delete;

        operator GError* ()  noexcept { return m_error; }
        operator GError** () noexcept { return &m_error; }

        auto error()   const noexcept { return m_error != nullptr; }
        auto domain()  const noexcept { return error() ? m_error->domain : 0; }
        auto code()    const noexcept { return error() ? m_error->code : -1; }
        auto message() const noexcept { return error() ? m_error->message : nullptr; }

        void assert_no_error() const noexcept { g_assert_no_error(m_error); }

        bool matches(GQuark domain, int code) const noexcept
        {
                return error() && g_error_matches(m_error, domain, code);
        }

        void reset() noexcept { g_clear_error(&m_error); }

        bool propagate(GError** error) noexcept { g_propagate_error(error, m_error); m_error = nullptr; return false; }

private:
        GError* m_error{nullptr};
};

class Timer {
public:
        /* If the callback returns true, the timer is repeated; if the
         * callback returns false, the timer is removed.
         */
        using callback_type = std::function<bool()>;

        Timer(callback_type callback,
              char const* name)
                : m_callback(callback),
                  m_name(name)
        {
        }

        ~Timer() noexcept
        {
                abort();
        }

        Timer(Timer const&) = delete;
        Timer(Timer&&) = delete;
        Timer& operator=(Timer const&) = delete;
        Timer& operator=(Timer&&) = delete;

        constexpr operator bool() const noexcept { return m_source_id != 0; }

        class Priority {
        public:
                enum {
                      eHIGH = G_PRIORITY_HIGH,
                      eDEFAULT = G_PRIORITY_DEFAULT,
                      eHIGH_IDLE = G_PRIORITY_HIGH_IDLE,
                      eDEFAULT_IDLE = G_PRIORITY_DEFAULT_IDLE,
                      eLOW = G_PRIORITY_LOW,
                };
        };

        void schedule(unsigned int timeout,
                      int priority = Priority::eDEFAULT) noexcept
        {
                abort();
                m_source_id = g_timeout_add_full(priority,
                                                 timeout,
                                                 s_dispatch_timer_cb,
                                                 this,
                                                 s_destroy_timer_cb);
        }

        void schedule_seconds(unsigned int timeout,
                              int priority = Priority::eDEFAULT) noexcept
        {
                abort();
                m_source_id = g_timeout_add_seconds_full(priority,
                                                         timeout,
                                                         s_dispatch_timer_cb,
                                                         this,
                                                         s_destroy_timer_cb);
        }

        void schedule_idle(int priority = Priority::eDEFAULT) noexcept
        {
                abort();
                m_source_id = g_idle_add_full(priority,
                                              s_dispatch_timer_cb,
                                              this,
                                              s_destroy_timer_cb);
        }

        void abort() noexcept
        {
                if (m_source_id != 0) {
                        g_source_remove(m_source_id);
                        m_source_id = 0;
                }

                m_rescheduled = false;
        }

private:
        callback_type m_callback{};
        char const* m_name{nullptr};
        guint m_source_id{0};
        bool m_rescheduled{false};

        bool dispatch() noexcept {
                auto const id = m_source_id;
                auto const rv = m_callback();

                /* The Timer may have been re-scheduled or removed from within
                 * the callback. In this case, the callback must return false!
                 * m_source_id is now different (since the old source
                 * ID is still associated with the main context until we return from
                 * this function), after which invalidate_source() will be called,
                 * but must not overwrite m_source_id.
                 * In the non-rescheduled case, invalidate_source() must set
                 * m_source_id to 0.
                 */
                m_rescheduled = id != m_source_id;
                assert(!m_rescheduled || rv == false);
                return rv;
        }

        static gboolean s_dispatch_timer_cb(void* data) noexcept
        {
                auto timer = reinterpret_cast<Timer*>(data);
                return timer->dispatch();
        }

        void invalidate_source() noexcept
        {
                if (!m_rescheduled)
                        m_source_id = 0;
                m_rescheduled = false;
        }

        static void s_destroy_timer_cb(void* data) noexcept
        {
                auto timer = reinterpret_cast<Timer*>(data);
                timer->invalidate_source();
        }
};

} // namespace vte::glib
