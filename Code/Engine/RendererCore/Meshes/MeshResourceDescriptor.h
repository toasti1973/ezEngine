#pragma once

#include <Foundation/IO/Stream.h>
#include <Foundation/Math/BoundingBoxSphere.h>
#include <RendererCore/AnimationSystem/SkeletonResource.h>
#include <RendererCore/Meshes/MeshBufferResource.h>

class EZ_RENDERERCORE_DLL ezMeshResourceDescriptor
{
public:
  struct SubMesh
  {
    EZ_DECLARE_POD_TYPE();

    ezUInt32 m_uiPrimitiveCount;
    ezUInt32 m_uiFirstPrimitive;
    ezUInt32 m_uiMaterialIndex;

    ezBoundingBoxSphere m_Bounds;
  };

  struct Material
  {
    ezString m_sPath;
  };

  ezMeshResourceDescriptor();

  void Clear();

  ezMeshBufferResourceDescriptor& MeshBufferDesc();

  const ezMeshBufferResourceDescriptor& MeshBufferDesc() const;

  void UseExistingMeshBuffer(const ezMeshBufferResourceHandle& hBuffer);

  void AddSubMesh(ezUInt32 uiPrimitiveCount, ezUInt32 uiFirstPrimitive, ezUInt32 uiMaterialIndex);

  void SetMaterial(ezUInt32 uiMaterialIndex, const char* szPathToMaterial);

  void Save(ezStreamWriter& stream);
  ezResult Save(const char* szFile);

  ezResult Load(ezStreamReader& stream);
  ezResult Load(const char* szFile);

  const ezMeshBufferResourceHandle& GetExistingMeshBuffer() const;

  ezArrayPtr<const Material> GetMaterials() const;

  ezArrayPtr<const SubMesh> GetSubMeshes() const;

  void ComputeBounds();
  const ezBoundingBoxSphere& GetBounds() const;

  struct BoneData
  {
    ezMat4 m_GlobalInverseBindPoseMatrix;
    ezUInt16 m_uiBoneIndex = ezInvalidJointIndex;

    ezResult Serialize(ezStreamWriter& stream) const;
    ezResult Deserialize(ezStreamReader& stream);
  };

  ezSkeletonResourceHandle m_hDefaultSkeleton;
  ezHashTable<ezHashedString, BoneData> m_Bones;

private:
  ezHybridArray<Material, 8> m_Materials;
  ezHybridArray<SubMesh, 8> m_SubMeshes;
  ezMeshBufferResourceDescriptor m_MeshBufferDescriptor;
  ezMeshBufferResourceHandle m_hMeshBuffer;
  ezBoundingBoxSphere m_Bounds;
};
