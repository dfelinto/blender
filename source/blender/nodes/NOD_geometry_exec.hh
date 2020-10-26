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

#include "FN_generic_pointer.hh"

#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"

#include "BKE_geometry.hh"

namespace blender::nodes {

using bke::Geometry;
using bke::GeometryPtr;
using fn::CPPType;
using fn::GMutablePointer;

/**
 * This container maps names to values of arbitrary type. It is used to represent the input and
 * output values of geometry nodes.
 *
 * The string keys are only referenced, but the values are owned.
 */
class GValueByName {
 private:
  /* Used to allocate values owned by this container. */
  LinearAllocator<> &allocator_;
  Map<StringRef, GMutablePointer> values_;

 public:
  GValueByName(LinearAllocator<> &allocator) : allocator_(allocator)
  {
  }

  ~GValueByName()
  {
    /* Destruct all values that have not been destructed. */
    for (GMutablePointer value : values_.values()) {
      value.type()->destruct(value.get());
    }
  }

  /* Add a value to the container. The container is responsible for destructing the value that is
   * passed in. */
  void transfer_ownership_in(StringRef name, GMutablePointer value)
  {
    values_.add_new(name, value);
  }

  /* Add a value to the container. The caller remains responsible for destructing the value. */
  void move_in(StringRef name, GMutablePointer value)
  {
    const CPPType &type = *value.type();
    void *buffer = allocator_.allocate(type.size(), type.alignment());
    type.move_to_uninitialized(value.get(), buffer);
    values_.add_new(name, GMutablePointer{type, buffer});
  }

  /* Add a value to the container. */
  template<typename T> void move_in(StringRef name, T &&value)
  {
    this->move_in(name, GMutablePointer{&value});
  }

  /* Remove the value for the given name from the container and remove it. The caller is
   * responsible for freeing it. */
  GMutablePointer extract(StringRef name)
  {
    return values_.pop(name);
  }

  /* Remove the value for the given name from the container and remove it. */
  template<typename T> T extract(StringRef name)
  {
    GMutablePointer value = values_.pop(name);
    const CPPType &type = *value.type();
    BLI_assert(type.is<T>());
    T return_value;
    type.relocate_to_initialized(value.get(), &return_value);
    return return_value;
  }
};

}  // namespace blender::nodes
