#pragma once

#include <Core/World/World.h>
#include <Core/World/GameObject.h>
#include <GameUtils/DataStructures/GameGrid.h>

class UnitComponent;
typedef ezComponentManagerSimple<UnitComponent> UnitComponentManager;

struct UnitType
{
  enum Enum
  {
    Default,
  };
};

class UnitComponent : public ezComponent
{
  EZ_DECLARE_COMPONENT_TYPE(UnitComponent, UnitComponentManager);

public:
  UnitComponent();

  

  UnitType::Enum GetUnitType() const { return m_UnitType; }
  void SetUnitType(UnitType::Enum type) { m_UnitType = type; }

  ezDeque<ezVec3> m_Path;

  ezInt32 m_iCurDirection;

private:
  // Called by UnitComponentManager
  virtual void Update();

  void MoveAlongPath();

  UnitType::Enum m_UnitType;
  ezVec2I32 m_GridCoordinate;
  
};

