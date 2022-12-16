/*
 * Copyright © 2018 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "wl_data_device.h"
#include "wl_data_source.h"
#include "wl_surface.h"

#include "mir/events/pointer_event.h"
#include "mir/geometry/forward.h"
#include "mir/input/composite_event_filter.h"
#include "mir/scene/clipboard.h"
#include "mir/scene/surface.h"
#include "mir/wayland/client.h"
#include "mir/wayland/protocol_error.h"

#include "mir_toolkit/events/enums.h"
#include "mir_toolkit/events/event.h"

namespace mf = mir::frontend;
namespace mi = mir::input;
namespace ms = mir::scene;
namespace mw = mir::wayland;
namespace mev = mir::events;

class mf::WlDataDevice::ClipboardObserver : public ms::ClipboardObserver
{
public:
    ClipboardObserver(WlDataDevice* device) : device{device}
    {
    }

private:
    void paste_source_set(std::shared_ptr<ms::ClipboardSource> const& source) override
    {
        if (device)
        {
            device.value().paste_source_set(source);
        }
    }

    wayland::Weak<WlDataDevice> const device;
};

class mf::WlDataDevice::CursorObserver : public mi::EventFilter
{
public:
    CursorObserver(mf::DragWlSurface& surface, mf::WlDataDevice& data_device)
        : surface{surface},
          data_device{data_device}
    {}
          
    bool handle(MirEvent const& event) override
    {
        if (mir_event_get_type(&event) == mir_event_type_input)
        {
            auto const input_ev = mir_event_get_input_event(&event);
            auto const& ev_type = mir_input_event_get_type(input_ev);
            if (ev_type == mir_input_event_type_pointer)
            {
                std::shared_ptr<MirEvent> owned_event = mev::clone_event(event);
                auto const pointer_event = mir_input_event_get_pointer_event(owned_event->to_input());

                if (pointer_event->buttons() != mir_pointer_button_primary)
                {
                    data_device.end_drag();
                    return false;
                }

                auto const x = mir_pointer_event_axis_value(pointer_event, mir_pointer_axis_x);
                auto const y = mir_pointer_event_axis_value(pointer_event, mir_pointer_axis_y);

                auto const top_left = mir::geometry::Point{x, y};

                surface.scene_surface().value()->move_to(top_left);

                // TODO - send_motion_event()

                return false;
            }
        }

        return false;
    }

private:
    mf::DragWlSurface& surface;
    mf::WlDataDevice& data_device;
};

class mf::WlDataDevice::Offer : public wayland::DataOffer
{
public:
    Offer(WlDataDevice* device, std::shared_ptr<scene::ClipboardSource> const& source);

    void accept(uint32_t serial, std::optional<std::string> const& mime_type) override
    {
        (void)serial, (void)mime_type;
    }

    void receive(std::string const& mime_type, mir::Fd fd) override;

    void finish() override
    {
    }

    void set_actions(uint32_t dnd_actions, uint32_t preferred_action) override
    {
        (void)dnd_actions, (void)preferred_action;
    }

private:
    friend mf::WlDataDevice;
    wayland::Weak<WlDataDevice> const device;
    std::shared_ptr<scene::ClipboardSource> const source;
};

mf::WlDataDevice::Offer::Offer(WlDataDevice* device, std::shared_ptr<scene::ClipboardSource> const& source) :
    mw::DataOffer(*device),
    device{device},
    source{source}
{
    device->send_data_offer_event(resource);
    for (auto const& type : source->mime_types())
    {
        send_offer_event(type);
    }
}

void mf::WlDataDevice::Offer::receive(std::string const& mime_type, mir::Fd fd)
{
    if (device && device.value().current_offer.is(*this))
    {
        source->initiate_send(mime_type, fd);
    }
}

mf::WlDataDevice::WlDataDevice(
    wl_resource* new_resource,
    Executor& wayland_executor,
    scene::Clipboard& clipboard,
    mf::WlSeat& seat,
    mi::CompositeEventFilter& composite_event_filter)
    : mw::DataDevice(new_resource, Version<3>()),
      clipboard{clipboard},
      seat{seat},
      composite_event_filter{composite_event_filter},
      clipboard_observer{std::make_shared<ClipboardObserver>(this)}
{
    clipboard.register_interest(clipboard_observer, wayland_executor);
    // this will call focus_on() with the initial state
    seat.add_focus_listener(client, this);
}

mf::WlDataDevice::~WlDataDevice()
{
    clipboard.unregister_interest(*clipboard_observer);
    seat.remove_focus_listener(client, this);
}

void mf::WlDataDevice::set_selection(std::optional<wl_resource*> const& source, uint32_t serial)
{
    // TODO: verify serial
    (void)serial;
    if (source)
    {
        auto const wl_source = WlDataSource::from(source.value());
        wl_source->set_clipboard_paste_source();
    }
    else
    {
        clipboard.clear_paste_source();
    }
}

void mf::WlDataDevice::start_drag(
    std::optional<wl_resource*> const& source,
    wl_resource* origin,
    std::optional<wl_resource*> const& icon,
    uint32_t serial)
{
    // TODO: "The [origin surface] and client must have an active implicit grab that matches the serial"
    (void)source;
    if (!origin)
    {
        BOOST_THROW_EXCEPTION(
            mw::ProtocolError(resource, Error::role, "Origin surface does not exist."));
    }

    auto const icon_surface = WlSurface::from(icon.value_or(nullptr));  // TODO - is this safe?

    drag_surface.emplace(icon_surface);
    drag_surface->create_scene_surface();

    // serial is never null
    auto const drag_event = client->event_for(serial).value();
    if (mir_event_get_type(drag_event.get()) == mir_event_type_input)
    {
        auto const input_ev = mir_event_get_input_event(drag_event.get());
        auto const& ev_type = mir_input_event_get_type(input_ev);
        if (ev_type == mir_input_event_type_pointer)
        {
            auto const pointer_event = mir_input_event_get_pointer_event(input_ev);
            auto const x = mir_pointer_event_axis_value(pointer_event, mir_pointer_axis_x);
            auto const y = mir_pointer_event_axis_value(pointer_event, mir_pointer_axis_y);
            auto const top_left = mir::geometry::Point{x, y};

            drag_surface->scene_surface().value()->move_to(top_left);
        }
    }

    cursor_observer = std::make_shared<CursorObserver>(drag_surface.value(), *this);
    composite_event_filter.prepend(cursor_observer);

    // TODO: set initial position of drag_surface to current cursor position
}

void mf::WlDataDevice::end_drag()
{
    // TODO - detect if on surface expecting data, then copy into it
    send_leave_event();
    cursor_observer.reset();
    drag_surface.reset();
    current_offer = {};
}

void mf::WlDataDevice::focus_on(WlSurface* surface)
{
    has_focus = static_cast<bool>(surface);
    paste_source_set(clipboard.paste_source());
}

void mf::WlDataDevice::paste_source_set(std::shared_ptr<scene::ClipboardSource> const& source)
{
    if (source && has_focus)
    {
        if (!current_offer || current_offer.value().source != source)
        {
            current_offer = wayland::make_weak(new Offer{this, source});
            send_selection_event(current_offer.value().resource);
        }
    }
    else
    {
        if (current_offer)
        {
            current_offer = {};
            send_selection_event(std::nullopt);
        }
    }
}
