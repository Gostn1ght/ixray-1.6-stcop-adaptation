#pragma once
class IELAdapter;
struct SELFlyingSettings;

class CELFlyingAttack final
{
public:
	bool TryHit(IELAdapter& Adapter, const SELFlyingSettings& Settings, u16 TargetIdentifier);
	bool IsAttacking() const;

private:
	u32 NextAttack = 0;
	u32 AttackUntil = 0;
};
