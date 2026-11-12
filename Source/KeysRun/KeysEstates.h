#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysEstates.generated.h"

class UProceduralMeshComponent;
class UTextRenderComponent;

// Keys Run Estates, ported from the web game's shared/content/estates-plat.ts (235 parcels of buildable shore, from
// Harbor Row behind the casino to the islands off Miami, cut from the real outlines), content/estates.ts (tiers and
// pricing, the founders' lots, the build options, the dock site search and the site layout), the client's
// ui/estates/panel.ts (the land office: chart, deed and build sheet) and entities/estates/** (what stands on a lot:
// villa, fishing shack, Japanese garden, stilt house or modern glass; no dock, a skiff dock, a sportfish slip or a
// superyacht T-head with a helipad; the biggest catch hung over the door, on a gallows at the dock or on a pedestal;
// trophies round the grounds; a pool; a fire pit). Up to three lots per angler, bought from the wallet.
UCLASS()
class KEYSRUN_API AKeysEstates : public AActor
{
	GENERATED_BODY()

public:
	AKeysEstates();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	static AKeysEstates* Get(const UObject* WorldContext);

	enum class ETier : uint8 { Harbor, Private, Point, Oceanfront, Bayfront, Biscayne, Mainland };
	struct FParcel { FString Id, Name, Island, Side; ETier Tier; double CX, CZ, Area, Frontage; TArray<FVector2D> Poly; double X0, X1, Z0, Z1; };
	struct FBuild { FString Res = TEXT("villa"), Dock = TEXT("sport"), Fish = TEXT("marlin"), Mount = TEXT("gallows"); double Lb = 512; int32 Trophies = 3; bool bPool = true, bFire = false; };
	struct FLayout
	{
		double Heading = 0, SX = 0, SZ = 1;
		double HX = 0, HZ = 0, HW = 10, HD = 10, HY = 0;
		bool bDock = false; double DX0 = 0, DZ0 = 0, DX1 = 0, DZ1 = 0, DW = 0, DLen = 0, THead = 0;
		double MX = 0, MZ = 0;
		bool bPool = false; double PX = 0, PZ = 0;
		bool bFire = false; double FX = 0, FZ = 0;
		TArray<FVector2D> Trophies;
	};

	const TArray<FParcel>& Parcels() const { return All; }
	static FString TierLabel(ETier T);
	static FString TierBlurb(ETier T);
	static FLinearColor TierColor(ETier T);
	int64 PriceOf(const FParcel& P) const;
	const FParcel* ById(const FString& Id) const;
	const FParcel* ParcelAt(double X, double Z) const;
	FString Founder(const FString& Id) const;   // "Klein" / "Larry" for the founders' lots, else empty
	FBuild BuildOf(const FString& Id) const;
	static FString BuildToString(const FBuild& B);
	FBuild BuildFromString(const FParcel& P, const FString& S) const;
	FBuild DefaultBuild(const FParcel& P) const;
	FLayout Layout(const FParcel& P, const FBuild& B) const;
	// the land office
	FString Buy(const FString& Id);
	void SetBuild(const FString& Id, const FBuild& B);
	void Refresh();   // rebuild what stands on every held lot

	// ---- the panel (HUD draws it, the pawn drives it): regions, the selected lot, the build sheet's row
	bool bOpen = false;
	int32 Region = 0, Sel = 0, SheetRow = -1;
	TArray<int32> RegionList;   // parcel indices in the current region, west to east
	static const TArray<FString>& RegionNames();
	void Open(bool bOn);
	void SetRegion(int32 R);
	void HandleInput(class APlayerController* PC);
	void RegionBox(double& X0, double& Z0, double& Size) const;
	const FParcel* Selected() const { return RegionList.IsValidIndex(Sel) ? &All[RegionList[Sel]] : nullptr; }

private:
	bool DockSite(const FParcel& P, double& OX, double& OZ, double& OSX, double& OSZ, TArray<bool>& Wet) const;
	void Seaward(const FParcel& P, double& SX, double& SZ) const;
	bool Dry(const FParcel& P, double X, double Z) const;
	void BuildLot(const FParcel& P, const FBuild& B, struct FKeysMesh& M, TArray<TPair<FVector, FString>>& Signs);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> LotMeshes;
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> LotSigns;
	TArray<FParcel> All;
	TMap<FString, int32> Index;
	TMap<int32, double> MedianArea;
	bool bDirty = true;
	FString Built;
};
