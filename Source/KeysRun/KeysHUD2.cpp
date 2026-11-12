#include "KeysHUD.h"
#include "KeysBoatPawn.h"
#include "KeysKit.h"
#include "KeysGeo.h"
#include "KeysLocals.h"
#include "KeysHandgun.h"
#include "KeysTreasure.h"
#include "KeysEstates.h"
#include "KeysTourneyShow.h"
#include "KeysSlots.h"
#include "KeysTownData.h"
#include "KeysData.h"
#include "Engine/Texture2D.h"
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
		if (const AKeysEstates* Es = AKeysEstates::Get(this))
			for (const FString& Id : P->Estates)
				if (const AKeysEstates::FParcel* Pc = Es->ById(Id)) { const FVector2D A = ChartXY(Pc->CX, Pc->CZ, X, Y, Size); DrawRect(Gold, A.X - 3.5f * U, A.Y - 3.5f * U, 7 * U, 7 * U); }
		for (const FVector2D& G : Fl) { const FVector2D A = ChartXY(G.X, G.Y, X, Y, Size); DrawRect(FLinearColor(.35f, .36f, .4f), A.X - 3 * U, A.Y - 3 * U, 6 * U, 6 * U); }
	}
}

void AKeysHUD::DrawMore(AKeysBoatPawn* P)
{
	// the tournament's titles and ceremony (intro.ts): letterbox bars and a lower-third card
	if (const AKeysTourneyShow* Sh = AKeysTourneyShow::Get(this); Sh && (Sh->bLetterbox || Sh->CardKind))
	{
		const float Bar = Canvas->SizeY * .11f;
		if (Sh->bLetterbox) { DrawRect(FLinearColor(0, 0, 0, .96f), 0, 0, Canvas->SizeX, Bar); DrawRect(FLinearColor(0, 0, 0, .96f), 0, Canvas->SizeY - Bar, Canvas->SizeX, Bar); }
		if (Sh->CardKind)
		{
			const float A = float(FMath::Clamp(Sh->CardAge / .35, 0.0, 1.0)), X = 80 * U - (1 - A) * 40 * U;
			const float Y = Canvas->SizeY - Bar - (Sh->CardKind == 1 ? 210 : 150) * U;
			const FLinearColor TitleC = Sh->CardKind == 3 || Sh->CardKind == 4 ? Gold : FLinearColor::White;
			DrawRect(FLinearColor(Gold.R, Gold.G, Gold.B, A), X - 18 * U, Y + 6 * U, 6 * U, (Sh->CardKind == 1 ? 150 : 96) * U);
			Text(Sh->CardKind == 4 ? FString(TEXT("CHAMPION")) : Sh->CardTitle.ToUpper(), X, Y, Sh->CardKind == 1 ? 2.4f : 2.1f, TitleC * A);
			Text(Sh->CardSub, X, Y + (Sh->CardKind == 1 ? 58 : 52) * U, 1.15f, FLinearColor(.85f, .9f, .95f, A));
			float CX = X;
			for (const FString& C : Sh->CardChips)
			{
				float TW = 0, TH = 0;
				GetTextSize(C, TW, TH, GEngine->GetMediumFont(), .95f * U);
				DrawRect(FLinearColor(.1f, .35f, .45f, .85f * A), CX, Y + 100 * U, TW + 24 * U, TH + 10 * U);
				Text(C, CX + 12 * U, Y + 105 * U, .95f, FLinearColor(1, 1, 1, A));
				CX += TW + 34 * U;
			}
		}
	}
	// the wreck treasure prompt under water (wreck-treasure.ts): "F  Prise open the strongbox", or what is in sight and how far
	if (const AKeysTreasure* Tr = AKeysTreasure::Get(this); Tr && !Tr->Prompt.IsEmpty())
	{
		float TW = 0, TH = 0;
		GetTextSize(Tr->Prompt, TW, TH, GEngine->GetMediumFont(), 1.1f * U);
		const float X = Canvas->SizeX * .5f - TW * .5f, Y = Canvas->SizeY * .62f;
		Panel(X - 16 * U, Y - 8 * U, TW + 32 * U, TH + 16 * U, .6f);
		if (Tr->bPromptKey) DrawRect(Gold, X - 16 * U, Y - 8 * U, 4 * U, TH + 16 * U);
		Text(Tr->Prompt, Canvas->SizeX * .5f, Y, 1.1f, Tr->bPromptKey ? Gold : FLinearColor::White, nullptr, true);
	}
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
	if (const AKeysEstates* Es = AKeysEstates::Get(this); Es && Es->bOpen) { DrawEstates(P); return true; }
	if (const AKeysSlots* Sl = AKeysSlots::Get(this); Sl && Sl->bOpen) { DrawSlots(P); return true; }
	if (P->bGearOpen) { DrawGear(P); return true; }
	return false;
}

namespace KeysEstatesText { FString ResLabel(const FString& K); FString DockLabel(const FString& K); FString MountLabel(const FString& K); }

// the land office (web ui/estates/panel.ts): the chart of the region with every parcel in its tier's colour, the deed of
// the one picked, and on your own lots the build sheet
void AKeysHUD::DrawEstates(AKeysBoatPawn* P)
{
	const AKeysEstates* Es = AKeysEstates::Get(this);
	if (!Es) return;
	DrawRect(FLinearColor(.01f, .03f, .05f, .9f), 0, 0, Canvas->SizeX, Canvas->SizeY);
	const float M = 30 * U, Sz = FMath::Min(Canvas->SizeY - 150 * U, Canvas->SizeX * .56f);
	const float CX0 = M, CY0 = 92 * U;
	Text(TEXT("Keys Run Estates - the land office"), M, 28 * U, 1.5f, Gold);
	Text(FString::Printf(TEXT("%s  -  %s"), *AKeysEstates::RegionNames()[Es->Region], *KeysKit::Money(double(P->Cash))), Canvas->SizeX - M - 420 * U, 36 * U, 1.1f, FLinearColor::White);
	double RX0, RZ0, RS;
	Es->RegionBox(RX0, RZ0, RS);
	// the region's chart: rebuilt when the region changes (buff land, blues by depth)
	if (!EstChart || EstChartRegion != Es->Region)
	{
		constexpr int32 N = 256;
		if (!EstChart) { EstChart = UTexture2D::CreateTransient(N, N, PF_B8G8R8A8); EstChart->SRGB = true; EstChart->Filter = TF_Bilinear; }
		TArray<FColor> Px;
		Px.SetNumUninitialized(N * N);
		for (int32 j = 0; j < N; j++)
			for (int32 i = 0; i < N; i++)
			{
				const double X = RX0 + (i + .5) / N * RS, Z = RZ0 + (j + .5) / N * RS;
				FLinearColor C;
				if (LandH(X, Z) > .1) C = FLinearColor(.86f, .80f, .62f);
				else C = FMath::Lerp(FLinearColor(.62f, .86f, .93f), FLinearColor(.10f, .28f, .52f), float(FMath::Clamp(FMath::Loge(DepthFast(X, Z) + 1) / FMath::Loge(401.0), 0.0, 1.0)));
				Px[j * N + i] = FColor(uint8(C.R * 255), uint8(C.G * 255), uint8(C.B * 255), 255);
			}
		FTexture2DMipMap& Mip = EstChart->GetPlatformData()->Mips[0];
		void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Data, Px.GetData(), Px.Num() * sizeof(FColor));
		Mip.BulkData.Unlock();
		EstChart->UpdateResource();
		EstChartRegion = Es->Region;
	}
	DrawTexture(EstChart, CX0, CY0, Sz, Sz, 0, 0, 1, 1);
	auto XY = [&](double X, double Z) { return FVector2D(CX0 + (X - RX0) / RS * Sz, CY0 + (Z - RZ0) / RS * Sz); };
	const AKeysEstates::FParcel* Selp = Es->Selected();
	for (int32 Ix : Es->RegionList)
	{
		const AKeysEstates::FParcel& Pc = Es->Parcels()[Ix];
		const bool bMine = P->Estates.Contains(Pc.Id), bFounder = !Es->Founder(Pc.Id).IsEmpty(), bSel = &Pc == Selp;
		FLinearColor C = AKeysEstates::TierColor(Pc.Tier);
		if (!bMine && !bFounder && !bSel) C.A = .8f;
		const float Th = (bSel ? 3.f : bMine ? 2.5f : 1.3f) * U;
		for (int32 k = 0; k < Pc.Poly.Num(); k++)
		{
			const FVector2D A = XY(Pc.Poly[k].X, Pc.Poly[k].Y), B = XY(Pc.Poly[(k + 1) % Pc.Poly.Num()].X, Pc.Poly[(k + 1) % Pc.Poly.Num()].Y);
			DrawLine(A.X, A.Y, B.X, B.Y, bSel ? FLinearColor::White : bMine ? Gold : C, Th);
		}
		const FVector2D Ct = XY(Pc.CX, Pc.CZ);
		if (bMine || bFounder) DrawRect(bMine ? Gold : FLinearColor(.9f, .9f, .9f), Ct.X - 3 * U, Ct.Y - 3 * U, 6 * U, 6 * U);
	}
	// you are here
	{
		const FVector2D B = XY(P->BoatX(), P->BoatZ());
		if (B.X > CX0 && B.X < CX0 + Sz && B.Y > CY0 && B.Y < CY0 + Sz) { DrawRect(FLinearColor(.95f, .1f, .1f), B.X - 4 * U, B.Y - 4 * U, 8 * U, 8 * U); Text(TEXT("you"), B.X + 6 * U, B.Y - 8 * U, .7f, FLinearColor(.95f, .2f, .2f)); }
	}
	// the legend
	{
		float LX = CX0, LY = CY0 + Sz + 12 * U;
		for (int32 t = 0; t < 7; t++)
		{
			const FString L = AKeysEstates::TierLabel(AKeysEstates::ETier(t));
			DrawRect(AKeysEstates::TierColor(AKeysEstates::ETier(t)), LX, LY + 4 * U, 12 * U, 12 * U);
			float TW = 0, TH = 0;
			GetTextSize(L, TW, TH, GEngine->GetMediumFont(), .7f * U);
			Text(L, LX + 16 * U, LY, .7f, Dim);
			LX += TW + 34 * U;
		}
	}
	// the deed
	const float DX = CX0 + Sz + 30 * U, DW = Canvas->SizeX - DX - M;
	float Y = CY0;
	Panel(DX - 14 * U, CY0 - 10 * U, DW + 28 * U, Sz + 20 * U, .5f);
	if (!Selp) { Text(TEXT("Pick a lot on the chart."), DX, Y, 1.1f); return; }
	const AKeysEstates::FParcel& Pc = *Selp;
	const bool bMine = P->Estates.Contains(Pc.Id);
	const FString Fd = Es->Founder(Pc.Id);
	DrawRect(AKeysEstates::TierColor(Pc.Tier), DX, Y + 6 * U, 12 * U, 12 * U);
	Text(AKeysEstates::TierLabel(Pc.Tier), DX + 18 * U, Y, .85f, AKeysEstates::TierColor(Pc.Tier));
	Text(FString::Printf(TEXT("%d of %d"), Es->Sel + 1, Es->RegionList.Num()), DX + DW - 90 * U, Y, .75f, Dim);
	Y += 28 * U;
	Y += Text(Pc.Name, DX, Y, 1.35f, FLinearColor::White) + 4 * U;
	Y += Wrapped((Pc.Island == Pc.Name ? FString() : Pc.Island.Replace(TEXT(" (Vaca Key)"), TEXT("")) + TEXT(" - ")) + Pc.Side + TEXT(". ") + AKeysEstates::TierBlurb(Pc.Tier), DX, Y, DW, .8f, Dim) + 10 * U;
	if (bMine) Y += Text(TEXT("Held by you"), DX, Y, 1.1f, Gold) + 6 * U;
	else if (!Fd.IsEmpty()) Y += Text(FString::Printf(TEXT("Held by %s  -  founding owner"), *Fd), DX, Y, 1.0f, FLinearColor(.9f, .9f, .9f)) + 6 * U;
	else
	{
		Y += Text(KeysKit::Money(double(Es->PriceOf(Pc))), DX, Y, 1.6f, Gold);
		Y += Text(P->Estates.Num() >= 3 ? TEXT("You hold three lots, the most one angler may.") : TEXT("Enter - buy this lot"), DX, Y, .85f, P->Cash >= Es->PriceOf(Pc) ? Teal : Dim) + 6 * U;
	}
	const KeysGeo::FMarina& TM = Marinas()[0];
	auto Km = [](double D) { return D < 1000 ? FString::Printf(TEXT("%d m"), FMath::RoundToInt(D / 10) * 10) : FString::Printf(TEXT("%.1f km"), D / 1000); };
	Y += Text(FString::Printf(TEXT("Area %s m2   -   %s %d m   -   To town %s   -   From you %s"), *FString::FormatAsNumber(FMath::RoundToInt(Pc.Area)), Pc.Tier == AKeysEstates::ETier::Private ? TEXT("Shoreline") : TEXT("Frontage"),
		FMath::RoundToInt(Pc.Frontage), *Km(FMath::Sqrt(FMath::Square(TM.SX - Pc.CX) + FMath::Square(TM.SZ - Pc.CZ))), *Km(FMath::Sqrt(FMath::Square(P->BoatX() - Pc.CX) + FMath::Square(P->BoatZ() - Pc.CZ)))), DX, Y, .78f, Dim) + 14 * U;
	if (bMine)
	{
		const AKeysEstates::FBuild B = Es->BuildOf(Pc.Id);
		Y += Text(Es->SheetRow < 0 ? TEXT("Build sheet  (Tab or Enter to edit)") : TEXT("Build sheet  (Up / Down a row, Left / Right to change, Tab done)"), DX, Y, .85f, Gold) + 6 * U;
		const KeysData::FSpecies* Sp = KeysData::Species(B.Fish);
		const bool bHarbor = Pc.Tier == AKeysEstates::ETier::Harbor;
		const TArray<TPair<int32, FString>> Rows = {
			{0, TEXT("Residence      ") + KeysEstatesText::ResLabel(B.Res)},
			{1, TEXT("Dock           ") + (bHarbor ? FString(TEXT("(Harbor Row has none)")) : KeysEstatesText::DockLabel(B.Dock))},
			{2, TEXT("Biggest catch  ") + (Sp ? Sp->Name : B.Fish)},
			{3, FString::Printf(TEXT("Weight         %.1f lb"), B.Lb)},
			{4, TEXT("Hung           ") + KeysEstatesText::MountLabel(B.Mount)},
			{5, FString::Printf(TEXT("Trophies       %d"), B.Trophies)},
			{6, FString::Printf(TEXT("Pool           %s"), B.bPool ? TEXT("yes") : TEXT("no"))},
			{7, FString::Printf(TEXT("Fire pit       %s"), B.bFire ? TEXT("yes") : TEXT("no"))}};
		for (const TPair<int32, FString>& R : Rows)
		{
			if (bHarbor && R.Key == 1) continue;
			const bool bHot = Es->SheetRow == R.Key;
			if (bHot) DrawRect(FLinearColor(.2f, .5f, .55f, .45f), DX - 6 * U, Y - 2 * U, DW + 12 * U, 30 * U);
			Y += Text(R.Value, DX, Y, .9f, bHot ? FLinearColor::White : FLinearColor(.85f, .88f, .9f)) + 6 * U;
		}
	}
	Text(TEXT("Up / Down pick a lot  -  Left / Right the region  -  Enter buy or build  -  Tab the build sheet  -  , or Esc close"), Canvas->SizeX * .5f, Canvas->SizeY - 40 * U, .85f, Dim, nullptr, true);
}

// a Gaff & Gill slot machine (web game/casino/render.ts): the cabinet, three reels of fish, five lines, the paytable
void AKeysHUD::DrawSlots(AKeysBoatPawn* P)
{
	const AKeysSlots* Sl = AKeysSlots::Get(this);
	if (!Sl) return;
	const float W = 980 * U, H = 640 * U, X = Canvas->SizeX * .5f - W * .5f, Y = Canvas->SizeY * .5f - H * .5f;
	DrawRect(FLinearColor(.02f, .01f, .03f, .7f), 0, 0, Canvas->SizeX, Canvas->SizeY);
	DrawRect(FLinearColor(.35f, .05f, .08f, .97f), X, Y, W, H);
	DrawRect(Gold, X, Y, W, 6 * U); DrawRect(Gold, X, Y + H - 6 * U, W, 6 * U);
	Text(TEXT("GAFF & GILL"), X + W * .5f, Y + 18 * U, 1.8f, Gold, nullptr, true);
	Text(FString::Printf(TEXT("Five lines, always in play - pays %.1f cents on the dollar"), Sl->Rtp() * 100), X + W * .5f, Y + 64 * U, .85f, FLinearColor(1.f, .85f, .7f), nullptr, true);
	// the window: three reels of three
	const float RW = 170 * U, RH = 100 * U, WX = X + 50 * U, WY = Y + 110 * U;
	DrawRect(FLinearColor(.95f, .93f, .86f), WX - 10 * U, WY - 10 * U, RW * 3 + 40 * U, RH * 3 + 20 * U);
	for (int32 r = 0; r < 3; r++)
	{
		const double Frac = Sl->ReelPos[r] - FMath::FloorToDouble(Sl->ReelPos[r]);
		for (int32 k = 0; k < 3; k++)
		{
			const int32 Sym = Sl->Symbol(r, k);
			const float TX = WX + r * (RW + 10 * U), TY = WY + k * RH - float(Frac * RH);
			if (TY < WY - RH * .5f || TY > WY + RH * 2.5f) continue;
			DrawRect(Sl->bSpinning ? FLinearColor(.88f, .86f, .8f) : FLinearColor(1.f, 1.f, .97f), TX, TY + 3 * U, RW, RH - 6 * U);
			DrawRect(AKeysSlots::SymbolColor(Sym), TX + 14 * U, TY + 18 * U, 44 * U, RH - 36 * U);
			Text(AKeysSlots::SymbolName(Sym), TX + 68 * U, TY + RH * .5f - 14 * U, .95f, FLinearColor(.1f, .1f, .12f));
		}
	}
	// the lines that paid, once the reels are still
	if (!Sl->bSpinning)
		for (int32 l : Sl->WinLines)
		{
			const int32* Ln = AKeysSlots::Lines[l];
			for (int32 r = 0; r < 2; r++)
			{
				const FVector2D A(WX + r * (RW + 10 * U) + RW * .5f, WY + Ln[r] * RH + RH * .5f), B(WX + (r + 1) * (RW + 10 * U) + RW * .5f, WY + Ln[r + 1] * RH + RH * .5f);
				DrawLine(A.X, A.Y, B.X, B.Y, FLinearColor(1.f, .2f, .3f, .9f), 5 * U);
			}
		}
	// the paytable
	const float PX = WX + RW * 3 + 70 * U;
	float PY = WY;
	Text(TEXT("Pays, per line bet"), PX, PY, .95f, Gold); PY += 34 * U;
	const TCHAR* Pays[] = {TEXT("3 Blue Marlin   1000x"), TEXT("3 Tarpon         125x"), TEXT("3 Permit          40x"), TEXT("3 Bonefish        16x"), TEXT("3 Shrimp           8x"), TEXT("2 Shrimp (left)     1x"), TEXT("Old Boot        nothing")};
	for (const TCHAR* L : Pays) { Text(L, PX, PY, .85f, FLinearColor::White); PY += 28 * U; }
	// bet, cash, the result
	const float BY = Y + H - 150 * U;
	Text(FString::Printf(TEXT("Bet %s"), *KeysKit::Money(AKeysSlots::BetLadder[Sl->BetI])), X + 50 * U, BY, 1.4f, FLinearColor::White);
	Text(FString::Printf(TEXT("($%d a line)"), AKeysSlots::BetLadder[Sl->BetI] / 5), X + 260 * U, BY + 12 * U, .85f, Dim);
	Text(FString::Printf(TEXT("Cash %s"), *KeysKit::Money(double(P->Cash))), X + W - 330 * U, BY, 1.4f, Gold);
	if (!Sl->bSpinning && !Sl->ResultText.IsEmpty()) Text(Sl->ResultText, X + W * .5f, BY + 54 * U, 1.15f, Sl->LastWin > 0 ? Gold : FLinearColor(1.f, .85f, .8f), nullptr, true);
	Text(TEXT("Space spin  -  Left / Right the bet  -  Esc or F to get up"), X + W * .5f, Y + H - 40 * U, .85f, Dim, nullptr, true);
}

// the gear screen (web ui/gear.ts): what you own, what you carry, what is running, and what it all adds up to
void AKeysHUD::DrawGear(AKeysBoatPawn* P)
{
	const float W = 1100 * U, H = 720 * U, X = Canvas->SizeX * .5f - W * .5f, Y = Canvas->SizeY * .5f - H * .5f;
	Panel(X, Y, W, H, .9f);
	Text(TEXT("Gear"), X + 28 * U, Y + 18 * U, 1.5f, Gold);
	Text(FString::Printf(TEXT("Cash %s"), *KeysKit::Money(double(P->Cash))), X + W - 280 * U, Y + 24 * U, 1.2f, Gold);
	static const TCHAR* VenueIds[] = {TEXT("tackle"), TEXT("guns"), TEXT("bar"), TEXT("club"), TEXT("casino"), TEXT("diner"), TEXT("dive"), TEXT("liquor"), TEXT("gifts"), TEXT("pawn"), TEXT("laundromat"), TEXT("icecream"), TEXT("motel"), TEXT("convenience")};
	auto NameOf = [](const FString& Eff) -> FString
	{
		for (const TCHAR* V : VenueIds) if (const KeysTown::FVenue* Ve = KeysTown::Venue(V)) for (const KeysTown::FShelfItem& It : Ve->Sells) if (Eff == It.Effect) return It.Name;
		return Eff;
	};
	// owned
	float LX = X + 28 * U, LY = Y + 80 * U;
	Text(TEXT("Owned"), LX, LY, 1.0f, Teal); LY += 34 * U;
	TArray<FString> Gear;
	for (const FString& O : P->Owned) if (!O.StartsWith(TEXT("cosmetic.")) && !O.StartsWith(TEXT("boat."))) Gear.Add(NameOf(O));
	Gear.Sort();
	if (!Gear.Num()) { Text(TEXT("The stock rod and reel, 50 lb braid. The shops are in town (Q at a dock)."), LX, LY, .8f, Dim); LY += 28 * U; }
	for (const FString& G : Gear) { Text(G, LX, LY, .85f, FLinearColor::White); LY += 26 * U; }
	LY += 14 * U;
	Text(TEXT("Carrying"), LX, LY, 1.0f, Teal); LY += 34 * U;
	const TPair<const TCHAR*, const TCHAR*> Cons[] = {{TEXT("handgun.ammo"), TEXT("9 mm rounds (the console pistol, 9)")}, {TEXT("safety.flare"), TEXT("Parachute flares (Z with the pistol out)")},
		{TEXT("spear.shafts"), TEXT("Spare spear shafts")}, {TEXT("leader.fluoro"), TEXT("Fluorocarbon leaders")}};
	for (const auto& C : Cons) { Text(FString::Printf(TEXT("%d  %s"), P->CountOf(C.Key), C.Value), LX, LY, .85f, P->CountOf(C.Key) ? FLinearColor::White : Dim); LY += 26 * U; }
	if (P->BaitLeft() > 0) { Text(FString::Printf(TEXT("%d  casts of live bait"), P->BaitLeft()), LX, LY, .85f); LY += 26 * U; }
	LY += 14 * U;
	Text(TEXT("Wearing"), LX, LY, 1.0f, Teal); LY += 34 * U;
	for (const TCHAR* Slot : {TEXT("shirt"), TEXT("cap"), TEXT("glasses"), TEXT("flag")})
	{
		const FString K = P->Wearing.FindRef(Slot);
		Text(FString::Printf(TEXT("%s:  %s"), Slot, K.IsEmpty() ? TEXT("the usual") : *NameOf(K)), LX, LY, .85f, K.IsEmpty() ? Dim : FLinearColor::White);
		LY += 26 * U;
	}
	// what it adds up to, in human terms
	float RX = X + W * .55f, RY = Y + 80 * U;
	Text(TEXT("What it adds up to"), RX, RY, 1.0f, Teal); RY += 34 * U;
	auto Row = [&](const FString& S, bool bOn) { Text(S, RX, RY, .85f, bOn ? FLinearColor::White : Dim); RY += 28 * U; };
	const FString Rung = P->HasGear(TEXT("tackle.braid130")) ? TEXT("130 lb big-game rig") : P->HasGear(TEXT("tackle.braid80")) ? TEXT("80 lb braid, stand-up rig") : TEXT("50 lb braid, fluoro leader");
	Row(FString::Printf(TEXT("Line: %s"), *Rung), true);
	Row(FString::Printf(TEXT("Line holds %d%% of the stock outfit's strain"), FMath::RoundToInt(P->LineBreakNow() / 1.06 * 100)), P->LineBreakNow() > 1.07);
	Row(P->HasGear(TEXT("reel.lever")) ? TEXT("Retrieve 30% faster (lever-drag reel)") : TEXT("Retrieve: the stock reel"), P->HasGear(TEXT("reel.lever")));
	const double Air = (P->HasGear(TEXT("dive.tank")) ? 2.2 : 1) * (P->HasGear(TEXT("dive.wetsuit")) ? 1.15 : 1) * (P->HasGear(TEXT("dive.fins")) ? 1.1 : 1);
	Row(FString::Printf(TEXT("Bottom time %d s -> %d s"), 90, FMath::RoundToInt(90 * Air)), Air > 1.01);
	Row(P->HasGear(TEXT("spear.long")) ? TEXT("Speargun reach 18 m -> 24 m") : TEXT("Speargun reach 18 m"), P->HasGear(TEXT("spear.long")));
	Row(FString::Printf(TEXT("Pistol magazine %d rounds%s"), P->HasGear(TEXT("handgun.extmag")) ? 18 : 12, P->HasGear(TEXT("handgun.dot")) ? TEXT(", red dot: a 45% tighter group") : TEXT("")), P->HasGear(TEXT("handgun.extmag")) || P->HasGear(TEXT("handgun.dot")));
	Row(FString::Printf(TEXT("Lots held: %d of 3"), P->Estates.Num()), P->Estates.Num() > 0);
	RY += 14 * U;
	Text(TEXT("Running"), RX, RY, 1.0f, Teal); RY += 34 * U;
	TArray<AKeysBoatPawn::FBuffShow> Bf;
	P->LiveBuffs(Bf);
	if (!Bf.Num()) Row(TEXT("Nothing. The diner does coffee."), false);
	for (const AKeysBoatPawn::FBuffShow& B : Bf) Row(FString::Printf(TEXT("%s  %d:%02d"), *B.Label, B.Left / 60, B.Left % 60), true);
	Text(TEXT("Tab or Esc to close"), X + W * .5f, Y + H - 40 * U, .85f, Dim, nullptr, true);
}
