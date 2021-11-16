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
 */

#include "kms_framebuffer.h"

#include <xf86drmMode.h>

namespace mgg = mir::graphics::gbm;

mgg::FBHandle::FBHandle(int drm_fd, uint32_t fb_id)
        : drm_fd{drm_fd},
          fb_id{fb_id}
{
}

mgg::FBHandle::~FBHandle()
{
    // TODO: Some sort of logging on failure?
    drmModeRmFB(drm_fd, fb_id);
}

auto mgg::FBHandle::get_drm_fb_id() const -> uint32_t
{
    return fb_id;
}
