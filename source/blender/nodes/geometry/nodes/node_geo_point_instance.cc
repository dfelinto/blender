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

#include "BKE_mesh.h"
#include "DNA_pointcloud_types.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_point_instance_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_point_instance_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

namespace blender::nodes {
static void geo_point_instance_exec(bNode *UNUSED(node),
                                    GeoNodeInputs inputs,
                                    GeoNodeOutputs outputs)
{
  GeometryPtr geometry = inputs.extract<GeometryPtr>("Geometry");

  if (!geometry.has_value() || !geometry->has_pointcloud()) {
    outputs.set("Geometry", std::move(geometry));
    return;
  }

  /* For now make a mesh from the pointcloud instead of instancing another object / geometry. */
  const PointCloud *pointcloud = geometry->get_pointcloud_for_read();

  if (pointcloud == NULL) {
    outputs.set("Geometry", std::move(geometry));
    return;
  }

  /* TODO: Carry over attributes from poincloud verts to the instances. */
  Mesh *mesh_out = BKE_mesh_new_nomain(pointcloud->totpoint, 0, 0, 0, 0);
  BKE_mesh_from_pointcloud(pointcloud, mesh_out);

  /* For now, replace any existing mesh in the geometry. */
  make_geometry_mutable(geometry);
  geometry->replace_mesh(mesh_out);

  outputs.set("Geometry", std::move(geometry));
}
}  // namespace blender::nodes

void register_node_type_geo_point_instance()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_POINT_INSTANCE, "Point Instance", 0, 0);
  node_type_socket_templates(&ntype, geo_node_point_instance_in, geo_node_point_instance_out);
  ntype.geometry_node_execute = blender::nodes::geo_point_instance_exec;
  nodeRegisterType(&ntype);
}
