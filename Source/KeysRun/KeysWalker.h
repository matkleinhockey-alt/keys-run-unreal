#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeysHuman.h"
#include "KeysWalker.generated.h"

class UProceduralMeshComponent;

struct FKeysWalkButtons
{
	bool bFwd = false, bBack = false, bLeft = false, bRight = false, bSprint = false;
	double Turn = 0;                     // -1 / 0 / +1 from the arrow keys
	double LookYaw = 0, LookPitch = 0;   // mouse deltas this frame, radians
};

// Land mode: tie up at a dock (Q) and walk ashore. Ported from the web game's entities/land/** (controller.ts, camera.ts,
// input.ts) and shared/sim/walker.ts (the walker step: walk / run, accel time constant, slide along colliders, step-height
// limit). You can walk the docks of all four marinas, Boot Key Harbor City's pad and its buildings, and the dry land of the keys.
UCLASS()
class KEYSRUN_API AKeysWalker : public AActor
{
	GENERATED_BODY()

public:
	AKeysWalker();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	static AKeysWalker* Get(const UObject* WorldContext);

	bool IsAshore() const { return bAshore; }
	// tie up: true and on the dock, or false and why not
	bool TryEnter(double BoatX, double BoatZ, double BoatH, double BoatSpeed, double HullLen, FString& OutWhy);
	// Q ashore: board if close to the boat; otherwise the first press warns and a second within 4 s takes a cab back to the dock
	enum class EBoard : uint8 { Boarded, Far, Cab };
	EBoard RequestBoard();
	void ExitToBoat();
	void SetButtons(const FKeysWalkButtons& B) { Btn = B; }
	FString CycleView();
	// test hooks: put the walker at a world spot / a town-local spot and face a way
	bool DebugPlace(double PX, double PZ, double InYaw);
	bool DebugPlaceLocal(double U, double V, double InYaw);
	void DebugLook(double InYaw, double InPitch) { CamYaw = InYaw; CamPitch = InPitch; }

	void GetCamera(FVector& OutPos, FRotator& OutRot, float& OutFovH) const { OutPos = CamPosUE; OutRot = CamRotUE; OutFovH = CamFovH; }

	// ---- read by the HUD and the pawn
	FVector Pos() const { return FVector(X, Y, Z); }
	double BodyYaw() const { return Yaw; }
	const FString& Prompt() const { return PromptText; }
	const FString& NearId() const { return NearInteract; }
	const FString& NearVenue() const { return NearVenueId; }
	const FString& NearText() const { return NearPlaque; }
	FString PlaceName() const;
	FString BoatBearing() const;
	double BoardDistance() const;
	bool bShopOpen = false;
	FString ShopVenue;
	int32 ShopSel = 0;
	FString ShopGreeting;
	int32 Visits = 0;
	// what the walker wears ashore (gift-shop cosmetics)
	void SetOutfit(const FLinearColor& Shirt, bool bCap, const FLinearColor& Cap, bool bGlasses);

private:
	void Step(double Dt);
	bool CanStand(double PX, double PZ, double FromY, double& OutY) const;
	void UpdateCamera(double Dt);
	void UpdateBody();
	void FindInteract();

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Body;
	// Indoors: the dark rooms (the club, the casino, the bars) hold the exposure near what it is outside, so auto
	// exposure doesn't lift a dim room into a washed-out grey; the other shops get a gentler clamp.
	UPROPERTY() TObjectPtr<class UPostProcessComponent> Indoor;
	double IndoorW = 0;
	void UpdateIndoor(double Dt);
public:
	static double IndoorEV;   // the dark rooms' locked exposure (EV100); test step indoorev=
private:

	bool bAshore = false;
	double X = 0, Y = 0, Z = 0, VX = 0, VZ = 0, Yaw = 0, Gait = 0, Speed = 0;
	double CamYaw = 0, CamPitch = -.05;
	int32 CamMode = 1;   // 0 first person, 1 third person
	FVector CamPosUE = FVector::ZeroVector;
	FRotator CamRotUE = FRotator::ZeroRotator;
	float CamFovH = 90;
	double Fov = 72;
	double Boom = 3.7;
	FKeysWalkButtons Btn;
	// where you tied up
	double DockX = 0, DockZ = 0, MoorX = 0, MoorZ = 0, MoorH = 0, HullL = 8;
	double FarPressedAt = -100, Clock = 0;
	FString PromptText, NearInteract, NearVenueId, NearPlaque;
	FKeysHuman Me;
};
