/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2011, Blender Foundation.
 */

#pragma once

#include "COM_SingleThreadedOperation.h"
#include "DNA_node_types.h"

namespace blender::compositor {

/* Utility functions used by glare, tone-map and lens distortion. */
/* Some macros for color handling. */
typedef float fRGB[4];

/* TODO: replace with BLI_math_vector. */
/* multiply c2 by color rgb, rgb as separate arguments */
#define fRGB_rgbmult(c, r, g, b) \
  { \
    c[0] *= (r); \
    c[1] *= (g); \
    c[2] *= (b); \
  } \
  (void)0

class GlareBaseOperation : public SingleThreadedOperation {
 private:
  /**
   * \brief Cached reference to the inputProgram
   */
  SocketReader *m_inputProgram;

  /**
   * \brief settings of the glare node.
   */
  NodeGlare *m_settings;

 public:
  /**
   * Initialize the execution
   */
  void initExecution() override;

  /**
   * Deinitialize the execution
   */
  void deinitExecution() override;

  void setGlareSettings(NodeGlare *settings)
  {
    this->m_settings = settings;
  }
  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;

 protected:
  GlareBaseOperation();

  virtual void generateGlare(float *data, MemoryBuffer *inputTile, NodeGlare *settings) = 0;

  MemoryBuffer *createMemoryBuffer(rcti *rect) override;
};

}  // namespace blender::compositor
