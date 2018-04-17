
#pragma once

#include <RendererCore/Basics.h>
#include <Foundation/Reflection/Reflection.h>
#include <Foundation/Strings/HashedString.h>

struct ezEditableBoneGeometry
{
  typedef ezUInt8 StorageType;

  enum Enum
  {
    None = 0,
    Capsule,
    Sphere,
    Box,

    Default = None
  };
};

EZ_DECLARE_REFLECTABLE_TYPE(EZ_RENDERERCORE_DLL, ezEditableBoneGeometry);

class EZ_RENDERERCORE_DLL ezEditableSkeletonBone : public ezReflectedClass
{
  EZ_ADD_DYNAMIC_REFLECTION(ezEditableSkeletonBone, ezReflectedClass);

public:
  ezEditableSkeletonBone();
  ~ezEditableSkeletonBone();

  const char* GetName() const;
  void SetName(const char* sz);

  void ClearBones();

  // copies the properties for geometry etc. from another bone
  // does NOT copy the name, the transform or the children
  void CopyPropertiesFrom(const ezEditableSkeletonBone* pBone);

  ezHashedString m_sName;
  ezTransform m_Transform;
  ezEnum<ezEditableBoneGeometry> m_Geometry;

  float m_fLength = 0; // Box, Capsule, 0 means parent bone to this bone
  float m_fWidth = 0; // Box
  float m_fThickness = 0; // Sphere radius, Capsule radius

  ezHybridArray<ezEditableSkeletonBone*, 4> m_Children;
};

class EZ_RENDERERCORE_DLL ezEditableSkeleton : public ezReflectedClass
{
  EZ_ADD_DYNAMIC_REFLECTION(ezEditableSkeleton, ezReflectedClass );

public:
  ezEditableSkeleton();
  ~ezEditableSkeleton();

  void ClearBones();

  ezString m_sAnimationFile;

  ezEnum<ezBasisAxis> m_ForwardDir;
  ezEnum<ezBasisAxis> m_RightDir;
  ezEnum<ezBasisAxis> m_UpDir;

  ezHybridArray<ezEditableSkeletonBone*, 4> m_Children;
};

