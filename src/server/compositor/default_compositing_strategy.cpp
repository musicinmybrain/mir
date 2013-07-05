/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir/compositor/default_compositing_strategy.h"

#include "mir/compositor/rendering_operator.h"
#include "mir/compositor/overlay_renderer.h"
#include "mir/geometry/rectangle.h"
#include "mir/graphics/renderable.h"
#include "mir/graphics/renderer.h"
#include "mir/graphics/display_buffer.h"
#include "mir/surfaces/graphic_region.h"

#include <cassert>

namespace mc = mir::compositor;
namespace mg = mir::graphics;

mc::DefaultCompositingStrategy::DefaultCompositingStrategy(
    std::shared_ptr<Renderables> const& renderables,
    std::shared_ptr<mg::Renderer> const& renderer,
    std::shared_ptr<mc::OverlayRenderer> const& overlay_renderer)
    : renderables(renderables),
      renderer(renderer),
      overlay_renderer(overlay_renderer)
{
    assert(renderables);
    assert(renderer);
    assert(overlay_renderer);
}

namespace
{

struct FilterForVisibleRenderablesInRegion : public mc::FilterForRenderables
{
    FilterForVisibleRenderablesInRegion(mir::geometry::Rectangle const& enclosing_region)
        : enclosing_region(enclosing_region)
    {
    }
    bool operator()(mg::Renderable& renderable)
    {
        // TODO check against enclosing_region
        return renderable.should_be_rendered();
    }

    mir::geometry::Rectangle const& enclosing_region;
};

struct BypassFilter : public mc::FilterForRenderables
{
    BypassFilter() : bypassed(false), native_bo(nullptr)
    {
    }
    bool operator()(mg::Renderable& renderable)
    {
        // TODO
        bypassed = true;
        auto reg = renderable.graphic_region();
        native_bo = reg->native_buffer();
        return true;
    }

    bool bypassed;
    void *native_bo; // HACK
};

}

void mc::DefaultCompositingStrategy::render(
    graphics::DisplayBuffer& display_buffer)
{
    graphics::Renderer *bypass_renderer = display_buffer.direct_renderer();
    bool bypassed = false;

    //if (bypass_renderer)  // Some platforms can do bypass, not all
    if (false) // TODO enable when it works
    {
        BypassFilter bypass_filter;
        auto save_resource =
            [&](std::shared_ptr<void> const&)
            {
            };
        RenderingOperator bypass(*bypass_renderer, save_resource);

        display_buffer.make_current();
        renderables->for_each_if(bypass_filter, bypass);
        bypassed = bypass_filter.bypassed;
        if (bypassed)
            display_buffer.post_update(bypass_filter.native_bo);
    }

    if (!bypassed)
        BasicCompositingStrategy::render(display_buffer);
}

void mc::DefaultCompositingStrategy::compose_renderables(
    mir::geometry::Rectangle const& view_area,
    std::function<void(std::shared_ptr<void> const&)> save_resource)
{
    renderer->clear();

    RenderingOperator applicator(*renderer, save_resource);
    FilterForVisibleRenderablesInRegion selector(view_area);
    renderables->for_each_if(selector, applicator);

    overlay_renderer->render(view_area, save_resource);
}
