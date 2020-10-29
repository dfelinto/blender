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

#include "BLI_math_matrix.h"

#include "DNA_pointcloud_types.h"

#include "BKE_mesh.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_transform_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_VECTOR, N_("Translation"), 0.0f, 0.0f, 0.0f, 1.0f, -FLT_MAX, FLT_MAX, PROP_TRANSLATION},
    {SOCK_VECTOR, N_("Rotation"), 0.0f, 0.0f, 0.0f, 1.0f, -FLT_MAX, FLT_MAX, PROP_EULER},
    {SOCK_VECTOR, N_("Scale"), 1.0f, 1.0f, 1.0f, 1.0f, -FLT_MAX, FLT_MAX, PROP_XYZ},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_transform_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

using blender::float3;

static bool use_translate(const float3 rotation, const float3 scale)
{
  if (compare_ff(rotation.length_squared(), 0.0f, 1e-9f) != 1) {
    return false;
  }
  if (compare_ff(scale.x, 1.0f, 1e-9f) != 1 || compare_ff(scale.y, 1.0f, 1e-9f) != 1 ||
      compare_ff(scale.z, 1.0f, 1e-9f) != 1) {
    return false;
  }
  return true;
}

static void transform_mesh(Mesh *mesh,
                           const float3 translation,
                           const float3 rotation,
                           const float3 scale)
{
  /* Use only translation if rotation and scale are zero. */
  if (use_translate(rotation, scale)) {
    BKE_mesh_translate(mesh, translation, true);
  }
  else {
    float mat[4][4];
    loc_eul_size_to_mat4(mat, translation, rotation, scale);
    BKE_mesh_transform(mesh, mat, true);
  }
}

static void transform_pointcloud(PointCloud *pointcloud,
                                 const float3 translation,
                                 const float3 rotation,
                                 const float3 scale)
{
  /* Use only translation if rotation and scale don't apply. */
  if (use_translate(rotation, scale)) {
    for (int i = 0; i < pointcloud->totpoint; i++) {
      add_v3_v3(pointcloud->co[i], translation);
    }
  }
  else {
    float mat[4][4];
    loc_eul_size_to_mat4(mat, translation, rotation, scale);
    for (int i = 0; i < pointcloud->totpoint; i++) {
      mul_m4_v3(mat, pointcloud->co[i]);
    }
  }
}

namespace blender::nodes {
static void geo_transform_exec(bNode *UNUSED(node), GeoNodeInputs inputs, GeoNodeOutputs outputs)
{
  GeometryPtr geometry = inputs.extract<GeometryPtr>("Geometry");

  if (!geometry.has_value()) {
    outputs.set("Geometry", std::move(geometry));
    return;
  }

  const float3 translation = inputs.extract<float3>("Translation");
  const float3 rotation = inputs.extract<float3>("Rotation");
  const float3 scale = inputs.extract<float3>("Scale");

  make_geometry_mutable(geometry);

  if (geometry->has_mesh()) {
    Mesh *mesh = geometry->get_mesh_for_write();
    transform_mesh(mesh, translation, rotation, scale);
  }

  if (geometry->has_pointcloud()) {
    PointCloud *pointcloud = geometry->get_pointcloud_for_write();
    transform_pointcloud(pointcloud, translation, rotation, scale);
  }

  outputs.set("Geometry", std::move(geometry));
}
}  // namespace blender::nodes

void register_node_type_geo_transform()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_TRANSFORM, "Transform", 0, 0);
  node_type_socket_templates(&ntype, geo_node_transform_in, geo_node_transform_out);
  ntype.geometry_node_execute = blender::nodes::geo_transform_exec;
  nodeRegisterType(&ntype);
}
