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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 */

#include <cstring>
#include <iostream>
#include <string>

#include "MEM_guardedalloc.h"

#include "BLI_float3.hh"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_customdata.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_pointcloud.h"
#include "BKE_screen.h"
#include "BKE_simulation.h"

#include "BLO_read_write.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

#include "NOD_derived_node_tree.hh"
#include "NOD_geometry_exec.hh"
#include "NOD_node_tree_multi_function.hh"
#include "NOD_type_callbacks.hh"

using blender::float3;

static void initData(ModifierData *md)
{
  NodesModifierData *nmd = (NodesModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(nmd, modifier));

  MEMCPY_STRUCT_AFTER(nmd, DNA_struct_default_get(NodesModifierData), modifier);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  if (nmd->node_group != nullptr) {
    DEG_add_node_tree_relation(ctx->node, nmd->node_group, "Nodes Modifier");
  }
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  walk(userData, ob, (ID **)&nmd->node_group, IDWALK_CB_USER);
}

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *md,
                       bool UNUSED(useRenderParams))
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  UNUSED_VARS(nmd);
  return false;
}

static PointCloud *modifyPointCloud(ModifierData *md,
                                    const ModifierEvalContext *UNUSED(ctx),
                                    PointCloud *pointcloud)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  UNUSED_VARS(nmd);
  std::cout << __func__ << "\n";
  return pointcloud;
}

/* To be replaced soon. */
using namespace blender;
using namespace blender::nodes;
using namespace blender::fn;
using namespace blender::bke;

class GeometryNodesEvaluator {
 private:
  LinearAllocator<> allocator_;
  Map<const DInputSocket *, GMutablePointer> value_by_input_;
  Vector<const DInputSocket *> group_outputs_;
  MultiFunctionByNode &mf_by_node_;
  const DataTypeConversions &conversions_;

 public:
  GeometryNodesEvaluator(const Map<const DOutputSocket *, GMutablePointer> &group_input_data,
                         Vector<const DInputSocket *> group_outputs,
                         MultiFunctionByNode &mf_by_node)
      : group_outputs_(std::move(group_outputs)),
        mf_by_node_(mf_by_node),
        conversions_(get_implicit_type_conversions())
  {
    for (auto item : group_input_data.items()) {
      this->forward_to_inputs(*item.key, item.value);
    }
  }

  Vector<GMutablePointer> execute()
  {
    Vector<GMutablePointer> results;
    for (const DInputSocket *group_output : group_outputs_) {
      GMutablePointer result = this->get_input_value(*group_output);
      results.append(result);
    }
    for (GMutablePointer value : value_by_input_.values()) {
      value.destruct();
    }
    return results;
  }

 private:
  GMutablePointer get_input_value(const DInputSocket &socket_to_compute)
  {
    std::optional<GMutablePointer> value = value_by_input_.pop_try(&socket_to_compute);
    if (value.has_value()) {
      /* This input has been computed before, return it directly. */
      return *value;
    }

    Span<const DOutputSocket *> from_sockets = socket_to_compute.linked_sockets();
    Span<const DGroupInput *> from_group_inputs = socket_to_compute.linked_group_inputs();
    const int total_inputs = from_sockets.size() + from_group_inputs.size();
    BLI_assert(total_inputs <= 1);

    const CPPType &type = *socket_cpp_type_get(*socket_to_compute.typeinfo());

    if (total_inputs == 0) {
      /* The input is not connected, use the value from the socket itself. */
      bNodeSocket &bsocket = *socket_to_compute.bsocket();
      void *buffer = allocator_.allocate(type.size(), type.alignment());
      socket_cpp_value_get(bsocket, buffer);
      return GMutablePointer{type, buffer};
    }
    if (from_group_inputs.size() == 1) {
      /* The input gets its value from the input of a group that is not further connected. */
      bNodeSocket &bsocket = *from_group_inputs[0]->bsocket();
      void *buffer = allocator_.allocate(type.size(), type.alignment());
      socket_cpp_value_get(bsocket, buffer);
      return GMutablePointer{type, buffer};
    }

    /* Compute the socket now. */
    const DOutputSocket &from_socket = *from_sockets[0];
    this->compute_output_and_forward(from_socket);
    return value_by_input_.pop(&socket_to_compute);
  }

  void compute_output_and_forward(const DOutputSocket &socket_to_compute)
  {
    const DNode &node = socket_to_compute.node();

    /* Prepare inputs required to execute the node. */
    GValueByName node_inputs{allocator_};
    for (const DInputSocket *input_socket : node.inputs()) {
      if (input_socket->is_available()) {
        GMutablePointer value = this->get_input_value(*input_socket);
        node_inputs.transfer_ownership_in(input_socket->identifier(), value);
      }
    }

    /* Execute the node. */
    GValueByName node_outputs{allocator_};
    this->execute_node(node, node_inputs, node_outputs);

    /* Forward computed outputs to linked input sockets. */
    for (const DOutputSocket *output_socket : node.outputs()) {
      if (output_socket->is_available()) {
        GMutablePointer value = node_outputs.extract(output_socket->identifier());
        this->forward_to_inputs(*output_socket, value);
      }
    }
  }

  void execute_node(const DNode &node, GValueByName &node_inputs, GValueByName &node_outputs)
  {
    bNode *bnode = node.bnode();
    if (bnode->typeinfo->geometry_node_execute != nullptr) {
      bnode->typeinfo->geometry_node_execute(bnode, node_inputs, node_outputs);
      return;
    }

    /* Use the multi-function implementation of the node. */
    const MultiFunction &fn = *mf_by_node_.lookup(&node);
    MFContextBuilder context;
    MFParamsBuilder params{fn, 1};
    Vector<GMutablePointer> input_data;
    for (const DInputSocket *dsocket : node.inputs()) {
      if (dsocket->is_available()) {
        GMutablePointer data = node_inputs.extract(dsocket->identifier());
        params.add_readonly_single_input(GSpan(*data.type(), data.get(), 1));
        input_data.append(data);
      }
    }
    Vector<GMutablePointer> output_data;
    for (const DOutputSocket *dsocket : node.outputs()) {
      if (dsocket->is_available()) {
        const CPPType &type = *socket_cpp_type_get(*dsocket->typeinfo());
        void *buffer = allocator_.allocate(type.size(), type.alignment());
        params.add_uninitialized_single_output(GMutableSpan(type, buffer, 1));
        output_data.append(GMutablePointer(type, buffer));
      }
    }
    fn.call(IndexRange(1), params, context);
    for (GMutablePointer value : input_data) {
      value.destruct();
    }
    for (const int i : node.outputs().index_range()) {
      GMutablePointer value = output_data[i];
      node_outputs.move_in(node.output(i).identifier(), value);
      value.destruct();
    }
  }

  void forward_to_inputs(const DOutputSocket &from_socket, GMutablePointer value_to_forward)
  {
    Span<const DInputSocket *> to_sockets_all = from_socket.linked_sockets();

    const CPPType &from_type = *value_to_forward.type();

    Vector<const DInputSocket *> to_sockets_same_type;
    for (const DInputSocket *to_socket : to_sockets_all) {
      const CPPType &to_type = *socket_cpp_type_get(*to_socket->typeinfo());
      if (from_type == to_type) {
        to_sockets_same_type.append(to_socket);
      }
      else {
        void *buffer = allocator_.allocate(to_type.size(), to_type.alignment());
        if (conversions_.is_convertible(from_type, to_type)) {
          conversions_.convert(from_type, to_type, value_to_forward.get(), buffer);
        }
        else {
          to_type.copy_to_uninitialized(to_type.default_value(), buffer);
        }
        value_by_input_.add_new(to_socket, GMutablePointer{to_type, buffer});
      }
    }

    if (to_sockets_same_type.size() == 0) {
      /* This value is not further used, so destruct it. */
      value_to_forward.destruct();
    }
    else if (to_sockets_same_type.size() == 1) {
      /* This value is only used on one input socket, no need to copy it. */
      const DInputSocket *to_socket = to_sockets_same_type[0];
      value_by_input_.add_new(to_socket, value_to_forward);
    }
    else {
      /* Multiple inputs use the value, make a copy for every input except for one. */
      const DInputSocket *first_to_socket = to_sockets_same_type[0];
      Span<const DInputSocket *> other_to_sockets = to_sockets_same_type.as_span().drop_front(1);
      const CPPType &type = *value_to_forward.type();

      value_by_input_.add_new(first_to_socket, value_to_forward);
      for (const DInputSocket *to_socket : other_to_sockets) {
        void *buffer = allocator_.allocate(type.size(), type.alignment());
        type.copy_to_uninitialized(value_to_forward.get(), buffer);
        value_by_input_.add_new(to_socket, GMutablePointer{type, buffer});
      }
    }
  }
};

/**
 * Evaluate a node group to compute the output geometry.
 * Currently, this uses a fairly basic and inefficient algorithm that might compute things more
 * often than necessary. It's going to be replaced soon.
 */
static GeometryPtr compute_geometry(const DerivedNodeTree &tree,
                                    Span<const DOutputSocket *> group_input_sockets,
                                    const DInputSocket &socket_to_compute,
                                    GeometryPtr input_geometry,
                                    NodesModifierData *nmd)
{
  ResourceCollector resources;
  LinearAllocator<> &allocator = resources.linear_allocator();
  MultiFunctionByNode mf_by_node = get_multi_function_per_node(tree, resources);

  Map<const DOutputSocket *, GMutablePointer> group_inputs;

  if (group_input_sockets.size() > 0) {
    Span<const DOutputSocket *> remaining_input_sockets = group_input_sockets;

    /* If the group expects a geometry as first input, use the geometry that has been passed to
     * modifier. */
    const DOutputSocket *first_input_socket = group_input_sockets[0];
    if (first_input_socket->bsocket()->type == SOCK_GEOMETRY) {
      GeometryPtr *geometry_in = allocator.construct<GeometryPtr>(std::move(input_geometry));
      group_inputs.add_new(first_input_socket, geometry_in);
      remaining_input_sockets = remaining_input_sockets.drop_front(1);
    }

    /* Initialize remaining group inputs. */
    for (const DOutputSocket *socket : remaining_input_sockets) {
      const CPPType &type = *socket_cpp_type_get(*socket->typeinfo());
      if (type.is<float>()) {
        float *value_in = allocator.construct<float>(nmd->test_float_input);
        group_inputs.add_new(socket, value_in);
      }
      else {
        void *value_in = allocator.allocate(type.size(), type.alignment());
        type.copy_to_uninitialized(type.default_value(), value_in);
        group_inputs.add_new(socket, {type, value_in});
      }
    }
  }

  Vector<const DInputSocket *> group_outputs;
  group_outputs.append(&socket_to_compute);

  GeometryNodesEvaluator evaluator{group_inputs, group_outputs, mf_by_node};
  Vector<GMutablePointer> results = evaluator.execute();
  BLI_assert(results.size() == 1);
  GMutablePointer result = results[0];

  GeometryPtr output_geometry = std::move(*(GeometryPtr *)result.get());
  return output_geometry;
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *UNUSED(ctx), Mesh *mesh)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  if (nmd->node_group == nullptr) {
    return mesh;
  }

  NodeTreeRefMap tree_refs;
  DerivedNodeTree tree{nmd->node_group, tree_refs};
  ResourceCollector resources;

  Span<const DNode *> input_nodes = tree.nodes_by_type("NodeGroupInput");
  Span<const DNode *> output_nodes = tree.nodes_by_type("NodeGroupOutput");

  if (input_nodes.size() > 1) {
    return mesh;
  }
  if (output_nodes.size() != 1) {
    return mesh;
  }

  Span<const DOutputSocket *> group_inputs = (input_nodes.size() == 1) ?
                                                 input_nodes[0]->outputs().drop_back(1) :
                                                 Span<const DOutputSocket *>{};
  Span<const DInputSocket *> group_outputs = output_nodes[0]->inputs().drop_back(1);

  if (group_outputs.size() != 1) {
    return mesh;
  }

  const DInputSocket *group_output = group_outputs[0];
  if (group_output->idname() != "NodeSocketGeometry") {
    return mesh;
  }

  GeometryPtr input_geometry{new Geometry()};
  input_geometry->mesh_set_and_keep_ownership(mesh);

  GeometryPtr new_geometry = compute_geometry(
      tree, group_inputs, *group_outputs[0], std::move(input_geometry), nmd);
  make_geometry_mutable(new_geometry);
  Mesh *new_mesh = new_geometry->mesh_release();
  if (new_mesh == nullptr) {
    return BKE_mesh_new_nomain(0, 0, 0, 0, 0);
  }
  return new_mesh;
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiItemR(layout, ptr, "node_group", 0, NULL, ICON_MESH_DATA);
  uiItemR(layout, ptr, "test_float_input", 0, NULL, ICON_NONE);

  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Nodes, panel_draw);
}

static void blendWrite(BlendWriter *writer, const ModifierData *md)
{
  const NodesModifierData *nmd = reinterpret_cast<const NodesModifierData *>(md);
  UNUSED_VARS(nmd, writer);
}

static void blendRead(BlendDataReader *reader, ModifierData *md)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  UNUSED_VARS(nmd, reader);
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
  const NodesModifierData *nmd = reinterpret_cast<const NodesModifierData *>(md);
  NodesModifierData *tnmd = reinterpret_cast<NodesModifierData *>(target);
  UNUSED_VARS(nmd, tnmd);

  BKE_modifier_copydata_generic(md, target, flag);
}

static void freeData(ModifierData *md)
{
  NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
  UNUSED_VARS(nmd);
}

ModifierTypeInfo modifierType_Nodes = {
    /* name */ "Nodes",
    /* structName */ "NodesModifierData",
    /* structSize */ sizeof(NodesModifierData),
#ifdef WITH_GEOMETRY_NODES
    /* srna */ &RNA_NodesModifier,
#else
    /* srna */ &RNA_Modifier,
#endif
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh,
    /* icon */ ICON_MESH_DATA, /* TODO: Use correct icon. */

    /* copyData */ copyData,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ modifyPointCloud,
    /* modifyVolume */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ NULL,
    /* freeData */ freeData,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
    /* blendWrite */ blendWrite,
    /* blendRead */ blendRead,
};
