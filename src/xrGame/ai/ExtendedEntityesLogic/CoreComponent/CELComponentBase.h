#pragma once
#include "../Interfaces/IELLogic.h"
#include "../Interfaces/IELAdapter.h"
#include "../Perceptions/CELSharedMemory.h"
#include "../Perceptions/CELSharedHearing.h"
#include "../Perceptions/CELSharedEnemySharing.h"

class CELComponentBase : public IELLogic
{
public:
	CELComponentBase(xr_unique_ptr<IELAdapter> OwnerAdapter, const SELPerceptionProfile& OwnerProfile);
	~CELComponentBase() override = default;
	void Attach() override;
	void Detach() override;
	bool IsAttached() const override { return HasAttached; }
	void Observe() override;
	void Serialize(ISaveObject& Object) override {}
	u16 GetTargetIdentifier() const override { return TargetIdentifier; }
	void FrameUpdate(float DeltaTime) override;
	void Hear(const SELSoundEvent& SoundEvent) override;
	void ForgetTarget(u16 ObjectIdentifier) override;
	void RemoveObject(u16 ObjectIdentifier) override;
	bool Accept(const SELObservation& Observation, CBaseMonster& SourceMonster) override;

protected:
	void RememberEntity(CEntityAlive& TargetEntity, EELObservationSource Source, u32 ObservedAt);
	void PublishNativeMemory();
	void ObserveCloseContact();
	void ObservePeriphery();
	void ObserveVision(CBaseMonster& OwnerMonster, u32 CurrentTime);
	void ObserveDamage(CBaseMonster& OwnerMonster, u32 CurrentTime);
	bool HasVisualAcquired(CEntityAlive& Candidate, u32 CurrentTime);

	xr_unique_ptr<IELAdapter> Adapter;
	const SELPerceptionProfile& Profile;
	CELSharedMemory Memory;
	CELSharedHearing Hearing;
	CELSharedEnemySharing Sharing;
	u16 TargetIdentifier = u16(-1);
	u32 ReceiveAfter = 0;
	u32 NextCloseProbe = 0;
	u32 NextPeripheralProbe = 0;
	u32 PeripheralCursor = 0;
	u32 LastReceivedAt = 0;
	u16 LastSender = u16(-1);
	bool HasAttached = false;

private:
	struct SVisualAcquisition
	{
		u32 SampleTime = 0;
		u32 VisibleTime = 0;
	};
	xr_map<u16, SVisualAcquisition> VisualAcquisition;
};
