/*
 * Copyright © 2021 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2 or 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#ifndef MIR_GRAPHICS_GBM_KMS_FRAMEBUFFER_H_
#define MIR_GRAPHICS_GBM_KMS_FRAMEBUFFER_H_

#include "mir/graphics/platform.h"

namespace mir
{
namespace graphics
{
namespace gbm
{
class FBHandle : public Framebuffer
{
public:
    FBHandle(int drm_fd, uint32_t fb_id);

    ~FBHandle() override;

    auto get_drm_fb_id() const -> uint32_t;

private:
    int const drm_fd;
    uint32_t const fb_id;
};
}
}
}


#endif //MIR_GRAPHICS_GBM_KMS_FRAMEBUFFER_H_
