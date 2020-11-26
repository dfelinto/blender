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

#include "BLI_float3.hh"
#include "BLI_hash.h"
#include "BLI_math_vector.h"
#include "BLI_rand.hh"
#include "BLI_span.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_pointcloud.h"

#include "cySampleElim.hh"
#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_point_distribute_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_FLOAT, N_("Minimum Distance"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 100000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Maximum Density"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 100000.0f, PROP_NONE},
    {SOCK_STRING, N_("Density Attribute")},
    {SOCK_INT, N_("Seed"), 0, 0, 0, 0, -10000, 10000},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_point_distribute_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void node_point_distribute_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sock_min_dist = (bNodeSocket *)BLI_findlink(&node->inputs, 1);

  nodeSetSocketAvailability(sock_min_dist, ELEM(node->custom1, GEO_NODE_POINT_DISTRIBUTE_POISSON));
}

namespace blender::nodes {

static Vector<float3> random_scatter_points_from_mesh(const Mesh *mesh,
                                                      const float density,
                                                      const FloatReadAttribute &density_factors,
                                                      const int seed)
{
  /* This only updates a cache and can be considered to be logically const. */
  const MLoopTri *looptris = BKE_mesh_runtime_looptri_ensure(const_cast<Mesh *>(mesh));
  const int looptris_len = BKE_mesh_runtime_looptri_len(mesh);

  Vector<float3> points;

  for (const int looptri_index : IndexRange(looptris_len)) {
    const MLoopTri &looptri = looptris[looptri_index];
    const int v0_index = mesh->mloop[looptri.tri[0]].v;
    const int v1_index = mesh->mloop[looptri.tri[1]].v;
    const int v2_index = mesh->mloop[looptri.tri[2]].v;
    const float3 v0_pos = mesh->mvert[v0_index].co;
    const float3 v1_pos = mesh->mvert[v1_index].co;
    const float3 v2_pos = mesh->mvert[v2_index].co;
    const float v0_density_factor = std::max(0.0f, density_factors[v0_index]);
    const float v1_density_factor = std::max(0.0f, density_factors[v1_index]);
    const float v2_density_factor = std::max(0.0f, density_factors[v2_index]);
    const float looptri_density_factor = (v0_density_factor + v1_density_factor +
                                          v2_density_factor) /
                                         3.0f;
    const float area = area_tri_v3(v0_pos, v1_pos, v2_pos);

    const int looptri_seed = BLI_hash_int(looptri_index + seed);
    RandomNumberGenerator looptri_rng(looptri_seed);

    const float points_amount_fl = area * density * looptri_density_factor;
    const float add_point_probability = fractf(points_amount_fl);
    const bool add_point = add_point_probability > looptri_rng.get_float();
    const int point_amount = (int)points_amount_fl + (int)add_point;

    for (int i = 0; i < point_amount; i++) {
      const float3 bary_coords = looptri_rng.get_barycentric_coordinates();
      float3 point_pos;
      interp_v3_v3v3v3(point_pos, v0_pos, v1_pos, v2_pos, bary_coords);
      points.append(point_pos);
    }
  }

  return points;
}

static Vector<float3> poisson_scatter_points_from_mesh(const Mesh *mesh,
                                                       const float density,
                                                       const float min_dist,
                                                       const FloatReadAttribute &density_factors,
                                                       const int seed)
{
  /* This only updates a cache and can be considered to be logically const. */
  const MLoopTri *looptris = BKE_mesh_runtime_looptri_ensure(const_cast<Mesh *>(mesh));
  const int looptris_len = BKE_mesh_runtime_looptri_len(mesh);

  Vector<float3> points;

  if (min_dist <= FLT_EPSILON || density <= FLT_EPSILON) {
    return points;
  }

  // Scatter points randomly on the mesh with higher density (5-7) times higher than desired for
  // good quality possion disk distributions.

  int quality = 5;
  Vector<bool> remove_point;

  for (const int looptri_index : IndexRange(looptris_len)) {
    const MLoopTri &looptri = looptris[looptri_index];
    const int v0_index = mesh->mloop[looptri.tri[0]].v;
    const int v1_index = mesh->mloop[looptri.tri[1]].v;
    const int v2_index = mesh->mloop[looptri.tri[2]].v;
    const float3 v0_pos = mesh->mvert[v0_index].co;
    const float3 v1_pos = mesh->mvert[v1_index].co;
    const float3 v2_pos = mesh->mvert[v2_index].co;
    const float area = area_tri_v3(v0_pos, v1_pos, v2_pos);
    const float v0_density_factor = std::max(0.0f, density_factors[v0_index]);
    const float v1_density_factor = std::max(0.0f, density_factors[v1_index]);
    const float v2_density_factor = std::max(0.0f, density_factors[v2_index]);

    const int looptri_seed = BLI_hash_int(looptri_index + seed);
    RandomNumberGenerator looptri_rng(looptri_seed);

    const float points_amount_fl = area / (2.0f * sqrtf(3.0f) * min_dist * min_dist);
    const int point_amount = quality * (int)ceilf(points_amount_fl);

    for (int i = 0; i < point_amount; i++) {
      const float3 bary_coords = looptri_rng.get_barycentric_coordinates();
      float3 point_pos;
      interp_v3_v3v3v3(point_pos, v0_pos, v1_pos, v2_pos, bary_coords);
      points.append(point_pos);

      const float weight = bary_coords[0] * v0_density_factor +
                           bary_coords[1] * v1_density_factor + bary_coords[2] * v2_density_factor;
      remove_point.append(looptri_rng.get_float() <= density * weight);
    }
  }

  // Eliminate the scattered points until we get a possion distribution.

  cy::WeightedSampleElimination<float3, float, 3, size_t> wse;
  Vector<float3> output_points(points.size());
  bool is_progressive = false;

  float d_max = 2 * min_dist;

  float d_min = d_max * wse.GetWeightLimitFraction(points.size(), points.size() / quality);
  float alpha = wse.GetParamAlpha();
  std::vector<size_t> output_idx = wse.Eliminate_all(
      points.data(),
      points.size(),
      output_points.data(),
      output_points.size(),
      is_progressive,
      d_max,
      2,
      [d_min, alpha](float3 const & /*unused*/, float3 const & /*unused*/, float d2, float d_max) {
        float d = sqrtf(d2);
        if (d < d_min) {
          d = d_min;
        }
        return std::pow(1.0f - d / d_max, alpha);
      });

  output_points.resize(output_idx.size());
  points.clear();
  points.reserve(output_points.size());

  for (int i = 0; i < output_points.size(); i++) {
    if (!remove_point[output_idx[i]]) {
      continue;
    }
    points.append(output_points[i]);
  }

  return points;
}

static void geo_node_point_distribute_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  GeometrySet geometry_set_out;

  GeometryNodePointDistributeMethod distribute_method =
      static_cast<GeometryNodePointDistributeMethod>(params.node().custom1);

  if (!geometry_set.has_mesh()) {
    params.set_output("Geometry", std::move(geometry_set_out));
    return;
  }

  const float density = params.extract_input<float>("Maximum Density");
  const std::string density_attribute = params.extract_input<std::string>("Density Attribute");

  if (density <= 0.0f) {
    params.set_output("Geometry", std::move(geometry_set_out));
    return;
  }

  const MeshComponent &mesh_component = *geometry_set.get_component_for_read<MeshComponent>();
  const Mesh *mesh_in = mesh_component.get_for_read();

  const FloatReadAttribute density_factors = mesh_component.attribute_get_for_read<float>(
      density_attribute, ATTR_DOMAIN_POINT, 1.0f);
  const int seed = params.get_input<int>("Seed");

  Vector<float3> points;

  if (distribute_method == GEO_NODE_POINT_DISTRIBUTE_RANDOM) {
    points = random_scatter_points_from_mesh(mesh_in, density, density_factors, seed);
  }
  else if (distribute_method == GEO_NODE_POINT_DISTRIBUTE_POISSON) {
    const float min_dist = params.extract_input<float>("Minimum Distance");
    points = poisson_scatter_points_from_mesh(mesh_in, density, min_dist, density_factors, seed);
  }
  else {
    // Unhandled point distribution.
    BLI_assert(false);
  }

  PointCloud *pointcloud = BKE_pointcloud_new_nomain(points.size());
  memcpy(pointcloud->co, points.data(), sizeof(float3) * points.size());
  for (const int i : points.index_range()) {
    *(float3 *)(pointcloud->co + i) = points[i];
    pointcloud->radius[i] = 0.05f;
  }

  geometry_set_out.replace_pointcloud(pointcloud);
  params.set_output("Geometry", std::move(geometry_set_out));
}
}  // namespace blender::nodes

void register_node_type_geo_point_distribute()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_POINT_DISTRIBUTE, "Point Distribute", 0, 0);
  node_type_socket_templates(&ntype, geo_node_point_distribute_in, geo_node_point_distribute_out);
  node_type_update(&ntype, node_point_distribute_update);
  ntype.geometry_node_execute = blender::nodes::geo_node_point_distribute_exec;
  nodeRegisterType(&ntype);
}
