#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "CourtyardISMModuleCatalog.generated.h"
// 後で定義される型の名前を先に宣言し、ポインターなどの参照に使えるようにする。
class UStaticMesh;
// 後で定義される型の名前を先に宣言し、ポインターなどの参照に使えるようにする。
class UMaterialInterface;

/** A fixed mesh extracted from an editor module. Source assets are never edited. */
// 構造体をUEへ登録し、保存やBPでのMake/Breakに使えるようにする。
USTRUCT(BlueprintType)

struct FCourtyardISMMesh
{
    // UEが型登録・BP連携・保存のために生成する宣言を挿入する。
    GENERATED_BODY()
    // この後の値をUEのプロパティとして登録する（既定値・配置済みActorの両方から編集可能、BPで読み書き可能）。
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 静的メッシュ（Mesh）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<UStaticMesh> Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // メッシュの各スロットに設定する材質一覧（Materials）を配列として宣言する。
    TArray<TObjectPtr<UMaterialInterface>> Materials;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 部品のローカル位置・回転・拡大率（LocalTransform）を位置・回転・拡大率として宣言する。
    FTransform LocalTransform;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 衝突のプリセット名（CollisionProfile）をFNameとして用意し、"BlockAll"というUE文字列で初期化する。
    FName CollisionProfile = TEXT("BlockAll");
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 衝突判定を有効にする範囲（CollisionEnabled）をUEへ公開する列挙値を1バイトで保持する型として用意し、ECollisionEnabled::QueryAndPhysicsで初期化する。
    TEnumAsByte<ECollisionEnabled::Type> CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 自身の衝突オブジェクト種別（ObjectType）をUEへ公開する列挙値を1バイトで保持する型として用意し、ECC_WorldStaticで初期化する。
    TEnumAsByte<ECollisionChannel> ObjectType = ECC_WorldStatic;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 各衝突チャンネルへの応答設定（Responses）をFCollisionResponseContainerとして宣言する。
    FCollisionResponseContainer Responses;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // この部品に影響するライトのチャンネル（LightingChannels）をFLightingChannelsとして宣言する。
    FLightingChannels LightingChannels;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 影を落とす設定（bCastShadow）を真偽値として用意し、true（成立・有効）で初期化する。
    bool bCastShadow = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // デカールを受ける設定（bReceivesDecals）を真偽値として用意し、true（成立・有効）で初期化する。
    bool bReceivesDecals = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 部品を表示する設定（bVisible）を真偽値として用意し、true（成立・有効）で初期化する。
    bool bVisible = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // ゲーム中に非表示にする設定（bHiddenInGame）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bHiddenInGame = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // カスタム深度へ描く設定（bCustomDepth）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bCustomDepth = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // カスタムステンシルの番号（Stencil）を符号付き32ビット整数として用意し、0で初期化する。
    int32 Stencil = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // NavMesh計算へ影響する設定（bNavigation）を真偽値として用意し、true（成立・有効）で初期化する。
    bool bNavigation = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // この部品でNaniteを使わない設定（bDisallowNanite）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bDisallowNanite = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 距離場ライティングへ影響する設定（bAffectDistanceFieldLighting）を真偽値として用意し、true（成立・有効）で初期化する。
    bool bAffectDistanceFieldLighting = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // レイトレーシングで表示する設定（bVisibleInRayTracing）を真偽値として用意し、true（成立・有効）で初期化する。
    bool bVisibleInRayTracing = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 非表示でも影を落とす設定（bCastHiddenShadow）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bCastHiddenShadow = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 半透明描画の優先順位（TranslucencySortPriority）を符号付き32ビット整数として用意し、0で初期化する。
    int32 TranslucencySortPriority = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // トンネル手前のアーチかの識別（bStartArch）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bStartArch = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // トンネル奥のアーチかの識別（bEndArch）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bEndArch = false;
};
// 構造体をUEへ登録し、保存やBPでのMake/Breakに使えるようにする。
USTRUCT(BlueprintType)

struct FCourtyardISMDecal
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 使用するマテリアル（Material）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<UMaterialInterface> Material;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 部品のローカル位置・回転・拡大率（LocalTransform）を位置・回転・拡大率として宣言する。
    FTransform LocalTransform;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 範囲の大きさ（Size）を3次元ベクトルとして用意し、3次元の位置・方向・寸法を組み立てる（引数：128）で初期化する。
    FVector Size = FVector(128);
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // デカールを重ねる描画順（SortOrder）を符号付き32ビット整数として用意し、0で初期化する。
    int32 SortOrder = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 画面上の大きさによるデカールのフェード基準（FadeScreenSize）を単精度の実数として用意し、の01fで初期化する。
    float FadeScreenSize = .01f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 部品を表示する設定（bVisible）を真偽値として用意し、true（成立・有効）で初期化する。
    bool bVisible = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // ゲーム中に非表示にする設定（bHiddenInGame）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bHiddenInGame = false;
};
// 構造体をUEへ登録し、保存やBPでのMake/Breakに使えるようにする。
USTRUCT(BlueprintType)

struct FCourtyardISMComponentProperties
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 元の部品名（ComponentName）をFNameとして宣言する。
    FName ComponentName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // Properties（Properties）をキーで値を引く表として宣言する。
    TMap<FName, FString> Properties;
};
/** Behavioural actors stay actors, with copied Blueprint classes and instance settings. */
// 構造体をUEへ登録し、保存やBPでのMake/Breakに使えるようにする。
USTRUCT(BlueprintType)

struct FCourtyardISMActor
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 生成する小物Actorのクラス（ActorClass）をTSubclassOf<AActor>として宣言する。
    TSubclassOf<AActor> ActorClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 部品のローカル位置・回転・拡大率（LocalTransform）を位置・回転・拡大率として宣言する。
    FTransform LocalTransform;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // Properties（Properties）をキーで値を引く表として宣言する。
    TMap<FName, FString> Properties;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // Actorが持つコンポーネント一覧（Components）を配列として宣言する。
    TArray<FCourtyardISMComponentProperties> Components;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // Actorを隠す設定（bHidden）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bHidden = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // Actorの衝突を使う設定（bCollision）を真偽値として用意し、true（成立・有効）で初期化する。
    bool bCollision = true;
};
// 構造体をUEへ登録し、保存やBPでのMake/Breakに使えるようにする。
USTRUCT(BlueprintType)

struct FCourtyardISMModule
{
    GENERATED_BODY()
    // Identity only: this does not load or hard-reference the original level at runtime.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 元になった部品レベルの参照パス（SourceLevel）をFSoftObjectPathとして宣言する。
    FSoftObjectPath SourceLevel;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // Meshes（Meshes）を配列として宣言する。
    TArray<FCourtyardISMMesh> Meshes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 生成するデカールの件数または一覧（Decals）を配列として宣言する。
    TArray<FCourtyardISMDecal> Decals;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // 生成するActorの件数または一覧（Actors）を配列として宣言する。
    TArray<FCourtyardISMActor> Actors;
};
// クラスをUEへ登録し、指定に応じてBP継承やアセット参照に使えるようにする。
UCLASS(BlueprintType)

class ESCAPE_ON_STEP_DEMO_API UCourtyardISMModuleCatalog : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modules")
    // Modules（Modules）を配列として宣言する。
    TArray<FCourtyardISMModule> Modules;
};
