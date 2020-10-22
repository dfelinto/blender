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

static void get_socket_value(const bNodeSocket *bsocket, void *r_value)
{
  /* TODO: Move this into socket callbacks. */
  switch (bsocket->type) {
    case SOCK_GEOMETRY:
      new (r_value) GeometryPtr();
      break;
    case SOCK_FLOAT:
      *(float *)r_value = ((bNodeSocketValueFloat *)bsocket->default_value)->value;
      break;
    case SOCK_INT:
      *(int *)r_value = ((bNodeSocketValueInt *)bsocket->default_value)->value;
      break;
    default:
      BLI_assert(false);
  }
}

static const CPPType &get_socket_type(const bNodeSocket *bsocket)
{
  /* TODO: Move this into socket callbacks. */
  switch (bsocket->type) {
    case SOCK_GEOMETRY:
      return CPPType::get<GeometryPtr>();
    case SOCK_FLOAT:
      return CPPType::get<float>();
    case SOCK_INT:
      return CPPType::get<int>();
    default:
      BLI_assert(false);
      return CPPType::get<float>();
  }
}

static bool compute_input_socket(const DOutputSocket *group_input,
                                 GeometryPtr &group_input_geometry,
                                 const DInputSocket &socket_to_compute,
                                 void *r_value);

static bool compute_output_socket(const DOutputSocket *group_input,
                                  GeometryPtr &group_input_geometry,
                                  const DOutputSocket &socket_to_compute,
                                  void *r_value)
{
  if (group_input == &socket_to_compute) {
    new (r_value) GeometryPtr(group_input_geometry);
    return true;
  }

  const DNode &node = socket_to_compute.node();
  bNode *bnode = node.node_ref().bnode();

  LinearAllocator<> allocator;
  GValueByName node_inputs{allocator};

  /* Compute all inputs for the node. */
  for (const DInputSocket *input_socket : node.inputs()) {
    const CPPType &type = get_socket_type(input_socket->bsocket());
    void *buffer = allocator.allocate(type.size(), type.alignment());
    compute_input_socket(group_input, group_input_geometry, *input_socket, buffer);
    node_inputs.move_in(input_socket->bsocket()->identifier, GMutablePointer{type, buffer});
    type.destruct(buffer);
  }

  /* Execute the node itself. */
  GValueByName node_outputs{allocator};
  bnode->typeinfo->geometry_node_execute(bnode, node_inputs, node_outputs);

  /* Pass relevant value to the caller. */
  bNodeSocket *bsocket_to_compute = socket_to_compute.bsocket();
  const CPPType &type_to_compute = get_socket_type(bsocket_to_compute);
  GMutablePointer computed_value = node_outputs.extract(bsocket_to_compute->identifier);
  type_to_compute.relocate_to_uninitialized(computed_value.get(), r_value);
  return true;
}

/* Returns true on success. */
static bool compute_input_socket(const DOutputSocket *group_input,
                                 GeometryPtr &group_input_geometry,
                                 const DInputSocket &socket_to_compute,
                                 void *r_value)
{
  Span<const DOutputSocket *> from_sockets = socket_to_compute.linked_sockets();
  Span<const DGroupInput *> from_group_inputs = socket_to_compute.linked_group_inputs();
  const int total_inputs = from_sockets.size() + from_group_inputs.size();
  if (total_inputs >= 2) {
    return false;
  }
  if (total_inputs == 0) {
    bNodeSocket *bsocket = socket_to_compute.bsocket();
    get_socket_value(bsocket, r_value);
    return true;
  }
  if (from_group_inputs.size() == 1) {
    bNodeSocket *bsocket = from_group_inputs[0]->bsocket();
    BLI_assert(bsocket->type == socket_to_compute.bsocket()->type);
    get_socket_value(bsocket, r_value);
    return true;
  }

  const DOutputSocket &from_socket = *from_sockets[0];
  return compute_output_socket(group_input, group_input_geometry, from_socket, r_value);
}

/**
 * Evaluate a node group to compute the output geometry.
 * Currently, this uses a fairly basic and inefficient algorithm that might compute things more
 * often than necessary. It's going to be replaced soon.
 */
static GeometryPtr compute_geometry(const DOutputSocket *group_input,
                                    GeometryPtr &group_input_geometry,
                                    const DInputSocket &socket_to_compute)
{
  GeometryPtr output_geometry;
  const bool success = compute_input_socket(
      group_input, group_input_geometry, socket_to_compute, &output_geometry);
  if (!success) {
    return {};
  }
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

  GeometryPtr new_geometry = compute_geometry(group_inputs[0], input_geometry, *group_outputs[0]);
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
