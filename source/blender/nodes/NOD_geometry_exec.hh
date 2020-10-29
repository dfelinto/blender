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
 */

#pragma once

#include "FN_generic_value_map.hh"

#include "BKE_geometry.hh"

#include "DNA_node_types.h"

namespace blender::nodes {

using bke::Geometry;
using bke::GeometryPtr;
using bke::make_geometry_mutable;
using bke::MeshComponent;
using bke::PointCloudComponent;
using fn::CPPType;
using fn::GMutablePointer;
using fn::GValueMap;

class GeoNodeInputs {
 private:
  const bNode *node_;
  GValueMap<StringRef> &values_;

 public:
  GeoNodeInputs(const bNode &node, GValueMap<StringRef> &values) : node_(&node), values_(values)
  {
  }

  /**
   * Get the input value for the input socket with the given identifier.
   *
   * The node calling becomes responsible for destructing the value before it is done
   * executing. This method can only be called once for each identifier.
   */
  GMutablePointer extract(StringRef identifier)
  {
#ifdef DEBUG
    this->check_extract(identifier);
#endif
    return values_.extract(identifier);
  }

  /**
   * Get the input value for the input socket with the given identifier.
   *
   * This method can only be called once for each identifier.
   */
  template<typename T> T extract(StringRef identifier)
  {
#ifdef DEBUG
    this->check_extract(identifier, &CPPType::get<T>());
#endif
    return values_.extract<T>(identifier);
  }

 private:
  void check_extract(StringRef identifier, const CPPType *requested_type = nullptr);
};

class GeoNodeOutputs {
 private:
  const bNode *node_;
  GValueMap<StringRef> &values_;

 public:
  GeoNodeOutputs(const bNode &node, GValueMap<StringRef> &values) : node_(&node), values_(values)
  {
  }

  /**
   * Move-construct a new value based on the given value and store it for the given socket
   * identifier.
   */
  void set_by_move(StringRef identifier, GMutablePointer value)
  {
#ifdef DEBUG
    BLI_assert(value.type() != nullptr);
    BLI_assert(value.get() != nullptr);
    this->check_set(identifier, *value.type());
#endif
    values_.add_new_by_move(identifier, value);
  }

  /**
   * Store the output value for the given socket identifier.
   */
  template<typename T> void set(StringRef identifier, T &&value)
  {
#ifdef DEBUG
    this->check_set(identifier, CPPType::get<std::decay_t<T>>());
#endif
    values_.add_new(identifier, std::forward<T>(value));
  }

 private:
  void check_set(StringRef identifier, const CPPType &value_type);
};

}  // namespace blender::nodes
