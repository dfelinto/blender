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

#include "BLI_float3.hh"
#include "BLI_hash.hh"
#include "BLI_map.hh"
#include "BLI_user_counter.hh"

#include "BKE_geometry_set.h"

struct Mesh;
struct PointCloud;
struct Object;

/* Each geometry component has a specific type. The type determines what kind of data the component
 * stores. Functions modifying a geometry will usually just modify a subset of the component types.
 */
enum class GeometryComponentType {
  Mesh = 0,
  PointCloud = 1,
  Instances = 2,
};

enum class GeometryOwnershipType {
  /* The geometry is owned. This implies that it can be changed. */
  Owned = 0,
  /* The geometry can be changed, but someone else is responsible for freeing it. */
  Editable = 1,
  /* The geometry cannot be changed and someone else is responsible for freeing it. */
  ReadOnly = 2,
};

/* Make it possible to use the component type as key in hash tables. */
namespace blender {
template<> struct DefaultHash<GeometryComponentType> {
  uint64_t operator()(const GeometryComponentType &value) const
  {
    return (uint64_t)value;
  }
};
}  // namespace blender

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
 * A geometry set contains zero or more geometry components. There is at most one component of each
 * type. Individual components might be shared between multiple geometries. Shared components are
 * copied automatically when write access is requested.
 *
 * Copying a geometry set is a relatively cheap operation, because it does not copy the referenced
 * geometry components.
 */
class GeometrySet {
 private:
  using GeometryComponentPtr = blender::UserCounter<class GeometryComponent>;
  blender::Map<GeometryComponentType, GeometryComponentPtr> components_;

 public:
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

  friend std::ostream &operator<<(std::ostream &stream, const GeometrySet &geometry_set);
  friend bool operator==(const GeometrySet &a, const GeometrySet &b);
  uint64_t hash() const;

  /* Utility methods for creation. */
  static GeometrySet create_with_mesh(
      Mesh *mesh, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  static GeometrySet create_with_pointcloud(
      PointCloud *pointcloud, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);

  /* Utility methods for access. */
  bool has_mesh() const;
  bool has_pointcloud() const;
  bool has_instances() const;
  const Mesh *get_mesh_for_read() const;
  const PointCloud *get_pointcloud_for_read() const;
  Mesh *get_mesh_for_write();
  PointCloud *get_pointcloud_for_write();

  /* Utility methods for replacement. */
  void replace_mesh(Mesh *mesh, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  void replace_pointcloud(PointCloud *pointcloud,
                          GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
};

/** A geometry component that can store a mesh. */
class MeshComponent : public GeometryComponent {
 private:
  Mesh *mesh_ = nullptr;
  GeometryOwnershipType ownership_ = GeometryOwnershipType::Owned;
  /* Due to historical design choices, vertex group data is stored in the mesh, but the vertex
   * group names are stored on an object. Since we don't have an object here, we copy over the
   * names into this map. */
  blender::Map<std::string, int> vertex_group_names_;

 public:
  ~MeshComponent();
  GeometryComponent *copy() const override;

  void clear();
  bool has_mesh() const;
  void replace(Mesh *mesh, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  Mesh *release();

  void copy_vertex_group_names_from_object(const struct Object &object);
  int vertex_group_index(blender::StringRef vertex_group_name) const;

  const Mesh *get_for_read() const;
  Mesh *get_for_write();

  static constexpr inline GeometryComponentType type = GeometryComponentType::Mesh;
};

/** A geometry component that stores a point cloud. */
class PointCloudComponent : public GeometryComponent {
 private:
  PointCloud *pointcloud_ = nullptr;
  GeometryOwnershipType ownership_ = GeometryOwnershipType::Owned;

 public:
  ~PointCloudComponent();
  GeometryComponent *copy() const override;

  void clear();
  bool has_pointcloud() const;
  void replace(PointCloud *pointcloud,
               GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  PointCloud *release();

  const PointCloud *get_for_read() const;
  PointCloud *get_for_write();

  static constexpr inline GeometryComponentType type = GeometryComponentType::PointCloud;
};

/** A geometry component that stores instances. */
class InstancesComponent : public GeometryComponent {
 private:
  blender::Vector<blender::float3> positions_;
  blender::Vector<const Object *> objects_;

 public:
  ~InstancesComponent() = default;
  GeometryComponent *copy() const override;

  void replace(blender::Vector<blender::float3> positions,
               blender::Vector<const Object *> objects);
  void replace(blender::Vector<blender::float3> positions, const Object *object);

  blender::Span<const Object *> objects() const;
  blender::Span<blender::float3> positions() const;
  blender::MutableSpan<blender::float3> positions();
  int instances_amount() const;

  static constexpr inline GeometryComponentType type = GeometryComponentType::Instances;
};
