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

#include "BKE_geometry_set.hh"
#include "BKE_persistent_data_handle.hh"

#include "DNA_node_types.h"

namespace blender::nodes {

using bke::PersistentDataHandleMap;
using bke::PersistentObjectHandle;
using fn::CPPType;
using fn::GMutablePointer;
using fn::GValueMap;

class GeoNodeExecParams {
 private:
  const bNode &node_;
  GValueMap<StringRef> &input_values_;
  GValueMap<StringRef> &output_values_;
  const PersistentDataHandleMap &handle_map_;

 public:
  GeoNodeExecParams(const bNode &node,
                    GValueMap<StringRef> &input_values,
                    GValueMap<StringRef> &output_values,
                    const PersistentDataHandleMap &handle_map)
      : node_(node),
        input_values_(input_values),
        output_values_(output_values),
        handle_map_(handle_map)
  {
  }

  /**
   * Get the input value for the input socket with the given identifier.
   *
   * The node calling becomes responsible for destructing the value before it is done
   * executing. This method can only be called once for each identifier.
   */
  GMutablePointer extract_input(StringRef identifier)
  {
#ifdef DEBUG
    this->check_extract_input(identifier);
#endif
    return input_values_.extract(identifier);
  }

  /**
   * Get the input value for the input socket with the given identifier.
   *
   * This method can only be called once for each identifier.
   */
  template<typename T> T extract_input(StringRef identifier)
  {
#ifdef DEBUG
    this->check_extract_input(identifier, &CPPType::get<T>());
#endif
    return input_values_.extract<T>(identifier);
  }

  /**
   * Move-construct a new value based on the given value and store it for the given socket
   * identifier.
   */
  void set_output_by_move(StringRef identifier, GMutablePointer value)
  {
#ifdef DEBUG
    BLI_assert(value.type() != nullptr);
    BLI_assert(value.get() != nullptr);
    this->check_set_output(identifier, *value.type());
#endif
    output_values_.add_new_by_move(identifier, value);
  }

  /**
   * Store the output value for the given socket identifier.
   */
  template<typename T> void set_output(StringRef identifier, T &&value)
  {
#ifdef DEBUG
    this->check_set_output(identifier, CPPType::get<std::decay_t<T>>());
#endif
    output_values_.add_new(identifier, std::forward<T>(value));
  }

  /**
   * Get the node that is currently being executed.
   */
  const bNode &node() const
  {
    return node_;
  }

  const PersistentDataHandleMap &handle_map() const
  {
    return handle_map_;
  }

 private:
  void check_extract_input(StringRef identifier, const CPPType *requested_type = nullptr);
  void check_set_output(StringRef identifier, const CPPType &value_type);
};

}  // namespace blender::nodes
