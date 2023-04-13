////////////////////////////////////////////////////////////////////////////////
// Mighter2d
//
// Copyright (c) 2023 Kwena Mashamaite
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
////////////////////////////////////////////////////////////////////////////////

#ifndef MIGHTER2D_GRIDMOVERCONTAINER_H
#define MIGHTER2D_GRIDMOVERCONTAINER_H

#include "Mighter2d/Config.h"
#include "Mighter2d/core/physics/grid/GridMover.h"
#include "Mighter2d/core/object/ObjectContainer.h"
#include "Mighter2d/core/event/Event.h"
#include "Mighter2d/core/time/Time.h"

namespace mighter2d {
    
    /// @internal
    namespace priv {
        class RenderTarget;
    }

    /**
     * @brief A container for GridMover objects
     */
    class MIGHTER2D_API GridMoverContainer : public ObjectContainer<GridMover> {
    public:
        /**
         * @internal
         * @brief Update the grid movers
         * @param deltaTime Time passed since last update
         *
         * @warning This function is intended for internal use only and
         * should never be called outside of Mighter2d
         */
        void update(Time deltaTime);

        /**
         * @internal
         * @brief Handle a system event
         * @param event The system event to be handled
         *
         * @warning This function is intended for internal use only and
         * should never be called outside of Mighter2d
         */
        void handleEvent(Event event);

        /**
         * @internal
         * @brief Render the grid movers path
         * @param window Window to render path on
         *
         * @warning This function is intended for internal use only and
         * should never be called outside of Mighter2d
         */
        void render(priv::RenderTarget& window) const;
    };
}

#endif //MIGHTER2D_GRIDMOVERCONTAINER_H