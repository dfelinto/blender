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

#include "MEM_guardedalloc.h"

#include "BKE_subdiv.h"
#include "BKE_subdiv_mesh.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_subdivision_surface_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_INT, N_("Level"), 1, 0, 0, 0, 0, 6},
    {SOCK_BOOLEAN, N_("Simple")},
    {SOCK_BOOLEAN, N_("Optimal Display")},
    {SOCK_BOOLEAN, N_("Use Creases")},
    {SOCK_BOOLEAN, N_("Boundary Smooth")},
    {SOCK_BOOLEAN, N_("Smooth UVs")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_subdivision_surface_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

namespace blender::nodes {
static void geo_subdivision_surface_exec(bNode *UNUSED(node),
                                         GeoNodeInputs inputs,
                                         GeoNodeOutputs outputs)
{
  GeometryPtr geometry = inputs.extract<GeometryPtr>("Geometry");

  if (!geometry.has_value() || !geometry->has_mesh()) {
    outputs.set("Geometry", geometry);
    return;
  }

#ifndef WITH_OPENSUBDIV
  /* Return input geometry if Blender is build without OpenSubdiv. */
  outputs.set("Geometry", std::move(geometry));
  return;
#else
  const int subdiv_level = clamp_i(inputs.extract<int>("Level"), 0, 30);
  const bool is_simple = inputs.extract<bool>("Simple");
  const bool use_optimal_display = inputs.extract<bool>("Optimal Display");
  const bool use_crease = inputs.extract<bool>("Use Creases");
  const bool boundary_smooth = inputs.extract<bool>("Boundary Smooth");
  const bool smooth_uvs = inputs.extract<bool>("Smooth UVs");

  const Mesh *mesh_in = geometry->get_mesh_for_read();

  /* Only process subdivion if level is greater than 0. */
  if (subdiv_level == 0) {
    outputs.set("Geometry", std::move(geometry));
    return;
  }

  /* Mesh settings init. */
  SubdivToMeshSettings mesh_settings;
  mesh_settings.resolution = (1 << subdiv_level) + 1;
  mesh_settings.use_optimal_display = use_optimal_display;

  /* Subdivision settings init. */
  SubdivSettings subdiv_settings;
  subdiv_settings.is_simple = is_simple;
  subdiv_settings.is_adaptive = false;
  subdiv_settings.use_creases = use_crease;
  subdiv_settings.level = subdiv_level;

  subdiv_settings.vtx_boundary_interpolation = BKE_subdiv_vtx_boundary_interpolation_from_subsurf(
      boundary_smooth);
  subdiv_settings.fvar_linear_interpolation = BKE_subdiv_fvar_interpolation_from_uv_smooth(
      smooth_uvs);

  /* Apply subdiv to mesh. */
  Subdiv *subdiv = BKE_subdiv_update_from_mesh(nullptr, &subdiv_settings, mesh_in);

  /* In case of bad topology, skip to input mesh. */
  if (subdiv == NULL) {
    outputs.set("Geometry", std::move(geometry));
    return;
  }

  Mesh *mesh_out = BKE_subdiv_to_mesh(subdiv, &mesh_settings, mesh_in);

  make_geometry_mutable(geometry);

  geometry->replace_mesh(mesh_out);

  // BKE_subdiv_stats_print(&subdiv->stats);
  BKE_subdiv_free(subdiv);

  outputs.set("Geometry", std::move(geometry));
#endif
}
}  // namespace blender::nodes

void register_node_type_geo_subdivision_surface()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SUBDIVISION_SURFACE, "Subdivision Surface", 0, 0);
  node_type_socket_templates(
      &ntype, geo_node_subdivision_surface_in, geo_node_subdivision_surface_out);
  ntype.geometry_node_execute = blender::nodes::geo_subdivision_surface_exec;
  nodeRegisterType(&ntype);
}
