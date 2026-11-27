#if WITH_EDITOR
#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Event.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Knot.h"
#include "K2Node_IfThenElse.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Generation/Courtyard/Layout/CourtyardLayout.h"

namespace CourtyardRoutesEditor
{
bool Exec(const UEdGraphPin* P){return P->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec;}
bool Flow(UEdGraphNode* N){for(auto* P:N->Pins)if(Exec(P))return true;return false;}
UEdGraphPin* InExec(UEdGraphNode* N){for(auto* P:N->Pins)if(Exec(P)&&P->Direction==EGPD_Input)return P;return nullptr;}
bool UsesMember(UEdGraphPin* P,FName Name)
{
 if(!P)return false;
 for(auto* Source:P->LinkedTo)if(auto* Get=Cast<UK2Node_VariableGet>(Source->GetOwningNode());Get&&Get->VariableReference.GetMemberName()==Name)return true;
 return false;
}
void AddRoutePass(UBlueprint* BP)
{
 UEdGraph* G=nullptr;for(UEdGraph* Graph:BP->FunctionGraphs)if(Graph->GetName()==TEXT("EnsureFreeEndConnections"))G=Graph;
 check(G);
 for(UEdGraphNode* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N);C&&C->FunctionReference.GetMemberName()==TEXT("ImproveMazeConnections"))return;
 UK2Node_MacroInstance* FinalLoop=nullptr;
 for(UEdGraphNode* N:G->Nodes)
 {
  auto* Macro=Cast<UK2Node_MacroInstance>(N);if(!Macro||!UsesMember(Macro->FindPin(TEXT("Array")),TEXT("Selected")))continue;
  auto* Body=Macro->FindPin(TEXT("LoopBody"));if(!Body)continue;
  TArray<UEdGraphNode*> Queue;TSet<UEdGraphNode*> Seen;
  for(auto* P:Body->LinkedTo)Queue.Add(P->GetOwningNode());
  for(int32 I=0;I<Queue.Num()&&I<24;++I)
  {
   auto* Current=Queue[I];if(Seen.Contains(Current))continue;Seen.Add(Current);
   if(auto* C=Cast<UK2Node_CallFunction>(Current);C&&UsesMember(C->FindPin(TEXT("TargetArray")),TEXT("WorkLinks"))){FinalLoop=Macro;break;}
   for(auto* P:Current->Pins)if(Exec(P)&&P->Direction==EGPD_Output)for(auto* To:P->LinkedTo)Queue.Add(To->GetOwningNode());
  }
  if(FinalLoop)break;
 }
 check(FinalLoop);auto* Input=InExec(FinalLoop);check(Input&&Input->LinkedTo.Num()==1);auto* Previous=Input->LinkedTo[0];
 auto* Call=NewObject<UK2Node_CallFunction>(G);G->AddNode(Call,false,false);Call->CreateNewGuid();
 Call->SetFromFunction(UCourtyardLayoutLibrary::StaticClass()->FindFunctionByName(TEXT("ImproveMazeConnections")));Call->AllocateDefaultPins();
 Call->NodeComment=TEXT("Prefer hidden exits and multi-space return loops; preserve connected free ends.");Call->bCommentBubbleVisible=true;
 const auto* Schema=GetDefault<UEdGraphSchema_K2>();Input->BreakAllPinLinks();
 check(Schema->TryCreateConnection(Previous,Call->FindPinChecked(TEXT("execute"))));check(Schema->TryCreateConnection(Call->FindPinChecked(TEXT("then")),Input));
 for(const auto& Pair:TArray<TPair<FName,FName>>{{TEXT("WorkNodes"),TEXT("Nodes")},{TEXT("Candidates"),TEXT("Candidates")},{TEXT("Selected"),TEXT("Selected")},{TEXT("Degrees"),TEXT("Degrees")},{TEXT("WorkSettings"),TEXT("Settings")},{TEXT("WorkSeed"),TEXT("Seed")}})
 {
  auto* Get=NewObject<UK2Node_VariableGet>(G);G->AddNode(Get,false,false);Get->CreateNewGuid();Get->VariableReference.SetSelfMember(Pair.Key);Get->AllocateDefaultPins();
  check(Schema->TryCreateConnection(Get->FindPinChecked(Pair.Key),Call->FindPinChecked(Pair.Value)));
 }
}

// Positions only, apart from exec reroutes on false branches. No conditions,
// defaults, execution order or existing data connections are changed here.
struct FLayout
{
 UEdGraph* Graph;TSet<UEdGraphNode*> Placed;TArray<FBox2D> Occupied;int32 NextLane=1600,MaxY=0;
 FVector2D Size(UEdGraphNode* N) const
 {
  int32 Inputs=0,Outputs=0;for(auto* P:N->Pins)if(!P->bHidden)(P->Direction==EGPD_Input?Inputs:Outputs)++;
  return FVector2D(Cast<UK2Node_Knot>(N)?60:340,FMath::Max(120,FMath::Max(Inputs,Outputs)*26+90)+(N->bCommentBubbleVisible?70:0));
 }
 void Place(UEdGraphNode* N,int32 X,int32 Y,bool Avoid=false)
 {
  const auto S=Size(N);
  if(Avoid)
  {
   bool Again=true;while(Again){Again=false;const FBox2D Box(FVector2D(X-30,Y-30),FVector2D(X+S.X+30,Y+S.Y+30));for(const auto& Other:Occupied)if(Box.Intersect(Other)){Y=FMath::CeilToInt(Other.Max.Y)+100;Again=true;break;}}
  }
  N->NodePosX=X;N->NodePosY=Y;Placed.Add(N);Occupied.Add(FBox2D(FVector2D(X,Y),FVector2D(X+S.X,Y+S.Y)));MaxY=FMath::Max(MaxY,Y+int32(S.Y));
 }
 int32 Rank(const UEdGraphPin* P) const
 {
  const FString Name=P->PinName.ToString();
  if(Name==TEXT("then")||Name==TEXT("then_0")||Name==TEXT("Completed")||Name==TEXT("Finished")||Name==TEXT("Is Valid"))return 0;
  if(Name==TEXT("else")||Name==TEXT("Is Not Valid")||Name==TEXT("LoopBody")||Name==TEXT("Update"))return 2;
  return 1;
 }
 void Route(UEdGraphNode* N,int32 X,int32 Y)
 {
  if(Placed.Contains(N))return;Place(N,X,Y);
  TArray<UEdGraphPin*> Outputs;for(auto* P:N->Pins)if(Exec(P)&&P->Direction==EGPD_Output&&!P->LinkedTo.IsEmpty())Outputs.Add(P);
  Outputs.StableSort([&](const UEdGraphPin& A,const UEdGraphPin& B){return Rank(&A)<Rank(&B);});
  bool MainUsed=false;
  for(auto* P:Outputs)
  {
   const bool Side=Rank(P)==2||MainUsed;MainUsed=true;
   const auto Targets=P->LinkedTo;
   for(auto* To:Targets)
   {
    int32 TargetY=Y;
    if(Side){TargetY=NextLane;NextLane+=1600;}
    if(Side&&P->PinName==TEXT("else")&&!Cast<UK2Node_Knot>(To->GetOwningNode()))
    {
     auto* Knot=NewObject<UK2Node_Knot>(Graph);Graph->AddNode(Knot,false,false);Knot->CreateNewGuid();Knot->AllocateDefaultPins();
     Knot->GetInputPin()->PinType=P->PinType;Knot->GetOutputPin()->PinType=P->PinType;
     P->BreakLinkTo(To);P->MakeLinkTo(Knot->GetInputPin());Knot->GetOutputPin()->MakeLinkTo(To);
     Place(Knot,X+490,TargetY+35);
    }
    Route(To->GetOwningNode(),X+760,TargetY);
   }
  }
 }
 void Data(UEdGraphNode* N,int32 Depth=0)
 {
  if(Depth>100)return;
  int32 Slot=0;
  for(auto* P:N->Pins)if(!Exec(P)&&P->Direction==EGPD_Input)
   for(auto* Source:P->LinkedTo)
   {
    auto* Upstream=Source->GetOwningNode();
    if(Placed.Contains(Upstream)||Flow(Upstream)||Cast<UEdGraphNode_Comment>(Upstream))continue;
    Place(Upstream,N->NodePosX-400,N->NodePosY+350+Slot*260,true);++Slot;Data(Upstream,Depth+1);
   }
 }
 void Run()
 {
  struct FComment {UEdGraphNode_Comment* Node;TArray<UEdGraphNode*> Members;};TArray<FComment> Comments;
  TArray<UEdGraphNode*> Original;for(UEdGraphNode* N:Graph->Nodes)Original.Add(N);
  for(auto* N:Original)if(auto* C=Cast<UEdGraphNode_Comment>(N))
  {
   FComment Group;Group.Node=C;
   for(auto* Member:Original)if(!Cast<UEdGraphNode_Comment>(Member)&&Member->NodePosX>=C->NodePosX&&Member->NodePosY>=C->NodePosY&&Member->NodePosX<C->NodePosX+C->NodeWidth&&Member->NodePosY<C->NodePosY+C->NodeHeight)Group.Members.Add(Member);
   Comments.Add(MoveTemp(Group));
  }
  TArray<UEdGraphNode*> Roots;
  for(auto* N:Original)if(Cast<UK2Node_FunctionEntry>(N)||Cast<UK2Node_Event>(N))Roots.Add(N);
  Roots.Sort([](const UEdGraphNode& A,const UEdGraphNode& B){return A.NodePosY==B.NodePosY?A.NodePosX<B.NodePosX:A.NodePosY<B.NodePosY;});
  for(auto* Root:Roots){const int32 Base=Placed.IsEmpty()?0:MaxY+1800;NextLane=Base+1600;Route(Root,0,Base);}
  TArray<UEdGraphNode*> Ordered=Placed.Array();Ordered.Sort([](const UEdGraphNode& A,const UEdGraphNode& B){return A.NodePosY==B.NodePosY?A.NodePosX<B.NodePosX:A.NodePosY<B.NodePosY;});
  for(auto* N:Ordered)Data(N);
  int32 X=0,Y=MaxY+1000;
  for(auto* N:Original)if(!Placed.Contains(N)&&!Cast<UEdGraphNode_Comment>(N)){Place(N,X,Y,true);Data(N);X+=800;}
  for(auto& C:Comments)if(!C.Members.IsEmpty())
  {
   FBox2D Bounds(ForceInit);for(auto* Member:C.Members){Bounds+=FVector2D(Member->NodePosX,Member->NodePosY);Bounds+=FVector2D(Member->NodePosX,Member->NodePosY)+Size(Member);}
   C.Node->NodePosX=FMath::FloorToInt(Bounds.Min.X)-80;C.Node->NodePosY=FMath::FloorToInt(Bounds.Min.Y)-100;
   C.Node->NodeWidth=FMath::CeilToInt(Bounds.GetSize().X)+160;C.Node->NodeHeight=FMath::CeilToInt(Bounds.GetSize().Y)+180;
  }
 }
};
void Apply()
{
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/AutoCreateMap/Courtyard/ISMPrototype/Blueprints/BP_Courtyard_Generator_ISM.BP_Courtyard_Generator_ISM"));if(!BP)return;
 AddRoutePass(BP);
 for(UEdGraph* G:BP->UbergraphPages){FLayout Layout;Layout.Graph=G;Layout.Run();}
 for(UEdGraph* G:BP->FunctionGraphs){FLayout Layout;Layout.Graph=G;Layout.Run();}
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FCompilerResultsLog Results;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Results);check(Results.NumErrors==0);
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
 check(UPackage::SavePackage(BP->GetOutermost(),BP,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args));
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_ROUTES_AND_LAYOUT_APPLIED"));
}
static FAutoConsoleCommand Command(TEXT("CourtyardRoutes.Apply"),TEXT("Improve return routes and arrange the owned Blueprint left-to-right."),FConsoleCommandDelegate::CreateStatic(&Apply));
}
#endif
