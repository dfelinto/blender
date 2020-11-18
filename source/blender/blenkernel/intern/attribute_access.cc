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

#include <utility>

#include "BKE_attribute_access.hh"
#include "BKE_deform.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_color.hh"
#include "BLI_float2.hh"
#include "BLI_span.hh"

namespace blender::bke {

/* -------------------------------------------------------------------- */
/** \name Attribute accessor implementations.
 * \{ */

ReadAttribute::~ReadAttribute() = default;
WriteAttribute::~WriteAttribute() = default;

class VertexWeightWriteAttribute final : public WriteAttribute {
 private:
  MutableSpan<MDeformVert> dverts_;
  const int dvert_index_;

 public:
  VertexWeightWriteAttribute(MDeformVert *dverts, const int totvert, const int dvert_index)
      : WriteAttribute(ATTR_DOMAIN_VERTEX, CPPType::get<float>(), totvert),
        dverts_(dverts, totvert),
        dvert_index_(dvert_index)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    this->get_internal(dverts_, dvert_index_, index, r_value);
  }

  void set_internal(const int64_t index, const void *value) override
  {
    MDeformWeight *weight = BKE_defvert_ensure_index(&dverts_[index], dvert_index_);
    weight->weight = *reinterpret_cast<const float *>(value);
  }

  static void get_internal(const Span<MDeformVert> dverts,
                           const int dvert_index,
                           const int64_t index,
                           void *r_value)
  {
    const MDeformVert &dvert = dverts[index];
    for (const MDeformWeight &weight : Span(dvert.dw, dvert.totweight)) {
      if (weight.def_nr == dvert_index) {
        *(float *)r_value = weight.weight;
        return;
      }
    }
    *(float *)r_value = 0.0f;
  }
};

class VertexWeightReadAttribute final : public ReadAttribute {
 private:
  const Span<MDeformVert> dverts_;
  const int dvert_index_;

 public:
  VertexWeightReadAttribute(const MDeformVert *dverts, const int totvert, const int dvert_index)
      : ReadAttribute(ATTR_DOMAIN_VERTEX, CPPType::get<float>(), totvert),
        dverts_(dverts, totvert),
        dvert_index_(dvert_index)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    VertexWeightWriteAttribute::get_internal(dverts_, dvert_index_, index, r_value);
  }
};

template<typename T> class ArrayWriteAttribute final : public WriteAttribute {
 private:
  MutableSpan<T> data_;

 public:
  ArrayWriteAttribute(AttributeDomain domain, MutableSpan<T> data)
      : WriteAttribute(domain, CPPType::get<T>(), data.size()), data_(data)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    new (r_value) T(data_[index]);
  }

  void set_internal(const int64_t index, const void *value) override
  {
    data_[index] = *reinterpret_cast<const T *>(value);
  }
};

template<typename T> class ArrayReadAttribute final : public ReadAttribute {
 private:
  Span<T> data_;

 public:
  ArrayReadAttribute(AttributeDomain domain, Span<T> data)
      : ReadAttribute(domain, CPPType::get<T>(), data.size()), data_(data)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    new (r_value) T(data_[index]);
  }
};

template<typename StructT, typename ElemT, typename GetFuncT, typename SetFuncT>
class DerivedArrayWriteAttribute final : public WriteAttribute {
 private:
  MutableSpan<StructT> data_;
  GetFuncT get_function_;
  SetFuncT set_function_;

 public:
  DerivedArrayWriteAttribute(AttributeDomain domain,
                             MutableSpan<StructT> data,
                             GetFuncT get_function,
                             SetFuncT set_function)
      : WriteAttribute(domain, CPPType::get<ElemT>(), data.size()),
        data_(data),
        get_function_(std::move(get_function)),
        set_function_(std::move(set_function))
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    const StructT &struct_value = data_[index];
    const ElemT value = get_function_(struct_value);
    new (r_value) ElemT(value);
  }

  void set_internal(const int64_t index, const void *value) override
  {
    StructT &struct_value = data_[index];
    const ElemT &typed_value = *reinterpret_cast<const ElemT *>(value);
    set_function_(struct_value, typed_value);
  }
};

template<typename StructT, typename ElemT, typename GetFuncT>
class DerivedArrayReadAttribute final : public ReadAttribute {
 private:
  Span<StructT> data_;
  GetFuncT get_function_;

 public:
  DerivedArrayReadAttribute(AttributeDomain domain, Span<StructT> data, GetFuncT get_function)
      : ReadAttribute(domain, CPPType::get<ElemT>(), data.size()),
        data_(data),
        get_function_(std::move(get_function))
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    const StructT &struct_value = data_[index];
    const ElemT value = get_function_(struct_value);
    new (r_value) ElemT(value);
  }
};

class ConstantReadAttribute final : public ReadAttribute {
 private:
  void *value_;

 public:
  ConstantReadAttribute(AttributeDomain domain,
                        const int64_t size,
                        const CPPType &type,
                        const void *value)
      : ReadAttribute(domain, type, size)
  {
    value_ = MEM_mallocN_aligned(type.size(), type.alignment(), __func__);
    type.copy_to_uninitialized(value, value_);
  }

  ~ConstantReadAttribute()
  {
    this->cpp_type_.destruct(value_);
    MEM_freeN(value_);
  }

  void get_internal(const int64_t UNUSED(index), void *r_value) const override
  {
    this->cpp_type_.copy_to_uninitialized(value_, r_value);
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities for accessing attributes.
 * \{ */

static ReadAttributePtr read_attribute_from_custom_data(const CustomData &custom_data,
                                                        const int size,
                                                        const StringRef attribute_name,
                                                        const AttributeDomain domain)
{
  for (const CustomDataLayer &layer : Span(custom_data.layers, custom_data.totlayer)) {
    if (layer.name != nullptr && layer.name == attribute_name) {
      switch (layer.type) {
        case CD_PROP_FLOAT:
          return std::make_unique<ArrayReadAttribute<float>>(
              domain, Span(static_cast<float *>(layer.data), size));
        case CD_PROP_FLOAT2:
          return std::make_unique<ArrayReadAttribute<float2>>(
              domain, Span(static_cast<float2 *>(layer.data), size));
        case CD_PROP_FLOAT3:
          return std::make_unique<ArrayReadAttribute<float3>>(
              domain, Span(static_cast<float3 *>(layer.data), size));
        case CD_PROP_INT32:
          return std::make_unique<ArrayReadAttribute<int>>(
              domain, Span(static_cast<int *>(layer.data), size));
        case CD_PROP_COLOR:
          return std::make_unique<ArrayReadAttribute<Color4f>>(
              domain, Span(static_cast<Color4f *>(layer.data), size));
      }
    }
  }
  return {};
}

static WriteAttributePtr write_attribute_from_custom_data(CustomData custom_data,
                                                          const int size,
                                                          const StringRef attribute_name,
                                                          const AttributeDomain domain)
{
  for (const CustomDataLayer &layer : Span(custom_data.layers, custom_data.totlayer)) {
    if (layer.name != nullptr && layer.name == attribute_name) {
      switch (layer.type) {
        case CD_PROP_FLOAT:
          return std::make_unique<ArrayWriteAttribute<float>>(
              domain, MutableSpan(static_cast<float *>(layer.data), size));
        case CD_PROP_FLOAT2:
          return std::make_unique<ArrayWriteAttribute<float2>>(
              domain, MutableSpan(static_cast<float2 *>(layer.data), size));
        case CD_PROP_FLOAT3:
          return std::make_unique<ArrayWriteAttribute<float3>>(
              domain, MutableSpan(static_cast<float3 *>(layer.data), size));
        case CD_PROP_INT32:
          return std::make_unique<ArrayWriteAttribute<int>>(
              domain, MutableSpan(static_cast<int *>(layer.data), size));
        case CD_PROP_COLOR:
          return std::make_unique<ArrayWriteAttribute<Color4f>>(
              domain, MutableSpan(static_cast<Color4f *>(layer.data), size));
      }
    }
  }
  return {};
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Get Read/Write attributes for meshes.
 * \{ */

ReadAttributePtr mesh_attribute_get_for_read(const MeshComponent &mesh_component,
                                             const StringRef attribute_name)
{
  const Mesh *mesh = mesh_component.get_for_read();
  if (mesh == nullptr) {
    return {};
  }

  ReadAttributePtr corner_attribute = read_attribute_from_custom_data(
      mesh->ldata, mesh->totloop, attribute_name, ATTR_DOMAIN_CORNER);
  if (corner_attribute) {
    return corner_attribute;
  }

  if (attribute_name == "Position") {
    auto get_vertex_position = [](const MVert &vert) { return float3(vert.co); };
    return std::make_unique<
        DerivedArrayReadAttribute<MVert, float3, decltype(get_vertex_position)>>(
        ATTR_DOMAIN_VERTEX, Span(mesh->mvert, mesh->totvert), get_vertex_position);
  }

  const int vertex_group_index = mesh_component.vertex_group_index(attribute_name);
  if (vertex_group_index >= 0) {
    return std::make_unique<VertexWeightReadAttribute>(
        mesh->dvert, mesh->totvert, vertex_group_index);
  }

  ReadAttributePtr vertex_attribute = read_attribute_from_custom_data(
      mesh->vdata, mesh->totvert, attribute_name, ATTR_DOMAIN_VERTEX);
  if (vertex_attribute) {
    return vertex_attribute;
  }

  ReadAttributePtr edge_attribute = read_attribute_from_custom_data(
      mesh->edata, mesh->totedge, attribute_name, ATTR_DOMAIN_EDGE);
  if (edge_attribute) {
    return edge_attribute;
  }

  ReadAttributePtr polygon_attribute = read_attribute_from_custom_data(
      mesh->pdata, mesh->totpoly, attribute_name, ATTR_DOMAIN_POLYGON);
  if (polygon_attribute) {
    return polygon_attribute;
  }

  return {};
}

WriteAttributePtr mesh_attribute_get_for_write(MeshComponent &mesh_component,
                                               const StringRef attribute_name)
{
  Mesh *mesh = mesh_component.get_for_write();
  if (mesh == nullptr) {
    return {};
  }

  WriteAttributePtr corner_attribute = write_attribute_from_custom_data(
      mesh->ldata, mesh->totloop, attribute_name, ATTR_DOMAIN_CORNER);
  if (corner_attribute) {
    return corner_attribute;
  }

  if (attribute_name == "Position") {
    auto get_vertex_position = [](const MVert &vert) { return float3(vert.co); };
    auto set_vertex_position = [](MVert &vert, const float3 &co) { copy_v3_v3(vert.co, co); };
    return std::make_unique<DerivedArrayWriteAttribute<MVert,
                                                       float3,
                                                       decltype(get_vertex_position),
                                                       decltype(set_vertex_position)>>(
        ATTR_DOMAIN_VERTEX,
        MutableSpan(mesh->mvert, mesh->totvert),
        get_vertex_position,
        set_vertex_position);
  }

  const int vertex_group_index = mesh_component.vertex_group_index(attribute_name);
  if (vertex_group_index >= 0) {
    return std::make_unique<VertexWeightWriteAttribute>(
        mesh->dvert, mesh->totvert, vertex_group_index);
  }

  WriteAttributePtr vertex_attribute = write_attribute_from_custom_data(
      mesh->vdata, mesh->totvert, attribute_name, ATTR_DOMAIN_VERTEX);
  if (vertex_attribute) {
    return vertex_attribute;
  }

  WriteAttributePtr edge_attribute = write_attribute_from_custom_data(
      mesh->edata, mesh->totedge, attribute_name, ATTR_DOMAIN_EDGE);
  if (edge_attribute) {
    return edge_attribute;
  }

  WriteAttributePtr polygon_attribute = write_attribute_from_custom_data(
      mesh->pdata, mesh->totpoly, attribute_name, ATTR_DOMAIN_POLYGON);
  if (polygon_attribute) {
    return polygon_attribute;
  }

  return {};
}

ReadAttributePtr mesh_attribute_adapt_domain(const MeshComponent &UNUSED(mesh_component),
                                             ReadAttributePtr attribute,
                                             const AttributeDomain to_domain)
{
  if (!attribute) {
    return {};
  }
  const AttributeDomain from_domain = attribute->domain();
  if (from_domain == to_domain) {
    return attribute;
  }

  /* TODO: Actually implement domain interpolation. */
  return {};
}

static int get_domain_length(const MeshComponent &mesh_component, const AttributeDomain domain)
{
  const Mesh *mesh = mesh_component.get_for_read();
  if (mesh == nullptr) {
    return 0;
  }
  switch (domain) {
    case ATTR_DOMAIN_CORNER:
      return mesh->totloop;
    case ATTR_DOMAIN_VERTEX:
      return mesh->totvert;
    case ATTR_DOMAIN_EDGE:
      return mesh->totedge;
    case ATTR_DOMAIN_POLYGON:
      return mesh->totpoly;
    default:
      break;
  }
  return 0;
}

static ReadAttributePtr make_default_attribute(const MeshComponent &mesh_component,
                                               const AttributeDomain domain,
                                               const CPPType &cpp_type,
                                               const void *default_value)
{

  const int length = get_domain_length(mesh_component, domain);
  return std::make_unique<ConstantReadAttribute>(domain, length, cpp_type, default_value);
}

ReadAttributePtr mesh_attribute_get_for_read(const MeshComponent &mesh_component,
                                             const StringRef attribute_name,
                                             const CPPType &cpp_type,
                                             const AttributeDomain domain,
                                             const void *default_value)
{
  ReadAttributePtr attribute = mesh_attribute_get_for_read(mesh_component, attribute_name);
  auto get_default_or_empty = [&]() -> ReadAttributePtr {
    if (default_value != nullptr) {
      return make_default_attribute(mesh_component, domain, cpp_type, default_value);
    }
    return {};
  };

  if (!attribute) {
    return get_default_or_empty();
  }
  if (attribute->domain() != domain) {
    attribute = mesh_attribute_adapt_domain(mesh_component, std::move(attribute), domain);
  }
  if (!attribute) {
    return get_default_or_empty();
  }
  if (attribute->cpp_type() != cpp_type) {
    /* TODO: Support some type conversions. */
    return get_default_or_empty();
  }
  return attribute;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Get Read/Write attributes for point clouds.
 * \{ */

ReadAttributePtr pointcloud_attribute_get_for_read(const PointCloudComponent &pointcloud_component,
                                                   const StringRef attribute_name)
{
  const PointCloud *pointcloud = pointcloud_component.get_for_read();
  if (pointcloud == nullptr) {
    return {};
  }

  return read_attribute_from_custom_data(
      pointcloud->pdata, pointcloud->totpoint, attribute_name, ATTR_DOMAIN_POINT);
}

WriteAttributePtr pointcloud_attribute_get_for_write(
    const PointCloudComponent &pointcloud_component, const StringRef attribute_name)
{
  const PointCloud *pointcloud = pointcloud_component.get_for_read();
  if (pointcloud == nullptr) {
    return {};
  }

  return write_attribute_from_custom_data(
      pointcloud->pdata, pointcloud->totpoint, attribute_name, ATTR_DOMAIN_POINT);
}

static int get_domain_length(const PointCloudComponent &pointcloud_component)
{
  const PointCloud *pointcloud = pointcloud_component.get_for_read();
  if (pointcloud == nullptr) {
    return 0;
  }
  return pointcloud->totpoint;
}

ReadAttributePtr pointcloud_attribute_get_for_read(const PointCloudComponent &pointcloud_component,
                                                   const StringRef attribute_name,
                                                   const CPPType &cpp_type,
                                                   const void *default_value)
{
  ReadAttributePtr attribute = pointcloud_attribute_get_for_read(pointcloud_component,
                                                                 attribute_name);
  auto get_default_or_empty = [&]() -> ReadAttributePtr {
    if (default_value != nullptr) {
      const int length = get_domain_length(pointcloud_component);
      return std::make_unique<ConstantReadAttribute>(
          ATTR_DOMAIN_POINT, length, cpp_type, default_value);
    }
    return {};
  };

  if (!attribute) {
    return get_default_or_empty();
  }
  if (attribute->cpp_type() != cpp_type) {
    /* TODO: Support some type conversions. */
    return get_default_or_empty();
  }
  return attribute;
}

/** \} */

}  // namespace blender::bke
