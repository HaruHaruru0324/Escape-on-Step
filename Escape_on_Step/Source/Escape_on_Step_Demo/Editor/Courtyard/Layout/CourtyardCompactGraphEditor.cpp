#if WITH_EDITOR
#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Knot.h"
#include "NodeFactory.h"
#include "SGraphNode.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"

namespace CourtyardCompactGraph
{
struct FRect
{
 UEdGraphNode* Node=nullptr;
 FVector2D Position,Size;
};
// Preserve rows/columns and their order, but replace unused strips by modest padding.
// Constraints use measured Slate bounds, not a fixed distance between node origins.
void CompactAxis(TArray<FRect>& Rects,bool Horizontal)
{
 const int32 Axis=Horizontal?0:1,Other=1-Axis;
 const double Padding=Horizontal?88:80;
 TArray<int32> Order;for(int32 I=0;I<Rects.Num();++I)Order.Add(I);
 Order.StableSort([&](int32 A,int32 B){return Rects[A].Position[Axis]<Rects[B].Position[Axis];});
 TArray<int32> Done;double Previous=-16;
 for(int32 Begin=0;Begin<Order.Num();)
 {
  int32 End=Begin+1;const double Original=Rects[Order[Begin]].Position[Axis];
  while(End<Order.Num()&&Rects[Order[End]].Position[Axis]==Original)++End;
  double Position=Previous+16;
  for(int32 I=Begin;I<End;++I)
  {
   const auto& Current=Rects[Order[I]];
   for(int32 J:Done)
   {
    const auto& Earlier=Rects[J];
    if(Current.Position[Other]<Earlier.Position[Other]+Earlier.Size[Other]+24&&
       Current.Position[Other]+Current.Size[Other]+24>Earlier.Position[Other])
     Position=FMath::Max(Position,Earlier.Position[Axis]+Earlier.Size[Axis]+Padding);
   }
  }
  Position=FMath::CeilToDouble(Position/16)*16;
  for(int32 I=Begin;I<End;++I){Rects[Order[I]].Position[Axis]=Position;Done.Add(Order[I]);}
  Previous=Position;Begin=End;
 }
}
FBox2D Bounds(const TArray<FRect>& Rects)
{
 FBox2D Result(ForceInit);for(const auto& R:Rects){Result+=R.Position;Result+=R.Position+R.Size;}return Result;
}
int32 Overlaps(const TArray<FRect>& Rects)
{
 int32 Count=0;
 for(int32 I=0;I<Rects.Num();++I)for(int32 J=I+1;J<Rects.Num();++J)
  if(FBox2D(Rects[I].Position,Rects[I].Position+Rects[I].Size).Intersect(FBox2D(Rects[J].Position,Rects[J].Position+Rects[J].Size)))++Count;
 return Count;
}
// Everything relevant to the node logic is recorded before moving anything.
FString Wiring(const TArray<FRect>& Rects)
{
 FString Result;
 for(const auto& R:Rects)
 {
  Result+=R.Node->NodeGuid.ToString()+R.Node->GetClass()->GetPathName();
  for(auto* P:R.Node->Pins)
  {
   Result+=P->PinId.ToString()+P->PinName.ToString()+P->DefaultValue+P->DefaultTextValue.ToString()+GetPathNameSafe(P->DefaultObject);
   for(auto* To:P->LinkedTo)Result+=To->GetOwningNode()->NodeGuid.ToString()+To->PinId.ToString();
  }
 }
 return Result;
}
bool HasExecution(UEdGraphNode* Node)
{
 for(auto* Pin:Node->Pins)if(Pin->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec)return true;
 return false;
}
// Move inputs next to the consumers that use them, instead of retaining their old
// global row order. Search free vertical slots locally; never change a wire.
void BringInputsCloser(TArray<FRect>& Rects)
{
 TMap<UEdGraphNode*,int32> Indices;for(int32 I=0;I<Rects.Num();++I)Indices.Add(Rects[I].Node,I);
 const auto Free=[&](int32 Id,double Y)
 {
  const auto& R=Rects[Id];
  for(int32 J=0;J<Rects.Num();++J)if(J!=Id)
  {
   const auto& Other=Rects[J];
   if(R.Position.X<Other.Position.X+Other.Size.X+40&&R.Position.X+R.Size.X+40>Other.Position.X&&
      Y<Other.Position.Y+Other.Size.Y+64&&Y+R.Size.Y+64>Other.Position.Y)return false;
  }
  return true;
 };
 const auto PlaceNear=[&](int32 Id,double Desired)
 {
  auto& R=Rects[Id];Desired=FMath::CeilToDouble(Desired/16)*16;
  if(Desired>=R.Position.Y)return;
  TArray<double> Choices{Desired};
  for(int32 J=0;J<Rects.Num();++J)if(J!=Id)
  {
   const auto& Other=Rects[J];
   if(R.Position.X<Other.Position.X+Other.Size.X+40&&R.Position.X+R.Size.X+40>Other.Position.X)
   {
    const double Y=FMath::CeilToDouble((Other.Position.Y+Other.Size.Y+80)/16)*16;
    if(Y>=Desired&&Y<R.Position.Y)Choices.Add(Y);
   }
  }
  Choices.Sort();for(double Y:Choices)if(Y<R.Position.Y&&Free(Id,Y)){R.Position.Y=Y;break;}
 };
 for(int32 Pass=0;Pass<4;++Pass)
 {
  TSet<int32> Seen;
  TFunction<void(int32)> Inputs=[&](int32 Consumer)
  {
   if(Seen.Contains(Consumer))return;Seen.Add(Consumer);
   for(auto* Pin:Rects[Consumer].Node->Pins)if(Pin->Direction==EGPD_Input&&Pin->PinType.PinCategory!=UEdGraphSchema_K2::PC_Exec)
    for(auto* Source:Pin->LinkedTo)
     if(const int32* Id=Indices.Find(Source->GetOwningNode());Id&&!HasExecution(Rects[*Id].Node))
     {
      if(!Seen.Contains(*Id))PlaceNear(*Id,Rects[Consumer].Position.Y+80);
      Inputs(*Id);
     }
  };
  TArray<int32> Flow;
  for(int32 I=0;I<Rects.Num();++I)if(HasExecution(Rects[I].Node))Flow.Add(I);
  Flow.StableSort([&](int32 A,int32 B){return Rects[A].Position.Y==Rects[B].Position.Y?Rects[A].Position.X<Rects[B].Position.X:Rects[A].Position.Y<Rects[B].Position.Y;});
  for(int32 Id:Flow)Inputs(Id);
  // Unconnected leftovers should also sit in a nearby free space, not far below.
  for(int32 I=0;I<Rects.Num();++I)if(!Seen.Contains(I)&&!HasExecution(Rects[I].Node)){PlaceNear(I,0);Inputs(I);}
 }
 // Execution reroutes can be shared by several rows, so move those independently.
 for(int32 I=0;I<Rects.Num();++I)if(Cast<UK2Node_Knot>(Rects[I].Node))
 {
  double Desired=MAX_dbl;
  for(auto* P:Rects[I].Node->Pins)if(P->Direction==EGPD_Output)
   for(auto* To:P->LinkedTo)if(const int32* J=Indices.Find(To->GetOwningNode()))Desired=FMath::Min(Desired,Rects[*J].Position.Y+32);
  if(Desired==MAX_dbl)
   for(auto* P:Rects[I].Node->Pins)if(P->Direction==EGPD_Input)
    for(auto* To:P->LinkedTo)if(const int32* J=Indices.Find(To->GetOwningNode()))Desired=FMath::Min(Desired,Rects[*J].Position.Y+64);
  PlaceNear(I,Desired==MAX_dbl?0:Desired);
 }
}
// Keep horizontal execution rows intact, but place each row near its parent.
// Exclusive input trees travel with their row; shared inputs remain independent.
void BringRowsCloser(TArray<FRect>& Rects)
{
 TMap<UEdGraphNode*,int32> Index;TMap<int32,int32> RowIds;
 struct FRow {int32 OriginalY,Anchor;TArray<int32> Members;};TArray<FRow> Rows;
 TMap<int32,int32> Membership;
 for(int32 I=0;I<Rects.Num();++I)
 {
  Index.Add(Rects[I].Node,I);
  if(!HasExecution(Rects[I].Node)||Cast<UK2Node_Knot>(Rects[I].Node))continue;
  const int32 Y=FMath::RoundToInt(Rects[I].Position.Y);
  if(!RowIds.Contains(Y))RowIds.Add(Y,Rows.Add({Y,I,{}}));
  const int32 Row=RowIds.FindChecked(Y);Rows[Row].Members.Add(I);Membership.Add(I,Row);
 }
 for(int32 I=0;I<Rects.Num();++I)if(!Membership.Contains(I))
 {
  TSet<UEdGraphNode*> Seen;TSet<int32> Targets;TArray<UEdGraphNode*> Queue{Rects[I].Node};
  for(int32 Q=0;Q<Queue.Num();++Q)
  {
   auto* N=Queue[Q];if(Seen.Contains(N))continue;Seen.Add(N);
   for(auto* P:N->Pins)if(P->Direction==EGPD_Output)for(auto* To:P->LinkedTo)
   {
    auto* Other=To->GetOwningNode();const int32* J=Index.Find(Other);if(!J)continue;
    // Stop only at real execution nodes, not at earlier assigned input nodes.
    if(HasExecution(Other)&&!Cast<UK2Node_Knot>(Other))Targets.Add(Membership.FindChecked(*J));
    else Queue.Add(Other);
   }
  }
  if(Targets.Num()==1){const int32 Row=*Targets.CreateConstIterator();Rows[Row].Members.Add(I);Membership.Add(I,Row);}
 }
 TArray<int32> Order;for(int32 I=0;I<Rows.Num();++I)Order.Add(I);
 Order.Sort([&](int32 A,int32 B){return Rows[A].OriginalY<Rows[B].OriginalY;});
 for(int32 RowId:Order)
 {
  const auto& Row=Rows[RowId];const double OldY=Rects[Row.Anchor].Position.Y;double Desired=0;
  TSet<int32> Members;for(int32 I:Row.Members)Members.Add(I);
  for(int32 I:Row.Members)if(HasExecution(Rects[I].Node)&&!Cast<UK2Node_Knot>(Rects[I].Node))
   for(auto* P:Rects[I].Node->Pins)if(P->Direction==EGPD_Input&&P->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec)
   {
    TArray<UEdGraphNode*> Parents;for(auto* Source:P->LinkedTo)Parents.Add(Source->GetOwningNode());TSet<UEdGraphNode*> Seen;
    for(int32 K=0;K<Parents.Num();++K)
    {
     auto* N=Parents[K];if(Seen.Contains(N))continue;Seen.Add(N);
     if(Cast<UK2Node_Knot>(N)){for(auto* Pin:N->Pins)if(Pin->Direction==EGPD_Input)for(auto* Source:Pin->LinkedTo)Parents.Add(Source->GetOwningNode());continue;}
     const int32* J=Index.Find(N);if(!J)continue;const int32* ParentRow=Membership.Find(*J);
     if(ParentRow&&Rows[*ParentRow].OriginalY<Row.OriginalY)Desired=FMath::Max(Desired,Rects[*J].Position.Y+Rects[*J].Size.Y+80);
    }
   }
  for(int32 I:Row.Members)Desired=FMath::Max(Desired,OldY-Rects[I].Position.Y);
  Desired=FMath::CeilToDouble(Desired/16)*16;if(Desired>=OldY)continue;
  TArray<double> Choices{Desired};
  for(int32 I:Row.Members)for(int32 J=0;J<Rects.Num();++J)if(!Members.Contains(J))
  {
   const auto& R=Rects[I];const auto& Other=Rects[J];
   if(R.Position.X<Other.Position.X+Other.Size.X+40&&R.Position.X+R.Size.X+40>Other.Position.X)
   {
    const double Y=FMath::CeilToDouble((Other.Position.Y+Other.Size.Y+80-(R.Position.Y-OldY))/16)*16;
    if(Y>=Desired&&Y<OldY)Choices.Add(Y);
   }
  }
  Choices.Sort();
  for(double Y:Choices)
  {
   const double Delta=Y-OldY;bool Fits=true;
   for(int32 I:Row.Members)
   {
    const auto& R=Rects[I];
    for(int32 J=0;J<Rects.Num();++J)if(!Members.Contains(J))
    {
     const auto& Other=Rects[J];
     if(R.Position.X<Other.Position.X+Other.Size.X+40&&R.Position.X+R.Size.X+40>Other.Position.X&&
        R.Position.Y+Delta<Other.Position.Y+Other.Size.Y+64&&R.Position.Y+Delta+R.Size.Y+64>Other.Position.Y){Fits=false;break;}
    }
    if(!Fits)break;
   }
   if(Fits){for(int32 I:Row.Members)Rects[I].Position.Y+=Delta;break;}
  }
 }
}
void Apply()
{
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/AutoCreateMap/Courtyard/ISMPrototype/Blueprints/BP_Courtyard_Generator_ISM.BP_Courtyard_Generator_ISM"));check(BP);
 TArray<UEdGraph*> Graphs;for(UEdGraph* G:BP->UbergraphPages)Graphs.Add(G);for(UEdGraph* G:BP->FunctionGraphs)Graphs.Add(G);
 TArray<TSharedPtr<FJsonValue>> Reports;
 for(UEdGraph* Graph:Graphs)
 {
  struct FComment {UEdGraphNode_Comment* Node;TSet<UEdGraphNode*> Members;};TArray<FComment> Comments;
  TArray<FRect> Original;
  for(UEdGraphNode* N:Graph->Nodes)
  {
   if(auto* Comment=Cast<UEdGraphNode_Comment>(N)){Comments.Add({Comment,{}});continue;}
   const auto Widget=FNodeFactory::CreateNodeWidget(N);check(Widget.IsValid());Widget->SlatePrepass(1.f);
   const FVector2D Desired=Widget->GetDesiredSize();
   check(Desired.X>0&&Desired.Y>0);
   Original.Add({N,FVector2D(N->NodePosX,N->NodePosY),Desired});
  }
  if(Original.IsEmpty())continue;
  for(auto& Comment:Comments)
   for(const auto& R:Original)
    if(FBox2D(FVector2D(Comment.Node->NodePosX,Comment.Node->NodePosY),FVector2D(Comment.Node->NodePosX+Comment.Node->NodeWidth,Comment.Node->NodePosY+Comment.Node->NodeHeight)).IsInside(R.Position))Comment.Members.Add(R.Node);
  const FString Before=Wiring(Original);
  auto A=Original,B=Original;
  CompactAxis(A,true);CompactAxis(A,false);
  CompactAxis(B,false);CompactAxis(B,true);
  const auto Area=[](const TArray<FRect>& R){const auto S=Bounds(R).GetSize();return S.X*S.Y;};
  auto Selected=Overlaps(A)<=Overlaps(B)?(Overlaps(A)==Overlaps(B)&&Area(B)<Area(A)?B:A):B;
  BringInputsCloser(Selected);
  for(int32 Pass=0;Pass<3;++Pass){BringRowsCloser(Selected);BringInputsCloser(Selected);}
  check(Overlaps(Selected)==0);
  for(const auto& R:Selected){R.Node->NodePosX=FMath::RoundToInt(R.Position.X);R.Node->NodePosY=FMath::RoundToInt(R.Position.Y);}
  check(Before==Wiring(Selected));
  for(auto& C:Comments)if(!C.Members.IsEmpty())
  {
   FBox2D Group(ForceInit);for(const auto& R:Selected)if(C.Members.Contains(R.Node)){Group+=R.Position;Group+=R.Position+R.Size;}
   C.Node->NodePosX=FMath::FloorToInt(Group.Min.X)-48;C.Node->NodePosY=FMath::FloorToInt(Group.Min.Y)-64;
   C.Node->NodeWidth=FMath::CeilToInt(Group.GetSize().X)+96;C.Node->NodeHeight=FMath::CeilToInt(Group.GetSize().Y)+112;
  }
  auto Report=MakeShared<FJsonObject>();Report->SetStringField(TEXT("graph"),Graph->GetName());Report->SetNumberField(TEXT("nodes"),Selected.Num());
  const auto Old=Bounds(Original).GetSize(),New=Bounds(Selected).GetSize();
  Report->SetNumberField(TEXT("old_width"),Old.X);Report->SetNumberField(TEXT("old_height"),Old.Y);
  Report->SetNumberField(TEXT("width"),New.X);Report->SetNumberField(TEXT("height"),New.Y);Report->SetNumberField(TEXT("overlaps"),Overlaps(Selected));Report->SetBoolField(TEXT("wiring_unchanged"),Before==Wiring(Selected));
  TArray<TSharedPtr<FJsonValue>> Nodes;
  for(const auto& R:Selected)
  {
   auto Item=MakeShared<FJsonObject>();Item->SetStringField(TEXT("name"),R.Node->GetName());Item->SetStringField(TEXT("title"),R.Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
   Item->SetNumberField(TEXT("x"),R.Position.X);Item->SetNumberField(TEXT("y"),R.Position.Y);Item->SetNumberField(TEXT("w"),R.Size.X);Item->SetNumberField(TEXT("h"),R.Size.Y);Nodes.Add(MakeShared<FJsonValueObject>(Item));
  }
  Report->SetArrayField(TEXT("rects"),Nodes);Reports.Add(MakeShared<FJsonValueObject>(Report));
 }
 FBlueprintEditorUtils::MarkBlueprintAsModified(BP);FCompilerResultsLog Results;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Results);check(Results.NumErrors==0);
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
 check(UPackage::SavePackage(BP->GetOutermost(),BP,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args));
 auto Root=MakeShared<FJsonObject>();Root->SetArrayField(TEXT("graphs"),Reports);Root->SetNumberField(TEXT("compile_errors"),Results.NumErrors);
 FString Json;auto Writer=TJsonWriterFactory<>::Create(&Json);FJsonSerializer::Serialize(Root,Writer);
 FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectSavedDir()/TEXT("CourtyardCompactGraphReport.json")));
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_COMPACT_GRAPHS_SAVED graphs=%d"),Reports.Num());
}
static FAutoConsoleCommand Command(TEXT("CourtyardGraph.Compact"),TEXT("Compact only the positions of nodes in the owned ISM Blueprint."),FConsoleCommandDelegate::CreateStatic(&Apply));
}
#endif
