#pragma once

#include "CoreMinimal.h"

// 描画を含まない間取りモデル。座標はワールドcmではなく床セル単位。
namespace DungeonAdjacent
{
/** 矩形、または角の2×2セルを欠いたL字の部屋。X/Yは外接矩形の最小座標。 */
struct FRoom
{
	int32 X = 0, Y = 0, W = 0, H = 0;

	// INDEX_NONE: 矩形。0～3: 欠く角。bit0がX側、bit1がY側を反転する。
	int32 CutCorner = INDEX_NONE;

	/** 外接矩形内でも、L字の欠けに当たるセルは含めない。 */
	bool Contains(FIntPoint P) const
	{
		const int32 LocalX = P.X - X, LocalY = P.Y - Y;
		if (LocalX < 0 || LocalY < 0 || LocalX >= W || LocalY >= H)
		{
			return false;
		}
		if (CutCorner == INDEX_NONE)
		{
			return true;
		}
		const int32 FromCornerX = (CutCorner & 1) ? W - 1 - LocalX : LocalX;
		const int32 FromCornerY = (CutCorner & 2) ? H - 1 - LocalY : LocalY;
		return FromCornerX >= 2 || FromCornerY >= 2;
	}

	/** 中央付近の実床セル。プレイヤーや既存BPへの中心座標に使う。 */
	FIntPoint Center() const
	{
		const FIntPoint Mid(X + W / 2, Y + H / 2);
		return Contains(Mid) ? Mid : FIntPoint(Mid.X - 1, Mid.Y);
	}
};

/** A/Bは接続先の所有ID。0以上はRoomsの添字、負数は専用通路のID。 */
struct FDoor
{
	int32 A = 0, B = 0;
	TArray<FIntVector> Edges;
};

/** 生成と検査の結果。アクターやメッシュへの参照を持たない。 */
struct FLayout
{
	int32 Corridors = 0;
	int32 DeadEnds = 0; // 出口が2つ未満の部屋数。床セル単位の袋小路数ではない。
	int32 Branches = 0;
	int32 CorridorRooms = 0;
	TArray<FRoom> Rooms;
	TArray<FDoor> Doors;
	TMap<FIntPoint, int32> Cells; // 実床セル -> 部屋/通路の所有ID。
	TSet<FIntVector> Walls;		  // (X,Y,0): 東の壁、(X,Y,1): 北の壁。共有壁を一意に表す。
};

/**
 * Min/Maxを含む範囲に間取りを生成する。Target<=0は標準24～30室、正数は1～40室。
 * 最大8案を試し、条件を満たさない場合は最良案を返すため、呼び出し元でも合否を確認する。
 * 同じ範囲・Target・Seedなら同じ結果。乱数の呼び出し順も再現性の一部になる。
 */
FLayout Build(FIntPoint Min, FIntPoint Max, int32 Target, int32 Seed);
} // namespace DungeonAdjacent
