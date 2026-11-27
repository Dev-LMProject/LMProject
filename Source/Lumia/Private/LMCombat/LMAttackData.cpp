#include "LMCombat/LMAttackData.h"

// 기존 공격 데이터의 단일 소켓 쌍을 배열의 첫 줄로 옮긴다.
void ULMAttackData::PostLoad()
{
    Super::PostLoad();

    if (TraceSocketPairs.IsEmpty())
        TraceSocketPairs.Add(FLMTraceSocketPair{TraceStartSocket, TraceEndSocket});
}
