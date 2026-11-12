#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysTreasure.generated.h"

class UProceduralMeshComponent;

// The wreck treasure hunt, ported from the web game's shared/sim/treasure.ts (each wreck carries two to five
// containers - the purser's strongbox and then coins, cargo crates and ammunition cans - placed by a hashed search for
// level sand beside the hull, their loot hashed from the world seed and scaled by the depth) and the client's
// entities/life/wreck-treasure.ts and treasure-store.ts (the boxes on the bottom, half buried, the prompt, F to prise
// one open from 3.5 m, and the emptied set kept in the save). Cash goes to the wallet, rounds to the pistol.
UCLASS()
class KEYSRUN_API AKeysTreasure : public AActor
{
	GENERATED_BODY()

public:
	AKeysTreasure();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	static AKeysTreasure* Get(const UObject* WorldContext);

	enum class EKind : uint8 { Strongbox, Coins, Crate, AmmoCan };
	struct FBox { FString Id; int32 Wreck; int32 Index; EKind Kind; double X, Z, Yaw, Buried, Standoff; };
	struct FLoot { int32 Cash = 0, Ammo = 0; FString Label; };

	const TArray<FBox>& Boxes() const { return All; }
	FLoot LootOf(const FBox& B) const;
	// F under water: open what is in reach; the prompt for the HUD
	bool TryOpen();
	FString Prompt;
	bool bPromptKey = false;
	// test hook: the diver to the first unopened box
	bool DebugNearest(double& OX, double& OZ, double& OY) const;

private:
	void Rebuild();
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Mesh;
	TArray<FBox> All;
	int32 Reach = -1;
	double ReachDist = 0;
	TSet<FString> BuiltOpen;
	bool bDirty = true;
};
