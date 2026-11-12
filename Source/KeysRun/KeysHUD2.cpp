#include "KeysHUD.h"
#include "KeysBoatPawn.h"
#include "KeysKit.h"
#include "KeysGeo.h"
#include "KeysLocals.h"
#include "KeysHandgun.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"

using namespace KeysGeo;

namespace
{
	const FLinearColor Teal(.35f, .85f, .85f), Gold(1.f, .82f, .35f), Red(1.f, .3f, .25f), Dim(.75f, .8f, .85f);
}

// world tags and speech bubbles (the web game's DOM .tag): Larry, the pirates, the sandbar party, named boats
void AKeysHUD::DrawTags()
{
	TArray<FKeysTag>& Tags = KeysTags::Live();
	APlayerController* PC = GetOwningPlayerController();
	if (PC && Canvas)
	{
		FVector CamLoc; FRotator CamRot;
		PC->GetPlayerViewPoint(CamLoc, CamRot);
		for (const FKeysTag& Tg : Tags)
		{
			const FVector W = ToUE(Tg.Game.X, Tg.Game.Y, Tg.Game.Z);
			const double Dist = FVector::Dist(W, CamLoc) / 100;
			if (Dist > Tg.MaxDist || FVector::DotProduct(W - CamLoc, CamRot.Vector()) <= 0) continue;
			const FVector S = Project(W);
			if (S.Z <= 0 || S.X < -200 || S.X > Canvas->SizeX + 200) continue;
			UFont* F = GEngine->GetSmallFont();
			float TW = 0, TH = 0;
			GetTextSize(Tg.Text, TW, TH, F, .95f * U);
			if (Tg.bBubble)
			{
				// a white speech bubble with a little tail down to the speaker
				const float BW = FMath::Min(TW, 520 * U), Lines = FMath::CeilToFloat(TW / FMath::Max(1.f, BW));
				const float BX = S.X - BW * .5f - 12 * U, BY = S.Y - TH * Lines - 34 * U;
				DrawRect(FLinearColor(.97f, .97f, .94f, .92f), BX, BY, BW + 24 * U, TH * Lines + 14 * U);
				for (int32 k = 0; k < 8; k++) DrawRect(FLinearColor(.97f, .97f, .94f, .92f), S.X - (8 - k) * U, BY + TH * Lines + 14 * U + k * 1.5f * U, (16 - 2 * k) * U, 1.5f * U);
				if (Lines <= 1) DrawText(Tg.Text, FLinearColor(.08f, .08f, .1f), S.X - TW * .5f, BY + 7 * U, F, .95f * U);
				else
				{
					// wrap by words into the bubble
					TArray<FString> Words;
					Tg.Text.ParseIntoArray(Words, TEXT(" "));
					FString Line;
					float Y = BY + 7 * U;
					for (const FString& Wd : Words)
					{
						const FString Try = Line.IsEmpty() ? Wd : Line + TEXT(" ") + Wd;
						float LW = 0, LH = 0;
						GetTextSize(Try, LW, LH, F, .95f * U);
						if (LW > BW && !Line.IsEmpty()) { DrawText(Line, FLinearColor(.08f, .08f, .1f), BX + 12 * U, Y, F, .95f * U); Y += TH; Line = Wd; }
						else Line = Try;
					}
					if (!Line.IsEmpty()) DrawText(Line, FLinearColor(.08f, .08f, .1f), BX + 12 * U, Y, F, .95f * U);
				}
			}
			else
			{
				Panel(S.X - TW * .5f - 10 * U, S.Y - TH - 16 * U, TW + 20 * U, TH + 8 * U, .55f);
				Text(Tg.Text, S.X, S.Y - TH - 12 * U, .95f, FLinearColor(1.f, .92f, .75f), F, true);
			}
		}
	}
	Tags.Reset();
}

void AKeysHUD::DrawChartMore(AKeysBoatPawn* P, float X, float Y, float Size)
{
	if (const AKeysLocals* L = AKeysLocals::Get(this))
	{
		double SX, SZ;
		if (L->SandbarSite(SX, SZ))
		{
			const FVector2D A = ChartXY(SX, SZ, X, Y, Size);
			DrawRect(FLinearColor(.95f, .86f, .6f), A.X - 4 * U, A.Y - 4 * U, 8 * U, 8 * U);
			DrawRect(FLinearColor(.9f, .3f, .55f), A.X - 1.5f * U, A.Y - 1.5f * U, 3 * U, 3 * U);
		}
		if (L->LarryPos(SX, SZ))
		{
			const FVector2D A = ChartXY(SX, SZ, X, Y, Size);
			DrawRect(FLinearColor(.75f, .45f, .2f), A.X - 2.5f * U, A.Y - 2.5f * U, 5 * U, 5 * U);
		}
		TArray<FVector2D> Fl;
		L->FleetDots(Fl);
		for (const FVector2D& G : Fl) { const FVector2D A = ChartXY(G.X, G.Y, X, Y, Size); DrawRect(FLinearColor(.35f, .36f, .4f), A.X - 3 * U, A.Y - 3 * U, 6 * U, 6 * U); }
	}
}

void AKeysHUD::DrawMore(AKeysBoatPawn* P)
{
	// the boat pistol (web entities/handgun/hud.ts): crosshair opening with the spread, the magazine, the tally
	if (const AKeysHandgun* G = AKeysHandgun::Get(this); G && G->IsOut())
	{
		const float Cx = Canvas->SizeX * .5f, Cy = Canvas->SizeY * .5f;
		const float Focal = float(Canvas->SizeX * .5 / FMath::Tan(FMath::DegreesToRadians(75.0 / 2)));
		const float Gap = FMath::Max(5.f * U, float(G->Spread()) * Focal * 2.2f);
		const FLinearColor Ch = G->HitFlash > 0 ? FLinearColor(1.f, .35f, .25f) : FLinearColor(1.f, 1.f, 1.f, .9f);
		for (int32 k = 0; k < 4; k++)
		{
			const float DX = k == 0 ? 1.f : k == 1 ? -1.f : 0.f, DY = k == 2 ? 1.f : k == 3 ? -1.f : 0.f;
			DrawLine(Cx + DX * Gap, Cy + DY * Gap, Cx + DX * (Gap + 10 * U), Cy + DY * (Gap + 10 * U), FLinearColor(0, 0, 0, .5f), 3.5f * U);
			DrawLine(Cx + DX * Gap, Cy + DY * Gap, Cx + DX * (Gap + 10 * U), Cy + DY * (Gap + 10 * U), Ch, 2 * U);
		}
		DrawRect(Ch, Cx - 1.5f * U, Cy - 1.5f * U, 3 * U, 3 * U);
		if (G->HitFlash > 0)
			for (int32 s = -1; s <= 1; s += 2) { DrawLine(Cx - 14 * U, Cy - 14 * U * s, Cx - 6 * U, Cy - 6 * U * s, Ch, 2.5f * U); DrawLine(Cx + 14 * U, Cy + 14 * U * s, Cx + 6 * U, Cy + 6 * U * s, Ch, 2.5f * U); }
		// the magazine, bottom right
		const float BW = 300 * U, BH = 116 * U, BX = Canvas->SizeX - BW - 28 * U, BY = Canvas->SizeY - BH - 28 * U;
		Panel(BX, BY, BW, BH, .55f);
		Text(FString::Printf(TEXT("%d"), G->Mag), BX + 18 * U, BY + 10 * U, 2.4f, G->Mag == 0 ? Red : FLinearColor::White);
		Text(FString::Printf(TEXT("/ %d"), G->Reserve()), BX + 92 * U, BY + 34 * U, 1.2f, Dim);
		Text(TEXT("9 mm"), BX + 200 * U, BY + 14 * U, .8f, Dim);
		Text(FString::Printf(TEXT("Flares %d"), G->Flares()), BX + 200 * U, BY + 40 * U, .8f, G->Flares() ? Gold : Dim);
		for (int32 i = 0; i < G->MagSize; i++)
			DrawRect(i < G->Mag ? Gold : FLinearColor(.25f, .25f, .25f, .8f), BX + 18 * U + i * (BW - 36 * U) / G->MagSize, BY + 80 * U, FMath::Max(2.f, (BW - 36 * U) / G->MagSize - 3 * U), 12 * U);
		if (G->bReloading) { Bar(BX + 18 * U, BY + 98 * U, BW - 36 * U, 8 * U, float(1 - G->ReloadT / FMath::Max(.01, G->ReloadDur)), Teal); }
		Text(FString::Printf(TEXT("Fish stunned %d  -  Birds startled %d"), G->TallyFish, G->TallyBirds), Canvas->SizeX * .5f, 24 * U + 60 * U, .85f, FLinearColor(1.f, .92f, .75f), nullptr, true);
		if (G->NoteT > 0 && !G->Note.IsEmpty()) Text(G->Note, Cx, Cy + 60 * U, 1.f, Gold, nullptr, true);
		Text(TEXT("Click / Space fire - R reload - Z flare - mouse or arrows to aim - 9 or Esc holster"), Cx, Canvas->SizeY - 44 * U, .9f, Dim, nullptr, true);
	}
}

bool AKeysHUD::DrawPanels(AKeysBoatPawn* P)
{
	return false;
}
