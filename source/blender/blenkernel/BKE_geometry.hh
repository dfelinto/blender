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

/** \file
 * \ingroup bke
 */

#include <atomic>
#include <iostream>

#include "BLI_hash.hh"
#include "BLI_map.hh"
#include "BLI_user_counter.hh"

struct Mesh;
struct PointCloud;

namespace blender::bke {

/* An automatically reference counted geometry. */
using GeometryPtr = UserCounter<class Geometry>;

/* Each geometry component has a specific type. The type determines what kind of data the component
 * stores. Functions modifying a geometry will usually just modify a subset of the component types.
 */
enum class GeometryComponentType {
  Mesh,
  PointCloud,
};

}  // namespace blender::bke

/* Make it possible to use the component type as key in hash tables. */
namespace blender {
template<> struct DefaultHash<bke::GeometryComponentType> {
  uint64_t operator()(const bke::GeometryComponentType &value) const
  {
    return (uint64_t)value;
  }
};
}  // namespace blender

namespace blender::bke {

/**
 * This is the base class for specialized geometry component types.
 */
class GeometryComponent {
 private:
  /* The reference count has two purposes. When it becomes zero, the component is freed. When it is
   * larger than one, the component becomes immutable. */
  std::atomic<int> users_ = 1;

 public:
  virtual ~GeometryComponent();
  static GeometryComponent *create(GeometryComponentType component_type);

  /* The returned component should be of the same type as the type this is called on. */
  virtual GeometryComponent *copy() const = 0;

  void user_add();
  void user_remove();
  bool is_mutable() const;
};

template<typename T>
inline constexpr bool is_geometry_component_v = std::is_base_of_v<GeometryComponent, T>;

/**
 * A geometry contains zero or more geometry components. There is at most one component of each
 * type. Individual components might be shared between multiple geometries.
 *
 * Geometries are reference counted. This allows them to be shared without making unnecessary
 * copies. A geometry that is shared is immutable. If some code wants to change it,
 * #make_geometry_mutable should be called first.
 */
class Geometry {
 private:
  /* Number of users of this geometry. If this number goes to zero, the geometry is freed. If it
   * is above 1, the geometry is immutable. */
  std::atomic<int> users_ = 1;

  using GeometryComponentPtr = UserCounter<class GeometryComponent>;
  Map<GeometryComponentType, GeometryComponentPtr> components_;

 public:
  Geometry() = default;
  Geometry(const Geometry &other);
  Geometry(Geometry &&other) = delete;
  ~Geometry() = default;

  /* Disable copy and move assignment operators. */
  Geometry &operator=(const Geometry &other) = delete;
  Geometry &operator=(Geometry &&other) = delete;

  void user_add();
  void user_remove();
  bool is_mutable() const;

  GeometryComponent &get_component_for_write(GeometryComponentType component_type);
  template<typename Component> Component &get_component_for_write()
  {
    BLI_STATIC_ASSERT(is_geometry_component_v<Component>, "");
    return static_cast<Component &>(this->get_component_for_write(Component::type));
  }

  const GeometryComponent *get_component_for_read(GeometryComponentType component_type) const;
  template<typename Component> const Component *get_component_for_read() const
  {
    BLI_STATIC_ASSERT(is_geometry_component_v<Component>, "");
    return static_cast<const Component *>(get_component_for_read(Component::type));
  }

  /* Utility methods for creation. */
  static GeometryPtr create_with_mesh(Mesh *mesh, bool transfer_ownership = true);

  /* Utility methods for access. */
  bool has_mesh() const;
  bool has_pointcloud() const;
  const Mesh *get_mesh_for_read() const;
  const PointCloud *get_pointcloud_for_read() const;
  Mesh *get_mesh_for_write();
  PointCloud *get_pointcloud_for_write();

  /* Utility methods for replacement. */
  void replace_mesh(Mesh *mesh, bool transfer_ownership = true);
  void replace_pointcloud(PointCloud *pointcloud, bool transfer_ownership = true);
};

void make_geometry_mutable(GeometryPtr &geometry);

/** A geometry component that can store a mesh. */
class MeshComponent : public GeometryComponent {
 private:
  Mesh *mesh_ = nullptr;
  bool owned_ = false;

 public:
  ~MeshComponent();
  GeometryComponent *copy() const override;

  void clear();
  bool has_mesh() const;
  void replace(Mesh *mesh, bool transfer_ownership = true);
  Mesh *release();

  const Mesh *get_for_read() const;
  Mesh *get_for_write();

  static constexpr inline GeometryComponentType type = GeometryComponentType::Mesh;
};

/** A geometry component that stores a point cloud. */
class PointCloudComponent : public GeometryComponent {
 private:
  PointCloud *pointcloud_ = nullptr;
  bool owned_ = false;

 public:
  ~PointCloudComponent();
  GeometryComponent *copy() const override;

  void clear();
  bool has_pointcloud() const;
  void replace(PointCloud *pointcloud, bool transfer_ownership = true);
  PointCloud *release();

  const PointCloud *get_for_read() const;
  PointCloud *get_for_write();

  static constexpr inline GeometryComponentType type = GeometryComponentType::PointCloud;
};

}  // namespace blender::bke
