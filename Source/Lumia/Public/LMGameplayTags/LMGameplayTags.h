#pragma once

#include "NativeGameplayTags.h"

namespace LMGameplayTags
{
	/** 입력 액션과 능력을 연결하는 공통 공격 입력 태그. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Attack);
	//스캔 입력 태그
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Scan);
	
	/** 능력 입력을 차단하는 상태. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Input_Blocked);

	
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
	
	
	
	
	/** 입력 해석 능력과 실제 공격 능력을 구분한다. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Input_Attack);
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack);
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Combo);
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Heavy);
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Plunge);


	/** 입력 GA가 실행 중인 콤보 GA에 전달하는 사건. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Input_Light);
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Input_Heavy);

	/** CharacterMovement의 실제 착지를 능력에 전달하는 사건. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Movement_Landed);
	
	/** 애니메이션이 콤보 입력 가능 구간을 알리는 사건. */
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Combo_Open);
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Combo_Close);

	//Scan GA 용 쿨타임 태그
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Scan);
	
	//해킹용 State 태그
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hacking_NeedHacking);
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hacking_HackedComplete);
	
	//인터렉션 액터 구별 태그
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interaction_Actor_Hacking);
	LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interaction_Actor_ActiveTarget);
	
}