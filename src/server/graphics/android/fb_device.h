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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_GRAPHICS_ANDROID_FB_DEVICE_H_
#define MIR_GRAPHICS_ANDROID_FB_DEVICE_H_

#include "display_support_provider.h"

namespace mir
{
namespace graphics
{
namespace android
{

class AndroidBuffer;
class FBDevice : public DisplaySupportProvider 

{
public:
    virtual ~FBDevice() noexcept = default;

    virtual geometry::Size display_size() const = 0; 
    virtual geometry::PixelFormat display_format() const = 0; 
    virtual unsigned int number_of_framebuffers_available() const = 0;

    //todo: consolidate?? rename??
    virtual void set_next_frontbuffer(std::shared_ptr<AndroidBuffer> const& buffer) = 0;
    virtual void post(std::shared_ptr<AndroidBuffer> const& buffer) = 0;

protected:
    FBDevice() = default;
    FBDevice(FBDevice const&) = delete;
    FBDevice& operator=(FBDevice const&) = delete;
};

}
}
}
#endif /* MIR_GRAPHICS_ANDROID_FB_DEVICE_H_ */
