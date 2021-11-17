/*
 * Copyright © 2016 Canonical Ltd.
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

#include <epoxy/egl.h>

#include "platform.h"
#include "buffer_allocator.h"
#include "display.h"
#include "utils.h"

#include "mir/graphics/egl_error.h"
#include "mir/renderer/gl/context.h"

#include <boost/throw_exception.hpp>
#include <boost/exception/errinfo_file_name.hpp>
#include <sys/sysmacros.h>
#include <one_shot_device_observer.h>

namespace mg = mir::graphics;
namespace mge = mir::graphics::eglstream;
namespace mgc = mir::graphics::common;

namespace
{
const auto mir_xwayland_option = "MIR_XWAYLAND_OPTION";
}

mge::RenderingPlatform::RenderingPlatform(EGLDisplay dpy)
    : ctx{std::make_unique<BasicEGLContext>(dpy)}
{
    setenv(mir_xwayland_option, "-eglstream", 1);
}

mge::RenderingPlatform::~RenderingPlatform()
{
    unsetenv(mir_xwayland_option);
}

mir::UniqueModulePtr<mg::GraphicBufferAllocator> mge::RenderingPlatform::create_buffer_allocator(
    mg::Display const&)
{
    return mir::make_module_ptr<mge::BufferAllocator>(ctx->make_share_context());
}

auto mge::RenderingPlatform::maybe_create_interface(
    std::shared_ptr<GraphicBufferAllocator> const& /*allocator*/,
    RendererInterfaceBase::Tag const& type_tag) -> std::shared_ptr<RendererInterfaceBase>
{
    if (dynamic_cast<graphics::GLRenderingProvider::Tag const*>(&type_tag))
    {
        return std::make_shared<mge::GLRenderingProvider>(ctx->make_share_context());
    }
    return nullptr;
}
