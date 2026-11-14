#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysMesh.h"
#include "KeysHuman.h"
#include "KeysDancers.h"
#include "KeysTown.generated.h"

class UProceduralMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;
class UMaterialInterface;
namespace KeysTown { struct FLot; }

// Builds town geometry in the web game's three.js town frame (x = u, y up, z = -v), so the web builders (build.ts, props.ts,
// walls.ts, interiors.ts, casino-*.ts) port almost line for line. Vertices go out in the component's frame: UE (x, -v... ) meters.
struct FTownBatch
{
	FKeysMesh M;
	// rotations are three.js Euler angles in 'YXZ' order, radians
	void Box(double CX, double CY, double CZ, double W, double H, double D, uint32 Color, double RX = 0, double RY = 0, double RZ = 0);
	void BoxC(double CX, double CY, double CZ, double W, double H, double D, const FLinearColor& Color, double RX = 0, double RY = 0, double RZ = 0);
	void Span(double X0, double X1, double Y0, double Y1, double Z0, double Z1, uint32 Color);
	void SpanC(double X0, double X1, double Y0, double Y1, double Z0, double Z1, const FLinearColor& Color);
	void Cyl(double CX, double Y0, double CZ, double RTop, double RBot, double H, uint32 Color, int32 Seg = 8, double RX = 0, double RY = 0, double RZ = 0);
	void Ball(double CX, double CY, double CZ, double R, uint32 Color, double SY = 1, int32 Seg = 8);
	// a triangular prism roof over a W x D footprint, ridge along z (or x if bAlongX)
	void Gable(double CX, double Y0, double CZ, double W, double D, double Rise, uint32 Color, bool bAlongX = false);
	// a flat one-sided rectangle facing +y (ground decals) from local ranges
	void Flat(double X0, double X1, double Z0, double Z1, double Y, const FLinearColor& Color);
	// a vertical rectangle in the plane z = Z, facing +z (bFront) or -z
	void Facade(double X0, double X1, double Y0, double Y1, double Z, bool bFront, const FLinearColor& Color);
	// a vertical rectangle in the plane x = X, facing +x or -x
	void SideFace(double Z0, double Z1, double Y0, double Y1, double X, bool bPlus, const FLinearColor& Color);
	void Tri3(const FVector& A3, const FVector& B3, const FVector& C3, const FVector& N3, const FLinearColor& Col);
	void Quad3(const FVector& A3, const FVector& B3, const FVector& C3, const FVector& D3, const FVector& N3, const FLinearColor& Col);
	void Tube3(const FVector& A3, const FVector& B3, double RA, double RB, int32 Sides, uint32 Color);
	// a hip roof over a W x D rectangle turned RY about the vertical, eaves at Y0, ridge Rise above them
	void HipRoof(double CX, double Y0, double CZ, double W, double D, double Rise, const FLinearColor& Color, double RY);
	// a smooth-shaded triangle with its own vertex normals (frame as above)
	void TriN(const FVector& A3, const FVector& B3, const FVector& C3, const FVector& NA, const FVector& NB, const FVector& NC, const FLinearColor& Col);
	// A coconut palm standing at Base3: a curving, tapering ringed trunk with a flared foot, a crown of arching fronds
	// whose leaflets hang in a V from the rachis, and a cluster of nuts. Lean is the top's offset (x, z) as a fraction of
	// the height; Seed varies the rest.
	void Palm(const FVector& Base3, double Ht, double LeanX, double LeanZ, double Seed, int32 Fronds = 11);
	int32 Tris() const { return M.T.Num() / 3; }
};

namespace KeysTownPaint
{
	FLinearColor Hex(uint32 C);
	FLinearColor Fade(uint32 C, double K = .14);
	// a figure's outfit by the web game's look name (people.ts humanOptionsFor)
	FKeysHumanLook LookFor(const FString& Look, int32 Variant);
}

// Boot Key Harbor City, Marathon: the town built in the web game, ported from apps/client/src/entities/town/** and
// world/town.ts. The pad, seawall and promenade; Harbor Drive, Palm Avenue, the back lane and Sombrero Boulevard; sixteen
// buildings with their paint, signs, awnings, hero signs and set dressing; walk-in interiors with counters, shelves and
// staff; the Gaff & Gill Casino with its aquarium wall, slot banks and marlin fountain; the Pink Flamingo with its stage and
// dancers; parked cars, palms, lamps, benches and people walking the sidewalks. Everything is built once at BeginPlay from
// KeysTownData and is hidden when the camera is far away; interiors and people only draw when you are close.
UCLASS()
class KEYSRUN_API AKeysTown : public AActor
{
	GENERATED_BODY()

public:
	AKeysTown();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	static AKeysTown* Get(const UObject* WorldContext);

	// the player's walking position (game meters), so people can step aside and interiors can light up; bAshore false = at sea
	void SetWalker(bool bAshore, const FVector& PosGame) { bWalkerAshore = bAshore; WalkerPos = PosGame; }
	// Harbor Row: rebuild the lots after a purchase (owned ids)
	void SetOwnedEstates(const TArray<FString>& Owned);
	// set from the time-of-day blend: 0 day .. 1 sunset/night (neon brightness)
	void SetDusk(double K);

private:
	void BuildGround(FTownBatch& S);
	void BuildPadEdges(FTownBatch& S);
	void BuildLot(const KeysTown::FLot& L, FTownBatch& S, FTownBatch& G);
	void BuildFacade(const KeysTown::FLot& L, FTownBatch& S, FTownBatch& G);
	void BuildNotices(const KeysTown::FLot& L, const TArray<FVector>& Panes, double DR, double F, FTownBatch& S);
	void BuildInteriors(FTownBatch& I, FTownBatch& G);
	void BuildCasinoInterior(FTownBatch& I, FTownBatch& G);
	void BuildProps(FTownBatch& S, FTownBatch& G);
	void BuildCars(FTownBatch& S);
	void BuildEstates();
	void BuildFar(FTownBatch& F);
	void AddSign(const FString& Text, double U, double Y, double V, double Size, const FLinearColor& Color, double YawDeg = 0, bool bInterior = false, double Width = 0);
	void UpdatePeople(double T, double Dt, const FVector& CamGame);
	// the room you are in gets real lights at its ceiling lamps (a small pool moved from room to room): warm white in the
	// shops, amber in the bars, pink in the Pink Flamingo with a key light on its stage (KeysTownInterior.cpp)
	void UpdateRoomLights(const KeysTown::FLot* L, const FVector& StageAt);
	const KeysTown::FLot* LitLot = nullptr;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Near;      // the street: pad, roads, buildings, props, cars
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Inside;    // interiors (drawn when you are close)
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Glow;      // neon, lit windows, bulbs (unlit)
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Far;       // bare boxes for the skyline
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> People;    // everyone in town, rebuilt each frame when close
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Estates;   // Harbor Row
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Tank;      // the casino aquarium's fish (animated)
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> Signs;
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> InteriorSigns;
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Lights;
	UPROPERTY() TObjectPtr<UMaterialInterface> GlowMat;

	struct FPerson
	{
		FKeysHuman H;
		FString Look;
		FString Inside;      // lot id: drawn only near / inside
		double U = 0, V = 0, Y = 0, Yaw = 0;
		// walkers: a loop of points along the sidewalks, a speed and where along it they are
		TArray<FVector2D> Path;
		double Speed = 0, S0 = 0, Len = 0;
		bool bDancer = false, bSeated = false;
		double Gait = 0;
	};
	TArray<FPerson> Folk;
	// the Pink Flamingo's stage dancers as the web game's rigged models (KeysDancers.h), one per dancer in Folk (by
	// order); empty when the models aren't there and the procedural dancers dance instead
	TArray<FKeysDancerFig> StageFigs;
	void HideStageFigs() { for (FKeysDancerFig& F : StageFigs) F.SetVisible(false); }
	TArray<FString> OwnedEstates;
	bool bWalkerAshore = false;
	FVector WalkerPos = FVector::ZeroVector;
	double Dusk = 0, AppliedDusk = -1;
	// the night glow (web entities/town/kit.ts lightMats): soft halos round the lamps and along the neon, and pools of
	// lamp light on the pavement, faded in by the sunset blend
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Halos;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> HaloMID;
	struct FHalo { FVector P; double R; FLinearColor C; bool bPool; };
	TArray<FHalo> HaloList;
	int32 Tier = -1;          // 0 near, 1 far, 2 hidden
	bool bInsideOn = false;
	double PeopleT = 0;
};
