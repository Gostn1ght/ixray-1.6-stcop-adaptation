#include "stdafx.h"
#include "Level.h"
#include "ai/ExtendedEntityesLogic/CoreComponent/TExtendedEntityesLogic.h"
#include "ai/ExtendedEntityesLogic/CoreComponent/CELSharedFactory.h"
#include "ai/ExtendedEntityesLogic/CELSharedSettings.h"
#include "ai/monsters/basemonster/base_monster.h"

// Освобождает принадлежащую компоненту реализацию расширения.
TExtendedEntityesLogic::~TExtendedEntityesLogic() = default;
// Связывает ECS-компонент расширенной логики с необходимыми службами и состоянием владельца.
TExtendedEntityesLogic::TExtendedEntityesLogic(TExtendedEntityesLogic&& Other) noexcept : OwnerMonster(Other.OwnerMonster), Logic(std::move(Other.Logic)), HasAttached(Other.HasAttached), ControlRevision(Other.ControlRevision), LastBehaviorUpdate(Other.LastBehaviorUpdate)
{
	Other.OwnerMonster = nullptr;
	Other.HasAttached = false;
}

// Передаёт владение реализацией компонента и очищает прежнее состояние.
TExtendedEntityesLogic& TExtendedEntityesLogic::operator=(TExtendedEntityesLogic&& Other) noexcept
{
	if (this != &Other)
	{
		Logic = std::move(Other.Logic);
		OwnerMonster = Other.OwnerMonster;
		HasAttached = Other.HasAttached;
		ControlRevision = Other.ControlRevision;
		LastBehaviorUpdate = Other.LastBehaviorUpdate;
		Other.OwnerMonster = nullptr;
		Other.HasAttached = false;
	}
	return *this;
}

// Привязывает ECS-компонент к владельцу без сохранения указателей на другие компоненты.
void TExtendedEntityesLogic::BeginComponent(IECSOwner* ComponentOwner)
{
	// ECS invokes this under its storage lock: bind only, do not reenter ECS.
	OwnerMonster = smart_cast<CBaseMonster*>(ComponentOwner);
}

// Завершает компонент и освобождает его реализацию расширения.
void TExtendedEntityesLogic::EndComponent()
{
	// The host may already be in base destruction. Normal Detach happens earlier.
	Logic.reset();
	OwnerMonster = nullptr;
	HasAttached = false;
}

// Загружает настройки и подготавливает состояние перед использованием.
void TExtendedEntityesLogic::Initialize()
{
	if (!OwnerMonster || !OnServer() || !OwnerMonster->g_Alive() || !CELSharedSettings::IsEnabled())
	{
		return;
	}
	if (Logic && ControlRevision != CELSharedSettings::Revision())
	{
		Detach();
	}
	ControlRevision = CELSharedSettings::Revision();
	if (!Logic)
	{
		Logic = CELSharedFactory::Create(*OwnerMonster);
	}
	if (Logic && CELSharedSettings::IsEnabled() && !HasAttached)
	{
		Logic->Attach();
		LastBehaviorUpdate = Device.dwTimeGlobal;
		HasAttached = true;
	}
}

// Снимает управление расширения и освобождает регистрации владельца.
void TExtendedEntityesLogic::Detach()
{
	if (Logic && HasAttached)
	{
		Logic->Detach();
	}
	Logic.reset();
	HasAttached = false;
}

// Проверяет подключение компонента, режим и актуальность ревизии управления.
bool TExtendedEntityesLogic::IsActive() const
{
	return HasAttached && ControlRevision == CELSharedSettings::Revision() && CELSharedSettings::IsEnabled();
}

// Обновляет личные наблюдения владельца с учётом доступного бюджета.
bool TExtendedEntityesLogic::Observe()
{
	if (!CELSharedSettings::IsEnabled())
	{
		Detach();
		return false;
	}
	Initialize();
	if (!IsActive())
	{
		return false;
	}
	Logic->Observe();
	return true;
}

// Сохраняет или восстанавливает состояние через штатный интерфейс сериализации.
void TExtendedEntityesLogic::Serialize(ISaveObject& Object)
{
	Initialize();
	if (Logic)
	{
		Logic->Serialize(Object);
	}
}

// Обновляет ECS-компонент расширенной логики и обрабатывает текущий результат выполнения.
EELControl TExtendedEntityesLogic::Update(float DeltaTime)
{
	if (!IsActive())
	{
		return EELControl::Native;
	}
	const u32 Elapsed = Device.dwTimeGlobal - LastBehaviorUpdate;
	LastBehaviorUpdate = Device.dwTimeGlobal;
	return Logic->Update(float(Elapsed) * .001f);
}

// Выполняет покадровую часть активной схемы без повторного выбора поведения.
void TExtendedEntityesLogic::FrameUpdate(float DeltaTime)
{
	if (IsActive())
	{
		Logic->FrameUpdate(DeltaTime);
	}
}

// Обрабатывает принятое звуковое событие по настроенным правилам реакции.
void TExtendedEntityesLogic::Hear(const SELSoundEvent& SoundEvent)
{
	if (IsActive())
	{
		Logic->Hear(SoundEvent);
	}
}

// Удаляет знания и ссылки, связанные с исчезнувшим объектом.
void TExtendedEntityesLogic::RemoveObject(u16 ObjectIdentifier)
{
	if (Logic)
	{
		Logic->RemoveObject(ObjectIdentifier);
	}
}

// Принимает сведения другого источника с проверкой правил передачи.
bool TExtendedEntityesLogic::Accept(const SELObservation& Observation, CBaseMonster& SourceMonster)
{
	return IsActive() && Logic->Accept(Observation, SourceMonster);
}

// Возвращает идентификатор цели, выбранной реализацией расширения.
u16 TExtendedEntityesLogic::GetTargetIdentifier() const
{
	return IsActive() ? Logic->GetTargetIdentifier() : u16(-1);
}
