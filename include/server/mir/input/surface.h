/*
 * Copyright © 2013 Canonical Ltd.
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
 *
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_INPUT_SURFACE_H_
#define MIR_INPUT_SURFACE_H_

#include "mir/geometry/rectangle.h"
#include <string>

namespace mir
{
namespace input
{
class Surface 
{
public:
    virtual std::string const& name() const = 0;
    virtual geometry::Rectangle size_and_position() const = 0;
    virtual bool input_region_contains(geometry::Point const& point) const = 0;

protected:
    Surface() = default; 
    virtual ~Surface() = default;
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface& ) = delete;
};

}
}
#endif /* MIR_INPUT_SURFACE_H_ */
