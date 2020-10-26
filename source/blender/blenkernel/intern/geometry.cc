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

#include "MEM_guardedalloc.h"

namespace blender::bke {

/* Make a copy of the geometry. */
Geometry::Geometry(const Geometry &other)
{
  if (other.mesh_ != nullptr) {
    mesh_ = BKE_mesh_copy_for_eval(other.mesh_, false);
    /* Own the new mesh, regardless of whether the original mesh was owned. */
    mesh_owned_ = true;
  }
}

Geometry::~Geometry()
{
  this->mesh_reset();
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

/**
 * Returns true when this geometry has a mesh component.
 */
bool Geometry::mesh_available() const
{
  return mesh_ != nullptr;
}

/**
 * Replace the mesh in the geometry. The caller remains the owner of the given mesh and is
 * responsible for freeing it eventually.
 */
void Geometry::mesh_set_and_keep_ownership(Mesh *mesh)
{
  BLI_assert(this->is_mutable());
  this->mesh_reset();
  mesh_ = mesh;
  mesh_owned_ = false;
}

/**
 * Replace the mesh in the geometry. The geometry becomes responsible for freeing the mesh.
 */
void Geometry::mesh_set_and_transfer_ownership(Mesh *mesh)
{
  BLI_assert(this->is_mutable());
  this->mesh_reset();
  mesh_ = mesh;
  mesh_owned_ = true;
}

/**
 * Clear any mesh data the geometry might have.
 */
void Geometry::mesh_reset()
{
  BLI_assert(this->is_mutable());
  if (mesh_ != nullptr) {
    if (mesh_owned_) {
      BKE_id_free(nullptr, mesh_);
    }
    mesh_ = nullptr;
  }
}

/**
 * Get the mesh from the geometry. This mesh should only be read and not modified. This can be used
 * on shared geometries.
 * Might return null.
 */
Mesh *Geometry::mesh_get_for_read()
{
  return mesh_;
}

/**
 * Get the mesh from the geometry. The caller is allowed to modify the mesh. This method can only
 * be used on mutable geometries.
 * Might return null.
 */
Mesh *Geometry::mesh_get_for_write()
{
  BLI_assert(this->is_mutable());
  return mesh_;
}

/**
 * Return the mesh in the geometry and remove it. This only works on mutable geometries.
 * Might return null;
 */
Mesh *Geometry::mesh_release()
{
  BLI_assert(this->is_mutable());
  Mesh *mesh = mesh_;
  mesh_ = nullptr;
  return mesh;
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

}  // namespace blender::bke
