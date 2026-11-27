#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DungeonAdjacentRoomLibrary.generated.h"

/** C++の間取りモデルと既存の描画BPをつなぐアダプター。 */
UCLASS()
class ESCAPE_ON_STEP_DEMO_API UDungeonAdjacentRoomLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

  public:
	/**
	 * 接続された間取りを作り、既存BPの床・照明・敵・ゴール配置を呼ぶ。
	 * DesiredRooms=0は標準24～30室。旧BPに保存された14も標準として扱う。
	 * Seed<0は実行ごとに抽選。固定Seedは同じC++版の生成を再現するために使う。
	 *
	 * 新しいゲームワールドで1回だけ実行する。失敗時は既存の再生成Subsystemへ依頼する。
	 * 既存の生成物をその場で削除しない。ワールド原点基準、床Z=0の構成を前提とする。
	 * BP_Dungeon_Generator_BackRoomのBP生成経路は、この関数を呼ばない。
	 */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|Adjacent Rooms", meta = (DefaultToSelf = "Generator"))
	static bool GenerateAdjacentDungeon(AActor* Generator, int32 DesiredRooms = 0, int32 Seed = -1);
};
