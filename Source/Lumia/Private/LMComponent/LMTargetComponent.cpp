#include "LMComponent/LMTargetComponent.h"

#include "LMGameplayTags/LMGameplayTags.h"

// 대상마다 검색을 반복하지 않도록 이 컴포넌트의 Tick을 사용하지 않는다.
ULMTargetComponent::ULMTargetComponent()
{
	// 대상 검색과 선정은 기존 오토포커싱 GA의 Task에서 한 번에 처리한다.
	PrimaryComponentTick.bCanEverTick = false;
}

// 이 Actor가 오토포커싱 후보 검사를 받을 수 있는 상태인지 반환한다.
bool ULMTargetComponent::CanFocus() const
{
	// 월드에 아직 등록되지 않았거나 제거된 컴포넌트의 위치는 대상 검색에 사용하지 않는다.
	if (!IsRegistered()) return false;

	// 허용 태그가 반드시 필요하며, 제외 태그가 함께 있으면 제외를 우선한다.
	return TargetTags.HasTag(LMGameplayTags::Focus_Target) && !TargetTags.HasTag(LMGameplayTags::Focus_Disabled);
}