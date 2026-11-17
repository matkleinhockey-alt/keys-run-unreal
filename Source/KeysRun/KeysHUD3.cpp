// The last pass's overlays: the town's service scenes (web game/services/index.ts and services.css): the motel's fade to
// black with the room card, the Pink Flamingo's doorstep vignette (the rope, the bouncer's opinion, what is going on
// behind the door: neon, sweeping beams, a sparkler or a spotlight) and the scratch-off card.
#include "KeysHUD.h"
#include "KeysBoatPawn.h"
#include "KeysCharter.h"
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

// Charter Captain (placeholder UI): the marina's charter board with three requests, and the active charter's panel
// (who, what they want, progress and the clock)
void AKeysHUD::DrawCharter(AKeysBoatPawn* P)
{
	const AKeysCharter* Ch = AKeysCharter::Get(this);
	if (!Ch || !P || P->IsDiving() || P->IsAshore()) return;
	const FLinearColor Panel(.03f, .06f, .09f, .78f), Gold(1.f, .82f, .35f), White(.95f, .97f, 1.f);
	const float W = Canvas->SizeX;
	if (P->bFreeFish) return;
	// the trip summary: the whole screen's attention until Enter
	if (Ch->IsSummaryOpen())
	{
		const AKeysCharter::FTripSummary& S = Ch->GetSummary();
		const float SW = 760 * U, Rows = float(FMath::Min(S.Fish.Num(), 8)), SH = (370 + Rows * 26 + (S.bLate || S.bFailed ? 26 : 0) + (S.Fish.Num() > 8 ? 24 : 0)) * U, X = (W - SW) * .5f, Y = (Canvas->SizeY - SH) * .5f;
		DrawRect(FLinearColor(0, 0, 0, .55f), 0, 0, W, Canvas->SizeY);
		DrawRect(FLinearColor(.03f, .06f, .09f, .95f), X, Y, SW, SH);
		const FLinearColor Edge = S.bFailed ? FLinearColor(.95f, .35f, .3f) : S.bLate ? FLinearColor(1.f, .6f, .25f) : FLinearColor(.25f, .85f, .45f);
		DrawRect(Edge, X, Y, SW, 5 * U);
		float Yy = Y + 16 * U;
		const FString Head = S.bFailed ? FString::Printf(TEXT("CHARTER FAILED - %s"), *S.FailReason) : S.bLate ? FString(TEXT("BACK LATE - TRIP OVER")) : FString(TEXT("TRIP SUMMARY"));
		Text(Head, X + 22 * U, Yy, 1.15f, Edge); Yy += 34 * U;
		Text(FString::Printf(TEXT("%s  -  %s"), *S.Client, *S.Target), X + 22 * U, Yy, .8f, White); Yy += 30 * U;
		// stars
		FString Stars;
		for (int32 i = 0; i < 5; i++) Stars += i < S.Stars ? TEXT("\u2605") : TEXT("\u2606");
		Text(Stars, X + SW - 22 * U - 150 * U, Y + 14 * U, 1.4f, Gold);
		// the fish
		Text(FString::Printf(TEXT("Fish (%d counted of %d asked for)"), S.Counted, S.Required), X + 22 * U, Yy, .8f, Gold); Yy += 26 * U;
		if (!S.Fish.Num()) { Text(TEXT("No fish this trip."), X + 36 * U, Yy, .75f, FLinearColor(.7f, .75f, .8f)); Yy += 26 * U; }
		for (int32 i = 0; i < S.Fish.Num() && i < 8; i++)
		{
			const AKeysCharter::FTripFish& F = S.Fish[i];
			Text(FString::Printf(TEXT("%s %s, %.1f lb"), F.bCounted ? TEXT("\u2713") : TEXT("\u2717"), *F.Name, F.Weight), X + 36 * U, Yy, .75f, F.bCounted ? FLinearColor(.5f, 1.f, .6f) : FLinearColor(1.f, .55f, .5f));
			Text(F.Verdict, X + 380 * U, Yy, .75f, FLinearColor(.75f, .8f, .85f));
			Yy += 26 * U;
		}
		if (S.Fish.Num() > 8) { Text(FString::Printf(TEXT("+ %d more"), S.Fish.Num() - 8), X + 36 * U, Yy, .7f, FLinearColor(.7f, .75f, .8f)); Yy += 24 * U; }
		Yy += 8 * U;
		const int32 Used = FMath::RoundToInt(S.TimeUsed), Lim = FMath::RoundToInt(S.TimeLimit);
		Text(FString::Printf(TEXT("Time used  %d:%02d of %d:%02d%s"), Used / 60, Used % 60, Lim / 60, Lim % 60, S.bLate ? TEXT("  (ran out)") : TEXT("")), X + 22 * U, Yy, .85f, S.bLate ? FLinearColor(1.f, .6f, .3f) : White); Yy += 34 * U;
		auto Line = [&](const FString& L, const FString& V, const FLinearColor& C)
		{
			Text(L, X + 22 * U, Yy, .82f, C);
			Text(V, X + SW - 160 * U, Yy, .82f, C);
			Yy += 26 * U;
		};
		auto Usd = [](int32 V) { return FText::AsNumber(V).ToString(); };
		Line(TEXT("Base pay"), TEXT("$") + Usd(S.BasePay), White);
		if (S.Counted < S.Required && !S.bFailed) Line(FString::Printf(TEXT("  short of the count: paid for %d of %d"), S.Counted, S.Required), FString::Printf(TEXT("x%.2f"), S.Completion), FLinearColor(1.f, .7f, .4f));
		Line(TEXT("Tip - time left on the clock"), TEXT("+$") + Usd(S.TipTime), FLinearColor(.6f, .9f, 1.f));
		Line(FString::Printf(TEXT("Tip - big fish (%d)"), S.BigFish), TEXT("+$") + Usd(S.TipBig), FLinearColor(.6f, .9f, 1.f));
		Line(FString::Printf(TEXT("Tip - extra fish (%d)"), S.ExtraFish), TEXT("+$") + Usd(S.TipExtra), FLinearColor(.6f, .9f, 1.f));
		if (S.bLate) Line(TEXT("Penalty - back late"), TEXT("pay halved"), FLinearColor(1.f, .55f, .45f));
		if (S.bFailed) Line(TEXT("Penalty - failed release"), TEXT("no pay"), FLinearColor(1.f, .55f, .45f));
		Yy += 6 * U;
		DrawRect(FLinearColor(1, 1, 1, .15f), X + 22 * U, Yy, SW - 44 * U, 2 * U); Yy += 10 * U;
		Text(TEXT("Total paid"), X + 22 * U, Yy, 1.1f, Gold);
		Text(TEXT("$") + Usd(S.Total), X + SW - 160 * U, Yy, 1.1f, Gold); Yy += 38 * U;
		Text(FString::Printf(TEXT("Money  $%s     -     Enter to close and pick your next charter"), *FText::AsNumber(P->Cash).ToString()), X + 22 * U, Y + SH - 30 * U, .72f, FLinearColor(.75f, .8f, .85f));
		return;
	}
	// your money, at the marina
	if (Ch->IsAtMarina())
	{
		const float MW = 220 * U, MX = W - MW - 24 * U, MY = 250 * U;
		DrawRect(Panel, MX, MY - 6 * U, MW, 32 * U);
		Text(FString::Printf(TEXT("Money  $%s"), *FText::AsNumber(P->Cash).ToString()), MX + 12 * U, MY, .9f, Gold);
	}
	if (Ch->IsBoardShown() && Ch->NumOffers())
	{
		const float BW = 520 * U, RowH = 114 * U, BH = 64 * U + RowH * Ch->NumOffers() + 30 * U, X = W - BW - 24 * U, Y = 285 * U;
		DrawRect(Panel, X, Y, BW, BH);
		DrawRect(Gold, X, Y, BW, 4 * U);
		Text(FString::Printf(TEXT("CHARTER BOARD - %s"), *Ch->MarinaName()), X + 16 * U, Y + 14 * U, 1.05f, Gold);
		for (int32 i = 0; i < Ch->NumOffers(); i++)
		{
			const FCharterRequest* R = Ch->Offer(i);
			if (!R) continue;
			const float RY = Y + 52 * U + i * RowH;
			DrawRect(FLinearColor(1, 1, 1, .05f), X + 10 * U, RY, BW - 20 * U, RowH - 8 * U);
			Text(FString::Printf(TEXT("F%d  %s"), i + 1, *R->ClientName), X + 20 * U, RY + 6 * U, .9f, White);
			Text(FString::Printf(TEXT("$%d"), R->BasePay), X + BW - 90 * U, RY + 6 * U, .9f, Gold);
			Text(R->RequestText, X + 20 * U, RY + 30 * U, .72f, FLinearColor(.8f, .86f, .92f));
			Text(FString::Printf(TEXT("%s  -  %.0f min"), *AKeysCharter::TargetLine(*R), R->TimeLimitMinutes), X + 20 * U, RY + 54 * U, .72f, FLinearColor(.6f, .85f, 1.f));
			Text(AKeysCharter::WhereLine(*R), X + 20 * U, RY + 78 * U, .68f, FLinearColor(.55f, .95f, .7f));
		}
		Text(TEXT("F1 / F2 / F3 to take a charter"), X + 16 * U, Y + BH - 26 * U, .72f, FLinearColor(.7f, .75f, .8f));
	}
	if (Ch->HasActiveCharter())
	{
		const FCharterRequest& R = Ch->GetActiveCharter();
		const float BW = 460 * U, BH = 150 * U, X = W - BW - 24 * U, Y = 285 * U;
		DrawRect(Panel, X, Y, BW, BH);
		DrawRect(FLinearColor(.2f, .75f, .55f), X, Y, 4 * U, BH);
		Text(FString::Printf(TEXT("CHARTER - %s"), *R.ClientName), X + 16 * U, Y + 10 * U, .9f, Gold);
		Text(R.RequestText, X + 16 * U, Y + 36 * U, .72f, White);
		Text(AKeysCharter::TargetLine(R), X + 16 * U, Y + 58 * U, .72f, FLinearColor(.6f, .85f, 1.f));
		Text(AKeysCharter::WhereLine(R), X + 16 * U, Y + 124 * U, .66f, FLinearColor(.55f, .95f, .7f));
		const int32 Sec = FMath::CeilToInt32(Ch->SecondsLeft());
		if (Ch->IsFailed())
			Text(FString::Printf(TEXT("Charter failed - %s"), *Ch->GetFailReason()), X + 16 * U, Y + 86 * U, 1.0f, FLinearColor(1.f, .4f, .35f));
		else if (Ch->IsComplete())
			Text(TEXT("Head back to the marina!"), X + 16 * U, Y + 84 * U, 1.2f, FLinearColor(.4f, 1.f, .6f));
		else
		{
			Text(FString::Printf(TEXT("%d / %d %s"), Ch->GetProgress(), R.RequiredCount, *AKeysCharter::ShortTarget(R, R.RequiredCount)), X + 16 * U, Y + 84 * U, 1.2f, White);
			const FString Clock = Ch->IsTimerRunning() ? FString::Printf(TEXT("%d:%02d"), Sec / 60, Sec % 60) : FString::Printf(TEXT("%d:%02d - starts when you leave the marina"), Sec / 60, Sec % 60);
			Text(Clock, X + 230 * U, Y + 88 * U, .85f, Ch->IsTimerRunning() && Sec < 600 ? FLinearColor(1.f, .45f, .35f) : FLinearColor(.85f, .9f, .95f));
		}
		Text(TEXT("F4 cancel"), X + BW - 90 * U, Y + BH - 22 * U, .6f, FLinearColor(.6f, .65f, .7f));
	}
	// the verdict on the last catch: big, in the middle, fading out
	if (Ch->ResultT > 0 && !Ch->ResultText.IsEmpty())
	{
		const float A = float(FMath::Min(1.0, Ch->ResultT / .6));
		const float Cx = W * .5f, BW = 640 * U, BH = 112 * U, Y = Canvas->SizeY * .2f;
		DrawRect(FLinearColor(.02f, .04f, .06f, .8f * A), Cx - BW * .5f, Y, BW, BH);
		const FLinearColor Edge = Ch->bResultGood ? FLinearColor(.25f, .85f, .45f, A) : FLinearColor(.95f, .35f, .3f, A);
		DrawRect(Edge, Cx - BW * .5f, Y, BW, 5 * U);
		FLinearColor TC = Edge; TC.A = A;
		Text(Ch->ResultText, Cx, Y + 18 * U, 1.7f, TC, GEngine->GetLargeFont(), true);
		Text(Ch->ResultSub, Cx, Y + 74 * U, .9f, FLinearColor(.9f, .93f, .96f, A), nullptr, true);
	}
}
