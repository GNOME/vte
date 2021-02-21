/*
 * Copyright © 2020 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "clipboard-gtk.hh"
#include "gtk-glue.hh"
#include "widget.hh"
#include "vteinternal.hh"

#include <new>
#include <stdexcept>
#include <utility>

namespace vte::platform {

// Note:
// Each Clipboard is owned via std::shared_ptr by Widget, which drops that ref on unrealize.
// The Clipboard keeps a std::weak_ref back on Widget, and converts that to a std::shared_ptr
// via .lock() only when it wants to dispatch a callback.
// Clipboard::Offer and Clipboard::Request own their Clipboard as a std::shared_ptr.

Clipboard::Clipboard(Widget& delegate,
                     ClipboardType type) /* throws */
        : m_delegate{delegate.weak_from_this()},
          m_type{type}
{
        auto display = gtk_widget_get_display(delegate.gtk());

        switch (type) {
        case ClipboardType::PRIMARY:
                m_clipboard = vte::glib::make_ref
#if VTE_GTK == 3
                        (gtk_clipboard_get_for_display(display, GDK_SELECTION_PRIMARY));
#elif VTE_GTK == 4
                        (gdk_display_get_primary_clipboard(display));
#endif
                break;
        case ClipboardType::CLIPBOARD:
                m_clipboard = vte::glib::make_ref
#if VTE_GTK == 3
                        (gtk_clipboard_get_for_display(display, GDK_SELECTION_CLIPBOARD));
#elif VTE_GTK == 4
                        (gdk_display_get_clipboard(display));
#endif
                break;
        }

        if (!m_clipboard)
                throw std::runtime_error{"Failed to create clipboard"};
}

#if VTE_GTK == 3

class Clipboard::Offer {
public:
        Offer(Clipboard& clipboard,
              OfferGetCallback get_callback,
              OfferClearCallback clear_callback)
                : m_clipboard{clipboard.shared_from_this()},
                  m_get_callback{get_callback},
                  m_clear_callback{clear_callback}
        {
        }

        ~Offer() = default;

        auto& clipboard() const noexcept { return *m_clipboard; }

        static void run(std::unique_ptr<Offer> offer,
                        ClipboardFormat format) noexcept
        {
                auto [targets, n_targets] = targets_for_format(format);

                // Transfers ownership of *offer to the clipboard. If setting succeeds,
                // the clipboard will own *offer until the clipboard_data_clear_cb
                // callback is called.
                // If setting the clipboard fails, the clear callback will never be
                // called.
                if (gtk_clipboard_set_with_data(offer->clipboard().platform(),
                                                targets, n_targets,
                                                clipboard_get_cb,
                                                clipboard_clear_cb,
                                                offer.get())) {
                        gtk_clipboard_set_can_store(offer->clipboard().platform(), targets, n_targets);
                        offer.release(); // transferred to clipboard above
                }
        }

private:
        std::shared_ptr<Clipboard> m_clipboard;
        OfferGetCallback m_get_callback;
        OfferClearCallback m_clear_callback;

        void dispatch_get(ClipboardFormat format,
                          GtkSelectionData* data) noexcept
        try
        {
                if (auto delegate = clipboard().m_delegate.lock()) {
                        auto str = (*delegate.*m_get_callback)(clipboard(), format);
                        if (!str)
                                return;

                        switch (format) {
                        case ClipboardFormat::TEXT:
                                // This makes yet another copy of the data... :(
                                gtk_selection_data_set_text(data, str->data(), str->size());
                                break;

                        case ClipboardFormat::HTML: {
                                auto [html, len] = text_to_utf16_mozilla(*str);

                                // This makes yet another copy of the data... :(
                                if (html) {
                                        gtk_selection_data_set(data,
                                                               gtk_selection_data_get_target(data),
                                                               // or gdk_atom_intern_static_string("text/html"),
                                                               16,
                                                               reinterpret_cast<guchar const*>(html.get()),
                                                               len);
                                }
                                break;
                        }
                        }
                }
        }
        catch (...)
        {
                vte::log_exception();
        }

        void dispatch_clear() noexcept
        try
        {
                if (auto delegate = clipboard().m_delegate.lock()) {
                        (*delegate.*m_clear_callback)(clipboard());
                }
        }
        catch (...)
        {
                vte::log_exception();
        }

        static void
        clipboard_get_cb(GtkClipboard* clipboard,
                         GtkSelectionData* data,
                         guint info,
                         void* user_data) noexcept
        {
                if (int(info) != vte::to_integral(ClipboardFormat::TEXT) &&
                    int(info) != vte::to_integral(ClipboardFormat::HTML))
                        return;

                reinterpret_cast<Offer*>(user_data)->dispatch_get(ClipboardFormat(info), data);
        }

        static void
        clipboard_clear_cb(GtkClipboard* clipboard,
                           void* user_data) noexcept
        {
                // Assume ownership of the Offer, and delete it after dispatching the callback
                auto offer = std::unique_ptr<Offer>{reinterpret_cast<Offer*>(user_data)};
                offer->dispatch_clear();
        }

        static std::pair<GtkTargetEntry*, int>
        targets_for_format(ClipboardFormat format)
        {
                switch (format) {
                case vte::platform::ClipboardFormat::TEXT: {
                        static GtkTargetEntry *text_targets = nullptr;
                        static int n_text_targets;

                        if (text_targets == nullptr) {
                                auto list = vte::take_freeable(gtk_target_list_new(nullptr, 0));
                                gtk_target_list_add_text_targets(list.get(),
                                                                 vte::to_integral(ClipboardFormat::TEXT));

                                text_targets = gtk_target_table_new_from_list(list.get(), &n_text_targets);
                        }

                        return {text_targets, n_text_targets};
                }

                case vte::platform::ClipboardFormat::HTML: {
                        static GtkTargetEntry *html_targets = nullptr;
                        static int n_html_targets;

                        if (html_targets == nullptr) {
                                auto list = vte::take_freeable(gtk_target_list_new(nullptr, 0));
                                gtk_target_list_add_text_targets(list.get(),
                                                                 vte::to_integral(ClipboardFormat::TEXT));
                                gtk_target_list_add(list.get(),
                                                    gdk_atom_intern_static_string("text/html"),
                                                    0,
                                                    vte::to_integral(ClipboardFormat::HTML));

                                html_targets = gtk_target_table_new_from_list(list.get(), &n_html_targets);
                        }

                        return {html_targets,  n_html_targets};
                }
                default:
                        g_assert_not_reached();
                }
        }


        static std::pair<vte::glib::StringPtr, size_t>
        text_to_utf16_mozilla(std::string_view const& str) noexcept
        {
                // Use g_convert() instead of g_utf8_to_utf16() since the former
                // adds a BOM which Mozilla requires for text/html format.
                auto len = size_t{};
                auto data = g_convert(str.data(), str.size(),
                                      "UTF-16", // conver to UTF-16
                                      "UTF-8", // convert from UTF-8
                                      nullptr, // out bytes_read
                                      &len,
                                      nullptr);
                return {vte::glib::take_string(data), len};
        }

}; // class Clipboard::Offer

#endif /* VTE_GTK == 3 */

class Clipboard::Request {
public:
        Request(Clipboard& clipboard,
                RequestDoneCallback done_callback,
                RequestFailedCallback failed_callback)
                : m_clipboard{clipboard.shared_from_this()},
                  m_done_callback{done_callback},
                  m_failed_callback{failed_callback}
        {
        }

        ~Request() = default;

        auto& clipboard() const noexcept { return *m_clipboard; }

        static void run(std::unique_ptr<Request> request) noexcept
        {
#if VTE_GTK == 3
                auto platform = request->clipboard().platform();
                gtk_clipboard_request_text(platform,
                                           text_received_cb,
                                           request.release());
#endif /* VTE_GTK */
        }

private:
        std::shared_ptr<Clipboard> m_clipboard;
        RequestDoneCallback m_done_callback;
        RequestFailedCallback m_failed_callback;

#if VTE_GTK == 3
        void dispatch(char const *text) noexcept
        try
        {
                if (auto delegate = clipboard().m_delegate.lock()) {
                        if (text)
                                (*delegate.*m_done_callback)(clipboard(), {text, strlen(text)});
                        else
                                (*delegate.*m_failed_callback)(clipboard());
                }
        }
        catch (...)
        {
                vte::log_exception();
        }

        static void text_received_cb(GtkClipboard *clipboard,
                                     char const* text,
                                     gpointer data) noexcept
        {
                auto request = std::unique_ptr<Request>{reinterpret_cast<Request*>(data)};
                request->dispatch(text);
        }

#endif /* VTE_GTK */

}; // class Clipboard::Request

void
Clipboard::offer_data(ClipboardFormat format,
                      OfferGetCallback get_callback,
                      OfferClearCallback clear_callback) /* throws */
{
#if VTE_GTK == 3
        Offer::run(std::make_unique<Offer>(*this, get_callback, clear_callback), format);
#endif
}

void
Clipboard::set_text(std::string_view const& text) noexcept
{
#if VTE_GTK == 3
        gtk_clipboard_set_text(platform(), text.data(), text.size());
#endif
}

void
Clipboard::request_text(RequestDoneCallback done_callback,
                        RequestFailedCallback failed_callback) /* throws */
{
        Request::run(std::make_unique<Request>(*this, done_callback, failed_callback));
}

} // namespace vte::platform
