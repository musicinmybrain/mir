/*
 * Copyright © 2012 Canonical Ltd.
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

#include "platform.h"
#include "buffer_allocator.h"
#include "display.h"
#include "mir/console_services.h"
#include "mir/emergency_cleanup_registry.h"
#include "mir/udev/wrapper.h"
#include "kms_framebuffer.h"
#include "mir/graphics/egl_error.h"
#include "mir/graphics/egl_extensions.h"

#define MIR_LOG_COMPONENT "platform-graphics-gbm-kms"
#include "mir/log.h"

#include <fcntl.h>
#include <boost/exception/all.hpp>

namespace mg = mir::graphics;
namespace mgg = mg::gbm;
namespace mgmh = mgg::helpers;

mgg::Platform::Platform(std::shared_ptr<DisplayReport> const& listener,
                        std::shared_ptr<ConsoleServices> const& vt,
                        EmergencyCleanupRegistry&,
                        BypassOption bypass_option,
                        std::unique_ptr<Quirks> quirks)
    : udev{std::make_shared<mir::udev::Context>()},
      drm{helpers::DRMHelper::open_all_devices(udev, *vt, *quirks)},
      // We assume the first DRM device is the boot GPU, and arbitrarily pick it as our
      // shell renderer.
      //
      // TODO: expose multiple rendering GPUs to the shell.
      gbm{std::make_shared<mgmh::GBMHelper>(drm.front()->fd)},
      listener{listener},
      vt{vt},
      bypass_option_{bypass_option}
{
}

namespace
{
auto gbm_device_for_udev_device(mir::udev::Device const& device) -> mgg::GBMDeviceUPtr
{
    if (auto node = device.devnode())
    {
        auto fd = open(node, O_RDWR | O_CLOEXEC);
        if (fd < 0)
        {
            BOOST_THROW_EXCEPTION((
                std::system_error{
                    errno,
                    std::system_category(),
                    "Failed to open DRM device"}));
        }
        mgg::GBMDeviceUPtr gbm{gbm_create_device(fd)};
        if (!gbm)
        {
            BOOST_THROW_EXCEPTION((std::runtime_error{"Failed to create GBM device"}));
        }
        return gbm;
    }

    BOOST_THROW_EXCEPTION((
        std::runtime_error{"Attempt to create GBM device from UDev device with no device node?!"}));
}

void initialise_egl(EGLDisplay dpy, int minimum_major_version, int minimum_minor_version)
{
    EGLint major, minor;

    if (eglInitialize(dpy, &major, &minor) == EGL_FALSE)
        BOOST_THROW_EXCEPTION(mg::egl_error("Failed to initialize EGL display"));

    if ((major < minimum_major_version) ||
        (major == minimum_major_version && minor < minimum_minor_version))
    {
        std::stringstream msg;
        msg << "Incompatible EGL version. Requested: "
            << minimum_major_version << "." << minimum_minor_version
            << " got: " << major << "." << minor;
        BOOST_THROW_EXCEPTION((std::runtime_error{msg.str()}));
    }
}

auto dpy_for_gbm_device(mgg::GBMDeviceUPtr const& device) -> EGLDisplay
{
    mg::EGLExtensions::PlatformBaseEXT platform_ext;

    auto const egl_display = platform_ext.eglGetPlatformDisplay(
        EGL_PLATFORM_GBM_KHR,      // EGL_PLATFORM_GBM_MESA has the same value.
        static_cast<EGLNativeDisplayType>(device.get()),
        nullptr);
    if (egl_display == EGL_NO_DISPLAY)
        BOOST_THROW_EXCEPTION(mg::egl_error("Failed to get EGL display"));

    return egl_display;
}
}

mgg::RenderingPlatform::RenderingPlatform(
    mir::udev::Device const& device,
    std::vector<std::shared_ptr<mg::DisplayPlatform>> const& /*displays*/)
    : device{gbm_device_for_udev_device(device)},
      dpy{dpy_for_gbm_device(this->device)}
{
    initialise_egl(dpy, 1, 4);
}

mir::UniqueModulePtr<mg::GraphicBufferAllocator> mgg::RenderingPlatform::create_buffer_allocator(
    mg::Display const& output)
{
    return make_module_ptr<mgg::BufferAllocator>(output);
}

auto mgg::RenderingPlatform::maybe_create_interface(
    std::shared_ptr<GraphicBufferAllocator> const& device,
    RendererInterfaceBase::Tag const& type_tag) -> std::shared_ptr<RendererInterfaceBase>
{
    if (dynamic_cast<GLRenderingProvider::Tag const*>(&type_tag))
    {
        auto ctx = std::dynamic_pointer_cast<BufferAllocator>(device)->shared_egl_context();
        return std::make_shared<mgg::GLRenderingProvider>(std::move(ctx));
    }
    return nullptr;
}


mir::UniqueModulePtr<mg::Display> mgg::Platform::create_display(
    std::shared_ptr<DisplayConfigurationPolicy> const& initial_conf_policy, std::shared_ptr<GLConfig> const& gl_config)
{
    return make_module_ptr<mgg::Display>(
        shared_from_this(),
        drm,
        gbm,
        vt,
        bypass_option_,
        initial_conf_policy,
        gl_config,
        listener);
}

mgg::BypassOption mgg::Platform::bypass_option() const
{
    return bypass_option_;
}

auto mgg::Platform::maybe_create_interface(DisplayInterfaceBase::Tag const& type_tag)
    -> std::shared_ptr<DisplayInterfaceBase>
{
    if (dynamic_cast<DumbDisplayProvider::Tag const*>(&type_tag))
    {
        return std::make_shared<mgg::DumbDisplayProvider>();
    }
    return {};
}
