#include "stdafx.h"
#include <cmath>
#include <algorithm>
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Entityes/Aggressive/CELFlyingAttackAnimation.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Entityes/Aggressive/Behaviors/CELFlyingAttackBehavior.h"
#include "ai/ExtendedEntityesLogic/Interfaces/IELAdapter.h"
#include "ai/ExtendedEntityesLogic/EntryPoint.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Navigation/CELFlyingGeometry.h"
#include "ai/monsters/basemonster/base_monster.h"
#include "ai/ExtendedEntityesLogic/Debug/Debug.h"

// Связывает фазы налёта с движением и проверкой фактического контакта.
CELFlyingAttackBehavior::CELFlyingAttackBehavior(IELAdapter& OwnerAdapter, const SELFlyingSettings& FlightSettings, CELFlyingMovement& FlightMovement, CELFlyingAttackAnimation& FlightAnimation)
	: CELFlyingBehaviorBase(OwnerAdapter, FlightSettings, FlightMovement), Animation(FlightAnimation) {}

// Снимает разрешение на контакт с целью и освобождает заявки схемы.
void CELFlyingAttackBehavior::Exit()
{
	Movement.SetContactTarget(u16(-1));
	Animation.Fly();
	CELFlyingBehaviorBase::Exit();
}

// Выбирает индивидуальное направление и размеры захода без общего для стаи угла.
void CELFlyingAttackBehavior::Enter()
{
	CELFlyingBehaviorBase::Enter();
	Animation.Fly();
	Phase = PlannedPhase = ActivePhase = EELAttackPhase::Climb;
	RefreshTarget();
	const Fvector Offset = Fvector().sub(Adapter.Monster().Position(), AimPosition);
	OrbitAngle = std::atan2(Offset.x, Offset.z) + Random.randF(-PI_DIV_4, PI_DIV_4);
	TurnSide = Random.randI(2) ? 1.f : -1.f;
	PassRadius = Settings.OrbitRadius * Random.randF(Settings.AttackApproachRatio, 1.f);
	PassHeight = Settings.Altitude * Random.randF(Settings.AttackApproachHeightRatio, 1.f);
	LastCorrectionAt = 0;
	IsDivePass = Random.randF() < Settings.AttackDiveProbability;
}

// При смене противника отменяет прежний налёт, сохраняя короткий безопасный участок движения.
void CELFlyingAttackBehavior::SetObservation(const SELObservation* TargetObservation)
{
	const bool HasNewTarget = TargetObservation && (!HasTarget || Observation.TargetIdentifier != TargetObservation->TargetIdentifier);
	if (HasNewTarget)
	{
		Movement.SetContactTarget(u16(-1));
		Movement.RetainRoutePrefix(std::max(Settings.CollisionRadius * 2.f, Movement.GetSpeed() * Settings.InterruptLookahead));
		Navigation.Cancel();
		HasPreparedRoute = false;
		Phase = PlannedPhase = ActivePhase = EELAttackPhase::Climb;
		TargetSampleAt = 0;
		TargetVelocity.set(0.f, 0.f, 0.f);
		AimPosition = TargetObservation->Position;
	}
	CELFlyingBehaviorBase::SetObservation(TargetObservation);
	RefreshTarget();
}

// Уточняет центр только видимой цели; скрытая цель остаётся в последней известной точке.
void CELFlyingAttackBehavior::RefreshTarget()
{
	CEntityAlive* TargetEntity = HasTarget ? ExtendedEntityes::FindEntity(Observation.TargetIdentifier) : nullptr;
	HasVisibleTarget = TargetEntity && Adapter.IsEnemy(*TargetEntity) && Adapter.CanSee(*TargetEntity);
	if (!HasVisibleTarget)
	{
		TargetVelocity.set(0.f, 0.f, 0.f);
		TargetSampleAt = 0;
		return;
	}
	Fvector CurrentCenter;
	TargetEntity->Center(CurrentCenter);
	const u32 CurrentTime = Device.dwTimeGlobal;
	if (TargetSampleAt && CurrentTime > TargetSampleAt)
	{
		TargetVelocity.sub(CurrentCenter, AimPosition).mul(1000.f / float(CurrentTime - TargetSampleAt));
	}
	AimPosition = CurrentCenter;
	Observation.Position = TargetEntity->Position();
	TargetSampleAt = CurrentTime;
}

// Прерывает устаревшее пикирование до окончания старого пути, не меняя проверенные точки на лету.
void CELFlyingAttackBehavior::CorrectDive()
{
	if (HasHitPass || ActivePhase != EELAttackPhase::Dive || Device.dwTimeGlobal - LastCorrectionAt < Settings.RouteRetryDelay)
	{
		return;
	}
	if (HasVisibleTarget && ActiveTargetPosition.distance_to_sqr(AimPosition) <= _sqr(Settings.AttackReplanDistance))
	{
		return;
	}
	Navigation.Cancel();
	HasPreparedRoute = false;
	Movement.RetainRoutePrefix(std::max(Settings.CollisionRadius * 2.f, Movement.GetSpeed() * Settings.InterruptLookahead));
	Phase = HasVisibleTarget ? EELAttackPhase::Dive : EELAttackPhase::Exit;
	ActiveTargetPosition = AimPosition;
	LastCorrectionAt = Device.dwTimeGlobal;
	RetryAfter = 0;
	if (!HasVisibleTarget)
	{
		ActivePhase = EELAttackPhase::Exit;
		Movement.SetContactTarget(u16(-1));
	}
}

// Уточняет цель до приёма фонового результата и планирует следующую фазу.
void CELFlyingAttackBehavior::Update(float DeltaTime)
{
	RefreshTarget();
	CorrectDive();
	CELFlyingBehaviorBase::Update(DeltaTime);
	if (Validation.State == EELBehaviorState::Running)
	{
		switch (ActivePhase)
		{
			case EELAttackPhase::Climb: Validation.Reason = "Набор высоты перед налётом"; break;
			case EELAttackPhase::Approach: Validation.Reason = "Снижающийся заход на цель"; break;
			case EELAttackPhase::Dive: Validation.Reason = IsDivePass ? "Пикирование к точке контакта" : "Атакующий полувираж"; break;
			case EELAttackPhase::Exit: Validation.Reason = "Выход из налёта"; break;
		}
	}
}

// Проверяет удар до переключения на выход; актуальные координаты читаются только игровым обновлением.
void CELFlyingAttackBehavior::FrameUpdate(float DeltaTime)
{
	RefreshTarget();
	CorrectDive();
	if (HasVisibleTarget && !HasHitPass && ActivePhase == EELAttackPhase::Dive)
	{
		HasHitPass = Attack.TryHit(Adapter, Settings, Observation.TargetIdentifier);
	}
	if (HasVisibleTarget && CELSharedDebug::Instance().IsDrawEnabled())
	{
		SELDebugEvent Event;
		Event.Kind = EELDebugEvent::Search;
		Event.OwnerIdentifier = Adapter.Monster().ID();
		Event.TargetIdentifier = Observation.TargetIdentifier;
		Adapter.Monster().XFORM().transform_tiny(Event.Start, CELFlyingAttack::LocalContactPoint(Adapter.Monster()));
		Event.Finish = AimPosition;
		Event.Radius = Settings.AttackDistance;
		Event.Label = "Центр видимой цели налёта";
		CELSharedDebug::Instance().PublishEvent(std::move(Event));
	}
	CELFlyingBehaviorBase::FrameUpdate(DeltaTime);
}

// Строит боковой выход с набором высоты относительно собственной позиции летуна.
void CELFlyingAttackBehavior::ChooseDeparture(const Fvector& Origin)
{
	if (Phase == EELAttackPhase::Exit)
	{
		TurnSide = Random.randI(2) ? 1.f : -1.f;
	}
	OrbitAngle += TurnSide * PI_DIV_2;
	GoalPosition = AimPosition;
	GoalPosition.x += std::sin(OrbitAngle) * PassRadius;
	GoalPosition.z += std::cos(OrbitAngle) * PassRadius;
	float SurfaceHeight = Observation.Position.y;
	CELFlyingGeometry::Surface(GoalPosition, SurfaceHeight);
	GoalPosition.y = std::max(SurfaceHeight + PassHeight, Origin.y);
	GoalDirection.set(TurnSide * std::cos(OrbitAngle), Settings.AttackPulloutPitch, -TurnSide * std::sin(OrbitAngle));
}

// Направляет промежуточный заход к цели с реальным снижением от высоты виража.
void CELFlyingAttackBehavior::ChooseApproach(const Fvector& Origin)
{
	Fvector Departure;
	Departure.sub(Origin, AimPosition);
	Departure.y = 0.f;
	if (Departure.square_magnitude() < EPS_L)
	{
		Departure.set(std::sin(OrbitAngle), 0.f, std::cos(OrbitAngle));
	}
	Departure.normalize_safe();
	GoalPosition.mad(AimPosition, Departure, PassRadius * Settings.AttackApproachRatio);
	const float Height = std::max(0.f, Origin.y + CELFlyingAttack::LocalContactPoint(Adapter.Monster()).y - AimPosition.y);
	GoalPosition.y = AimPosition.y + (IsDivePass ? Height * Settings.AttackApproachHeightRatio : std::min(Height, Settings.CollisionRadius + Settings.GroundClearance));
	GoalDirection.sub(AimPosition, GoalPosition);
	if (!IsDivePass)
	{
		GoalDirection.x += TurnSide * Departure.z * PassRadius * Settings.AttackArcSideRatio;
		GoalDirection.z -= TurnSide * Departure.x * PassRadius * Settings.AttackArcSideRatio;
		GoalDirection.y = 0.f;
	}
	GoalDirection.normalize_safe();
	GoalPosition = CELFlyingAttack::RootAtContact(Adapter.Monster(), GoalPosition, GoalDirection);
}

// Назначает контакт у центра цели с ограниченным упреждением и зазором под реальное тело летуна.
void CELFlyingAttackBehavior::ChooseContact(const Fvector& Origin)
{
	const float TravelTime = Origin.distance_to(AimPosition) / std::max(Settings.Speed, Movement.GetSpeed());
	Fvector Lead;
	Lead.mul(TargetVelocity, std::min(Settings.AttackLeadTime, TravelTime));
	if (Lead.magnitude() > Settings.OrbitRadius)
	{
		Lead.normalize_safe().mul(Settings.OrbitRadius);
	}
	const Fvector ContactPoint = Fvector().add(AimPosition, Lead);
	GoalDirection.sub(ContactPoint, Origin);
	GoalDirection.y = 0.f;
	GoalDirection.normalize_safe();
	GoalPosition = CELFlyingAttack::RootAtContact(Adapter.Monster(), ContactPoint, GoalDirection);
	float FloorHeight = Observation.Position.y;
	Fvector SupportPoint;
	Fvector SupportNormal;
	Fvector SupportOrigin = GoalPosition;
	SupportOrigin.y = ContactPoint.y;
	if (CELFlyingGeometry::QuerySupport(SupportOrigin, Settings.Altitude + Settings.CollisionCenterHeight, SupportPoint, SupportNormal, &Adapter.Monster()) == EELGeometryResult::Clear)
	{
		FloorHeight = SupportPoint.y;
	}
	GoalPosition.y = std::max(GoalPosition.y, FloorHeight + Settings.CollisionRadius - Settings.CollisionCenterHeight + Settings.GroundClearance);
}

// Формирует отдельную цель фазы и сохраняет снимок, относительно которого проверяется готовый маршрут.
void CELFlyingAttackBehavior::ChooseGoal()
{
	if (Phase == EELAttackPhase::Dive && !HasVisibleTarget)
	{
		Phase = EELAttackPhase::Exit;
	}
	PlannedPhase = Phase;
	PlannedTargetPosition = AimPosition;
	const Fvector Origin = Movement.HasRoute() ? Movement.EndPosition() : Adapter.Monster().Position();
	switch (Phase)
	{
		case EELAttackPhase::Climb:
		case EELAttackPhase::Exit: ChooseDeparture(Origin); break;
		case EELAttackPhase::Approach: ChooseApproach(Origin); break;
		case EELAttackPhase::Dive: ChooseContact(Origin); break;
	}
	const Fbox Bounds = CELFlyingGeometry::AirBounds(Settings.BoundaryMargin, Settings.CollisionRadius, Settings.CollisionCenterHeight);
	clamp(GoalPosition.x, Bounds.min.x, Bounds.max.x);
	clamp(GoalPosition.y, Bounds.min.y, Bounds.max.y);
	clamp(GoalPosition.z, Bounds.min.z, Bounds.max.z);
}

// Ограничивает плечо пикирования, чтобы начальная касательная не уводила кривую ниже контакта.
void CELFlyingAttackBehavior::ConfigureCurveRequest(SELNavigationRequest& Request)
{
	if (PlannedPhase != EELAttackPhase::Dive || Request.StartDirection.y >= -EPS_L)
	{
		return;
	}
	const float DescentHeight = Request.StartPosition.y - Request.GoalPosition.y;
	if (DescentHeight > EPS_L)
	{
		Request.CurveDistance = std::min(Request.CurveDistance, DescentHeight / -Request.StartDirection.y);
	}
}

// Отклоняет результат для сместившейся цели до передачи маршрута движению.
bool CELFlyingAttackBehavior::CanActivatePreparedRoute()
{
	RefreshTarget();
	if (PlannedPhase == EELAttackPhase::Climb || PlannedPhase == EELAttackPhase::Exit)
	{
		return true;
	}
	if (PlannedPhase == EELAttackPhase::Dive && !HasVisibleTarget)
	{
		Phase = EELAttackPhase::Exit;
		return false;
	}
	const float AllowedMovement = PlannedPhase == EELAttackPhase::Approach ? std::max(Settings.AttackReplanDistance, PassRadius * Settings.AttackApproachRatio) : Settings.AttackReplanDistance + TargetVelocity.magnitude() * Settings.AttackLeadTime;
	const Fvector& ExpectedPosition = PlannedTargetPosition;
	if (ExpectedPosition.distance_to_sqr(AimPosition) > _sqr(AllowedMovement))
	{
		Validation = {EELBehaviorState::WaitingForResult, "Цель сместилась: уточнение налёта"};
		return false;
	}
	return true;
}

// Переключает фазу только при фактическом начале движения по принятому маршруту.
void CELFlyingAttackBehavior::OnRouteActivated()
{
	ActivePhase = PlannedPhase;
	ActiveTargetPosition = AimPosition;
	HasHitPass = false;
	Movement.SetContactTarget(ActivePhase == EELAttackPhase::Dive ? Observation.TargetIdentifier : u16(-1));
	if (ActivePhase == EELAttackPhase::Dive)
	{
		Animation.Attack();
	}
	else
	{
		Animation.Fly();
	}
	switch (ActivePhase)
	{
		case EELAttackPhase::Climb: Phase = EELAttackPhase::Approach; break;
		case EELAttackPhase::Exit:
			IsDivePass = Random.randF() < Settings.AttackDiveProbability;
			Phase = EELAttackPhase::Approach;
			break;
		case EELAttackPhase::Approach: Phase = EELAttackPhase::Dive; break;
		case EELAttackPhase::Dive: Phase = EELAttackPhase::Exit; break;
	}
}

// Не продолжает схему для исчезнувшего, погибшего или запрещённого противника.
SELBehaviorValidation CELFlyingAttackBehavior::Validate() const
{
	CEntityAlive* TargetEntity = HasTarget ? ExtendedEntityes::FindEntity(Observation.TargetIdentifier) : nullptr;
	if (!TargetEntity || !Adapter.IsEnemy(*TargetEntity))
	{
		return {EELBehaviorState::InvalidTarget, "Цель налёта недоступна"};
	}
	return CELFlyingBehaviorBase::Validate();
}

// Освобождает отказавший налёт для штатной схемы воздушного восстановления.
EELRecovery CELFlyingAttackBehavior::Recover(const SELBehaviorValidation& Failure)
{
	return EELRecovery::Release;
}
