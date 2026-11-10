#pragma once

#include "NativeGameplayTags.h"

namespace LMGameplayTags
{
	/** 입력 액션과 능력을 연결하는 공통 공격 입력 태그. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Attack);
	/** 스캔 입력 태그 */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Scan);
	/** 능력 입력을 차단하는 상태. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Input_Blocked);
	/** Q 입력과 특수 스킬 입력 능력을 연결한다. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Special);
	/** 상호작용 입력 액션과 입력 능력을 연결한다. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Interact);

	
	/** 능력이나 루트 모션이 일반 이동 대신 캐릭터 이동을 담당하는 상태. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Movement_External);

	/** 일반 이동을 금지하는 상태. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Movement_Blocked);

	/** 능력 실행과 이동 제한에 사용하는 사망·기절 상태. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Stunned);

	/** 실제 공격 실행 상태와 콤보 실행 상태를 구분한다. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Action_Attacking);
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Attack_Combo);
	/** 현재 행동이 취소 가능한 마무리 구간에 있음을 나타낸다. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Action_Recovery);
	
	/** 콤보가 특수 스킬 준비·실행 단계에 들어갔음을 나타낸다. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Attack_Special);
	
	/** 입력 해석 능력과 실제 공격 능력을 구분한다. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Input_Attack);
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack);
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Combo);
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Heavy);
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Plunge);


	/** 입력 GA가 실행 중인 콤보 GA에 전달하는 사건. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Input_Light);
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Input_Heavy);
	/** Q 입력을 실행 중인 콤보 능력에 전달한다. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Input_Special);
	/** 특수 스킬 단계에서 새 좌클릭이 발생했음을 전달한다. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Input_SpecialAdvance);

	/** CharacterMovement의 실제 착지를 능력에 전달하는 사건. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Movement_Landed);
	
	/** 애니메이션이 콤보 입력 가능 구간을 알리는 사건. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Combo_Open);
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Combo_Close);
	/** 공격 애니메이션이 취소 가능한 마무리 구간에 도달했음을 알린다. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Recovery_Begin);
	/** 특수 준비 또는 SP02가 다음 입력을 기다릴 지점에 도달했음을 알린다. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Special_Hold);
	/** 공격 몽타주의 타격 허용 구간이 시작되었음을 알린다 */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_HitWindow_Begin);
	/** 타격 구간의 끝에 도달하여 마지막 유효 구간까지 처리하도록 알린다. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_HitWindow_End);
	/** 타격 구간이 중단되어 날은 피해 판정을 버리도록 알린다. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_HitWindow_Cancel);
	
	/** 이동이나 새 행동이 현재 마무리 동작의 취소를 요청한다. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Action_CancelRecovery);
	
	/** 피해 Spec에서 이번 실행의 피해량을 전달할 때 사용하는 이름표다. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);

	//Scan GA 용 쿨타임 태그
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Scan);
	
	//해킹용 State 태그
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hacking_NeedHacking);
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hacking_HackedComplete);
	
	//인터렉션 액터 구별 태그
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interaction_Actor_Hacking);
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interaction_Actor_ActiveTarget);
	
}
