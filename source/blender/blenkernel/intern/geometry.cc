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

#include "BKE_geometry.hh"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_pointcloud.h"

#include "MEM_guardedalloc.h"

namespace blender::bke {

/* -------------------------------------------------------------------- */
/** \name Geometry Component
 * \{ */

GeometryComponent ::~GeometryComponent()
{
}

GeometryComponent *GeometryComponent::create(GeometryComponentType component_type)
{
  switch (component_type) {
    case GeometryComponentType::Mesh:
      return new MeshComponent();
    case GeometryComponentType::PointCloud:
      return new PointCloudComponent();
  }
  BLI_assert(false);
  return nullptr;
}

void GeometryComponent::user_add()
{
  users_.fetch_add(1);
}

void GeometryComponent::user_remove()
{
  const int new_users = users_.fetch_sub(1) - 1;
  if (new_users == 0) {
    delete this;
  }
}

bool GeometryComponent::is_mutable() const
{
  /* If the item is shared, it is read-only. */
  /* The user count can be 0, when this is called from the destructor. */
  return users_ <= 1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Geometry
 * \{ */

/* Makes a copy of the geometry. The individual components are shared with the original geometry.
 * Therefore, this is a relatively cheap operation.
 */
Geometry::Geometry(const Geometry &other) : components_(other.components_)
{
}

void Geometry::user_add()
{
  users_.fetch_add(1);
}

void Geometry::user_remove()
{
  const int new_users = users_.fetch_sub(1) - 1;
  if (new_users == 0) {
    delete this;
  }
}

bool Geometry::is_mutable() const
{
  /* If the geometry is shared, it is read-only. */
  /* The user count can be 0, when this is called from the destructor. */
  return users_ <= 1;
}

/* This method can only be used when the geometry is mutable. It returns a mutable geometry
 * component of the given type.
 */
GeometryComponent &Geometry::get_component_for_write(GeometryComponentType component_type)
{
  BLI_assert(this->is_mutable());
  return components_.add_or_modify(
      component_type,
      [&](GeometryComponentPtr *value_ptr) -> GeometryComponent & {
        /* If the component did not exist before, create a new one. */
        new (value_ptr) GeometryComponentPtr(GeometryComponent::create(component_type));
        return **value_ptr;
      },
      [&](GeometryComponentPtr *value_ptr) -> GeometryComponent & {
        GeometryComponentPtr &value = *value_ptr;
        if (value->is_mutable()) {
          /* If the referenced component is already mutable, return it directly. */
          return *value;
        }
        /* If the referenced component is shared, make a copy. The copy is not shared and is
         * therefore mutable. */
        GeometryComponent *copied_component = value->copy();
        value = GeometryComponentPtr{copied_component};
        return *copied_component;
      });
}

/* Get the component of the given type. Might return null if the component does not exist yet. */
const GeometryComponent *Geometry::get_component_for_read(
    GeometryComponentType component_type) const
{
  const GeometryComponentPtr *component = components_.lookup_ptr(component_type);
  if (component != nullptr) {
    return component->get();
  }
  return nullptr;
}

/* Returns a read-only mesh or null. */
const Mesh *Geometry::get_mesh_for_read() const
{
  const MeshComponent *component = this->get_component_for_read<MeshComponent>();
  return (component == nullptr) ? nullptr : component->get_for_read();
}

/* Returns true when the geometry has a mesh component that has a mesh. */
bool Geometry::has_mesh() const
{
  const MeshComponent *component = this->get_component_for_read<MeshComponent>();
  return component != nullptr && component->has_mesh();
}

/* Returns a read-only point cloud of null. */
const PointCloud *Geometry::get_pointcloud_for_read() const
{
  const PointCloudComponent *component = this->get_component_for_read<PointCloudComponent>();
  return (component == nullptr) ? nullptr : component->get_for_read();
}

/* Returns true when the geometry has a point cloud component that has a point cloud. */
bool Geometry::has_pointcloud() const
{
  const PointCloudComponent *component = this->get_component_for_read<PointCloudComponent>();
  return component != nullptr && component->has_pointcloud();
}

/* Create a new geometry that only contains the given mesh. Ownership of the mesh is transferred to
 * the new geometry. */
GeometryPtr Geometry::create_with_mesh(Mesh *mesh, bool transfer_ownership)
{
  Geometry *geometry = new Geometry();
  MeshComponent &component = geometry->get_component_for_write<MeshComponent>();
  component.replace(mesh, transfer_ownership);
  return geometry;
}

/* Clear the existing mesh and replace it with the given one. */
void Geometry::replace_mesh(Mesh *mesh, bool transfer_ownership)
{
  MeshComponent &component = this->get_component_for_write<MeshComponent>();
  component.replace(mesh, transfer_ownership);
}

/* Clear the existing point cloud and replace with the given one. */
void Geometry::replace_pointcloud(PointCloud *pointcloud, bool transfer_ownership)
{
  PointCloudComponent &pointcloud_component = this->get_component_for_write<PointCloudComponent>();
  pointcloud_component.replace(pointcloud, transfer_ownership);
}

/* Returns a mutable mesh or null. No ownership is transferred. */
Mesh *Geometry::get_mesh_for_write()
{
  MeshComponent &component = this->get_component_for_write<MeshComponent>();
  return component.get_for_write();
}

/* Returns a mutable point cloud or null. No ownership is transferred. */
PointCloud *Geometry::get_pointcloud_for_write()
{
  PointCloudComponent &component = this->get_component_for_write<PointCloudComponent>();
  return component.get_for_write();
}

/**
 * Changes the given pointer so that it points to a mutable geometry. This might do nothing, create
 * a new empty geometry or copy the entire geometry.
 */
void make_geometry_mutable(GeometryPtr &geometry)
{
  if (!geometry) {
    /* If the pointer is null, create a new instance. */
    geometry = GeometryPtr{new Geometry()};
  }
  else if (geometry->is_mutable()) {
    /* If the instance is mutable already, do nothing. */
  }
  else {
    /* This geometry is shared, make a copy that is independent of the other users. */
    Geometry *shared_geometry = geometry.release();
    Geometry *new_geometry = new Geometry(*shared_geometry);
    shared_geometry->user_remove();
    geometry = GeometryPtr{new_geometry};
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Component
 * \{ */

MeshComponent::~MeshComponent()
{
  this->clear();
}

GeometryComponent *MeshComponent::copy() const
{
  MeshComponent *new_component = new MeshComponent();
  if (mesh_ != nullptr) {
    new_component->mesh_ = BKE_mesh_copy_for_eval(mesh_, false);
    new_component->owned_ = true;
  }
  return new_component;
}

void MeshComponent::clear()
{
  BLI_assert(this->is_mutable());
  if (mesh_ != nullptr) {
    if (owned_) {
      BKE_id_free(nullptr, mesh_);
    }
    mesh_ = nullptr;
  }
}

bool MeshComponent::has_mesh() const
{
  return mesh_ != nullptr;
}

/* Clear the component and replace it with the new mesh. */
void MeshComponent::replace(Mesh *mesh, bool transfer_ownership)
{
  BLI_assert(this->is_mutable());
  this->clear();
  mesh_ = mesh;
  owned_ = transfer_ownership;
}

/* Return the mesh and clear the component. The caller takes over responsibility for freeing the
 * mesh (if the component was responsible before). */
Mesh *MeshComponent::release()
{
  BLI_assert(this->is_mutable());
  Mesh *mesh = mesh_;
  mesh_ = nullptr;
  return mesh;
}

/* Get the mesh from this component. This method can be used by multiple threads at the same
 * time. Therefore, the returned mesh should not be modified. No ownership is transferred. */
const Mesh *MeshComponent::get_for_read() const
{
  return mesh_;
}

/* Get the mesh from this component. This method can only be used when the component is mutable,
 * i.e. it is not shared. The returned mesh can be modified. No ownership is transferred. */
Mesh *MeshComponent::get_for_write()
{
  BLI_assert(this->is_mutable());
  return mesh_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pointcloud Component
 * \{ */

PointCloudComponent::~PointCloudComponent()
{
  this->clear();
}

GeometryComponent *PointCloudComponent::copy() const
{
  PointCloudComponent *new_component = new PointCloudComponent();
  if (pointcloud_ != nullptr) {
    new_component->pointcloud_ = BKE_pointcloud_copy_for_eval(pointcloud_, false);
    new_component->owned_ = true;
  }
  return new_component;
}

void PointCloudComponent::clear()
{
  BLI_assert(this->is_mutable());
  if (pointcloud_ != nullptr) {
    if (owned_) {
      BKE_id_free(nullptr, pointcloud_);
    }
    pointcloud_ = nullptr;
  }
}

bool PointCloudComponent::has_pointcloud() const
{
  return pointcloud_ != nullptr;
}

/* Clear the component and replace it with the new point cloud. */
void PointCloudComponent::replace(PointCloud *pointcloud, bool transfer_ownership)
{
  BLI_assert(this->is_mutable());
  this->clear();
  pointcloud_ = pointcloud;
  owned_ = transfer_ownership;
}

/* Return the point cloud and clear the component. The caller takes over responsibility for freeing
 * the point cloud (if the component was responsible before). */
PointCloud *PointCloudComponent::release()
{
  BLI_assert(this->is_mutable());
  PointCloud *pointcloud = pointcloud_;
  pointcloud_ = nullptr;
  return pointcloud;
}

/* Get the point cloud from this component. This method can be used by multiple threads at the same
 * time. Therefore, the returned point cloud should not be modified. No ownership is transferred.
 */
const PointCloud *PointCloudComponent::get_for_read() const
{
  return pointcloud_;
}

/* Get the point cloud from this component. This method can only be used when the component is
 * mutable, i.e. it is not shared. The returned point cloud can be modified. No ownership is
 * transferred. */
PointCloud *PointCloudComponent::get_for_write()
{
  BLI_assert(this->is_mutable());
  return pointcloud_;
}

/** \} */

}  // namespace blender::bke
