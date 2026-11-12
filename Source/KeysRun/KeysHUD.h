#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "KeysHUD.generated.h"

class UTexture2D;
class UFont;
class AKeysBoatPawn;

// fish finder history: one column per ping, depths in meters
struct FKeysSonarMark { float Depth; float Px; FLinearColor Color; };   // Px: mark height in the JS canvas's pixels (192 tall)
struct FKeysSonarCol { float Bottom = 0; TArray<FKeysSonarMark> Marks; };
struct FKeysSonarFish { double X, Z, Depth, Size; };

// Start screen, gauges, chart, fight meters and the catch card, drawn straight onto the canvas.
UCLASS()
class KEYSRUN_API AKeysHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;
	virtual void NotifyHitBoxBeginCursorOver(FName BoxName) override;

	// Other systems (schools, bait, wildlife) can put targets on the fish finder: call every frame for anything under the water.
	// Game meters; Depth is below the surface; Size about 1 for a mid-size fish (bigger draws redder and taller).
	void AddSonarContact(double X, double Z, double Depth, double Size) { Contacts.Add({X, Z, Depth, Size}); }

private:
	void BuildChart();
	void DrawMenu(AKeysBoatPawn* P);
	void DrawControlsTable(float X, float Y, float W);
	// a clickable rectangle: draws the panel and registers a hit box named Box
	void Button(FName Box, float X, float Y, float W, float H, bool bHot, int32 Priority = 0);
	void DrawGauges(AKeysBoatPawn* P);
	void DrawChart(AKeysBoatPawn* P);
	void DrawFishing(AKeysBoatPawn* P);
	void DrawCard(AKeysBoatPawn* P);
	void DrawDiving(AKeysBoatPawn* P, class AKeysDiver* D);
	void DrawExtras(AKeysBoatPawn* P);
	// chartplotter + fish finder (the JS ELEC block) and the local leaderboard
	void UpdateElectronics(AKeysBoatPawn* P, double Now);
	void PushSonarColumn(AKeysBoatPawn* P);
	void RenderGps(AKeysBoatPawn* P);
	void DrawElectronics(AKeysBoatPawn* P);
	void DrawGps(AKeysBoatPawn* P, float X, float Y, float W, float H);
	void DrawSonar(AKeysBoatPawn* P, float X, float Y, float W, float H);
	void DrawBoard(AKeysBoatPawn* P);
	void DrawCrewBoard(float X, float Y, float W);
	void DrawLogin(AKeysBoatPawn* P);
	void DrawFriends(AKeysBoatPawn* P);

	void Panel(float X, float Y, float W, float H, float Alpha = .5f);
	float Text(const FString& S, float X, float Y, float Scale, const FLinearColor& C = FLinearColor::White, UFont* Font = nullptr, bool bCenter = false);
	float Wrapped(const FString& S, float X, float Y, float W, float Scale, const FLinearColor& C = FLinearColor::White);
	void Bar(float X, float Y, float W, float H, float Fill, const FLinearColor& C);
	FVector2D ChartXY(double GX, double GZ, float X, float Y, float Size) const;

	UPROPERTY() TObjectPtr<UTexture2D> Chart;
	UPROPERTY() TObjectPtr<UTexture2D> GpsTex;
	UPROPERTY() TObjectPtr<UTexture2D> VignetteTex;
	TArray<FColor> GpsPx;
	double GpsCX = 0, GpsCZ = 0, GpsRange = -1, GpsT = 0;   // where the GPS texture was last drawn
	TArray<FKeysSonarCol> SonarCols;
	TArray<FKeysSonarFish> SonarFish, Contacts;
	TArray<FVector2D> Trail;   // the boat's track, one point a second
	double SonMax = 20, SonT = 0, TrailT = 0, LastNow = -1;
	float U = 1;   // UI scale (1 at 1080p)
};
