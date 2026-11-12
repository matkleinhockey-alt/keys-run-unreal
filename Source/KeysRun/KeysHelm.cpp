// The boat's lights, ported from the web game's entities/boat/model.ts (red and green sidelights on the bow, the white
// all-round light up top, the underwater lights in the boat's teal under the hull and their glow on the water, and the
// blue cockpit lights at night). ; switches the underwater and cockpit lights (the web game's L, which here logs you
// in). The helm's instruments are in KeysDash.cpp.
#include "KeysBoatPawn.h"
#include "KeysKit.h"
#include "KeysGeo.h"
#include "KeysMesh.h"
#include "KeysWorld.h"
#include "ProceduralMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

using namespace KeysGeo;
using namespace KeysKit;

namespace
{
	const FLinearColor Teal = Hex(0x25e8d2);
}

void AKeysBoatPawn::ToggleLights()
{
	bUwLights = !bUwLights;
	ShowToast(bUwLights ? TEXT("Underwater lights on.") : TEXT("Underwater lights off."));
}

void AKeysBoatPawn::UpdateHelm(double Dt, double T)
{
	if (!BoatLights)
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
		BoatLights = Make(TEXT("BoatLights"));
		if (UMaterialInterface* GM = AKeysWorld::GlowMaterial()) LightMID = UMaterialInstanceDynamic::Create(GM, this);
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
	BoatLights->SetVisibility(bShow);
	// ---- the lights: the sidelights and the all-round white always burn (brighter at dusk); the underwater and cockpit lights on ;
	const double L = S.Len, MidX = KeysMesh::BoatLayout(S).MidX;
	FKeysMesh G;
	{
		const double BX = L * .3, Half = KeysMesh::InnerHalfBeam(S, BX) + .2, NavY = KeysMesh::SheerAt(S, BX) - .1;
		const float K = float(.35 + .65 * Dusk);
		G.AddGlowBlob(FVector(BX, -Half, NavY), .08 + .35 * Dusk, FLinearColor(1.f, .16f, .16f, K));
		G.AddGlowBlob(FVector(BX, Half, NavY), .08 + .35 * Dusk, FLinearColor(.16f, 1.f, .35f, K));
		FVector Tower;
		const KeysMesh::FBoatLayout Lo = KeysMesh::BoatLayout(S);
		const double TopZ = KeysMesh::TowerEye(S, Tower) && Tower.Z > Lo.TopZ + 2.5 ? Tower.Z + 1.2 : Lo.TopZ + .45;
		G.AddGlowBlob(FVector(Lo.TopX - .1, 0, TopZ), .06 + .3 * Dusk, FLinearColor(1.f, 1.f, .95f, K));
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
	// ---- the instrument panel: gauges, the two big screens and the throttle (KeysDash.cpp)
	UpdateDash(Dt, T);
}
