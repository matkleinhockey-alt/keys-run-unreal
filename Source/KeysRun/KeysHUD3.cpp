// The last pass's overlays: the town's service scenes (web game/services/index.ts and services.css): the motel's fade to
// black with the room card, the Pink Flamingo's doorstep vignette (the rope, the bouncer's opinion, what is going on
// behind the door: neon, sweeping beams, a sparkler or a spotlight) and the scratch-off card.
#include "KeysHUD.h"
#include "KeysBoatPawn.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"

namespace
{
	const FLinearColor Dim3(.75f, .8f, .85f), Neon(1.f, .35f, .75f);
	float Ease(double T) { return float(FMath::Clamp(T, 0.0, 1.0)); }
}

void AKeysHUD::DrawServices(AKeysBoatPawn* P)
{
	const float W = Canvas->SizeX, H = Canvas->SizeY, Cx = W * .5f;
	const double Now = GetWorld()->GetTimeSeconds();
	UFont* Big = GEngine->GetLargeFont();
	// J from far off while diving: the fade back aboard
	if (P->ReboardAlpha() > 0) DrawRect(FLinearColor(0, 0, 0, float(P->ReboardAlpha())), 0, 0, W, H);
	// the motel: fade out, the room card on black, fade back in to a sunset already in place
	if (P->SleepAlpha() > 0)
	{
		DrawRect(FLinearColor(0, 0, 0, float(P->SleepAlpha())), 0, 0, W, H);
		if (P->SleepCard())
		{
			Text(TEXT("Sunset Motor Lodge"), Cx, H * .38f, 2.0f, FLinearColor(1.f, .62f, .35f), Big, true);
			Text(TEXT("Room 7. The ice machine hums all night."), Cx, H * .38f + 80 * U, 1.1f, FLinearColor::White, nullptr, true);
			Text(TEXT("You wake up to sunset."), Cx, H * .38f + 120 * U, 1.1f, FLinearColor::White, nullptr, true);
			Text(TEXT("Progress saved."), Cx, H * .38f + 160 * U, .9f, Dim3, nullptr, true);
		}
	}
	const AKeysBoatPawn::FSvcCard& C = P->SvcCard;
	if (!C.bOn) return;
	const double A = C.Age;
	const float In = Ease(A / .5) * Ease((C.Life - A) / .6);
	if (C.Kind == TEXT("scratch"))
	{
		// the card: six cells scratched off one by one, then the result
		const float CW = 620 * U, CH = 560 * U, X = Cx - CW * .5f, Y = H * .5f - CH * .5f;
		DrawRect(FLinearColor(0, 0, 0, .45f * In), 0, 0, W, H);
		DrawRect(FLinearColor(.98f, .93f, .7f, In), X, Y, CW, CH);
		DrawRect(FLinearColor(.85f, .2f, .25f, In), X, Y, CW, 70 * U);
		Text(C.Title, Cx, Y + 14 * U, 1.7f, FLinearColor(1, 1, 1, In), Big, true);
		const float GW = 150 * U, GH = 110 * U, GX = Cx - GW * 1.5f - 12 * U, GY = Y + 96 * U;
		for (int32 i = 0; i < C.Cells.Num() && i < 6; i++)
		{
			const float CX0 = GX + (i % 3) * (GW + 12 * U), CY0 = GY + (i / 3) * (GH + 12 * U);
			const float Rev = Ease((A - .5 - i * .35) / .3);
			DrawRect(FLinearColor(.94f, .9f, .78f, In), CX0, CY0, GW, GH);
			Text(C.Cells[i].ToUpper(), CX0 + GW * .5f, CY0 + GH * .5f - 14 * U, 1.1f, FLinearColor(.2f, .3f, .45f, In * Rev), nullptr, true);
			// the silver coating, scratched away left to right
			if (Rev < 1) DrawRect(FLinearColor(.72f, .74f, .78f, In), CX0 + GW * Rev, CY0, GW * (1 - Rev), GH);
		}
		float Yy = GY + 2 * (GH + 12 * U) + 14 * U;
		if (A > 3)
		{
			Text(C.Prize > 0 ? FString::Printf(TEXT("$%d"), C.Prize) : FString(TEXT("No luck")), Cx, Yy, 2.0f, C.Prize > 0 ? FLinearColor(.1f, .55f, .2f, In) : FLinearColor(.45f, .3f, .3f, In), Big, true);
			Yy += 70 * U;
			for (int32 i = 0; i < C.Lines.Num(); i++)
			{
				const float La = In * Ease((A - 3.3 - i * .6) / .4);
				Yy += Wrapped(C.Lines[i], X + 30 * U, Yy, CW - 60 * U, .78f, FLinearColor(.2f, .2f, .25f, La)) + 6 * U;
			}
		}
		Text(TEXT("Space to put it away"), Cx, Y + CH - 34 * U, .8f, FLinearColor(.4f, .35f, .3f, In), nullptr, true);
		return;
	}
	// the cabaret's doorstep: a full-screen neon vignette
	DrawRect(FLinearColor(.06f, .0f, .05f, .9f * In), 0, 0, W, H);
	// sweeping stage beams from the top corners: soft translucent wedges
	for (int32 k = 0; k < 6; k++)
	{
		const double Ang = FMath::Sin(Now * (.6 + k * .13) + k) * .55 + (k % 2 ? .35 : -.35);
		const FVector2D S0(k % 2 ? W * .82f : W * .18f, -10 * U);
		const FVector2D Dir(float(FMath::Sin(Ang)), float(FMath::Cos(Ang)));
		const float Len = H * 1.2f;
		const FLinearColor BC = k % 3 == 0 ? FLinearColor(1.f, .3f, .7f, .16f * In) : k % 3 == 1 ? FLinearColor(.4f, .6f, 1.f, .13f * In) : FLinearColor(1.f, .85f, .4f, .11f * In);
		// a wedge of soft squares (Canvas lines ignore alpha)
		for (int32 q = 0; q < 90; q++)
		{
			const float F = q / 89.f, Sz = (10 + F * 130) * U;
			const FVector2D Q = S0 + Dir * Len * F;
			DrawRect(FLinearColor(BC.R, BC.G, BC.B, BC.A * .16f), Q.X - Sz * .5f, Q.Y - Sz * .5f, Sz, Sz);
		}
	}
	if (C.Kind == TEXT("spotlight"))
	{
		// the spotlight on you
		for (int32 r = 6; r >= 1; r--) { const float R = r * 50 * U; DrawRect(FLinearColor(1.f, .95f, .8f, .03f * In), Cx - R, H * .72f - R * .5f, 2 * R, R); }
	}
	if (C.Kind == TEXT("sparkler"))
	{
		// a sparkler: hot specks spraying from one point
		const float SX = Cx + 260 * U, SY = H * .62f;
		for (int32 k = 0; k < 60; k++)
		{
			const double Ph = FMath::Fmod(Now * 1.7 + k * .618, 1.0), Ang = k * 2.399 + Now * .3;
			const float R = float(Ph) * 140 * U;
			DrawRect(FLinearColor(1.f, .9f, .55f, (1 - float(Ph)) * In), SX + float(FMath::Cos(Ang)) * R, SY - float(FMath::Abs(FMath::Sin(Ang))) * R, 4 * U, 4 * U);
		}
		DrawRect(FLinearColor(.9f, .9f, .95f, In), SX - 2 * U, SY, 4 * U, 90 * U);
	}
	// the neon sign, flickering a little
	const float Fl = (FMath::Sin(Now * 23) > .93 ? .55f : 1.f) * In;
	for (int32 g = 3; g >= 1; g--)
		for (int32 d = 0; d < 4; d++)
		{
			const float Ox = (d % 2 ? 1 : -1) * g * 2.5f * U, Oy = (d / 2 ? 1 : -1) * g * 2.5f * U;
			Text(C.Title, Cx + Ox, H * .2f + Oy, 2.4f, FLinearColor(1.f, .2f, .6f, .1f * Fl), Big, true);
		}
	Text(C.Title, Cx, H * .2f, 2.4f, FLinearColor(1.f, .55f, .85f, Fl), Big, true);
	// the lines, one after another
	float Yy = H * .2f + 120 * U;
	for (int32 i = 0; i < C.Lines.Num(); i++)
	{
		const float La = In * Ease((A - .4 - i * .9) / .5);
		Yy += Wrapped(C.Lines[i], Cx - 420 * U, Yy, 840 * U, 1.05f, FLinearColor(1, 1, 1, La)) + 18 * U;
	}
	Text(TEXT("Space to go in"), Cx, H - 70 * U, .85f, FLinearColor(Neon.R, Neon.G, Neon.B, In * .8f), nullptr, true);
}
