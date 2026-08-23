// 捕捉系统原生 GameplayTag 定义（UE 5.3+ NativeGameplayTags 写法）

#pragma once

#include "NativeGameplayTags.h"

namespace CaptureTags
{
	// 帕鲁正在被捕捉中（防多球竞争 / 阻止能力激活）
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Pal_BeingCaptured);
	// 帕鲁已被玩家召唤出战（禁止再被捕捉，收回时移除）
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Pal_Summoned);
	// 投掷帕鲁球输入标签（挂在投掷能力 AbilityTags 上，按对应按键即可激活）
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_InputTag_Throw);
	// 玩家攻击输入标签（鼠标左键，激活玩家普攻能力）
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_InputTag_Attack);
	// 回合制战斗卷入标记（打上的帕鲁暂停自由战斗、禁止实时捕捉，由战斗流程统一结算）
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Battle_Battling);
}
