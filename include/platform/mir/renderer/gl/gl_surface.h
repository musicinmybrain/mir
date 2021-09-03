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
 * Authored by: Christopher James Halse Rogers
 */

#ifndef MIR_RENDERER_GL_SURFACE_H_
#define MIR_RENDERER_GL_SURFACE_H_

#include <memory>

namespace mir
{
namespace graphics
{
class Buffer;

namespace gl
{
class OutputSurface
{
public:
    virtual ~OutputSurface() = default;

    virtual void bind() = 0;

    virtual void make_current() = 0;

    // Naming: SwapBuffers? Commit? Claim current buffer?
    virtual auto commit() -> std::unique_ptr<graphics::Buffer> = 0;
};
}
}
}

#endif //MIR_RENDERER_GL_SURFACE_H_
