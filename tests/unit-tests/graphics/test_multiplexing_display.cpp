/*
 * Copyright © 2022 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 or 3,
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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include "mir/graphics/display_configuration.h"
#include "mir/test/doubles/mock_display.h"

#include "mir_toolkit/common.h"
#include "src/server/graphics/multiplexing_display.h"


namespace mtd = mir::test::doubles;
namespace mg = mir::graphics;
namespace geom = mir::geometry;

namespace rc
{
template<>
struct Arbitrary<geom::Size>
{
    static auto arbitrary() -> Gen<geom::Size>
    {
        return gen::build<geom::Size>(
            gen::set(&geom::Size::width, gen::map(gen::inRange(0, 32000), [](int w) { return geom::Width{w}; })),
            gen::set(&geom::Size::height, gen::map(gen::inRange(0, 32000), [](int h) { return geom::Height{h}; })));
    }
};

template<>
struct Arbitrary<MirPixelFormat>
{
    static auto arbitrary() -> Gen<MirPixelFormat> 
    {
        return gen::cast<MirPixelFormat>(gen::inRange<int>(0, mir_pixel_formats));
    }
};

template<>
struct Arbitrary<MirSubpixelArrangement>
{
    static auto arbitrary() -> Gen<MirSubpixelArrangement> 
    {
        return gen::element<MirSubpixelArrangement>(
            mir_subpixel_arrangement_unknown,
            mir_subpixel_arrangement_horizontal_rgb,
            mir_subpixel_arrangement_horizontal_bgr,
            mir_subpixel_arrangement_vertical_rgb,
            mir_subpixel_arrangement_vertical_bgr,
            mir_subpixel_arrangement_none);
    }
};

template<>
struct Arbitrary<mg::DisplayConfigurationMode>
{
    static auto arbitrary() -> Gen<mg::DisplayConfigurationMode>
    {
        auto refresh_in_mHz = gen::inRange(1000, 400000);
        auto refresh = gen::map(refresh_in_mHz, [](int mHz) { return static_cast<double>(mHz) / 1000; });
        return gen::construct<mg::DisplayConfigurationMode>(
            gen::arbitrary<geom::Size>(),
            refresh);
    }
};

template<>
struct Arbitrary<mir::graphics::DisplayConfigurationOutputType>
{
    static auto arbitrary() -> Gen<mg::DisplayConfigurationOutputType>
    {
        return gen::element<mg::DisplayConfigurationOutputType>(
            mg::DisplayConfigurationOutputType::unknown,
            mg::DisplayConfigurationOutputType::vga,
            mg::DisplayConfigurationOutputType::dvii,
            mg::DisplayConfigurationOutputType::dvid,
            mg::DisplayConfigurationOutputType::dvia,
            mg::DisplayConfigurationOutputType::composite,
            mg::DisplayConfigurationOutputType::svideo,
            mg::DisplayConfigurationOutputType::lvds,
            mg::DisplayConfigurationOutputType::component,
            mg::DisplayConfigurationOutputType::ninepindin,
            mg::DisplayConfigurationOutputType::displayport,
            mg::DisplayConfigurationOutputType::hdmia,
            mg::DisplayConfigurationOutputType::hdmib,
            mg::DisplayConfigurationOutputType::tv,
            mg::DisplayConfigurationOutputType::edp,
            mg::DisplayConfigurationOutputType::virt,
            mg::DisplayConfigurationOutputType::dsi,
            mg::DisplayConfigurationOutputType::dpi);
    }
};
/*
template<>
struct Arbitrary<mg::DisplayConfigurationOutput>
{
    static auto arbitrary() -> Gen<mg::DisplayConfigurationOutput>
    {
        
    }
};
*/
}

namespace
{


mg::DisplayConfigurationOutput const hidpi_laptop {

    mg::DisplayConfigurationOutputId{3},
    mg::DisplayConfigurationCardId{2},
    mg::DisplayConfigurationLogicalGroupId{1},
    mg::DisplayConfigurationOutputType::edp,
    {
        mir_pixel_format_argb_8888,
        mir_pixel_format_xrgb_8888
    },
    {
        {geom::Size{3840, 2160}, 59.98},
    },
    0,
    geom::Size{340, 190},
    true,
    true,
    geom::Point{0,0},
    0,
    mir_pixel_format_xrgb_8888,
    mir_power_mode_on,
    mir_orientation_normal,
    2.0f,
    mir_form_factor_monitor,
    mir_subpixel_arrangement_horizontal_rgb,
    {},
    mir_output_gamma_unsupported,
    {},
    {}
};
}

TEST(MultiplexingDisplay, forwards_for_each_display_sync_group)
{
    using namespace testing;

    std::vector<std::unique_ptr<mg::Display>> displays;

    for (int i = 0u; i < 2; ++i)
    {
        auto mock_display = std::make_unique<NiceMock<mtd::MockDisplay>>();
        EXPECT_CALL(*mock_display, for_each_display_sync_group(_)).Times(1);
        displays.push_back(std::move(mock_display));
    }

    mg::MultiplexingDisplay display{std::move(displays)};

    display.for_each_display_sync_group([](auto const&) {});
}

TEST(MultiplexingDisplay, configuration_is_union_of_all_displays)
{
    using namespace testing;


}
