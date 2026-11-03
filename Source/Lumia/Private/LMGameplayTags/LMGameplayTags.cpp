#include "LMGameplayTags/LMGameplayTags.h"

namespace LMGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Attack, "InputTag.Attack", "공격 입력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Input_Blocked, "State.Input.Blocked", "능력 입력 차단");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Movement_External, "State.Movement.External", "능력이나 루트 모션이 이동을 담당하는 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Movement_Blocked, "State.Movement.Blocked", "일반 이동 차단");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "사망 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Stunned, "State.Stunned", "기절 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Movement_Landed, "Event.Movement.Landed", "캐릭터의 실제 착지");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Input_Attack, "Ability.Input.Attack", "공격 입력을 해석하는 능력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack, "Ability.Attack", "실제 공격을 실행하는 능력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack_Combo, "Ability.Attack.Combo", "일반 공격 콤보");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack_Heavy, "Ability.Attack.Heavy", "강공격");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack_Plunge, "Ability.Attack.Plunge", "낙하 공격");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Action_Attacking, "State.Action.Attacking", "실제 공격 실행 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Attack_Combo, "State.Attack.Combo", "콤보 공격 실행 중");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Input_Light, "Event.Combat.Input.Light", "콤보에 전달하는 일반 공격 입력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Input_Heavy, "Event.Combat.Input.Heavy", "콤보에 전달하는 강공격 입력");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Combo_Open, "Event.Combat.Combo.Open", "콤보 입력 구간 시작");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Combo_Close, "Event.Combat.Combo.Close", "콤보 입력 구간 종료");

	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Scan, "LM.Cooldown.Scan");
	
}