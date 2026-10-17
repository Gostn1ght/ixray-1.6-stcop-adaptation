#include "stdafx.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Entityes/Aggressive/CELFlyingAttack.h"
#include "ai/ExtendedEntityesLogic/Interfaces/IELAdapter.h"
#include "ai/ExtendedEntityesLogic/EntryPoint.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/CELFlyingSettings.h"
#include "ai/monsters/basemonster/base_monster.h"
#include "entity_alive.h"
#include "Level.h"

// Выбирает переднюю точку тела с учётом смещения геометрии относительно корня модели.
Fvector CELFlyingAttack::LocalContactPoint(const CBaseMonster& OwnerMonster)
{
	const Fbox& Bounds = OwnerMonster.BoundingBox();
	return Fvector().set((Bounds.min.x + Bounds.max.x) * .5f, (Bounds.min.y + Bounds.max.y) * .5f, Bounds.max.z);
}

// Переводит цель для передней точки тела в положение корня на горизонтальном выходе.
Fvector CELFlyingAttack::RootAtContact(const CBaseMonster& OwnerMonster, const Fvector& TargetPoint, const Fvector& Direction)
{
	float Heading;
	float Pitch;
	Direction.getHP(Heading, Pitch);
	Fmatrix Orientation;
	Orientation.setHPB(Heading, 0.f, 0.f);
	Fvector ContactOffset;
	Orientation.transform_dir(ContactOffset, LocalContactPoint(OwnerMonster));
	return Fvector().sub(TargetPoint, ContactOffset);
}

// Проверяет фактический контакт атаки и наносит допустимый урон цели.
bool CELFlyingAttack::TryHit(IELAdapter& Adapter, const SELFlyingSettings& Settings, u16 TargetIdentifier)
{
	if (s32(Device.dwTimeGlobal - NextAttack) < 0)
	{
		return false;
	}
	CEntityAlive* TargetEntity = ExtendedEntityes::FindEntity(TargetIdentifier);
	if (!TargetEntity || !Adapter.IsEnemy(*TargetEntity) || Adapter.Monster().EnemyMan.get_enemy() != TargetEntity)
	{
		return false;
	}
	CBaseMonster& OwnerMonster = Adapter.Monster();
	Fvector AttackPoint = OwnerMonster.Position();
	OwnerMonster.XFORM().transform_tiny(AttackPoint, LocalContactPoint(OwnerMonster));
	Fmatrix TargetInverse;
	TargetInverse.invert(TargetEntity->XFORM());
	Fvector TargetPoint;
	TargetInverse.transform_tiny(TargetPoint, AttackPoint);
	clamp(TargetPoint.x, TargetEntity->BoundingBox().min.x, TargetEntity->BoundingBox().max.x);
	clamp(TargetPoint.y, TargetEntity->BoundingBox().min.y, TargetEntity->BoundingBox().max.y);
	clamp(TargetPoint.z, TargetEntity->BoundingBox().min.z, TargetEntity->BoundingBox().max.z);
	TargetEntity->XFORM().transform_tiny(TargetPoint);
	Fvector Direction;
	Direction.sub(TargetPoint, AttackPoint);
	const float Distance = Direction.magnitude();
	const float ContactRadius = Settings.AttackDistance;
	if (Distance > ContactRadius)
	{
		return false;
	}
	if (Distance > EPS_L)
	{
		Direction.div(Distance);
	}
	else
	{
		Direction = OwnerMonster.XFORM().k;
	}
	collide::rq_result Hit;
	if (Distance > EPS_L && Level().ObjectSpace.RayPick(AttackPoint, Direction, Distance, collide::rqtStatic, Hit, &OwnerMonster))
	{
		return false;
	}
	Fmatrix OwnerInverse;
	OwnerInverse.invert(OwnerMonster.XFORM());
	OwnerInverse.transform_dir(Direction);
	OwnerMonster.HitEntity(TargetEntity, Settings.AttackDamage, Settings.AttackImpulse, Direction, ALife::eHitTypeWound, true);
	NextAttack = Device.dwTimeGlobal + Settings.AttackCooldown;
	AttackUntil = Device.dwTimeGlobal + Settings.AttackIndicatorTime;
	return true;
}

// Сообщает, действует ли сейчас фаза атаки или её отладочный индикатор.
bool CELFlyingAttack::IsAttacking() const
{
	return s32(Device.dwTimeGlobal - AttackUntil) < 0;
}
