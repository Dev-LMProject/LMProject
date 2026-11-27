#include "LMCombat/LMCombatEffectData.h"

// 파생 BP 여부와 관계없이 설정 파일에 등록한 타입 이름을 유지한다.
FPrimaryAssetId ULMCombatEffectData::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("LMCombatEffectData"), GetFName());
}
