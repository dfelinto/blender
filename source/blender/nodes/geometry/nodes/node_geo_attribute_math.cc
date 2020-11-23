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

#include "BKE_attribute.h"
#include "BKE_attribute_access.hh"

#include "BLI_array.hh"
#include "BLI_math_base_safe.h"
#include "BLI_rand.hh"

#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"

static bNodeSocketTemplate geo_node_attribute_math_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_STRING, N_("Attribute A")},
    {SOCK_FLOAT, N_("A"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_STRING, N_("Attribute B")},
    {SOCK_FLOAT, N_("B"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_STRING, N_("Result")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_attribute_math_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_node_attribute_math_init(bNodeTree *UNUSED(tree), bNode *node)
{
  node->custom1 = NODE_MATH_ADD;
  node->custom2 = GEO_NODE_USE_ATTRIBUTE_A | GEO_NODE_USE_ATTRIBUTE_B;
}

static void geo_node_random_attribute_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sock_attribute_a = (bNodeSocket *)BLI_findlink(&node->inputs, 1);
  bNodeSocket *sock_float_a = sock_attribute_a->next;
  bNodeSocket *sock_attribute_b = sock_float_a->next;
  bNodeSocket *sock_float_b = sock_attribute_b->next;

  GeometryNodeUseAttributeFlag flag = static_cast<GeometryNodeUseAttributeFlag>(node->custom2);

  nodeSetSocketAvailability(sock_attribute_a, flag & GEO_NODE_USE_ATTRIBUTE_A);
  nodeSetSocketAvailability(sock_attribute_b, flag & GEO_NODE_USE_ATTRIBUTE_B);
  nodeSetSocketAvailability(sock_float_a, !(flag & GEO_NODE_USE_ATTRIBUTE_A));
  nodeSetSocketAvailability(sock_float_b, !(flag & GEO_NODE_USE_ATTRIBUTE_B));
}

namespace blender::nodes {

/**
 * Do implicit conversion from other attribute types to float.
 */
static Array<float> attribute_ensure_float_type(ReadAttributePtr attribute)
{
  Array<float> array(attribute->size());
  if (attribute->cpp_type().is<float>()) {
    FloatReadAttribute float_attribute = std::move(attribute);
    for (const int i : IndexRange(float_attribute.size())) {
      array[i] = float_attribute[i];
    }
  }
  else if (attribute->cpp_type().is<float3>()) {
    Float3ReadAttribute float3_attribute = std::move(attribute);
    for (const int i : IndexRange(float3_attribute.size())) {
      array[i] = float3_attribute[i].length();
    }
  }
#if 0 /* More CPP types to support in the future. */
  else if (attribute->cpp_type().is<Color4f>()) {
  }
  else if (attribute->cpp_type().is<float2>()) {
  }
  else if (attribute->cpp_type().is<int>()) {
  }
#endif

  return array;
}

static void math_operation_attribute_float(Array<float> input_a,
                                           float input_b,
                                           FloatWriteAttribute float_attribute,
                                           const int operation)
{
  switch (operation) {
    case NODE_MATH_ADD:
      for (const int i : IndexRange(input_a.size())) {
        float_attribute.set(i, input_a[i] + input_b);
      }
      break;
    case NODE_MATH_SUBTRACT:
      for (const int i : IndexRange(input_a.size())) {
        float_attribute.set(i, input_a[i] - input_b);
      }
      break;
    case NODE_MATH_MULTIPLY:
      for (const int i : IndexRange(input_a.size())) {
        float_attribute.set(i, input_a[i] * input_b);
      }
      break;
    case NODE_MATH_DIVIDE:
      /* Protect against division by zero. */
      if (input_b == 0.0f) {
        for (const int i : IndexRange(input_a.size())) {
          float_attribute.set(i, 0.0f);
        }
      }
      else {
        for (const int i : IndexRange(input_a.size())) {
          float_attribute.set(i, input_a[i] / input_b);
        }
      }
      break;
  }
}

static void math_operation_float_attribute(float input_a,
                                           Array<float> input_b,
                                           FloatWriteAttribute float_attribute,
                                           const int operation)
{
  switch (operation) {
    case NODE_MATH_ADD:
      for (const int i : IndexRange(float_attribute.size())) {
        float_attribute.set(i, input_a + input_b[i]);
      }
      break;
    case NODE_MATH_SUBTRACT:
      for (const int i : IndexRange(float_attribute.size())) {
        float_attribute.set(i, input_a - input_b[i]);
      }
      break;
    case NODE_MATH_MULTIPLY:
      for (const int i : IndexRange(float_attribute.size())) {
        float_attribute.set(i, input_a * input_b[i]);
      }
      break;
    case NODE_MATH_DIVIDE:
      for (const int i : IndexRange(float_attribute.size())) {
        /* Protect against division by zero. */
        float_attribute.set(i, safe_divide(input_a, input_b[i]));
      }
      break;
  }
}

static void math_operation_attribute_attribute(Array<float> input_a,
                                               Array<float> input_b,
                                               FloatWriteAttribute float_attribute,
                                               const int operation)
{
  switch (operation) {
    case NODE_MATH_ADD:
      for (const int i : IndexRange(float_attribute.size())) {
        float_attribute.set(i, input_a[i] + input_b[i]);
      }
      break;
    case NODE_MATH_SUBTRACT:
      for (const int i : IndexRange(float_attribute.size())) {
        float_attribute.set(i, input_a[i] - input_b[i]);
      }
      break;
    case NODE_MATH_MULTIPLY:
      for (const int i : IndexRange(float_attribute.size())) {
        float_attribute.set(i, input_a[i] * input_b[i]);
      }
      break;
    case NODE_MATH_DIVIDE:
      for (const int i : IndexRange(float_attribute.size())) {
        /* Protect against division by zero. */
        float_attribute.set(i, safe_divide(input_a[i], input_b[i]));
      }
      break;
  }
}

static void attribute_math_calc(GeometryComponent &component, const GeoNodeExecParams &params)
{
  const bNode &node = params.node();
  const int operation = params.node().custom1;
  const std::string result_name = params.get_input<std::string>("Result");

  /* The result type of this node is always float. */
  const CustomDataType result_type = bke::cpp_type_to_custom_data_type(CPPType::get<float>());

  /* Use a single float for none or one of the inputs, which leads to three cases,
   * because the order of the inputs in the operations can matter. */
  GeometryNodeUseAttributeFlag flag = static_cast<GeometryNodeUseAttributeFlag>(node.custom2);
  if ((flag & GEO_NODE_USE_ATTRIBUTE_A) && (flag & GEO_NODE_USE_ATTRIBUTE_B)) {
    /* Two attributes. */
    const std::string attribute_a_name = params.get_input<std::string>("Attribute A");
    const std::string attribute_b_name = params.get_input<std::string>("Attribute B");
    ReadAttributePtr attribute_a = component.attribute_try_get_for_read(attribute_a_name);
    ReadAttributePtr attribute_b = component.attribute_try_get_for_read(attribute_b_name);
    if (!attribute_a || !attribute_b) {
      return;
    }

    /* Don't handle domain interpolation for now. */
    AttributeDomain domain_a = attribute_a->domain();
    AttributeDomain domain_b = attribute_b->domain();
    if (domain_a != domain_b) {
      return;
    }

    BLI_assert(attribute_a->size() == attribute_b->size());

    Array<float> input_a = attribute_ensure_float_type(std::move(attribute_a));
    Array<float> input_b = attribute_ensure_float_type(std::move(attribute_b));

    WriteAttributePtr result = component.attribute_try_ensure_for_write(
        result_name, domain_a, result_type);
    if (!result) {
      return;
    }

    math_operation_attribute_attribute(input_a, input_b, std::move(result), operation);
  }
  else if (flag & GEO_NODE_USE_ATTRIBUTE_A) {
    /* Attribute and float. */
    const std::string attribute_a_name = params.get_input<std::string>("Attribute A");
    ReadAttributePtr attribute_a = component.attribute_try_get_for_read(attribute_a_name);
    if (!attribute_a) {
      return;
    }

    AttributeDomain domain = attribute_a->domain();
    Array<float> input_a = attribute_ensure_float_type(std::move(attribute_a));
    const float input_b = params.get_input<float>("B");

    WriteAttributePtr result = component.attribute_try_ensure_for_write(
        result_name, domain, result_type);
    if (!result) {
      return;
    }

    math_operation_attribute_float(input_a, input_b, std::move(result), operation);
  }
  else if (flag & GEO_NODE_USE_ATTRIBUTE_B) {
    /* Float and attribute. */
    const std::string attribute_b_name = params.get_input<std::string>("Attribute B");
    ReadAttributePtr attribute_b = component.attribute_try_get_for_read(attribute_b_name);
    if (!attribute_b) {
      return;
    }

    AttributeDomain domain = attribute_b->domain();
    Array<float> input_b = attribute_ensure_float_type(std::move(attribute_b));
    const float input_a = params.get_input<float>("A");

    WriteAttributePtr result = component.attribute_try_ensure_for_write(
        result_name, domain, result_type);
    if (!result) {
      return;
    }

    math_operation_float_attribute(input_a, input_b, std::move(result), operation);
  }
  else {
    /* There was no attribute provided, so just return. Otherwise choosing which domain to use
     * for a new attribute would be completely arbitrary, and we don't want to make those sort
     * of decisions in the attribute math node. */
    return;
  }
}

static void geo_node_attribute_math_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  if (geometry_set.has<MeshComponent>()) {
    attribute_math_calc(geometry_set.get_component_for_write<MeshComponent>(), params);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    attribute_math_calc(geometry_set.get_component_for_write<PointCloudComponent>(), params);
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_attribute_math()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_ATTRIBUTE_MATH, "Attribute Math", 0, 0);
  node_type_socket_templates(&ntype, geo_node_attribute_math_in, geo_node_attribute_math_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_math_exec;
  node_type_update(&ntype, geo_node_random_attribute_update);
  node_type_init(&ntype, geo_node_attribute_math_init);
  nodeRegisterType(&ntype);
}
