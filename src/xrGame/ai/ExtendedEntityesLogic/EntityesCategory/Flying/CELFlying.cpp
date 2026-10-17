#include "stdafx.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/CELFlying.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Behaviors/CELFlyingBehaviorBase.h"
#include "ai/ExtendedEntityesLogic/Debug/Debug.h"
#include "ai/monsters/basemonster/base_monster.h"

// Связывает общую логику воздушной категории с необходимыми службами и состоянием владельца.
CELFlying::CELFlying(xr_unique_ptr<IELAdapter> OwnerAdapter, const SELPerceptionProfile& OwnerProfile) : CELComponentBase(std::move(OwnerAdapter), OwnerProfile), FlightSettings(CELFlyingSettings::Read(*Adapter->Monster().cNameSect())), Movement(*Adapter, FlightSettings), BehaviorManager(*this, *Adapter, FlightSettings, Movement) {}
// Завершает общую логику воздушной категории и освобождает связанные ресурсы.
CELFlying::~CELFlying()
{
	ClearBehavior();
}
// Завершает выбранную схему и очищает управление категории.
void CELFlying::ClearBehavior()
{
	BehaviorManager.Clear();
}
// Выбирает воздушную схему через общий механизм её жизненного цикла.
void CELFlying::SelectFlight(CELFlyingBehaviorBase& Behavior)
{
	BehaviorManager.SelectFlight(Behavior);
}
// Снимает управление расширения и освобождает регистрации владельца.
void CELFlying::Detach()
{
	ClearBehavior();
	Movement.Stop();
	CELComponentBase::Detach();
}
// Обновляет общую логику воздушной категории и обрабатывает текущий результат выполнения.
EELControl CELFlying::Update(float DeltaTime)
{
	return BehaviorManager.UpdateFlight(DeltaTime);
}

// Возвращает цель для отображения в общей отладке.
Fvector CELFlying::DebugGoal() const
{
	return BehaviorManager.Goal();
}
// Выполняет покадровую часть активной схемы без повторного выбора поведения.
void CELFlying::FrameUpdate(float DeltaTime)
{
	if (!HasAttached || !Adapter->CanControl())
	{
		return;
	}
	Movement.Update(DeltaTime);
	BehaviorManager.FrameUpdate(DeltaTime);
	if (!CELSharedDebug::Instance().IsEnabled())
	{
		return;
	}
	SELDebugRecord Record;
	Record.OwnerIdentifier = Adapter->Monster().ID();
	Record.Position = Adapter->Monster().Position();
	Record.GoalPosition = DebugGoal();
	Record.Speed = Movement.GetSpeed();
	Record.TargetIdentifier = TargetIdentifier;
	Record.Radius = FlightSettings.CollisionRadius;
	Record.IsAttacking = IsAttacking();
	Record.KnownTargets = u32(Memory.GetRecords().size());
	Record.Validation = BehaviorManager.GetValidation().State;
	Record.State = BehaviorManager.GetValidation().Reason;
	CELSharedDebug::Instance().Publish(std::move(Record));
}
