#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LMLoreData.generated.h"

class ULMLoreWidget;

// 하나의 로어 대화에 표시할 발신자와 메시지다.
USTRUCT(BlueprintType)
struct LUMIA_API FLMLoreLine
{
	GENERATED_BODY()

	// 이 메시지를 보낸 인물의 이름이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Lore")
	FText Sender;

	// 대화 내용은 여러 줄로 작성할 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Lore", meta = (MultiLine = "true"))
	FText Message;
};

// 상호작용 대상이 공유하거나 복제해 사용할 로어 대화 원본이다.
UCLASS(BlueprintType)
class LUMIA_API ULMLoreData : public UDataAsset
{
	GENERATED_BODY()

public:
	// 로어 창 상단에 표시할 제목이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Lore")
	FText Title;

	// 배열 순서대로 발신자와 메시지를 표시한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Lore", meta = (TitleProperty = "Sender"))
	TArray<FLMLoreLine> Lines;

	// 이 로어를 표시할 위젯이며 데이터 에셋을 복제할 때 함께 유지된다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Lore", meta = (AdvancedDisplay))
	TSubclassOf<ULMLoreWidget> WidgetClass;
};
