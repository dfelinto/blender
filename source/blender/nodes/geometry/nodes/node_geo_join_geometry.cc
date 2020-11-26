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
#include "BKE_mesh_runtime.h"
#include "BKE_pointcloud.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_join_geometry_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_join_geometry_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

namespace blender::nodes {

static Mesh *join_mesh_topology_and_builtin_attributes(Span<const MeshComponent *> src_components)
{
  int totverts = 0;
  int totloops = 0;
  int totedges = 0;
  int totpolys = 0;

  for (const MeshComponent *mesh_component : src_components) {
    const Mesh *mesh = mesh_component->get_for_read();
    totverts += mesh->totvert;
    totloops += mesh->totloop;
    totedges += mesh->totedge;
    totpolys += mesh->totpoly;
  }

  const Mesh *first_input_mesh = src_components[0]->get_for_read();
  Mesh *new_mesh = BKE_mesh_new_nomain(totverts, totedges, 0, totloops, totpolys);
  BKE_mesh_copy_settings(new_mesh, first_input_mesh);

  int vert_offset = 0;
  int loop_offset = 0;
  int edge_offset = 0;
  int poly_offset = 0;
  for (const MeshComponent *mesh_component : src_components) {
    const Mesh *mesh = mesh_component->get_for_read();
    if (mesh == nullptr) {
      continue;
    }

    for (const int i : IndexRange(mesh->totvert)) {
      const MVert &old_vert = mesh->mvert[i];
      MVert &new_vert = new_mesh->mvert[vert_offset + i];
      new_vert = old_vert;
    }

    for (const int i : IndexRange(mesh->totedge)) {
      const MEdge &old_edge = mesh->medge[i];
      MEdge &new_edge = new_mesh->medge[edge_offset + i];
      new_edge = old_edge;
      new_edge.v1 += vert_offset;
      new_edge.v2 += vert_offset;
    }
    for (const int i : IndexRange(mesh->totloop)) {
      const MLoop &old_loop = mesh->mloop[i];
      MLoop &new_loop = new_mesh->mloop[loop_offset + i];
      new_loop = old_loop;
      new_loop.v += vert_offset;
      new_loop.e += edge_offset;
    }
    for (const int i : IndexRange(mesh->totpoly)) {
      const MPoly &old_poly = mesh->mpoly[i];
      MPoly &new_poly = new_mesh->mpoly[poly_offset + i];
      new_poly = old_poly;
      new_poly.loopstart += loop_offset;
    }

    vert_offset += mesh->totvert;
    loop_offset += mesh->totloop;
    edge_offset += mesh->totedge;
    poly_offset += mesh->totpoly;
  }

  return new_mesh;
}

template<typename Component>
static Array<const GeometryComponent *> to_base_components(Span<const Component *> components)
{
  return components;
}

static Set<std::string> find_all_attribute_names(Span<const GeometryComponent *> components)
{
  Set<std::string> attribute_names;
  for (const GeometryComponent *component : components) {
    Set<std::string> names = component->attribute_names();
    for (const std::string &name : names) {
      attribute_names.add(name);
    }
  }
  return attribute_names;
}

static void determine_final_data_type_and_domain(Span<const GeometryComponent *> components,
                                                 StringRef attribute_name,
                                                 CustomDataType *r_type,
                                                 AttributeDomain *r_domain)
{
  for (const GeometryComponent *component : components) {
    ReadAttributePtr attribute = component->attribute_try_get_for_read(attribute_name);
    if (attribute) {
      /* TODO: Use data type with most information. */
      *r_type = bke::cpp_type_to_custom_data_type(attribute->cpp_type());
      /* TODO: Use highest priority domain. */
      *r_domain = attribute->domain();
      return;
    }
  }
  BLI_assert(false);
}

static void fill_new_attribute(Span<const GeometryComponent *> src_components,
                               StringRef attribute_name,
                               const CustomDataType data_type,
                               const AttributeDomain domain,
                               WriteAttributePtr &output_attribute)
{
  const CPPType *cpp_type = bke::custom_data_type_to_cpp_type(data_type);
  BLI_assert(cpp_type != nullptr);

  int offset = 0;
  for (const GeometryComponent *component : src_components) {
    const int domain_size = component->attribute_domain_size(domain);
    ReadAttributePtr read_attribute = component->attribute_get_for_read(
        attribute_name, domain, data_type, nullptr);

    AlignedBuffer<64, 64> buffer;
    BLI_assert(64 >= cpp_type->size());
    for (const int i : IndexRange(domain_size)) {
      read_attribute->get(i, buffer.ptr());
      output_attribute->set(offset + i, buffer.ptr());
      cpp_type->destruct(buffer.ptr());
    }

    offset += domain_size;
  }
}

static void join_attributes(Span<const GeometryComponent *> src_components,
                            GeometryComponent &result,
                            Span<StringRef> ignored_attributes = {})
{
  Set<std::string> attribute_names = find_all_attribute_names(src_components);
  for (StringRef name : ignored_attributes) {
    attribute_names.remove(name);
  }

  for (const std::string &attribute_name : attribute_names) {
    CustomDataType data_type;
    AttributeDomain domain;
    determine_final_data_type_and_domain(src_components, attribute_name, &data_type, &domain);

    result.attribute_try_create(attribute_name, domain, data_type);
    WriteAttributePtr write_attribute = result.attribute_try_get_for_write(attribute_name);
    if (!write_attribute ||
        &write_attribute->cpp_type() != bke::custom_data_type_to_cpp_type(data_type) ||
        write_attribute->domain() != domain) {
      continue;
    }
    fill_new_attribute(src_components, attribute_name, data_type, domain, write_attribute);
  }
}

static void join_components(Span<const MeshComponent *> src_components, GeometrySet &result)
{
  Mesh *new_mesh = join_mesh_topology_and_builtin_attributes(src_components);

  MeshComponent &dst_component = result.get_component_for_write<MeshComponent>();
  dst_component.replace(new_mesh);

  /* The position attribute is handled above already. */
  join_attributes(to_base_components(src_components), dst_component, {"position"});
}

static void join_components(Span<const PointCloudComponent *> src_components, GeometrySet &result)
{
  int totpoints = 0;
  for (const PointCloudComponent *pointcloud_component : src_components) {
    totpoints += pointcloud_component->attribute_domain_size(ATTR_DOMAIN_POINT);
  }

  PointCloudComponent &dst_component = result.get_component_for_write<PointCloudComponent>();
  PointCloud *pointcloud = BKE_pointcloud_new_nomain(totpoints);
  dst_component.replace(pointcloud);

  join_attributes(to_base_components(src_components), dst_component);
}

static void join_components(Span<const InstancesComponent *> src_components, GeometrySet &result)
{
  InstancesComponent &dst_component = result.get_component_for_write<InstancesComponent>();
  for (const InstancesComponent *component : src_components) {
    const int size = component->instances_amount();
    Span<const Object *> objects = component->objects();
    Span<float3> positions = component->positions();
    Span<float3> rotations = component->rotations();
    Span<float3> scales = component->scales();
    for (const int i : IndexRange(size)) {
      dst_component.add_instance(objects[i], positions[i], rotations[i], scales[i]);
    }
  }
}

template<typename Component>
static void join_component_type(Span<const GeometrySet *> src_geometry_sets, GeometrySet &result)
{
  Vector<const Component *> components;
  for (const GeometrySet *geometry_set : src_geometry_sets) {
    const Component *component = geometry_set->get_component_for_read<Component>();
    if (component != nullptr && !component->is_empty()) {
      components.append(component);
    }
  }

  if (components.size() == 0) {
    return;
  }
  if (components.size() == 1) {
    result.add(*components[0]);
    return;
  }
  join_components(components, result);
}

static void geo_node_join_geometry_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set_a = params.extract_input<GeometrySet>("Geometry");
  GeometrySet geometry_set_b = params.extract_input<GeometrySet>("Geometry_001");
  GeometrySet geometry_set_result;

  std::array<const GeometrySet *, 2> src_geometry_sets = {&geometry_set_a, &geometry_set_b};

  join_component_type<MeshComponent>(src_geometry_sets, geometry_set_result);
  join_component_type<PointCloudComponent>(src_geometry_sets, geometry_set_result);
  join_component_type<InstancesComponent>(src_geometry_sets, geometry_set_result);

  params.set_output("Geometry", std::move(geometry_set_result));
}
}  // namespace blender::nodes

void register_node_type_geo_join_geometry()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_JOIN_GEOMETRY, "Join Geometry", 0, 0);
  node_type_socket_templates(&ntype, geo_node_join_geometry_in, geo_node_join_geometry_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_join_geometry_exec;
  nodeRegisterType(&ntype);
}
