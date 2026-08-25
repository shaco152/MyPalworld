#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HitReactInterface.generated.h"

UINTERFACE(BlueprintType)
class FINALPROJECT_API UHitReactInterface : public UInterface
{
	GENERATED_BODY()
};

/** 普攻命中反应窄接口；资源 Actor 用它触发掉落，不把采集逻辑耦合进 GAS 属性集。 */
class FINALPROJECT_API IHitReactInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "HitReact")
	void ReceiveHitReact(AActor* HitInstigator, const FHitResult& HitResult);
};
