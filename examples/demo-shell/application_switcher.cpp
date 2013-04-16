/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "application_switcher.h"

#include "mir/shell/session_manager.h"

#include <linux/input.h>

#include <assert.h>

namespace me = mir::examples;
namespace msh = mir::shell;

me::ApplicationSwitcher::ApplicationSwitcher()
{
}

void me::ApplicationSwitcher::set_focus_controller(std::shared_ptr<msh::SessionManager> const& shell)
{
    focus_controller = shell;
}

bool me::ApplicationSwitcher::handles(MirEvent const& event)
{
    if (!focus_controller)
        return false;
    if (event.key.type != mir_event_type_key)
        return false;
    if (event.key.action != 0) // Key down
        return false;
    if (event.key.scan_code != KEY_TAB)  // TODO: Use keycode once we support keymapping on the server side
        return false;

    focus_controller->focus_next();

    return true;
}
