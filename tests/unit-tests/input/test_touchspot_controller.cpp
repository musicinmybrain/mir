/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "src/server/input/touchspot_controller.h"

#include "mir/graphics/graphic_buffer_allocator.h"
#include "mir/graphics/renderable.h"
#include "mir/graphics/buffer_writer.h"
#include "mir_test/fake_shared.h"
#include "mir_test_doubles/stub_buffer.h"
#include "mir_test_doubles/stub_scene.h"
#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/stub_input_scene.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include <assert.h>

namespace mi = mir::input;
namespace mg = mir::graphics;
namespace mt = mir::test;
namespace mtd = mt::doubles;
namespace geom = mir::geometry;

namespace
{

struct MockBufferAllocator : public mg::GraphicBufferAllocator
{
    MOCK_METHOD1(alloc_buffer, std::shared_ptr<mg::Buffer>(mg::BufferProperties const&));
    MOCK_METHOD0(supported_pixel_formats, std::vector<MirPixelFormat>(void));
};

struct StubBufferWriter : public mg::BufferWriter
{
    void write(std::shared_ptr<mg::Buffer> const& /* buffer */,
        unsigned char const* /* data */, size_t /* size */) override
    {
    }
};

struct StubScene : public mtd::StubInputScene
{
    void add_input_visualization(std::shared_ptr<mg::Renderable> const& overlay) override
    {
        overlays.push_back(overlay);
    }

    void remove_input_visualization(std::weak_ptr<mg::Renderable> const& overlay) override
    {
        auto l = overlay.lock();
        assert(l);
        
        auto it = std::find(overlays.begin(), overlays.end(), l);
        assert(it != overlays.end());

        overlays.erase(it);
    }
    
    void expect_spots_at(std::vector<geom::Point> spots)
    {
        for (auto overlay : overlays)
        {
            auto pos = overlay->screen_position().top_left;
            auto it = std::find(spots.begin(), spots.end(), pos);
            EXPECT_FALSE(it == spots.end());
            spots.erase(it);
        }
        // If there are left over spots then we didn't have an overlay corresponding to one
        EXPECT_EQ(0, spots.size());
    }
    
    std::vector<std::shared_ptr<mg::Renderable>> overlays;
};

struct TestTouchspotController : public ::testing::Test
{
    TestTouchspotController()
        : allocator(std::make_shared<MockBufferAllocator>()),
          writer(std::make_shared<StubBufferWriter>()),
          scene(std::make_shared<StubScene>())
    {
    }
    std::shared_ptr<MockBufferAllocator> const allocator;
    std::shared_ptr<mg::BufferWriter> const writer;
    std::shared_ptr<StubScene> const scene;
};

MATCHER(SoftwareBuffer, "")
{
    auto properties = static_cast<mg::BufferProperties const&>(arg);
    if (properties.usage != mg::BufferUsage::software)
        return false;
    return true;
}

}

TEST_F(TestTouchspotController, allocates_software_buffer_for_touchspots)
{
    using namespace ::testing;

    EXPECT_CALL(*allocator, alloc_buffer(SoftwareBuffer())).Times(1)
        .WillOnce(Return(std::make_shared<mtd::StubBuffer>()));
    mi::TouchspotController controller(allocator, writer, scene);
}

TEST_F(TestTouchspotController, touches_result_in_renderables_in_stack)
{
    using namespace ::testing;
    
    EXPECT_CALL(*allocator, alloc_buffer(SoftwareBuffer())).Times(1)
        .WillOnce(Return(std::make_shared<mtd::StubBuffer>()));
    mi::TouchspotController controller(allocator, writer, scene);
    
    controller.visualize_touches({ {{0,0}, 1} });

    scene->expect_spots_at({{0, 0}});
}

TEST_F(TestTouchspotController, spots_move)
{
    using namespace ::testing;
    
    EXPECT_CALL(*allocator, alloc_buffer(SoftwareBuffer())).Times(1)
        .WillOnce(Return(std::make_shared<mtd::StubBuffer>()));
    mi::TouchspotController controller(allocator, writer, scene);
    
    controller.visualize_touches({ {{0,0}, 1} });
    scene->expect_spots_at({{0, 0}});
    controller.visualize_touches({ {{1,1}, 1} });
    scene->expect_spots_at({{1, 1}});
}

TEST_F(TestTouchspotController, multiple_spots)
{
    using namespace ::testing;
    
    EXPECT_CALL(*allocator, alloc_buffer(SoftwareBuffer())).Times(1)
        .WillOnce(Return(std::make_shared<mtd::StubBuffer>()));
    mi::TouchspotController controller(allocator, writer, scene);
    
    controller.visualize_touches({ {{0,0}, 1}, {{1, 1}, 1}, {{3, 3}, 1} });
    scene->expect_spots_at({{0, 0}, {1, 1}, {3, 3}});
    controller.visualize_touches({ {{0,0}, 1}, {{1, 1}, 1}, {{3, 3}, 1}, {{5, 5}, 1} });
    scene->expect_spots_at({{0, 0}, {1, 1}, {3, 3}, {5, 5}});
    controller.visualize_touches({ {{1,1}, 1} });
    scene->expect_spots_at({{1, 1}});
    controller.visualize_touches({});
    scene->expect_spots_at({});
}
