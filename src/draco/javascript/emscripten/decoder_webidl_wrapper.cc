// Copyright 2016 The Draco Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "draco/javascript/emscripten/decoder_webidl_wrapper.h"

#include "draco/compression/decode.h"
#include "draco/mesh/mesh.h"
#include "draco/mesh/mesh_stripifier.h"

using draco::DecoderBuffer;
using draco::Mesh;
using draco::Metadata;
using draco::PointAttribute;
using draco::PointCloud;
using draco::Status;

MetadataQuerier::MetadataQuerier() : entry_names_metadata_(nullptr) {}

bool MetadataQuerier::HasEntry(const Metadata &metadata,
                               const char *entry_name) const {
  return metadata.entries().count(entry_name) > 0;
}

long MetadataQuerier::GetIntEntry(const Metadata &metadata,
                                  const char *entry_name) const {
  int32_t value = 0;
  const std::string name(entry_name);
  metadata.GetEntryInt(name, &value);
  return value;
}

void MetadataQuerier::GetIntEntryArray(const draco::Metadata &metadata,
                                       const char *entry_name,
                                       DracoInt32Array *out_values) const {
  const std::string name(entry_name);
  std::vector<int32_t> values;
  metadata.GetEntryIntArray(name, &values);
  out_values->MoveData(std::move(values));
}

double MetadataQuerier::GetDoubleEntry(const Metadata &metadata,
                                       const char *entry_name) const {
  double value = 0;
  const std::string name(entry_name);
  metadata.GetEntryDouble(name, &value);
  return value;
}

const char *MetadataQuerier::GetStringEntry(const Metadata &metadata,
                                            const char *entry_name) {
  const std::string name(entry_name);
  if (!metadata.GetEntryString(name, &last_string_returned_)) return nullptr;

  const char *value = last_string_returned_.c_str();
  return value;
}

long MetadataQuerier::NumEntries(const Metadata &metadata) const {
  return metadata.num_entries();
}

const char *MetadataQuerier::GetEntryName(const Metadata &metadata,
                                          int entry_id) {
  if (entry_names_metadata_ != &metadata) {
    entry_names_.clear();
    entry_names_metadata_ = &metadata;
    // Initialize the list of entry names.
    for (auto &&entry : metadata.entries()) {
      entry_names_.push_back(entry.first);
    }
  }
  if (entry_id < 0 || entry_id >= entry_names_.size()) return nullptr;
  return entry_names_[entry_id].c_str();
}

Decoder::Decoder() {}

draco_EncodedGeometryType Decoder::GetEncodedGeometryType(
    DecoderBuffer *in_buffer) {
  return draco::Decoder::GetEncodedGeometryType(in_buffer).value();
}

const Status *Decoder::DecodeBufferToPointCloud(DecoderBuffer *in_buffer,
                                                PointCloud *out_point_cloud) {
  last_status_ = decoder_.DecodeBufferToGeometry(in_buffer, out_point_cloud);
  return &last_status_;
}

const Status *Decoder::DecodeBufferToMesh(DecoderBuffer *in_buffer,
                                          Mesh *out_mesh) {
  last_status_ = decoder_.DecodeBufferToGeometry(in_buffer, out_mesh);
  return &last_status_;
}

long Decoder::GetAttributeId(const PointCloud &pc,
                             draco_GeometryAttribute_Type type) const {
  return pc.GetNamedAttributeId(type);
}

const PointAttribute *Decoder::GetAttribute(const PointCloud &pc, long att_id) {
  return pc.attribute(att_id);
}

const PointAttribute *Decoder::GetAttributeByUniqueId(const PointCloud &pc,
                                                      long unique_id) {
  return pc.GetAttributeByUniqueId(unique_id);
}

long Decoder::GetAttributeIdByName(const PointCloud &pc,
                                   const char *attribute_name) {
  const std::string entry_value(attribute_name);
  return pc.GetAttributeIdByMetadataEntry("name", entry_value);
}

long Decoder::GetAttributeIdByMetadataEntry(const PointCloud &pc,
                                            const char *metadata_name,
                                            const char *metadata_value) {
  const std::string entry_name(metadata_name);
  const std::string entry_value(metadata_value);
  return pc.GetAttributeIdByMetadataEntry(entry_name, entry_value);
}

bool Decoder::GetFaceFromMesh(const Mesh &m,
                              draco::FaceIndex::ValueType face_id,
                              DracoInt32Array *out_values) {
  const Mesh::Face &face = m.face(draco::FaceIndex(face_id));
  const auto ptr = reinterpret_cast<const int32_t *>(face.data());
  out_values->MoveData(std::vector<int32_t>({ptr, ptr + face.size()}));
  return true;
}

long Decoder::GetTriangleStripsFromMesh(const Mesh &m,
                                        DracoInt32Array *strip_values) {
  draco::MeshStripifier stripifier;
  std::vector<int32_t> strip_indices;
  if (!stripifier.GenerateTriangleStripsWithDegenerateTriangles(
          m, std::back_inserter(strip_indices))) {
    return 0;
  }
  strip_values->MoveData(std::move(strip_indices));
  return stripifier.num_strips();
}

template <typename T>
bool GetTrianglesArray(const draco::Mesh &m, T *out_values,
                       const int out_size) {
  const uint32_t num_faces = m.num_faces();
  if (num_faces * 3 != out_size) {
    return false;
  }

  for (uint32_t face_id = 0; face_id < num_faces; ++face_id) {
    const Mesh::Face &face = m.face(draco::FaceIndex(face_id));
    if (face.size() != 3 || face[0].value() > std::numeric_limits<T>::max() ||
        face[1].value() > std::numeric_limits<T>::max() ||
        face[2].value() > std::numeric_limits<T>::max()) {
      return false;
    }

    out_values[face_id * 3 + 0] = static_cast<T>(face[0].value());
    out_values[face_id * 3 + 1] = static_cast<T>(face[1].value());
    out_values[face_id * 3 + 2] = static_cast<T>(face[2].value());
  }
  return true;
}

bool Decoder::GetTrianglesUInt16Array(const draco::Mesh &m, void *out_values,
                                      const int out_size) {
  return GetTrianglesArray<uint16_t>(
      m, reinterpret_cast<uint16_t *>(out_values), out_size);
}

bool Decoder::GetTrianglesUInt32Array(const draco::Mesh &m, void *out_values,
                                      const int out_size) {
  return GetTrianglesArray<uint32_t>(
      m, reinterpret_cast<uint32_t *>(out_values), out_size);
}

bool Decoder::GetAttributeFloat(const PointAttribute &pa,
                                draco::AttributeValueIndex::ValueType val_index,
                                DracoFloat32Array *out_values) {
  const int kMaxAttributeFloatValues = 4;
  const int components = pa.num_components();
  float values[kMaxAttributeFloatValues] = {-2.0, -2.0, -2.0, -2.0};
  if (!pa.ConvertValue<float>(draco::AttributeValueIndex(val_index), values))
    return false;
  out_values->MoveData({values, values + components});
  return true;
}

bool Decoder::GetAttributeFloatForAllPoints(const PointCloud &pc,
                                            const PointAttribute &pa,
                                            DracoFloat32Array *out_values) {
  const int components = pa.num_components();
  const int num_points = pc.num_points();
  const int num_entries = num_points * components;
  const int kMaxAttributeFloatValues = 4;
  float values[kMaxAttributeFloatValues] = {-2.0, -2.0, -2.0, -2.0};
  int entry_id = 0;

  out_values->Resize(num_entries);
  for (draco::PointIndex i(0); i < num_points; ++i) {
    const draco::AttributeValueIndex val_index = pa.mapped_index(i);
    if (!pa.ConvertValue<float>(val_index, values)) return false;
    for (int j = 0; j < components; ++j) {
      out_values->SetValue(entry_id++, values[j]);
    }
  }
  return true;
}

bool Decoder::GetAttributeFloatArrayForAllPoints(const PointCloud &pc,
                                                 const PointAttribute &pa,
                                                 void *out_values,
                                                 const int out_size) {
  const int components = pa.num_components();
  const int num_points = pc.num_points();
  const int num_entries = num_points * components;
  if (num_entries != out_size) {
    return false;
  }

  const int kMaxAttributeFloatValues = 4;
  float values[kMaxAttributeFloatValues] = {-2.0, -2.0, -2.0, -2.0};
  int entry_id = 0;
  float *floats = reinterpret_cast<float *>(out_values);

  for (draco::PointIndex i(0); i < num_points; ++i) {
    const draco::AttributeValueIndex val_index = pa.mapped_index(i);
    if (!pa.ConvertValue<float>(val_index, values)) return false;
    for (int j = 0; j < components; ++j) {
      floats[entry_id++] = values[j];
    }
  }
  return true;
}

bool Decoder::GetAttributeInt8ForAllPoints(const PointCloud &pc,
                                           const PointAttribute &pa,
                                           DracoInt8Array *out_values) {
  return GetAttributeDataForAllPoints<DracoInt8Array, int8_t>(
      pc, pa, draco::DT_INT8, draco::DT_UINT8, out_values);
}

bool Decoder::GetAttributeUInt8ForAllPoints(const PointCloud &pc,
                                            const PointAttribute &pa,
                                            DracoUInt8Array *out_values) {
  return GetAttributeDataForAllPoints<DracoUInt8Array, uint8_t>(
      pc, pa, draco::DT_INT8, draco::DT_UINT8, out_values);
}

bool Decoder::GetAttributeUInt8ArrayForAllPoints(const PointCloud &pc,
                                                 const PointAttribute &pa,
                                                 void *out_values,
                                                 const int out_size) {
  return GetAttributeDataArrayForAllPoints<uint8_t>(pc, pa, draco::DT_UINT8,
                                                    out_values, out_size);
}

bool Decoder::GetAttributeInt16ForAllPoints(const PointCloud &pc,
                                            const PointAttribute &pa,
                                            DracoInt16Array *out_values) {
  return GetAttributeDataForAllPoints<DracoInt16Array, int16_t>(
      pc, pa, draco::DT_INT16, draco::DT_UINT16, out_values);
}

bool Decoder::GetAttributeUInt16ForAllPoints(const PointCloud &pc,
                                             const PointAttribute &pa,
                                             DracoUInt16Array *out_values) {
  return GetAttributeDataForAllPoints<DracoUInt16Array, uint16_t>(
      pc, pa, draco::DT_INT16, draco::DT_UINT16, out_values);
}

bool Decoder::GetAttributeUInt16ArrayForAllPoints(const PointCloud &pc,
                                                  const PointAttribute &pa,
                                                  void *out_values,
                                                  const int out_size) {
  return GetAttributeDataArrayForAllPoints<uint16_t>(pc, pa, draco::DT_UINT16,
                                                     out_values, out_size);
}

bool Decoder::GetAttributeInt32ForAllPoints(const PointCloud &pc,
                                            const PointAttribute &pa,
                                            DracoInt32Array *out_values) {
  return GetAttributeDataForAllPoints<DracoInt32Array, int32_t>(
      pc, pa, draco::DT_INT32, draco::DT_UINT32, out_values);
}

bool Decoder::GetAttributeIntForAllPoints(const PointCloud &pc,
                                          const PointAttribute &pa,
                                          DracoInt32Array *out_values) {
  return GetAttributeInt32ForAllPoints(pc, pa, out_values);
}

bool Decoder::GetAttributeUInt32ForAllPoints(const PointCloud &pc,
                                             const PointAttribute &pa,
                                             DracoUInt32Array *out_values) {
  return GetAttributeDataForAllPoints<DracoUInt32Array, uint32_t>(
      pc, pa, draco::DT_INT32, draco::DT_UINT32, out_values);
}

bool Decoder::GetAttributeUInt32ArrayForAllPoints(const PointCloud &pc,
                                                  const PointAttribute &pa,
                                                  void *out_values,
                                                  const int out_size) {
  return GetAttributeDataArrayForAllPoints<uint32_t>(pc, pa, draco::DT_UINT32,
                                                     out_values, out_size);
}

void Decoder::SkipAttributeTransform(draco_GeometryAttribute_Type att_type) {
  decoder_.SetSkipAttributeTransform(att_type);
}

const Metadata *Decoder::GetMetadata(const PointCloud &pc) const {
  return pc.GetMetadata();
}

const Metadata *Decoder::GetAttributeMetadata(const PointCloud &pc,
                                              long att_id) const {
  return pc.GetAttributeMetadataByAttributeId(att_id);
}
