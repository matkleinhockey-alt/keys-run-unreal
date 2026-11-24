// Test hooks for auditing the reef itself (the corals, fans, sponges and grass AKeysReef plants), read only: nothing
// here changes the reef. Two console commands, reached from a script as con=<command>:
//
//   krbottom.reef [metres]            counts everything AKeysReef plants within that distance (default 150) of the
//                                     diver or the camera: by kind, its depth range, its height range, the reef-life
//                                     zone it stands in, and for the fans how far their face is turned off the
//                                     surge. Lines start "KRBottom".
//   krbottom.look <kind> [metres] [bearing] [nth]
//                                     puts the diver that far (default 2.5) from the nearest (or nth nearest) of a
//                                     kind and aims the mask at it. Bearing 0 (the default) stands the diver on the
//                                     offshore side looking in along the surge; 90 looks along the reef.
//   krbottom.ground <species key>     puts the diver where that reef-life species has the most ground within 2 km
//                                     (its own zones, depths and Where), so that reef:find has something to find.
//   krbottom.goto <zone> [least] [greatest depth]
//                                     puts the diver over the nearest bottom of that reef-life zone (sand, seagrass,
//                                     hardbottom, reeftop, crest, spurtop, patchreef, forereef, deepwall).
//
// Kinds: the code-built ones are elkhorn0 staghorn0 brain0 star0 seafan0 seaplume0 barrel0 tube0 encrusting0
// seagrass0; the modelled ones elkhorn staghorn pillar brain star finger seafan searod lettuce blackcoral barrel
// azurevase elephantear algae sargassum turtlegrass. (The two tables below repeat KeysReef.cpp's order and sizes:
// they are only for this report.)
#include "KeysReefLife.h"
#include "KeysReef.h"
#include "KeysGeo.h"
#include "KeysSea.h"
#include "KeysDiver.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"

namespace KeysReefLifeBottomAudit
{
	// H: the height of the code-built shape at scale 1 (0: a model, whose scale is its height in metres). W: its width
	// at scale 1. Fan: 0 not a fan, 1 its face looks along local z (the card), 2 along local x (the sea fan model).
	struct FKind { const TCHAR* Key; double H, W; int32 Fan; };
	static const FKind Kinds[] = {
		{TEXT("elkhorn0"), 1.0, 1.7, 0}, {TEXT("staghorn0"), 1.1, 1.6, 0}, {TEXT("brain0"), .56, 2.0, 0}, {TEXT("star0"), .72, 2.0, 0}, {TEXT("seafan0"), 1.7, 1.5, 1},
		{TEXT("seaplume0"), 1.3, .5, 0}, {TEXT("barrel0"), 1.0, 1.56, 0}, {TEXT("tube0"), 1.1, .9, 0}, {TEXT("encrusting0"), .24, 2.0, 0}, {TEXT("seagrass0"), .6, .3, 0},
		{TEXT("elkhorn"), 0, 1.0, 0}, {TEXT("staghorn"), 0, 1.08, 0}, {TEXT("pillar"), 0, 1.0, 0}, {TEXT("brain"), 0, 1.4, 0}, {TEXT("star"), 0, 1.6, 0}, {TEXT("finger"), 0, 1.5, 0},
		{TEXT("seafan"), 0, 1.19, 2}, {TEXT("searod"), 0, .9, 0}, {TEXT("lettuce"), 0, 1, 0}, {TEXT("blackcoral"), 0, 1, 0}, {TEXT("barrel"), 0, 1.03, 0}, {TEXT("azurevase"), 0, 1, 0},
		{TEXT("elephantear"), 0, 1, 0}, {TEXT("algae"), 0, 1, 0}, {TEXT("sargassum"), 0, 1, 0}, {TEXT("turtlegrass"), 0, 1.15, 0},
	};
	constexpr int32 NumKinds = UE_ARRAY_COUNT(Kinds);

	static double HeightOf(const AKeysReef::FInst& I) { return Kinds[I.Species].H > 0 ? Kinds[I.Species].H * I.SY : I.SY; }
	static double WidthOf(const AKeysReef::FInst& I) { return Kinds[I.Species].W * (Kinds[I.Species].Fan == 2 ? I.SZ : I.SX); }
	// degrees between a fan's face and the surge (0: square across it, 90: edge on)
	static double FanOff(const AKeysReef::FInst& I)
	{
		const FVector2D N = KeysSea::ChainNormal(I.X);
		const double S = FMath::Sin(I.RotY), C = FMath::Cos(I.RotY);
		const FVector2D Face = Kinds[I.Species].Fan == 2 ? FVector2D(C, -S) : FVector2D(S, C);
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FMath::Abs(FVector2D::DotProduct(Face, N)), 0.0, 1.0)));
	}

	static AKeysReef* Reef(UWorld* W) { TActorIterator<AKeysReef> It(W); return It ? *It : nullptr; }
	static FVector Where(UWorld* W)
	{
		if (AKeysDiver* D = AKeysDiver::Get(W); D && D->IsDiving()) return FVector(D->State().X, D->State().Y, D->State().Z);
		if (APlayerCameraManager* C = UGameplayStatics::GetPlayerCameraManager(W, 0)) { const FVector L = C->GetCameraLocation(); return FVector(L.X / 100, L.Z / 100, L.Y / 100); }
		return FVector::ZeroVector;
	}
	template <typename F> static void EachWithin(AKeysReef* R, const FVector& At, double Radius, F&& Fn)
	{
		const int32 C0x = FMath::FloorToInt32((At.X - Radius) / 50), C1x = FMath::FloorToInt32((At.X + Radius) / 50);
		const int32 C0z = FMath::FloorToInt32((At.Z - Radius) / 50), C1z = FMath::FloorToInt32((At.Z + Radius) / 50);
		for (int32 cz = C0z; cz <= C1z; cz++)
			for (int32 cx = C0x; cx <= C1x; cx++)
				for (const AKeysReef::FInst& I : R->ChunkInstances(cx, cz))
					if (!I.Den && I.Species >= 0 && I.Species < NumKinds && FMath::Square(I.X - At.X) + FMath::Square(I.Z - At.Z) <= Radius * Radius) Fn(I);
	}

	static void CmdReef(const TArray<FString>& Args, UWorld* W)
	{
		AKeysReef* R = W ? Reef(W) : nullptr;
		if (!R) { UE_LOG(LogTemp, Log, TEXT("KRBottom reef: no reef actor")); return; }
		const double Radius = Args.Num() ? FMath::Clamp(FCString::Atod(*Args[0]), 10.0, 400.0) : 150.0;
		const FVector At = Where(W);
		struct FRow { int32 N = 0, Band[5] = {0}; double D0 = 1e9, D1 = -1e9, DSum = 0, H0 = 1e9, H1 = -1e9, HSum = 0, Off = 0, OffMax = 0; TMap<uint32, int32> Zone; };
		TArray<FRow> Rows;
		Rows.SetNum(NumKinds);
		EachWithin(R, At, Radius, [&](const AKeysReef::FInst& I)
		{
			FRow& Rw = Rows[I.Species];
			const double D = -KeysSea::SeafloorHeightAt(I.X, I.Z), H = HeightOf(I);
			Rw.N++;
			Rw.D0 = FMath::Min(Rw.D0, D); Rw.D1 = FMath::Max(Rw.D1, D); Rw.DSum += D;
			Rw.H0 = FMath::Min(Rw.H0, H); Rw.H1 = FMath::Max(Rw.H1, H); Rw.HSum += H;
			Rw.Band[D < 2 ? 0 : D < 5 ? 1 : D < 10 ? 2 : D < 15 ? 3 : 4]++;
			Rw.Zone.FindOrAdd(KeysReefLife::HabitatAt(I.X, I.Z).Zone)++;
			if (Kinds[I.Species].Fan) { const double O = FanOff(I); Rw.Off += O; Rw.OffMax = FMath::Max(Rw.OffMax, O); }
		});
		const KeysReefLife::FHabitat Here = KeysReefLife::HabitatAt(At.X, At.Z);
		UE_LOG(LogTemp, Log, TEXT("KRBottom reef at %.0f, %.0f (%s; reef-life zone %s, floor %.1f m), everything planted within %.0f m:"), At.X, At.Z, *KeysGeo::ZoneAt(At.X, At.Z), KeysReefLife::ZoneName(Here.Zone), Here.Depth, Radius);
		for (int32 k = 0; k < NumKinds; k++)
		{
			const FRow& Rw = Rows[k];
			if (!Rw.N) { UE_LOG(LogTemp, Log, TEXT("KRBottom reef %-12s none"), Kinds[k].Key); continue; }
			FString Zn;
			for (const TPair<uint32, int32>& Z : Rw.Zone) Zn += FString::Printf(TEXT(" %s %d,"), KeysReefLife::ZoneName(Z.Key), Z.Value);
			const FString Fan = Kinds[k].Fan ? FString::Printf(TEXT("; face off square to the surge: mean %.0f deg, most %.0f"), Rw.Off / Rw.N, Rw.OffMax) : FString();
			UE_LOG(LogTemp, Log, TEXT("KRBottom reef %-12s %4d; depth %.1f-%.1f m (mean %.1f; 0-2 m %d, 2-5 m %d, 5-10 m %d, 10-15 m %d, 15 m+ %d); height %.2f-%.2f m (mean %.2f); zones:%s%s"), Kinds[k].Key, Rw.N,
				Rw.D0, Rw.D1, Rw.DSum / Rw.N, Rw.Band[0], Rw.Band[1], Rw.Band[2], Rw.Band[3], Rw.Band[4], Rw.H0, Rw.H1, Rw.HSum / Rw.N, *Zn, *Fan);
		}
	}

	static void CmdLook(const TArray<FString>& Args, UWorld* W)
	{
		AKeysReef* R = W ? Reef(W) : nullptr;
		AKeysDiver* Dv = W ? AKeysDiver::Get(W) : nullptr;
		if (!R || !Dv || !Dv->IsDiving() || !Args.Num()) { UE_LOG(LogTemp, Log, TEXT("KRBottom look: needs a kind, and the diver in the water")); return; }
		int32 Kind = -1;
		for (int32 k = 0; k < NumKinds; k++) if (Args[0] == Kinds[k].Key) Kind = k;
		if (Kind < 0) { UE_LOG(LogTemp, Log, TEXT("KRBottom look: no kind '%s'"), *Args[0]); return; }
		const double Dist = Args.Num() > 1 ? FMath::Clamp(FCString::Atod(*Args[1]), .5, 20.0) : 2.5, Bearing = Args.Num() > 2 ? FCString::Atod(*Args[2]) : 0;
		const int32 Nth = Args.Num() > 3 ? FMath::Max(0, FCString::Atoi(*Args[3])) : 0;
		const FVector At = Where(W);
		TArray<AKeysReef::FInst> Found;
		EachWithin(R, At, 160, [&](const AKeysReef::FInst& I) { if (I.Species == Kind) Found.Add(I); });
		if (!Found.Num()) { UE_LOG(LogTemp, Log, TEXT("KRBottom look %s: none within 160 m of %.0f, %.0f"), Kinds[Kind].Key, At.X, At.Z); return; }
		Found.Sort([&](const AKeysReef::FInst& A, const AKeysReef::FInst& B) { return FMath::Square(A.X - At.X) + FMath::Square(A.Z - At.Z) < FMath::Square(B.X - At.X) + FMath::Square(B.Z - At.Z); });
		const AKeysReef::FInst& I = Found[FMath::Min(Nth, Found.Num() - 1)];
		const double H = HeightOf(I), Floor = KeysSea::SeafloorHeightAt(I.X, I.Z);
		const FVector2D N = KeysSea::ChainNormal(I.X);
		const double Br = FMath::DegreesToRadians(Bearing);
		const FVector2D Dir(N.X * FMath::Cos(Br) - N.Y * FMath::Sin(Br), N.X * FMath::Sin(Br) + N.Y * FMath::Cos(Br));
		const FVector Mid(I.X, Floor + H * .5, I.Z);
		Dv->DebugCmd(TEXT("at"), FString::Printf(TEXT("%.3f,%.3f"), I.X + Dir.X * Dist, I.Z + Dir.Y * Dist));
		Dv->DebugCmd(TEXT("depth"), FString::Printf(TEXT("%.3f"), -(Mid.Y + .1)));
		Dv->DebugCmd(TEXT("hold"), FString::Printf(TEXT("%.3f"), -Dv->State().Y));
		const FVector Eye(Dv->State().X, Dv->State().Y + .15, Dv->State().Z), To = (Mid - Eye).GetSafeNormal();
		Dv->DebugCmd(TEXT("look"), FString::Printf(TEXT("%.4f,%.4f"), FMath::Atan2(-To.X, -To.Z), FMath::Asin(FMath::Clamp(To.Y, -1.0, 1.0))));
		const FString Fan = Kinds[Kind].Fan ? FString::Printf(TEXT(", its face %.0f deg off square to the surge"), FanOff(I)) : FString();
		UE_LOG(LogTemp, Log, TEXT("KRBottom look %s (%d of %d within 160 m): at %.1f, %.1f, floor %.1f m deep, %.2f m tall, %.2f m wide%s, reef-life zone %s, game zone %s; the diver is %.1f m from it at %.1f m, bearing %.0f deg off the offshore side"),
			Kinds[Kind].Key, FMath::Min(Nth, Found.Num() - 1) + 1, Found.Num(), I.X, I.Z, -Floor, H, WidthOf(I), *Fan, KeysReefLife::ZoneName(KeysReefLife::HabitatAt(I.X, I.Z).Zone), *KeysGeo::ZoneAt(I.X, I.Z), (Mid - Eye).Size(), -Eye.Y, Bearing);
	}

	static void CmdGoto(const TArray<FString>& Args, UWorld* W)
	{
		AKeysDiver* Dv = W ? AKeysDiver::Get(W) : nullptr;
		if (!Dv || !Dv->IsDiving() || !Args.Num()) { UE_LOG(LogTemp, Log, TEXT("KRBottom goto: needs a zone, and the diver in the water")); return; }
		const double D0 = Args.Num() > 1 ? FCString::Atod(*Args[1]) : 2, D1 = Args.Num() > 2 ? FCString::Atod(*Args[2]) : 10;
		const FVector At = Where(W);
		for (double Rr = 0; Rr < 2500; Rr += 12)
			for (int32 k = 0; k < FMath::Max(1, int32(Rr / 6)); k++)
			{
				const double A = k * 2 * PI / FMath::Max(1, int32(Rr / 6)), X = At.X + FMath::Cos(A) * Rr, Z = At.Z + FMath::Sin(A) * Rr;
				const KeysReefLife::FHabitat H = KeysReefLife::HabitatAt(X, Z);
				if (!H.Zone || H.Depth < D0 || H.Depth > D1 || FString(KeysReefLife::ZoneName(H.Zone)).Replace(TEXT(" "), TEXT("")) != Args[0]) continue;
				Dv->DebugCmd(TEXT("at"), FString::Printf(TEXT("%.3f,%.3f"), X, Z));
				Dv->DebugCmd(TEXT("depth"), FString::Printf(TEXT("%.3f"), H.Depth - 1.2));
				Dv->DebugCmd(TEXT("hold"), FString::Printf(TEXT("%.3f"), -Dv->State().Y));
				UE_LOG(LogTemp, Log, TEXT("KRBottom goto %s: the diver is put at %.0f, %.0f, %.0f m from where it was (floor %.1f m, grass %.2f, reef %.2f, %.0f m off the chain, game zone %s)"), *Args[0], X, Z, Rr, H.Depth, H.Grass, H.Reef, KeysGeo::DZAt(X, Z), *KeysGeo::ZoneAt(X, Z));
				return;
			}
		UE_LOG(LogTemp, Log, TEXT("KRBottom goto %s: none within 2.5 km in %.1f-%.1f m"), *Args[0], D0, D1);
	}

	static void CmdGround(const TArray<FString>& Args, UWorld* W)
	{
		// where a reef-life species has the most ground: the first place within 2 km where the 25 points of a 100 m
		// square suit it by 0.6 or more on average (its own zones, depths and Where; reef cover as the framework counts it)
		AKeysDiver* Dv = W ? AKeysDiver::Get(W) : nullptr;
		const int32 SpI = Args.Num() ? KeysReefLife::FindSpecies(Args[0]) : -1;
		if (!Dv || !Dv->IsDiving() || SpI < 0) { UE_LOG(LogTemp, Log, TEXT("KRBottom ground: needs a registered species key, and the diver in the water")); return; }
		const KeysReefLife::FSpecies& S = KeysReefLife::Registry()[SpI];
		auto Suit = [&S](double X, double Z)
		{
			using namespace KeysReefLife;
			const FHabitat H = HabitatAt(X, Z);
			if (!(S.Zones & H.Zone) || H.Depth <= S.Depth[0] || H.Depth >= S.Depth[3]) return 0.0;
			double V = H.Depth < S.Depth[1] ? (H.Depth - S.Depth[0]) / FMath::Max(1e-6, S.Depth[1] - S.Depth[0]) : H.Depth > S.Depth[2] ? (S.Depth[3] - H.Depth) / FMath::Max(1e-6, S.Depth[3] - S.Depth[2]) : 1.0;
			if (H.Zone & (ZCrest | ZTop | ZSpur | ZPatch | ZFore | ZWall)) V *= FMath::Clamp(H.Reef * 1.3, 0.0, 1.0);
#if defined(KEYS_REEFLIFE_BOTTOM_HOOKS)
			if (S.Where) V *= S.Where(X, Z, H);   // (the species' own say: only with the bottom-animal hooks, request reefbottom R1)
#endif
			return V;
		};
		const FVector At = Where(W);
		double Best = -1, BX = At.X, BZ = At.Z;
		for (double Rr = 0; Rr < 2000 && Best < .6; Rr += 30)
			for (int32 k = 0; k < FMath::Max(1, int32(Rr / 15)) && Best < .6; k++)
			{
				const double A = k * 2 * PI / FMath::Max(1, int32(Rr / 15)), X = At.X + FMath::Cos(A) * Rr, Z = At.Z + FMath::Sin(A) * Rr;
				if (Suit(X, Z) <= 0) continue;
				double Sum = 0;
				for (int32 j = -2; j <= 2; j++) for (int32 i = -2; i <= 2; i++) Sum += Suit(X + i * 25, Z + j * 25);
				if (Sum / 25 > Best) { Best = Sum / 25; BX = X; BZ = Z; }
			}
		if (Best < 0) { UE_LOG(LogTemp, Log, TEXT("KRBottom ground %s: no ground for it within 2 km"), *Args[0]); return; }
		const KeysReefLife::FHabitat H = KeysReefLife::HabitatAt(BX, BZ);
		Dv->DebugCmd(TEXT("at"), FString::Printf(TEXT("%.3f,%.3f"), BX, BZ));
		Dv->DebugCmd(TEXT("depth"), FString::Printf(TEXT("%.3f"), H.Depth - 1.2));
		Dv->DebugCmd(TEXT("hold"), FString::Printf(TEXT("%.3f"), -Dv->State().Y));
		UE_LOG(LogTemp, Log, TEXT("KRBottom ground %s: the diver is put at %.0f, %.0f, %.0f m from where it was; mean suitability of the 100 m square there %.2f (zone %s, floor %.1f m, %.0f m off the chain, game zone %s)"), *Args[0], BX, BZ,
			FMath::Sqrt(FMath::Square(BX - At.X) + FMath::Square(BZ - At.Z)), Best, KeysReefLife::ZoneName(H.Zone), H.Depth, KeysGeo::DZAt(BX, BZ), *KeysGeo::ZoneAt(BX, BZ));
	}

	static FAutoConsoleCommandWithWorldAndArgs GGroundCmd(TEXT("krbottom.ground"), TEXT("Keys Run reefbottom audit: put the diver where a reef-life species has the most ground near"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdGround));
	static FAutoConsoleCommandWithWorldAndArgs GGotoCmd(TEXT("krbottom.goto"), TEXT("Keys Run reefbottom audit: put the diver over the nearest bottom of a reef-life zone (sand, seagrass, hardbottom, reeftop ...) [least depth] [greatest depth]"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdGoto));
	static FAutoConsoleCommandWithWorldAndArgs GReefCmd(TEXT("krbottom.reef"), TEXT("Keys Run reefbottom audit: count what the reef plants near the diver or the camera"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdReef));
	static FAutoConsoleCommandWithWorldAndArgs GLookCmd(TEXT("krbottom.look"), TEXT("Keys Run reefbottom audit: put the diver in front of the nearest coral of a kind"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdLook));
}
