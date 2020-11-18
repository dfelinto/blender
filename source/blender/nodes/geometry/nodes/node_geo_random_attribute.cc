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

#include "node_geometry_util.hh"

#include "BLI_rand.hh"

#include "DNA_mesh_types.h"

static bNodeSocketTemplate geo_node_random_attribute_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_STRING, N_("Attribute")},
    {SOCK_VECTOR, N_("Min"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_VECTOR, N_("Max"), 1.0f, 1.0f, 1.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_INT, N_("Seed"), 0, 0, 0, 0, -10000, 10000},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_random_attribute_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_attribute_random_init(bNodeTree *UNUSED(tree), bNode *node)
{
  node->custom1 = CD_PROP_FLOAT;
}

namespace blender::nodes {

static void geo_random_attribute_exec(GeoNodeExecParams params)
{
  const bNode &node = params.node();
  const int data_type = node.custom1;
  const AttributeDomain domain = static_cast<AttributeDomain>(node.custom2);

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  const std::string attribute_name = params.extract_input<std::string>("Attribute");
  const float3 min_value = params.extract_input<float3>("Min");
  const float3 max_value = params.extract_input<float3>("Max");
  const int seed = params.extract_input<int>("Seed");

  MeshComponent &mesh_component = geometry_set.get_component_for_write<MeshComponent>();
  Mesh *mesh = mesh_component.get_for_write();
  if (mesh == nullptr) {
    params.set_output("Geometry", geometry_set);
    return;
  }

  WriteAttributePtr attribute = bke::mesh_attribute_get_for_write(mesh_component, attribute_name);

  if (!attribute) {
    BKE_id_attribute_new(&mesh->id, attribute_name.c_str(), data_type, domain, nullptr);
    attribute = bke::mesh_attribute_get_for_write(mesh_component, attribute_name);
  }

  RandomNumberGenerator rng;
  rng.seed_random(seed);

  if (attribute) {
    const int size = attribute->size();
    if (attribute->cpp_type().is<float>()) {
      FloatWriteAttribute float_attribute = std::move(attribute);
      for (const int i : IndexRange(size)) {
        const float value = rng.get_float() * (max_value.x - min_value.x) + min_value.x;
        float_attribute.set(i, value);
      }
    }
    else if (attribute->cpp_type().is<float3>()) {
      Float3WriteAttribute float3_attribute = std::move(attribute);
      for (const int i : IndexRange(size)) {
        const float x = rng.get_float();
        const float y = rng.get_float();
        const float z = rng.get_float();
        const float3 value = float3(x, y, z) * (max_value - min_value) + min_value;
        float3_attribute.set(i, value);
      }
    }
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_random_attribute()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_RANDOM_ATTRIBUTE, "Random Attribute", 0, 0);
  node_type_socket_templates(&ntype, geo_node_random_attribute_in, geo_node_random_attribute_out);
  node_type_init(&ntype, geo_attribute_random_init);
  ntype.geometry_node_execute = blender::nodes::geo_random_attribute_exec;
  nodeRegisterType(&ntype);
}
