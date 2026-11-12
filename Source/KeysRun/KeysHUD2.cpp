#include "KeysHUD.h"
#include "KeysBoatPawn.h"
#include "KeysKit.h"
#include "KeysGeo.h"
#include "KeysLocals.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"

using namespace KeysGeo;

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
}

bool AKeysHUD::DrawPanels(AKeysBoatPawn* P)
{
	return false;
}
