/*
 * Copyright © 2022 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 or 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "src/server/frontend_wayland/shm_backing.h"

#include <linux/memfd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <boost/throw_exception.hpp>
#include <experimental/type_traits>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace
{
bool error_indicates_tmpfile_not_supported(int error)
{
    return
        error == EISDIR ||        // Directory exists, but no support for O_TMPFILE
        error == ENOENT ||        // Directory doesn't exist, and no support for O_TMPFILE
        error == EOPNOTSUPP ||    // Filesystem that directory resides on does not support O_TMPFILE
        error == EINVAL;          // There apparently exists at least one development board that has a kernel
                                  // that incorrectly returns EINVAL. Yay.
}

int memfd_create(char const* name, unsigned int flags)
{
    return static_cast<int>(syscall(SYS_memfd_create, name, flags));
}

auto make_shm_fd(size_t size) -> mir::Fd
{
    int fd = memfd_create("mir-shm-test", MFD_CLOEXEC);
    if (fd == -1 && errno == ENOSYS)
    {
        fd = open("/dev/shm", O_TMPFILE | O_RDWR | O_EXCL | O_CLOEXEC, S_IRWXU);

        // Workaround for filesystems that don't support O_TMPFILE
        if (fd == -1 && error_indicates_tmpfile_not_supported(errno))
        {
            char template_filename[] = "/dev/shm/wlcs-buffer-XXXXXX";
            fd = mkostemp(template_filename, O_CLOEXEC);
            if (fd != -1)
            {
                if (unlink(template_filename) < 0)
                {
                    close(fd);
                    fd = -1;
                }
            }
        }
    }

    if (fd == -1)
    {
        BOOST_THROW_EXCEPTION(
            std::system_error(errno, std::system_category(), "Failed to open temporary file"));
    }

    if (ftruncate(fd, size) == -1)
    {
        close(fd);
        BOOST_THROW_EXCEPTION(
            std::system_error(errno, std::system_category(), "Failed to resize temporary file"));
    }

    return mir::Fd{fd};
}
}

TEST(ShmBacking, can_get_rw_range_covering_whole_pool)
{
    using namespace testing;

    constexpr size_t const shm_size = 4000;
    auto shm_fd = make_shm_fd(shm_size);
    auto backing = mir::shm::rw_pool_from_fd(shm_fd, shm_size);

    auto mappable = backing->get_rw_range(0, shm_size);

    auto mapping = mappable->map_rw();

    constexpr std::byte const fill_value{0xab};
    ::memset(mapping->data(), std::to_integer<int>(fill_value), shm_size);
    for(auto i = 0; i < shm_size; ++i)
    {
        EXPECT_THAT((*mapping)[i], Eq(fill_value));
    }
}

TEST(ShmBacking, get_rw_range_checks_the_range_fits)
{
    using namespace testing;

    constexpr size_t const shm_size = 4000;
    auto shm_fd = make_shm_fd(shm_size);
    auto backing = mir::shm::rw_pool_from_fd(shm_fd, shm_size);

    // Check each range from [0, shm_size + 1] - [shm_size - 1, shm_size + 1]
    for (auto i = 0u; i < shm_size - 1; ++i)
    {
        EXPECT_THROW(
            backing->get_rw_range(i, shm_size + 1 - i),
            std::runtime_error
        );
    }
}

TEST(ShmBacking, get_rw_range_checks_handle_overflows)
{
    using namespace testing;

    constexpr size_t const shm_size = 4000;
    auto shm_fd = make_shm_fd(shm_size);
    auto backing = mir::shm::rw_pool_from_fd(shm_fd, shm_size);

    EXPECT_THROW(
        backing->get_rw_range(std::numeric_limits<size_t>::max() - 1, 2),
        std::runtime_error
    );

    EXPECT_THROW(
        backing->get_rw_range(2, std::numeric_limits<size_t>::max() - 1),
        std::runtime_error
    );
}

TEST(ShmBackingDeathTest, reads_from_range_fault_after_range_and_backing_are_destroyed)
{
    using namespace testing;

    constexpr size_t const shm_size = 4000;
    auto shm_fd = make_shm_fd(shm_size);
    auto backing = mir::shm::rw_pool_from_fd(shm_fd, shm_size);

    auto range = backing->get_rw_range(0, shm_size);
    auto map = range->map_rw();

    // First demonstrate that we *can* read it while the range/backing is live
    for (auto const& c : *map)
    {
        // We haven't written anything explicitly, so the kernel has helpfully 0-initialised it
        EXPECT_THAT(c, Eq(std::byte{0}));
    }

    // Free all the resources!
    range = nullptr;
    backing = nullptr;
    EXPECT_EXIT(
        {
            for (auto const& c : *map)
            {
                EXPECT_THAT(c, Eq(std::byte{0}));
            }
        },
        KilledBySignal(SIGSEGV),
        ""
    );
}

TEST(ShmBackingDeathTest, writes_to_range_fault_after_range_and_backing_are_destroyed)
{
    using namespace testing;

    constexpr size_t const shm_size = 4000;
    auto shm_fd = make_shm_fd(shm_size);
    auto backing = mir::shm::rw_pool_from_fd(shm_fd, shm_size);

    auto range = backing->get_rw_range(0, shm_size);
    auto map = range->map_rw();

    // First demonstrate that we *can* read it while the range/backing is live
    ::memset(map->data(), 'a', map->len());

    // Free all the resources!
    range = nullptr;
    backing = nullptr;
    EXPECT_EXIT(
        {
            ::memset(map->data(), 'a', map->len());
        },
        KilledBySignal(SIGSEGV),
        ""
    );
}

TEST(ShmBacking, two_rw_ranges_see_each_others_changes)
{
    using namespace testing;

    constexpr size_t const shm_size = 4000;
    auto shm_fd = make_shm_fd(shm_size);
    auto backing = mir::shm::rw_pool_from_fd(shm_fd, shm_size);

    auto range_one = backing->get_rw_range(0, shm_size);
    auto range_two = backing->get_rw_range(shm_size / 2, shm_size / 2);

    auto map_one = range_one->map_rw();
    auto map_two = range_two->map_rw();

    auto const mapping_one_fill = std::byte{0xaa};
    auto const mapping_two_fill = std::byte{0xce};
    ::memset(map_one->data(), std::to_integer<int>(mapping_one_fill), map_one->len());
    ::memset(map_two->data(), std::to_integer<int>(mapping_two_fill), map_two->len());

    for (auto const& a : *map_two)
    {
        EXPECT_THAT(a, Eq(mapping_two_fill));
    }

    for (auto i = 0; i < shm_size / 2; ++i)
    {
        EXPECT_THAT((*map_one)[i], Eq(mapping_one_fill));
    }
    for (auto i = shm_size / 2; i < shm_size; ++i)
    {
        EXPECT_THAT((*map_one)[i], Eq(mapping_two_fill));
    }
}

TEST(ShmBacking, range_stays_vaild_after_backing_destroyed)
{
    using namespace testing;

    constexpr size_t const shm_size = 4000;
    auto shm_fd = make_shm_fd(shm_size);
    auto backing = mir::shm::rw_pool_from_fd(shm_fd, shm_size);

    auto range = backing->get_rw_range(0, shm_size);

    backing = nullptr;

    auto map = range->map_rw();
    ::memset(map->data(), 's', map->len());

    for (auto const& a : *map)
    {
        EXPECT_THAT(a, Eq(std::byte{'s'}));
    }
}

TEST(ShmBacking, map_into_valid_memory_is_not_marked_as_faulted)
{
    using namespace testing;

    constexpr size_t const shm_size = 4000;
    auto shm_fd = make_shm_fd(shm_size);
    auto backing = mir::shm::rw_pool_from_fd(shm_fd, shm_size);

    auto range = backing->get_rw_range(0, shm_size);

    auto map = range->map_rw();
    ::memset(map->data(), 's', map->len());

    for (auto const& a : *map)
    {
        EXPECT_THAT(a, Eq(std::byte{'s'}));
    }

    EXPECT_FALSE(range->access_fault());
}

TEST(ShmBacking, read_from_invalid_memory_returns_0)
{
    using namespace testing;

    constexpr size_t const shm_size = 4000;
    constexpr size_t const claimed_size = shm_size * 2;    // Lie about our backing size
    auto shm_fd = make_shm_fd(shm_size);
    auto backing = mir::shm::rw_pool_from_fd(shm_fd, claimed_size);

    auto range = backing->get_rw_range(0, claimed_size);

    auto map = range->map_ro();

    for (auto const& a : *map)
    {
        EXPECT_THAT(a, Eq(std::byte{0}));
    }
}

TEST(ShmBacking, access_fault_is_true_after_invaild_read)
{
    using namespace testing;

    constexpr size_t const shm_size = 4000;
    constexpr size_t const claimed_size = shm_size * 2;    // Lie about our backing size
    auto shm_fd = make_shm_fd(shm_size);
    auto backing = mir::shm::rw_pool_from_fd(shm_fd, claimed_size);

    auto range = backing->get_rw_range(0, claimed_size);

    auto map = range->map_ro();

    for (auto const& a : *map)
    {
        EXPECT_THAT(a, Eq(std::byte{0}));
    }

    EXPECT_TRUE(range->access_fault());
}
