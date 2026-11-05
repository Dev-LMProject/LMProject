#include "LMGameplayTags/LMGameplayTags.h"

namespace LMGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Attack, "LM.InputTag.Attack", "공격 입력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Special, "LM.InputTag.Special", "특수 스킬 입력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Scan, "LM.InputTag.Scan", "스캔 입력");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Input_Blocked, "LM.State.Input.Blocked", "능력 입력 차단");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Movement_External, "LM.State.Movement.External", "능력이나 루트 모션이 이동을 담당하는 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Movement_Blocked, "LM.State.Movement.Blocked", "일반 이동 차단");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "LM.State.Dead", "사망 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Stunned, "LM.State.Stunned", "기절 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Attack_Special, "LM.State.Attack.Special", "특수 스킬 준비 및 실행 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Action_Recovery, "LM.State.Action.Recovery", "현재 행동의 마무리를 취소할 수 있는 상태");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Movement_Landed, "LM.Event.Movement.Landed", "캐릭터의 실제 착지");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Input_Attack, "LM.Ability.Input.Attack", "공격 입력을 해석하는 능력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack, "LM.Ability.Attack", "실제 공격을 실행하는 능력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack_Combo, "LM.Ability.Attack.Combo", "일반 공격 콤보");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack_Heavy, "LM.Ability.Attack.Heavy", "강공격");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack_Plunge, "LM.Ability.Attack.Plunge", "낙하 공격");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Action_Attacking, "LM.State.Action.Attacking", "실제 공격 실행 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Attack_Combo, "LM.State.Attack.Combo", "콤보 공격 실행 중");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Input_Light, "LM.Event.Combat.Input.Light", "콤보에 전달하는 일반 공격 입력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Input_Heavy, "LM.Event.Combat.Input.Heavy", "콤보에 전달하는 강공격 입력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Input_Special, "LM.Event.Combat.Input.Special", "콤보에 전달하는 Q 입력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Input_SpecialAdvance, "LM.Event.Combat.Input.SpecialAdvance", "특수 스킬의 다음 단계를 요청하는 좌클릭");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Combo_Open, "LM.Event.Combat.Combo.Open", "콤보 입력 구간 시작");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Combo_Close, "LM.Event.Combat.Combo.Close", "콤보 입력 구간 종료");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Special_Hold, "LM.Event.Combat.Special.Hold", "특수 스킬 입력 대기 지점");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Recovery_Begin, "LM.Event.Combat.Recovery.Begin", "공격 마무리 구간 시작");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Action_CancelRecovery, "LM.Event.Action.CancelRecovery", "현재 마무리 동작 취소 요청");

	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Scan, "LM.Cooldown.Scan");
}