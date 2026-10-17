#include "stdafx.h"
#include "ai/ExtendedEntityesLogic/Debug/Debug.h"
#include "ai/ExtendedEntityesLogic/Debug/DebugDrawHud.h"
#include "ai/ExtendedEntityesLogic/Debug/DebugDrawWorld.h"

// Возвращает единственный общий экземпляр службы.
CELSharedDebug& CELSharedDebug::Instance()
{
	static CELSharedDebug Debug;
	return Debug;
}

// Переключает мировую отладку и очищает ненужные записи.
void CELSharedDebug::SetDrawEnabled(bool IsEnabled)
{
	xrCriticalSectionGuard Guard(QueueLock);
	HasDraw = IsEnabled;
	if (!HasDraw && !HasStats)
	{
		Records.clear();
	}
}

// Переключает статистику HUD и очищает ненужные записи.
void CELSharedDebug::SetStatsEnabled(bool IsEnabled)
{
	xrCriticalSectionGuard Guard(QueueLock);
	HasStats = IsEnabled;
	if (!HasDraw && !HasStats)
	{
		Records.clear();
	}
}

// Сообщает, включена ли соответствующая подсистема.
bool CELSharedDebug::IsEnabled() const
{
	return HasDraw || HasStats;
}

// Добавляет или обновляет запись в общей очереди подсистемы.
void CELSharedDebug::Publish(SELDebugRecord Record)
{
	xrCriticalSectionGuard Guard(QueueLock);
	if (!HasDraw && !HasStats)
	{
		return;
	}
	Record.ExpiresAt = Device.dwTimeGlobal + 1500;
	xr_vector<SELDebugRecord>::iterator RecordIterator;
	for (RecordIterator = Records.begin(); RecordIterator != Records.end(); ++RecordIterator)
	{
		if (RecordIterator->OwnerIdentifier == Record.OwnerIdentifier)
		{
			*RecordIterator = std::move(Record);
			return;
		}
	}
	if (Records.size() < 1024)
	{
		Records.push_back(std::move(Record));
	}
}

// Выводит подготовленные данные отладки средствами соответствующего слоя.
void CELSharedDebug::Draw()
{
	xrCriticalSectionGuard Guard(QueueLock);
	xr_vector<SELDebugRecord>::iterator RecordIterator = Records.begin();
	while (RecordIterator != Records.end())
	{
		if (s32(Device.dwTimeGlobal - RecordIterator->ExpiresAt) >= 0)
		{
			RecordIterator = Records.erase(RecordIterator);
		}
		else
		{
			++RecordIterator;
		}
	}
	if (HasDraw)
	{
		CELSharedDebugDrawWorld::Draw(Records);
	}
	if (HasStats)
	{
		CELSharedDebugDrawHud::Draw(Records, Frame);
	}
}

// Сбрасывает накопленные данные и незавершённую работу подсистемы.
void CELSharedDebug::Clear()
{
	xrCriticalSectionGuard Guard(QueueLock);
	Records.clear();
	Frame = SELDebugFrame();
}

// Публикует снимок общей статистики для последующей отрисовки.
void CELSharedDebug::PublishFrame(const SELDebugFrame& Snapshot)
{
	if (!IsEnabled())
	{
		return;
	}
	xrCriticalSectionGuard Guard(QueueLock);
	Frame = Snapshot;
}
