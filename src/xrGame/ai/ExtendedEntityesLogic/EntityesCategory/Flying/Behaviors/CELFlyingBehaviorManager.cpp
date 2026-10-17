#include "stdafx.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Behaviors/CELFlyingBehaviorManager.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/CELFlying.h"
#include "ai/ExtendedEntityesLogic/Interfaces/IELAdapter.h"
#include "ai/monsters/basemonster/base_monster.h"

// Связывает выбор воздушных схем и реакцию на испуг с необходимыми службами и состоянием владельца.
CELFlyingBehaviorManager::CELFlyingBehaviorManager(CELFlying& OwnerLogic, IELAdapter& OwnerAdapter, const SELFlyingSettings& FlightSettings, CELFlyingMovement& FlightMovement)
	: Owner(OwnerLogic), Movement(FlightMovement), CruiseBehavior(OwnerAdapter, FlightSettings, FlightMovement) {}
// Завершает выбор воздушных схем и реакцию на испуг и освобождает связанные ресурсы.
CELFlyingBehaviorManager::~CELFlyingBehaviorManager()
{
	Clear();
}
// Сбрасывает накопленные данные и незавершённую работу подсистемы.
void CELFlyingBehaviorManager::Clear()
{
	CELSharedBehaviorManager::Clear();
	ActiveFlight = nullptr;
}
// Выбирает воздушную схему через общий механизм её жизненного цикла.
void CELFlyingBehaviorManager::SelectFlight(CELFlyingBehaviorBase& Behavior)
{
	Select(&Behavior);
	ActiveFlight = &Behavior;
}
// Обновляет активную воздушную схему и проверяет её состояние.
EELControl CELFlyingBehaviorManager::UpdateFlight(float DeltaTime, CELFlyingBehaviorBase* DesiredBehavior)
{
	if (!Owner.HasAttached || !Owner.Profile.CanUseBehaviors || !Owner.Adapter->CanControl())
	{
		Clear();
		Movement.Stop();
		return EELControl::Native;
	}
	const bool IsPanicking = s32(Device.dwTimeGlobal - Owner.Hearing.PanicUntil) < 0;
	const bool IsInvestigating = !DesiredBehavior && s32(Device.dwTimeGlobal - Owner.Hearing.InvestigateUntil) < 0;
	if (IsPanicking || IsInvestigating)
	{
		Owner.TargetIdentifier = u16(-1);
		CruiseBehavior.SetAttention(Owner.Hearing.AttentionPosition, IsPanicking);
		SelectFlight(CruiseBehavior);
	}
	else if (DesiredBehavior)
	{
		SelectFlight(*DesiredBehavior);
	}
	else if (Owner.FlightSettings.CanWander)
	{
		CruiseBehavior.ClearAttention();
		SelectFlight(CruiseBehavior);
	}
	else
	{
		Clear();
		Movement.Stop();
		return EELControl::Extended;
	}
	Update(DeltaTime);
	return EELControl::Extended;
}
// Выполняет покадровую часть активной схемы без повторного выбора поведения.
void CELFlyingBehaviorManager::FrameUpdate(float DeltaTime)
{
	if (ActiveFlight && IsActive(*ActiveFlight))
	{
		ActiveFlight->FrameUpdate(DeltaTime);
	}
}
// Возвращает текущую точку назначения активной схемы.
Fvector CELFlyingBehaviorManager::Goal() const
{
	return ActiveFlight && IsActive(*ActiveFlight) ? ActiveFlight->Goal() : Owner.Adapter->Monster().Position();
}
