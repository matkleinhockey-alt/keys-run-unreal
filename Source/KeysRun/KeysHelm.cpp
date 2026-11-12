// The glass helm and the boat's lights, ported from the web game's entities/boat/electronics.ts (the chartplotter and
// fish finder screens on the console: depth bands, the track, the boat, the bottom return and fish arches) and
// entities/boat/model.ts (red and green sidelights on the bow, the white all-round light up top, the underwater lights
// in the boat's teal under the hull and their glow on the water, and the blue cockpit lights at night). ; switches the
// underwater and cockpit lights (the web game's L, which here logs you in).
#include "KeysBoatPawn.h"
#include "KeysKit.h"
#include "KeysGeo.h"
#include "KeysMesh.h"
#include "KeysLife.h"
#include "KeysWorld.h"
#include "ProceduralMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

using namespace KeysGeo;
using namespace KeysKit;

namespace
{
	constexpr int32 ScrW = 44, ScrH = 28;
	const FLinearColor Teal = Hex(0x25e8d2);
	FLinearColor Band(double D)
	{
		// the chartplotter's depth bands (electronics.ts): land, 0-6 ft, 6-12, 12-20, 20-66, 66-200 and deeper
		if (D <= .36) return Hex(0xc8b27a);
		if (D < 1.8) return Hex(0xbfe8f5);
		if (D < 3.6) return Hex(0x8fd3ee);
		if (D < 6) return Hex(0x5bb5e0);
		if (D < 20) return Hex(0x2f86c4);
		if (D < 60) return Hex(0x1a5a99);
		return Hex(0x0d3466);
	}
}

void AKeysBoatPawn::ToggleLights()
{
	bUwLights = !bUwLights;
	ShowToast(bUwLights ? TEXT("Underwater lights on.") : TEXT("Underwater lights off."));
}

void AKeysBoatPawn::UpdateHelm(double Dt, double T)
{
	if (!HelmScreens)
	{
		auto Make = [this](const TCHAR* Name)
		{
			UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(this, FName(Name));
			C->SetupAttachment(GetRootComponent());
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(false);
			C->RegisterComponent();
			return C;
		};
		HelmScreens = Make(TEXT("HelmScreens"));
		BoatLights = Make(TEXT("BoatLights"));
		if (UMaterialInterface* GM = AKeysWorld::GlowMaterial())
		{
			ScreenMID = UMaterialInstanceDynamic::Create(GM, this);
			LightMID = UMaterialInstanceDynamic::Create(GM, this);
		}
		for (int32 i = 0; i < 3; i++)
		{
			UPointLightComponent* L = NewObject<UPointLightComponent>(this);
			L->SetupAttachment(GetRootComponent());
			L->SetCastShadows(false);
			L->SetLightColor(i < 2 ? Teal : FLinearColor(.16f, .36f, 1.f));
			L->SetAttenuationRadius(i < 2 ? 650 : 450);
			L->RegisterComponent();
			UwLights.Add(L);
		}
	}
	const KeysData::FBoatSpec& S = Spec();
	const double Dusk = AKeysWorld::Dusk(this);
	const bool bShow = bRunning && !IsAshore();
	HelmScreens->SetVisibility(bShow);
	BoatLights->SetVisibility(bShow);
	// ---- the lights: the sidelights and the all-round white always burn (brighter at dusk); the underwater and cockpit lights on ;
	const double L = S.Len, MidX = .05 * L;
	FKeysMesh G;
	{
		const double BX = L * .3, Half = KeysMesh::InnerHalfBeam(S, BX) + .2, NavY = KeysMesh::SheerAt(S, BX) - .1;
		const float K = float(.35 + .65 * Dusk);
		G.AddGlowBlob(FVector(BX, -Half, NavY), .08 + .35 * Dusk, FLinearColor(1.f, .16f, .16f, K));
		G.AddGlowBlob(FVector(BX, Half, NavY), .08 + .35 * Dusk, FLinearColor(.16f, 1.f, .35f, K));
		FVector Tower;
		const double TopZ = KeysMesh::TowerEye(S, Tower) ? Tower.Z + 1.2 : KeysMesh::SoleAt(S, MidX) + 2.75;
		G.AddGlowBlob(FVector(MidX - .6, 0, TopZ), .06 + .3 * Dusk, FLinearColor(1.f, 1.f, .95f, K));
	}
	const double SternX = KeysMesh::SternX(S);
	const bool bUw = bUwLights && bShow;
	if (bUw)
	{
		// the pucks along the bottom and their teal pools on the water
		for (double Tt : {.08, .32, .58})
			for (int32 sx = -1; sx <= 1; sx += 2)
			{
				const double PX = SternX + Tt * L, Hb = KeysMesh::InnerHalfBeam(S, PX) + .15;
				G.AddGlowBlob(FVector(PX, sx * Hb, -.2), .09, FLinearColor(.8f, 1.f, .98f, 1.f));
				const int32 From = G.V.Num();
				const double R = 2.6;
				G.AddQuad(FVector(PX - R, sx * Hb * 1.6 - R, .04), FVector(PX + R, sx * Hb * 1.6 - R, .04), FVector(PX + R, sx * Hb * 1.6 + R, .04), FVector(PX - R, sx * Hb * 1.6 + R, .04), FLinearColor(Teal.R, Teal.G, Teal.B, float(.18 + .25 * Dusk)), FVector(0, 0, 1));
				for (int32 i = From; i < G.UV.Num(); i++) { const FVector& V = G.V[i]; G.UV[i] = FVector2D(2.0 + (V.X / 100 - (PX - R)) / (2 * R), (V.Y / 100 - (sx * Hb * 1.6 - R)) / (2 * R)); }
			}
		// the cockpit: a blue line inside the gunwales at knee height
		for (int32 sx = -1; sx <= 1; sx += 2)
			for (int32 k = 0; k < 8; k++)
			{
				const double X0 = SternX + (.04 + k * .07) * L, X1 = SternX + (.04 + (k + 1) * .07) * L;
				const int32 From = G.V.Num();
				G.AddTube(FVector(X0, sx * (KeysMesh::InnerHalfBeam(S, X0) - .04), KeysMesh::SoleAt(S, X0) + .36), FVector(X1, sx * (KeysMesh::InnerHalfBeam(S, X1) - .04), KeysMesh::SoleAt(S, X1) + .36), .012, .012, 4, FLinearColor(.11f, .28f, .78f, 1.f));
				for (int32 i = From; i < G.UV.Num(); i++) G.UV[i] = FVector2D(2.5, .5);
			}
	}
	G.Commit(BoatLights, 0, LightMID ? (UMaterialInterface*)LightMID : AKeysWorld::GlowMaterial());
	if (LightMID) LightMID->SetScalarParameterValue(TEXT("Bright"), float(.9 + 1.4 * Dusk));
	for (int32 i = 0; i < UwLights.Num(); i++)
	{
		UPointLightComponent* Lt = UwLights[i];
		const bool bOn = bUw && (i < 2 || Dusk > .3);
		Lt->SetVisibility(bOn);
		if (!bOn) continue;
		if (i < 2) { Lt->SetRelativeLocation(FVector(SternX + .7 + i * L * .2, 0, -1.1) * 100); Lt->SetIntensity(float(250 + 900 * Dusk)); }
		else { Lt->SetRelativeLocation(FVector(MidX - 1.0, 0, KeysMesh::SoleAt(S, MidX) + 2.2) * 100); Lt->SetIntensity(float(120 * Dusk)); }
	}
	// ---- the glass helm: two screens on the dash, the chart on the left and the sounder on the right, a few times a second
	HelmT -= Dt;
	if (HelmT > 0) return;
	HelmT = .2;
	SonarHist.Add(float(DepthHere()));
	if (SonarHist.Num() > ScrW) SonarHist.RemoveAt(0);
	const double CZ = KeysMesh::SoleAt(S, MidX), SW = .34, SH = SW * ScrH / ScrW, Tilt = .45;
	FKeysMesh M;
	auto Screen = [&](double Y0, TFunctionRef<FLinearColor(int32, int32)> Px)
	{
		// a bezel, then the pixels, standing on the dash leaning back toward the helm
		const FVector C(MidX + .08, Y0, CZ + 1.38);
		const FVector Ux(0, 1, 0), Uy(FMath::Sin(Tilt), 0, FMath::Cos(Tilt)), Out(-FMath::Cos(Tilt), 0, FMath::Sin(Tilt));
		const FVector B0 = C - Ux * (SW / 2 + .015) - Uy * (SH / 2 + .015) + Out * .002;
		M.AddQuad(B0, B0 + Ux * (SW + .03), B0 + Ux * (SW + .03) + Uy * (SH + .03), B0 + Uy * (SH + .03), FLinearColor(.02f, .02f, .025f, 1.f), Out);
		for (int32 j = 0; j < ScrH; j++)
			for (int32 i = 0; i < ScrW; i++)
			{
				const FVector P0 = C - Ux * (SW / 2) - Uy * (SH / 2) + Out * .004 + Ux * (SW * i / ScrW) + Uy * (SH * (ScrH - 1 - j) / ScrH);
				const int32 From = M.V.Num();
				M.AddQuad(P0, P0 + Ux * (SW / ScrW), P0 + Ux * (SW / ScrW) + Uy * (SH / ScrH), P0 + Uy * (SH / ScrH), Px(i, j), Out);
				for (int32 k = From; k < M.UV.Num(); k++) M.UV[k] = FVector2D(2.5, .5);
			}
	};
	// the chartplotter: heading up, 400 m to the edge, depth bands, the boat in the middle
	const double Range = FMath::Clamp(ChartRange, 120.0, 2400.0), Hd = H;
	Screen(-.19, [&](int32 i, int32 j) -> FLinearColor
	{
		const double U = (i + .5) / ScrW - .5, V = (j + .5) / ScrH - .5;
		if (FMath::Abs(U) < .03 && FMath::Abs(V) < .045) return FLinearColor(1.f, .2f, .15f, 1.f);
		const double Fx = -FMath::Sin(Hd), Fz = -FMath::Cos(Hd), Rx = FMath::Cos(Hd), Rz = -FMath::Sin(Hd);
		const double Along = -V * Range * 2 * ScrH / ScrW, Across = U * Range * 2;
		const double GX = X + Fx * Along + Rx * Across, GZ = Z + Fz * Along + Rz * Across;
		FLinearColor C = Band(LandH(GX, GZ) > .1 ? 0 : DepthFast(GX, GZ));
		C.A = 1;
		return C * .85f;
	});
	// the fish finder: the last few seconds of bottom, fish as yellow and red marks
	TArray<FVector> Fish;
	TArray<double> Lens;
	if (const AKeysLife* Lf = AKeysLife::Get(this)) Lf->FishNear(X, Z, 12, Fish, Lens);
	const double MaxD = FMath::Max(6.0, FMath::Max(SonarHist.Num() ? FMath::Max(SonarHist) : 6.f, 6.f) * 1.3);
	Screen(.19, [&](int32 i, int32 j) -> FLinearColor
	{
		const int32 Col = SonarHist.Num() - ScrW + i;
		if (Col < 0) return FLinearColor(.02f, .05f, .12f, 1.f);
		const double D = (j + .5) / ScrH * MaxD, Bottom = SonarHist[Col];
		if (D > Bottom + MaxD * .08) return FLinearColor(.35f, .12f, .05f, 1.f);
		if (D > Bottom) return FLinearColor(.95f, .3f, .1f, 1.f);
		if (i == ScrW - 1)
			for (int32 k = 0; k < Fish.Num(); k++)
				if (FMath::Abs(-Fish[k].Y - D) < MaxD * .06 + Lens[k] * .2) return Lens[k] > 1 ? FLinearColor(1.f, .25f, .2f, 1.f) : FLinearColor(1.f, .9f, .2f, 1.f);
		return j == 0 ? FLinearColor(.2f, .5f, .95f, 1.f) : FLinearColor(.02f, .07f, .16f, 1.f);
	});
	M.Commit(HelmScreens, 0, ScreenMID ? (UMaterialInterface*)ScreenMID : AKeysWorld::GlowMaterial());
	if (ScreenMID) ScreenMID->SetScalarParameterValue(TEXT("Bright"), float(.55 + .25 * Dusk));
}
