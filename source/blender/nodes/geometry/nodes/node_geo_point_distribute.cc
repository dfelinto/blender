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

// #include "BLI_float3.hh" /* Can't get this to work. I'm probably doing it completely wrong. */
#include "BLI_math_vector.h"
#include "BLI_rand.h"
#include "BLI_span.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_pointcloud.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_point_distribute_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_FLOAT, N_("Density"), 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 100000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Minimum Radius"), 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f, PROP_NONE},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_point_distribute_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void mesh_loop_tri_corner_coords(
    const Mesh *mesh, const int index, float **r_v0, float **r_v1, float **r_v2)
{
  MVert *mverts = mesh->mvert;
  MLoop *mloops = mesh->mloop;

  MLoopTri looptri = mesh->runtime.looptris.array[index];
  const MLoop *mloop_0 = &mloops[looptri.tri[0]];
  const MLoop *mloop_1 = &mloops[looptri.tri[1]];
  const MLoop *mloop_2 = &mloops[looptri.tri[2]];
  *r_v0 = mverts[mloop_0->v].co;
  *r_v1 = mverts[mloop_1->v].co;
  *r_v2 = mverts[mloop_2->v].co;
}

static PointCloud *scatter_pointcloud_from_mesh(Mesh *mesh,
                                                const float density,
                                                const float UNUSED(minimum_radius))
{

  BKE_mesh_runtime_looptri_ensure(mesh);
  const int looptris_len = mesh->runtime.looptris.len;
  blender::Array<float> weights(looptris_len);

  /* Calculate area for every triangle and the total area for the whole mesh. */
  float area_total = 0.0f;
  for (int i = 0; i < looptris_len; i++) {
    float *v0, *v1, *v2;
    mesh_loop_tri_corner_coords(mesh, i, &v0, &v1, &v2);
    const float tri_area = area_tri_v3(v0, v1, v2);

    weights[i] = tri_area;
    area_total += tri_area;
  }

  /* Fill an array with the sums of each weight. No need to normalize by the area of the largest
   * triangle since it's all relative anyway. Reuse the orginal weights array to avoid allocating
   * another.
   *
   * TODO: Multiply by a vertex group / attribute here as well. */
  float weight_total = 0.0f;
  for (int i = 0; i < looptris_len; i++) {
    weight_total += weights[i];
    weights[i] = weight_total;
  }

  /* Calculate the total number of points and create the pointcloud. */
  const int points_len = round_fl_to_int(area_total * density);
  PointCloud *pointcloud = BKE_pointcloud_new_nomain(points_len);

  /* Distribute the points. */
  RNG *rng = BLI_rng_new(0);
  for (int i = 0; i < points_len; i++) {
    const int random_weight_total = BLI_rng_get_float(rng) * weight_total;

    /* Find the triangle index based on the weights. TODO: Use binary search. */
    int i_tri = 0;
    for (; i_tri < looptris_len; i_tri++) {
      if (weights[i_tri] > random_weight_total) {
        break;
      }
    }
    BLI_assert(i_tri < looptris_len);

    /* Place the point randomly in the selected triangle. */
    float *v0, *v1, *v2;
    mesh_loop_tri_corner_coords(mesh, i_tri, &v0, &v1, &v2);

    float co[3];
    BLI_rng_get_tri_sample_float_v3(rng, v0, v1, v2, co);
    pointcloud->co[i][0] = co[0];
    pointcloud->co[i][1] = co[1];
    pointcloud->co[i][2] = co[2];
    pointcloud->radius[i] = 0.05f; /* TODO: Use radius attribute / vertex vgroup. */
  }

  return pointcloud;
}

namespace blender::nodes {
static void geo_point_distribute_exec(bNode *UNUSED(node),
                                      GValueByName &inputs,
                                      GValueByName &outputs)
{
  GeometryPtr geometry = inputs.extract<GeometryPtr>("Geometry");

  if (!geometry.has_value() || !geometry->mesh_available()) {
    outputs.move_in("Geometry", std::move(geometry));
    return;
  }

  const float density = inputs.extract<float>("Density");
  const float minimum_radius = inputs.extract<float>("Minimum Radius");

  if (density <= 0.0f) {
    outputs.move_in("Geometry", std::move(geometry));
    return;
  }

  /* For now, replace any existing pointcloud in the geometry. */
  bke::make_geometry_mutable(geometry);
  geometry->pointcloud_reset();

  /* Only need "write" to calculate the runtime "looptris" data. */
  Mesh *mesh_in = geometry->mesh_get_for_write();

  PointCloud *pointcloud_out = scatter_pointcloud_from_mesh(mesh_in, density, minimum_radius);

  /* For now don't output the original mesh. In the future we could output this too. */
  geometry->pointcloud_set_and_transfer_ownership(pointcloud_out);

  outputs.move_in("Geometry", std::move(geometry));
}
}  // namespace blender::nodes

void register_node_type_geo_point_distribute()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_POINT_DISTRIBUTE, "Point Distribute", 0, 0);
  node_type_socket_templates(&ntype, geo_node_point_distribute_in, geo_node_point_distribute_out);
  ntype.geometry_node_execute = blender::nodes::geo_point_distribute_exec;
  nodeRegisterType(&ntype);
}
