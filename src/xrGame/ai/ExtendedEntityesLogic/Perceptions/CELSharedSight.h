#pragma once
class CBaseMonster;
class CObject;

enum class EELSightResult : u8
{
	Visible,
	Occluded,
	WaitingForBudget
};

class CELSharedSight final
{
public:
	static EELSightResult Check(CBaseMonster& OwnerMonster, CObject& Candidate, float Angle, float Radius);
	static bool Visible(CBaseMonster& OwnerMonster, CObject& Candidate, float Angle, float Radius);
};
