#include "KeysLife.h"
#include "KeysWrecks.h"
#include "KeysSea.h"
#include "KeysBoatPawn.h"
#include "KeysMesh.h"
#include "KeysWorld.h"
#include "KeysFinal.h"
#include "ProceduralMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "Templates/Function.h"
#include "Templates/UniquePtr.h"

using namespace KeysGeo;
using KeysData::FZoneRow;

// ---------------------------------------------------------------- species looks (JS VIS / SHAPE tables)
enum class EKeysKind : uint8 { Fish, Tuna, Shark, Ray, Turtle, Manatee, Dolphin };
enum class EKeysPat : uint8 { None, Bars, Dots, Gspots, Lbars, Line, Mottle, Spot, Spots, Wbars, Yline, Ystripe };
enum class EKeysAct : uint8 { None, Tail, Roll, Breathe, Porpoise, Bust, Glide, Blow };
enum class EKeysTail : uint8 { Fork, Lunate, Truncate, Round, Lyre, Hetero, Flukes };
enum class EKeysFin : uint8 { Tri, Sickle, Long, Sail, Spiny, Round, Filament, Trail };

struct FKeysFin { double T0, T1, H; EKeysFin Style; };

struct FKeysVis
{
	FString Key;
	EKeysKind Kind = EKeysKind::Fish;
	double Len = 1, H = 0, W = 0, Sickle = .5, Speed = 1, Spread = 0, DMin = 0, DMax = 1e5, Bill = 0, Wing = 0;
	FLinearColor Back, Belly, Fin, Finlet;   // sRGB values
	EKeysPat Pattern = EKeysPat::None;
	EKeysAct Act = EKeysAct::None;
	bool bBlunt = false, bWings = false, bSail = false, bFlap = false, bLongPec = false, bFlat = false, bNoCatch = false;
	int32 Level = 1;   // 0 surface, 1 mid, 2 bottom
	int32 SMin = 1, SMax = 1;
	bool IsTuna() const { return Kind == EKeysKind::Tuna; }
	int32 Cap() const { return FMath::Max(6, SMax * 4); }
};

struct FKeysShape
{
	double Peak = .38, Nose = .8, Ped = .2, TL = .22, TH = .55, Pec = .15, Belly = 1, Hump = 0;
	EKeysTail Tail = EKeysTail::Fork;
	TArray<FKeysFin> Dor, Anal;
	bool bFinlets = false, bWings = false, bShark = false;
};

namespace
{
	constexpr double LifeVSC = 1.35;
	constexpr int32 LifePopTarget = 24;
	constexpr double LifePopR = 230;

	double LRand(double A, double B) { return FMath::FRandRange(A, B); }
	double LLerp(double A, double B, double T) { return A + (B - A) * T; }
	double LClamp(double V, double A, double B) { return V < A ? A : V > B ? B : V; }
	double LHyp(double A, double B) { return FMath::Sqrt(A * A + B * B); }
	double LHash2(double X, double Z) { const double H = FMath::Sin(X * 127.1 + Z * 311.7) * 43758.5453; return H - FMath::FloorToDouble(H); }
	double LWrapPi(double A) { return FMath::Fmod(FMath::Fmod(A + PI, 2 * PI) + 2 * PI, 2 * PI) - PI; }
	double LAngLerp(double A, double B, double T) { return A + LWrapPi(B - A) * FMath::Min(1.0, T); }
	double LFloorY(double D) { return -(.25 + FMath::Min(D, 14.0) * .55); }
	// colors are worked on in sRGB like the JS and turned linear (the project's vertex color convention) at the end
	FLinearColor LHex(uint32 C) { return FLinearColor(((C >> 16) & 255) / 255.f, ((C >> 8) & 255) / 255.f, (C & 255) / 255.f, 1.f); }
	FLinearColor LLin(const FLinearColor& S) { return FLinearColor(S.ToFColor(false)); }
	FLinearColor LMix(const FLinearColor& A, const FLinearColor& B, double T) { return A + (B - A) * float(T); }
	FVector LG2U(const FVector& P) { return FVector(P.X * 100.0, P.Z * 100.0, P.Y * 100.0); }
	FVector LN2U(const FVector& N) { return FVector(N.X, N.Z, N.Y); }

	// 3x3 rotation in the JS (three.js) frame: x east, y up, z south
	struct FLifeR3
	{
		double M[3][3];
		static FLifeR3 Ident() { FLifeR3 R; for (int32 i = 0; i < 3; i++) for (int32 j = 0; j < 3; j++) R.M[i][j] = i == j ? 1 : 0; return R; }
		static FLifeR3 RX(double A) { FLifeR3 R = Ident(); const double C = FMath::Cos(A), S = FMath::Sin(A); R.M[1][1] = C; R.M[1][2] = -S; R.M[2][1] = S; R.M[2][2] = C; return R; }
		static FLifeR3 RY(double A) { FLifeR3 R = Ident(); const double C = FMath::Cos(A), S = FMath::Sin(A); R.M[0][0] = C; R.M[0][2] = S; R.M[2][0] = -S; R.M[2][2] = C; return R; }
		static FLifeR3 RZ(double A) { FLifeR3 R = Ident(); const double C = FMath::Cos(A), S = FMath::Sin(A); R.M[0][0] = C; R.M[0][1] = -S; R.M[1][0] = S; R.M[1][1] = C; return R; }
		// three.js Euler orders
		static FLifeR3 YXZ(double X, double Y, double Z) { return RY(Y) * RX(X) * RZ(Z); }
		static FLifeR3 XYZ(double X, double Y, double Z) { return RX(X) * RY(Y) * RZ(Z); }
		FLifeR3 operator*(const FLifeR3& B) const
		{
			FLifeR3 R;
			for (int32 i = 0; i < 3; i++) for (int32 j = 0; j < 3; j++) R.M[i][j] = M[i][0] * B.M[0][j] + M[i][1] * B.M[1][j] + M[i][2] * B.M[2][j];
			return R;
		}
		FVector operator*(const FVector& V) const
		{
			return FVector(M[0][0] * V.X + M[0][1] * V.Y + M[0][2] * V.Z, M[1][0] * V.X + M[1][1] * V.Y + M[1][2] * V.Z, M[2][0] * V.X + M[2][1] * V.Y + M[2][2] * V.Z);
		}
	};

	const TMap<FString, FKeysVis>& LifeVisTable()
	{
		static TMap<FString, FKeysVis> M;
		if (M.Num()) return M;
		auto Add = [](const TCHAR* K) -> FKeysVis& { FKeysVis& V = M.Add(FString(K)); V.Key = K; return V; };
	// generated from the JS VIS table (index.html ~2154)
	{ FKeysVis& V = Add(TEXT("bonefish")); V.Kind = EKeysKind::Fish; V.Len = 0.65; V.H = 0.15; V.W = 0.11; V.Back = LHex(0x6f8a82); V.Belly = LHex(0xe3ebe8); V.Fin = LHex(0x9fb1aa); V.Finlet = V.Fin; V.Level = 1; V.Speed = 1.4; V.SMin = 3; V.SMax = 7; V.Spread = 2.4; V.DMin = 0.35; V.DMax = 4; V.Act = EKeysAct::Tail; }
	{ FKeysVis& V = Add(TEXT("permit")); V.Kind = EKeysKind::Fish; V.Len = 0.75; V.H = 0.42; V.W = 0.1; V.Back = LHex(0x5e6f78); V.Belly = LHex(0xd9e1e4); V.Fin = LHex(0x22282d); V.Finlet = V.Fin; V.Level = 1; V.Speed = 1.3; V.SMin = 2; V.SMax = 4; V.Spread = 2.6; V.DMin = 0.5; V.DMax = 6; V.Act = EKeysAct::Tail; }
	{ FKeysVis& V = Add(TEXT("tarpon")); V.Kind = EKeysKind::Fish; V.Len = 1.8; V.H = 0.42; V.W = 0.22; V.Back = LHex(0x46626f); V.Belly = LHex(0xeef3f5); V.Fin = LHex(0x57707c); V.Finlet = V.Fin; V.Level = 1; V.Speed = 1.2; V.SMin = 3; V.SMax = 7; V.Spread = 4.5; V.DMin = 1.5; V.DMax = 14; V.Act = EKeysAct::Roll; }
	{ FKeysVis& V = Add(TEXT("barracuda")); V.Kind = EKeysKind::Fish; V.Len = 1.3; V.H = 0.16; V.W = 0.13; V.Back = LHex(0x4f5f66); V.Belly = LHex(0xe4eaec); V.Fin = LHex(0x2b3338); V.Finlet = V.Fin; V.Pattern = EKeysPat::Spots; V.Level = 0; V.Speed = 0.6; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 0.6; V.DMax = 20; }
	{ FKeysVis& V = Add(TEXT("snook")); V.Kind = EKeysKind::Fish; V.Len = 0.85; V.H = 0.19; V.W = 0.13; V.Back = LHex(0x8a8461); V.Belly = LHex(0xecebdf); V.Fin = LHex(0xc9b860); V.Finlet = V.Fin; V.Pattern = EKeysPat::Line; V.Level = 1; V.Speed = 1.1; V.SMin = 2; V.SMax = 4; V.Spread = 2.2; V.DMin = 0.8; V.DMax = 9; }
	{ FKeysVis& V = Add(TEXT("redfish")); V.Kind = EKeysKind::Fish; V.Len = 0.8; V.H = 0.22; V.W = 0.14; V.Back = LHex(0xa8653a); V.Belly = LHex(0xefd9c4); V.Fin = LHex(0x8c4f2c); V.Finlet = V.Fin; V.Pattern = EKeysPat::Spot; V.Level = 1; V.Speed = 1; V.SMin = 2; V.SMax = 4; V.Spread = 2.2; V.DMin = 0.4; V.DMax = 5; V.Act = EKeysAct::Tail; }
	{ FKeysVis& V = Add(TEXT("trout")); V.Kind = EKeysKind::Fish; V.Len = 0.55; V.H = 0.13; V.W = 0.09; V.Back = LHex(0x6d7a6a); V.Belly = LHex(0xe8ece4); V.Fin = LHex(0x8a8f78); V.Finlet = V.Fin; V.Pattern = EKeysPat::Spots; V.Level = 1; V.Speed = 1.2; V.SMin = 3; V.SMax = 6; V.Spread = 2.2; V.DMin = 0.6; V.DMax = 6; }
	{ FKeysVis& V = Add(TEXT("mangrove")); V.Kind = EKeysKind::Fish; V.Len = 0.42; V.H = 0.14; V.W = 0.08; V.Back = LHex(0x7b4b40); V.Belly = LHex(0xd9b3a7); V.Fin = LHex(0xa2584a); V.Finlet = V.Fin; V.Level = 1; V.Speed = 1.1; V.SMin = 6; V.SMax = 12; V.Spread = 2.8; V.DMin = 1.5; V.DMax = 20; }
	{ FKeysVis& V = Add(TEXT("yellowtail")); V.Kind = EKeysKind::Fish; V.Len = 0.42; V.H = 0.12; V.W = 0.07; V.Back = LHex(0x5f7aa0); V.Belly = LHex(0xe8eef4); V.Fin = LHex(0xf2cc2f); V.Finlet = V.Fin; V.Pattern = EKeysPat::Yline; V.Level = 1; V.Speed = 1.5; V.SMin = 8; V.SMax = 14; V.Spread = 3.2; V.DMin = 2; V.DMax = 40; }
	{ FKeysVis& V = Add(TEXT("mutton")); V.Kind = EKeysKind::Fish; V.Len = 0.65; V.H = 0.22; V.W = 0.11; V.Back = LHex(0x7a6b4e); V.Belly = LHex(0xefcfc4); V.Fin = LHex(0xd3684f); V.Finlet = V.Fin; V.Level = 2; V.Speed = 0.9; V.SMin = 1; V.SMax = 3; V.Spread = 2; V.DMin = 2; V.DMax = 40; }
	{ FKeysVis& V = Add(TEXT("hogfish")); V.Kind = EKeysKind::Fish; V.Len = 0.6; V.H = 0.27; V.W = 0.11; V.Back = LHex(0xd68f73); V.Belly = LHex(0xf5ddd2); V.Fin = LHex(0xb25e45); V.Finlet = V.Fin; V.Level = 2; V.Speed = 0.8; V.SMin = 1; V.SMax = 2; V.Spread = 2; V.DMin = 2.5; V.DMax = 40; }
	{ FKeysVis& V = Add(TEXT("grouper")); V.Kind = EKeysKind::Fish; V.Len = 1.15; V.H = 0.38; V.W = 0.3; V.Back = LHex(0x4c4a43); V.Belly = LHex(0x8c8778); V.Fin = LHex(0x3a3833); V.Finlet = V.Fin; V.Pattern = EKeysPat::Mottle; V.Level = 2; V.Speed = 0.5; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 3; V.DMax = 60; }
	{ FKeysVis& V = Add(TEXT("parrotfish")); V.Kind = EKeysKind::Fish; V.Len = 0.55; V.H = 0.2; V.W = 0.12; V.Back = LHex(0x2f8f8a); V.Belly = LHex(0x7fd3c3); V.Fin = LHex(0xd26aa0); V.Finlet = V.Fin; V.Pattern = EKeysPat::Mottle; V.Level = 2; V.Speed = 0.9; V.SMin = 2; V.SMax = 5; V.Spread = 2.5; V.DMin = 2; V.DMax = 30; }
	{ FKeysVis& V = Add(TEXT("angelfish")); V.Kind = EKeysKind::Fish; V.Len = 0.38; V.H = 0.4; V.W = 0.06; V.Back = LHex(0x2c5fa8); V.Belly = LHex(0xe8c33a); V.Fin = LHex(0xf2d23b); V.Finlet = V.Fin; V.Level = 1; V.Speed = 0.7; V.SMin = 1; V.SMax = 2; V.Spread = 1; V.DMin = 2; V.DMax = 30; }
	{ FKeysVis& V = Add(TEXT("mahi")); V.Kind = EKeysKind::Fish; V.Len = 1.25; V.H = 0.36; V.W = 0.14; V.Back = LHex(0x2f8f6a); V.Belly = LHex(0xe3d24a); V.Fin = LHex(0x2a6fa3); V.Finlet = V.Fin; V.Pattern = EKeysPat::Spots; V.bBlunt = true; V.Level = 0; V.Speed = 2.2; V.SMin = 3; V.SMax = 6; V.Spread = 3.5; V.DMin = 25; V.DMax = 100000; }
	{ FKeysVis& V = Add(TEXT("blackfin")); V.Kind = EKeysKind::Fish; V.Len = 0.8; V.H = 0.22; V.W = 0.18; V.Back = LHex(0x1e2d4f); V.Belly = LHex(0xd7dde4); V.Fin = LHex(0xe3c64a); V.Finlet = V.Fin; V.Level = 0; V.Speed = 3; V.SMin = 6; V.SMax = 10; V.Spread = 3.5; V.DMin = 25; V.DMax = 100000; }
	{ FKeysVis& V = Add(TEXT("wahoo")); V.Kind = EKeysKind::Fish; V.Len = 1.6; V.H = 0.2; V.W = 0.15; V.Back = LHex(0x2a5d8f); V.Belly = LHex(0xdfe8ef); V.Fin = LHex(0x203e5c); V.Finlet = V.Fin; V.Pattern = EKeysPat::Bars; V.Level = 0; V.Speed = 3.2; V.SMin = 1; V.SMax = 2; V.Spread = 3; V.DMin = 25; V.DMax = 100000; }
	{ FKeysVis& V = Add(TEXT("sailfish")); V.Kind = EKeysKind::Fish; V.Len = 2.4; V.H = 0.32; V.W = 0.15; V.Back = LHex(0x21336e); V.Belly = LHex(0xe3e8f0); V.Fin = LHex(0x2f4ea8); V.Finlet = V.Fin; V.Level = 0; V.Speed = 2; V.SMin = 1; V.SMax = 3; V.Spread = 6; V.DMin = 25; V.DMax = 100000; V.Bill = 0.35; V.bSail = true; }
	{ FKeysVis& V = Add(TEXT("yellowfin")); V.Kind = EKeysKind::Tuna; V.Len = 1.5; V.H = 0.36; V.W = 0.3; V.Back = LHex(0x1b2b57); V.Belly = LHex(0xe3e8ee); V.Fin = LHex(0xf2c62e); V.Finlet = LHex(0xf2c62e); V.Sickle = 0.95; V.Pattern = EKeysPat::Ystripe; V.Level = 0; V.Speed = 3.2; V.SMin = 10; V.SMax = 18; V.Spread = 5; V.DMin = 30; V.DMax = 100000; V.Act = EKeysAct::Bust; }
	{ FKeysVis& V = Add(TEXT("bluefin")); V.Kind = EKeysKind::Tuna; V.Len = 2.3; V.H = 0.6; V.W = 0.5; V.Back = LHex(0x121f45); V.Belly = LHex(0xd9dfe6); V.Fin = LHex(0x27324e); V.Finlet = LHex(0xe8c440); V.Sickle = 0.42; V.Pattern = EKeysPat::Spots; V.Level = 0; V.Speed = 3; V.SMin = 6; V.SMax = 12; V.Spread = 6; V.DMin = 30; V.DMax = 100000; V.Act = EKeysAct::Bust; }
	{ FKeysVis& V = Add(TEXT("albacore")); V.Kind = EKeysKind::Tuna; V.Len = 1; V.H = 0.26; V.W = 0.22; V.Back = LHex(0x263559); V.Belly = LHex(0xe6ebf1); V.Fin = LHex(0x2c3a5c); V.Finlet = LHex(0xd9dde2); V.Sickle = 0.4; V.Level = 0; V.Speed = 3.3; V.SMin = 10; V.SMax = 16; V.Spread = 4; V.DMin = 30; V.DMax = 100000; V.Act = EKeysAct::Bust; V.bLongPec = true; }
	{ FKeysVis& V = Add(TEXT("cobia")); V.Kind = EKeysKind::Fish; V.Len = 1.3; V.H = 0.22; V.W = 0.22; V.Back = LHex(0x3b3328); V.Belly = LHex(0xe8e2d6); V.Fin = LHex(0x2c261f); V.Finlet = V.Fin; V.Pattern = EKeysPat::Line; V.Level = 1; V.Speed = 1.1; V.SMin = 1; V.SMax = 3; V.Spread = 3; V.DMin = 2; V.DMax = 80; }
	{ FKeysVis& V = Add(TEXT("amberjack")); V.Kind = EKeysKind::Fish; V.Len = 1.3; V.H = 0.34; V.W = 0.2; V.Back = LHex(0x6d6a52); V.Belly = LHex(0xdfe3e2); V.Fin = LHex(0xc9b25a); V.Finlet = V.Fin; V.Level = 1; V.Speed = 1.6; V.SMin = 3; V.SMax = 7; V.Spread = 4; V.DMin = 15; V.DMax = 100000; }
	{ FKeysVis& V = Add(TEXT("kingfish")); V.Kind = EKeysKind::Fish; V.Len = 1.4; V.H = 0.2; V.W = 0.15; V.Back = LHex(0x3d5a7a); V.Belly = LHex(0xe8eef2); V.Fin = LHex(0x334d68); V.Finlet = V.Fin; V.Level = 0; V.Speed = 3; V.SMin = 2; V.SMax = 5; V.Spread = 5; V.DMin = 12; V.DMax = 100000; }
	{ FKeysVis& V = Add(TEXT("cero")); V.Kind = EKeysKind::Fish; V.Len = 0.7; V.H = 0.14; V.W = 0.09; V.Back = LHex(0x3f6286); V.Belly = LHex(0xeef2f5); V.Fin = LHex(0xc9b25a); V.Finlet = V.Fin; V.Pattern = EKeysPat::Gspots; V.Level = 0; V.Speed = 2.2; V.SMin = 3; V.SMax = 6; V.Spread = 3; V.DMin = 4; V.DMax = 60; }
	{ FKeysVis& V = Add(TEXT("jackcrevalle")); V.Kind = EKeysKind::Fish; V.Len = 0.85; V.H = 0.32; V.W = 0.14; V.Back = LHex(0x4f6a6e); V.Belly = LHex(0xe6e4c6); V.Fin = LHex(0xd9bd4a); V.Finlet = V.Fin; V.Level = 1; V.Speed = 2; V.SMin = 4; V.SMax = 9; V.Spread = 4; V.DMin = 1; V.DMax = 40; }
	{ FKeysVis& V = Add(TEXT("ladyfish")); V.Kind = EKeysKind::Fish; V.Len = 0.6; V.H = 0.12; V.W = 0.08; V.Back = LHex(0x7f9aa0); V.Belly = LHex(0xeef3f4); V.Fin = LHex(0xa8b8bd); V.Finlet = V.Fin; V.Level = 0; V.Speed = 1.8; V.SMin = 4; V.SMax = 10; V.Spread = 4; V.DMin = 0.5; V.DMax = 7; }
	{ FKeysVis& V = Add(TEXT("sheepshead")); V.Kind = EKeysKind::Fish; V.Len = 0.55; V.H = 0.3; V.W = 0.1; V.Back = LHex(0xc9c9c0); V.Belly = LHex(0xeeeeea); V.Fin = LHex(0x3a3a38); V.Finlet = V.Fin; V.Pattern = EKeysPat::Bars; V.Level = 2; V.Speed = 0.8; V.SMin = 2; V.SMax = 6; V.Spread = 2.5; V.DMin = 2; V.DMax = 20; }
	{ FKeysVis& V = Add(TEXT("pompano")); V.Kind = EKeysKind::Fish; V.Len = 0.42; V.H = 0.2; V.W = 0.07; V.Back = LHex(0x9aa9ad); V.Belly = LHex(0xf1e9b0); V.Fin = LHex(0xe5c84a); V.Finlet = V.Fin; V.Level = 1; V.Speed = 1.6; V.SMin = 3; V.SMax = 7; V.Spread = 3; V.DMin = 0.8; V.DMax = 8; }
	{ FKeysVis& V = Add(TEXT("tripletail")); V.Kind = EKeysKind::Fish; V.Len = 0.65; V.H = 0.32; V.W = 0.12; V.Back = LHex(0x4e4434); V.Belly = LHex(0x6b5e47); V.Fin = LHex(0x3b3226); V.Finlet = V.Fin; V.Pattern = EKeysPat::Mottle; V.Level = 0; V.Speed = 0.3; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 2; V.DMax = 100000; }
	{ FKeysVis& V = Add(TEXT("lionfish")); V.Kind = EKeysKind::Fish; V.Len = 0.35; V.H = 0.2; V.W = 0.1; V.Back = LHex(0xa5432e); V.Belly = LHex(0xf2e3d6); V.Fin = LHex(0xc25a40); V.Finlet = V.Fin; V.Pattern = EKeysPat::Wbars; V.Level = 2; V.Speed = 0.4; V.SMin = 1; V.SMax = 2; V.Spread = 1.5; V.DMin = 3; V.DMax = 60; }
	{ FKeysVis& V = Add(TEXT("goliath")); V.Kind = EKeysKind::Fish; V.Len = 2.2; V.H = 0.8; V.W = 0.62; V.Back = LHex(0x6b5c3e); V.Belly = LHex(0x9a8a64); V.Fin = LHex(0x5a4d33); V.Finlet = V.Fin; V.Pattern = EKeysPat::Mottle; V.Level = 2; V.Speed = 0.3; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 4; V.DMax = 60; }
	{ FKeysVis& V = Add(TEXT("graytrigger")); V.Kind = EKeysKind::Fish; V.Len = 0.45; V.H = 0.26; V.W = 0.08; V.Back = LHex(0x6f6f68); V.Belly = LHex(0xa8a79d); V.Fin = LHex(0x5d5d56); V.Finlet = V.Fin; V.Level = 1; V.Speed = 0.7; V.SMin = 1; V.SMax = 3; V.Spread = 2; V.DMin = 3; V.DMax = 100000; }
	{ FKeysVis& V = Add(TEXT("bullshark")); V.Kind = EKeysKind::Shark; V.Len = 2.3; V.H = 0.46; V.W = 0.46; V.Back = LHex(0x6b6f68); V.Belly = LHex(0xeceeea); V.Fin = LHex(0x5f635c); V.Finlet = V.Fin; V.Level = 1; V.Speed = 1.3; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 1.5; V.DMax = 40; }
	{ FKeysVis& V = Add(TEXT("lemonshark")); V.Kind = EKeysKind::Shark; V.Len = 2.3; V.H = 0.38; V.W = 0.38; V.Back = LHex(0xa6976b); V.Belly = LHex(0xe8e2c6); V.Fin = LHex(0x9a8a5f); V.Finlet = V.Fin; V.Level = 1; V.Speed = 1.2; V.SMin = 1; V.SMax = 2; V.Spread = 4; V.DMin = 0.8; V.DMax = 20; }
	{ FKeysVis& V = Add(TEXT("hammerhead")); V.Kind = EKeysKind::Shark; V.Len = 3.3; V.H = 0.5; V.W = 0.5; V.Back = LHex(0x6e7a80); V.Belly = LHex(0xeef0f1); V.Fin = LHex(0x5e6a70); V.Finlet = V.Fin; V.Level = 1; V.Speed = 1.4; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 6; V.DMax = 100000; }
	{ FKeysVis& V = Add(TEXT("marlin")); V.Kind = EKeysKind::Fish; V.Len = 3.3; V.H = 0.52; V.W = 0.36; V.Back = LHex(0x1b2f6e); V.Belly = LHex(0xe6ebf2); V.Fin = LHex(0x253f8a); V.Finlet = V.Fin; V.Pattern = EKeysPat::Lbars; V.Level = 0; V.Speed = 2.2; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 40; V.DMax = 100000; V.Bill = 0.3; }
	{ FKeysVis& V = Add(TEXT("blackmarlin")); V.Kind = EKeysKind::Fish; V.Len = 3.6; V.H = 0.62; V.W = 0.42; V.Back = LHex(0x20262f); V.Belly = LHex(0xdfe4ea); V.Fin = LHex(0x2a313b); V.Finlet = V.Fin; V.Level = 0; V.Speed = 2.1; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 40; V.DMax = 100000; V.Bill = 0.28; }
	{ FKeysVis& V = Add(TEXT("gag")); V.Kind = EKeysKind::Fish; V.Len = 0.9; V.H = 0.26; V.W = 0.2; V.Back = LHex(0x5d5a52); V.Belly = LHex(0xa49e90); V.Fin = LHex(0x45423c); V.Finlet = V.Fin; V.Pattern = EKeysPat::Mottle; V.Level = 2; V.Speed = 0.55; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 3; V.DMax = 60; }
	{ FKeysVis& V = Add(TEXT("redgrouper")); V.Kind = EKeysKind::Fish; V.Len = 0.75; V.H = 0.25; V.W = 0.2; V.Back = LHex(0x8a4a38); V.Belly = LHex(0xc79a86); V.Fin = LHex(0x6c3a2c); V.Finlet = V.Fin; V.Pattern = EKeysPat::Spots; V.Level = 2; V.Speed = 0.5; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 3; V.DMax = 60; }
	{ FKeysVis& V = Add(TEXT("swordfish")); V.Kind = EKeysKind::Fish; V.Len = 2.6; V.H = 0.45; V.W = 0.3; V.Back = LHex(0x3a3446); V.Belly = LHex(0xa9a4ae); V.Fin = LHex(0x2c2834); V.Finlet = V.Fin; V.Level = 1; V.Speed = 1.5; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 40; V.DMax = 100000; V.Bill = 0.6; }
	{ FKeysVis& V = Add(TEXT("flyingfish")); V.Kind = EKeysKind::Fish; V.Len = 0.32; V.H = 0.06; V.W = 0.05; V.Back = LHex(0x2d4f8f); V.Belly = LHex(0xdfe6f2); V.Fin = LHex(0x9fb6dd); V.Finlet = V.Fin; V.Level = 0; V.Speed = 2.5; V.SMin = 4; V.SMax = 9; V.Spread = 3; V.DMin = 25; V.DMax = 100000; V.Act = EKeysAct::Glide; V.bWings = true; }
	{ FKeysVis& V = Add(TEXT("blacktip")); V.Kind = EKeysKind::Shark; V.Len = 1.7; V.H = 0.28; V.W = 0.28; V.Back = LHex(0x7a8187); V.Belly = LHex(0xeef0f1); V.Fin = LHex(0x2a2e32); V.Finlet = V.Fin; V.Level = 1; V.Speed = 1.6; V.SMin = 1; V.SMax = 2; V.Spread = 5; V.DMin = 0.6; V.DMax = 20; }
	{ FKeysVis& V = Add(TEXT("nurse")); V.Kind = EKeysKind::Shark; V.Len = 2.2; V.H = 0.3; V.W = 0.38; V.Back = LHex(0x8a7556); V.Belly = LHex(0xc9b89a); V.Fin = LHex(0x7a6548); V.Finlet = V.Fin; V.Level = 2; V.Speed = 0.4; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 2; V.DMax = 30; }
	{ FKeysVis& V = Add(TEXT("stingray")); V.Kind = EKeysKind::Ray; V.Len = 1; V.Back = LHex(0x8a7b62); V.Belly = LHex(0xf0ebe0); V.Fin = LHex(0x8a7b62); V.Finlet = V.Fin; V.Level = 2; V.Speed = 0.5; V.SMin = 1; V.SMax = 2; V.Spread = 2.5; V.DMin = 0.4; V.DMax = 10; V.Wing = 1.1; }
	{ FKeysVis& V = Add(TEXT("eagleray")); V.Kind = EKeysKind::Ray; V.Len = 1.1; V.Back = LHex(0x2b2f38); V.Belly = LHex(0xf2f2f2); V.Fin = LHex(0x2b2f38); V.Finlet = V.Fin; V.Pattern = EKeysPat::Dots; V.Level = 1; V.Speed = 1.2; V.SMin = 1; V.SMax = 3; V.Spread = 4; V.DMin = 1.5; V.DMax = 25; V.bFlap = true; V.Wing = 2.3; }
	{ FKeysVis& V = Add(TEXT("turtle")); V.Kind = EKeysKind::Turtle; V.Len = 1; V.Back = LHex(0x7b5a34); V.Belly = LHex(0xd8c08a); V.Fin = LHex(0x7b5a34); V.Finlet = V.Fin; V.Pattern = EKeysPat::Mottle; V.Level = 1; V.Speed = 0.7; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 2; V.DMax = 80; V.Act = EKeysAct::Breathe; }
	{ FKeysVis& V = Add(TEXT("manatee")); V.Kind = EKeysKind::Manatee; V.Len = 3; V.Back = LHex(0x7d8384); V.Belly = LHex(0x9aa0a0); V.Fin = LHex(0x7d8384); V.Finlet = V.Fin; V.Level = 1; V.Speed = 0.35; V.SMin = 1; V.SMax = 2; V.Spread = 4; V.DMin = 1; V.DMax = 6; V.Act = EKeysAct::Breathe; }
	{ FKeysVis& V = Add(TEXT("dolphin")); V.Kind = EKeysKind::Dolphin; V.Len = 2.4; V.Back = LHex(0x646d78); V.Belly = LHex(0xd9dde2); V.Fin = LHex(0x646d78); V.Finlet = V.Fin; V.Level = 0; V.Speed = 3; V.SMin = 3; V.SMax = 6; V.Spread = 4.5; V.DMin = 3; V.DMax = 100000; V.Act = EKeysAct::Porpoise; }
	// the whales (web content/creatures.ts): pilot whales round the humps and offshore, the odd humpback; they blow at the surface
	{ FKeysVis& V = Add(TEXT("pilotwhale")); V.Kind = EKeysKind::Dolphin; V.Len = 6; V.H = 1.05; V.W = .95; V.Back = LHex(0x2a2a2c); V.Belly = LHex(0x3a3a3c); V.Fin = V.Back; V.Finlet = V.Fin; V.Level = 0; V.Speed = 2.4; V.SMin = 5; V.SMax = 14; V.Spread = 6; V.DMin = 12; V.DMax = 100000; V.Act = EKeysAct::Blow; }
	{ FKeysVis& V = Add(TEXT("humpback")); V.Kind = EKeysKind::Dolphin; V.Len = 15; V.H = 2.6; V.W = 2.1; V.Back = LHex(0x1c2430); V.Belly = LHex(0x8f97a0); V.Fin = V.Back; V.Finlet = V.Fin; V.Pattern = EKeysPat::Mottle; V.Level = 0; V.Speed = 1.8; V.SMin = 1; V.SMax = 2; V.Spread = 10; V.DMin = 25; V.DMax = 100000; V.Act = EKeysAct::Blow; }
	// the web game's later creatures (content/creatures.ts VIS)
	{ FKeysVis& V = Add(TEXT("flounder")); V.Kind = EKeysKind::Fish; V.Len = 0.5; V.H = 0.3; V.W = 0.055; V.Back = LHex(0x6b5f4a); V.Belly = LHex(0xefe9dd); V.Fin = LHex(0x5e5342); V.Finlet = V.Fin; V.Pattern = EKeysPat::Spot; V.Level = 2; V.Speed = 0.45; V.SMin = 1; V.SMax = 2; V.Spread = 2.2; V.DMin = 0.5; V.DMax = 14; V.bFlat = true; }
	{ FKeysVis& V = Add(TEXT("sole")); V.Kind = EKeysKind::Fish; V.Len = 0.26; V.H = 0.17; V.W = 0.03; V.Back = LHex(0x5e5444); V.Belly = LHex(0xefe9dd); V.Fin = LHex(0x4e4639); V.Finlet = V.Fin; V.Pattern = EKeysPat::Mottle; V.Level = 2; V.Speed = 0.38; V.SMin = 1; V.SMax = 3; V.Spread = 1.8; V.DMin = 0.5; V.DMax = 18; V.bFlat = true; }
	{ FKeysVis& V = Add(TEXT("halibut")); V.Kind = EKeysKind::Fish; V.Len = 1.25; V.H = 0.72; V.W = 0.13; V.Back = LHex(0x4a4a44); V.Belly = LHex(0xf0ece2); V.Fin = LHex(0x3e3e39); V.Finlet = V.Fin; V.Pattern = EKeysPat::Mottle; V.Level = 2; V.Speed = 0.6; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 18; V.DMax = 240; V.bFlat = true; }
	{ FKeysVis& V = Add(TEXT("bluerunner")); V.Kind = EKeysKind::Fish; V.Len = 0.5; V.H = 0.15; V.W = 0.09; V.Back = LHex(0x43707d); V.Belly = LHex(0xeef2f2); V.Fin = LHex(0xcdb94e); V.Finlet = V.Fin; V.Level = 1; V.Speed = 2; V.SMin = 12; V.SMax = 26; V.Spread = 4.2; V.DMin = 1.5; V.DMax = 120; }
	{ FKeysVis& V = Add(TEXT("schoolmaster")); V.Kind = EKeysKind::Fish; V.Len = 0.45; V.H = 0.16; V.W = 0.09; V.Back = LHex(0x8a7347); V.Belly = LHex(0xe8dcc0); V.Fin = LHex(0xe0ac3c); V.Finlet = V.Fin; V.Pattern = EKeysPat::Bars; V.Level = 1; V.Speed = 1.1; V.SMin = 8; V.SMax = 18; V.Spread = 3; V.DMin = 1.5; V.DMax = 30; }
	{ FKeysVis& V = Add(TEXT("lanesnapper")); V.Kind = EKeysKind::Fish; V.Len = 0.34; V.H = 0.12; V.W = 0.07; V.Back = LHex(0xc07a68); V.Belly = LHex(0xf2ded2); V.Fin = LHex(0xe8c050); V.Finlet = V.Fin; V.Pattern = EKeysPat::Yline; V.Level = 2; V.Speed = 1.1; V.SMin = 7; V.SMax = 15; V.Spread = 2.8; V.DMin = 2; V.DMax = 45; }
	{ FKeysVis& V = Add(TEXT("vermilion")); V.Kind = EKeysKind::Fish; V.Len = 0.4; V.H = 0.14; V.W = 0.08; V.Back = LHex(0xad3a2a); V.Belly = LHex(0xeec6b6); V.Fin = LHex(0xc24631); V.Finlet = V.Fin; V.Level = 1; V.Speed = 1.3; V.SMin = 9; V.SMax = 20; V.Spread = 3.2; V.DMin = 8; V.DMax = 90; }
	{ FKeysVis& V = Add(TEXT("lookdown")); V.Kind = EKeysKind::Fish; V.Len = 0.3; V.H = 0.33; V.W = 0.045; V.Back = LHex(0xb9c8d2); V.Belly = LHex(0xf4f8fa); V.Fin = LHex(0xd8e2e8); V.Finlet = V.Fin; V.Level = 1; V.Speed = 1.2; V.SMin = 6; V.SMax = 14; V.Spread = 2.4; V.DMin = 1.5; V.DMax = 25; }
	{ FKeysVis& V = Add(TEXT("palometa")); V.Kind = EKeysKind::Fish; V.Len = 0.3; V.H = 0.17; V.W = 0.05; V.Back = LHex(0x9fb0b8); V.Belly = LHex(0xf4f0e0); V.Fin = LHex(0x2a3038); V.Finlet = V.Fin; V.Pattern = EKeysPat::Bars; V.Level = 1; V.Speed = 1.8; V.SMin = 6; V.SMax = 13; V.Spread = 2.8; V.DMin = 0.6; V.DMax = 12; }
	{ FKeysVis& V = Add(TEXT("spanishmack")); V.Kind = EKeysKind::Fish; V.Len = 0.6; V.H = 0.13; V.W = 0.08; V.Back = LHex(0x3a6b8c); V.Belly = LHex(0xeef3f6); V.Fin = LHex(0x2d5370); V.Finlet = V.Fin; V.Pattern = EKeysPat::Gspots; V.Level = 0; V.Speed = 2.6; V.SMin = 7; V.SMax = 16; V.Spread = 4; V.DMin = 2; V.DMax = 50; }
	{ FKeysVis& V = Add(TEXT("bigeye")); V.Kind = EKeysKind::Tuna; V.Len = 1.6; V.H = 0.42; V.W = 0.34; V.Back = LHex(0x1c3050); V.Belly = LHex(0xe6ebf0); V.Fin = LHex(0x27405e); V.Finlet = LHex(0xf0c63a); V.Sickle = 0.5; V.Level = 1; V.Speed = 3; V.SMin = 5; V.SMax = 11; V.Spread = 5; V.DMin = 40; V.DMax = 100000; V.Act = EKeysAct::Bust; }
	{ FKeysVis& V = Add(TEXT("skipjack")); V.Kind = EKeysKind::Tuna; V.Len = 0.75; V.H = 0.2; V.W = 0.17; V.Back = LHex(0x23344f); V.Belly = LHex(0xdfe6ec); V.Fin = LHex(0x2b3a52); V.Finlet = LHex(0xc8d0d8); V.Sickle = 0.35; V.Pattern = EKeysPat::Line; V.Level = 0; V.Speed = 3.4; V.SMin = 14; V.SMax = 30; V.Spread = 5.5; V.DMin = 25; V.DMax = 100000; V.Act = EKeysAct::Bust; }
	{ FKeysVis& V = Add(TEXT("whaleshark")); V.Kind = EKeysKind::Shark; V.Len = 9; V.H = 1.5; V.W = 1.6; V.Back = LHex(0x3f4a55); V.Belly = LHex(0xe8ecee); V.Fin = LHex(0x35404a); V.Finlet = V.Fin; V.Pattern = EKeysPat::Gspots; V.Level = 0; V.Speed = 1.1; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 8; V.DMax = 100000; V.bNoCatch = true; }
	{ FKeysVis& V = Add(TEXT("giantmanta")); V.Kind = EKeysKind::Ray; V.Len = 2.4; V.Back = LHex(0x1d2127); V.Belly = LHex(0xf4f4f2); V.Fin = V.Back; V.Finlet = V.Fin; V.Pattern = EKeysPat::Spots; V.Level = 1; V.Speed = 1.5; V.SMin = 1; V.SMax = 2; V.Spread = 6; V.DMin = 4; V.DMax = 100000; V.Wing = 5.5; V.bFlap = true; V.bNoCatch = true; }
	{ FKeysVis& V = Add(TEXT("sawfish")); V.Kind = EKeysKind::Shark; V.Len = 4.2; V.H = 0.42; V.W = 0.5; V.Back = LHex(0x8d8c7c); V.Belly = LHex(0xe6e3d4); V.Fin = LHex(0x7b7a6b); V.Finlet = V.Fin; V.Level = 2; V.Speed = 0.6; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 0.8; V.DMax = 12; V.Bill = 0.26; V.bNoCatch = true; }
	{ FKeysVis& V = Add(TEXT("tigershark")); V.Kind = EKeysKind::Shark; V.Len = 4.2; V.H = 0.78; V.W = 0.72; V.Back = LHex(0x5f6a63); V.Belly = LHex(0xeceeea); V.Fin = LHex(0x515b55); V.Finlet = V.Fin; V.Pattern = EKeysPat::Bars; V.Level = 1; V.Speed = 1.5; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 6; V.DMax = 100000; }
	{ FKeysVis& V = Add(TEXT("mako")); V.Kind = EKeysKind::Shark; V.Len = 3.6; V.H = 0.58; V.W = 0.56; V.Back = LHex(0x1f4f86); V.Belly = LHex(0xf2f5f8); V.Fin = LHex(0x1b4472); V.Finlet = V.Fin; V.Level = 1; V.Speed = 3.4; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 20; V.DMax = 100000; }
	{ FKeysVis& V = Add(TEXT("greatwhite")); V.Kind = EKeysKind::Shark; V.Len = 5; V.H = 1.05; V.W = 1; V.Back = LHex(0x6a7078); V.Belly = LHex(0xf4f4f2); V.Fin = LHex(0x565c63); V.Finlet = V.Fin; V.Level = 1; V.Speed = 1.6; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 12; V.DMax = 100000; V.bNoCatch = true; }
	{ FKeysVis& V = Add(TEXT("spermwhale")); V.Kind = EKeysKind::Dolphin; V.Len = 14; V.H = 2.3; V.W = 2.1; V.Back = LHex(0x4a4542); V.Belly = LHex(0x6b6460); V.Fin = V.Back; V.Finlet = V.Fin; V.Pattern = EKeysPat::Mottle; V.Level = 0; V.Speed = 1.6; V.SMin = 1; V.SMax = 3; V.Spread = 12; V.DMin = 35; V.DMax = 100000; V.Act = EKeysAct::Blow; V.bBlunt = true; V.bNoCatch = true; }
	{ FKeysVis& V = Add(TEXT("mola")); V.Kind = EKeysKind::Fish; V.Len = 3; V.H = 2.6; V.W = 0.55; V.Back = LHex(0x8d939a); V.Belly = LHex(0xd8dce0); V.Fin = V.Back; V.Finlet = V.Fin; V.Pattern = EKeysPat::Mottle; V.Level = 0; V.Speed = 0.5; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 18; V.DMax = 100000; V.Act = EKeysAct::Roll; V.bNoCatch = true; }
	{ FKeysVis& V = Add(TEXT("warsaw")); V.Kind = EKeysKind::Fish; V.Len = 2.3; V.H = 0.72; V.W = 0.5; V.Back = LHex(0x3b332e); V.Belly = LHex(0x6d625a); V.Fin = LHex(0x2c2521); V.Finlet = V.Fin; V.Level = 2; V.Speed = 0.45; V.SMin = 1; V.SMax = 1; V.Spread = 0; V.DMin = 25; V.DMax = 100000; }
	{ FKeysVis& V = Add(TEXT("cubera")); V.Kind = EKeysKind::Fish; V.Len = 1.6; V.H = 0.52; V.W = 0.28; V.Back = LHex(0x5a4a46); V.Belly = LHex(0xb9a89c); V.Fin = LHex(0x3e3330); V.Finlet = V.Fin; V.Level = 2; V.Speed = 0.8; V.SMin = 1; V.SMax = 3; V.Spread = 3; V.DMin = 8; V.DMax = 60; }
	// the gold bluefin (entities/fish/legend-path.ts): never caught, placed by the wall clock (AKeysFinal)
	{ FKeysVis& V = Add(TEXT("goldbluefin")); V.Kind = EKeysKind::Tuna; V.Len = 3.1; V.H = .84; V.W = .66; V.Back = LHex(0x8a6a1c); V.Belly = LHex(0xf6e7a8); V.Fin = LHex(0xb8902c); V.Finlet = LHex(0xffd84a); V.Sickle = .46; V.Pattern = EKeysPat::Spots; V.Level = 0; V.Speed = 3.4; V.SMin = 3; V.SMax = 3; V.Spread = 5; V.DMin = 6; V.DMax = 100000; V.Act = EKeysAct::Bust; }
		return M;
	}

	const FKeysVis* LifeVis(const FString& K) { return LifeVisTable().Find(K); }

	const FKeysShape& LifeShapeOf(const FString& K)
	{
		static TMap<FString, FKeysShape> M;
		if (!M.Num())
		{
			auto Add = [](const TCHAR* Key) -> FKeysShape& { return M.Add(FString(Key)); };
	{ FKeysShape& S = Add(TEXT("bonefish")); S.Peak = 0.38; S.Nose = 1; S.Ped = 0.17; S.Tail = EKeysTail::Fork; S.TL = 0.24; S.TH = 0.5; S.Pec = 0.15; S.Dor.Add({0.33, 0.48, 0.55, EKeysFin::Tri}); S.Anal.Add({0.7, 0.78, 0.22, EKeysFin::Tri}); S.Belly = 0.9; }
	{ FKeysShape& S = Add(TEXT("permit")); S.Peak = 0.42; S.Nose = 0.42; S.Ped = 0.11; S.Tail = EKeysTail::Fork; S.TL = 0.34; S.TH = 0.62; S.Pec = 0.16; S.Dor.Add({0.4, 0.72, 0.32, EKeysFin::Sickle}); S.Anal.Add({0.46, 0.74, 0.34, EKeysFin::Sickle}); }
	{ FKeysShape& S = Add(TEXT("tarpon")); S.Peak = 0.38; S.Nose = 0.75; S.Ped = 0.16; S.Tail = EKeysTail::Fork; S.TL = 0.26; S.TH = 0.6; S.Pec = 0.12; S.Dor.Add({0.43, 0.56, 0.55, EKeysFin::Sickle}); S.Anal.Add({0.62, 0.78, 0.32, EKeysFin::Tri}); }
	{ FKeysShape& S = Add(TEXT("barracuda")); S.Peak = 0.52; S.Nose = 1.5; S.Ped = 0.26; S.Tail = EKeysTail::Fork; S.TL = 0.18; S.TH = 0.75; S.Pec = 0.09; S.Dor.Add({0.3, 0.38, 0.38, EKeysFin::Tri}); S.Dor.Add({0.64, 0.71, 0.42, EKeysFin::Tri}); S.Anal.Add({0.65, 0.72, 0.38, EKeysFin::Tri}); }
	{ FKeysShape& S = Add(TEXT("snook")); S.Peak = 0.34; S.Nose = 1; S.Ped = 0.22; S.Tail = EKeysTail::Fork; S.TL = 0.2; S.TH = 0.6; S.Pec = 0.12; S.Dor.Add({0.3, 0.45, 0.55, EKeysFin::Spiny}); S.Dor.Add({0.5, 0.66, 0.4, EKeysFin::Round}); S.Anal.Add({0.67, 0.75, 0.35, EKeysFin::Tri}); }
	{ FKeysShape& S = Add(TEXT("redfish")); S.Peak = 0.38; S.Nose = 0.75; S.Ped = 0.26; S.Tail = EKeysTail::Truncate; S.TL = 0.17; S.TH = 0.55; S.Pec = 0.14; S.Dor.Add({0.28, 0.43, 0.42, EKeysFin::Spiny}); S.Dor.Add({0.45, 0.72, 0.3, EKeysFin::Long}); S.Anal.Add({0.66, 0.77, 0.3, EKeysFin::Round}); }
	{ FKeysShape& S = Add(TEXT("trout")); S.Peak = 0.36; S.Nose = 1; S.Ped = 0.22; S.Tail = EKeysTail::Truncate; S.TL = 0.18; S.TH = 0.6; S.Pec = 0.12; S.Dor.Add({0.3, 0.42, 0.45, EKeysFin::Spiny}); S.Dor.Add({0.45, 0.72, 0.3, EKeysFin::Long}); S.Anal.Add({0.64, 0.78, 0.25, EKeysFin::Long}); }
	{ FKeysShape& S = Add(TEXT("mangrove")); S.Peak = 0.38; S.Nose = 0.8; S.Ped = 0.2; S.Tail = EKeysTail::Fork; S.TL = 0.22; S.TH = 0.55; S.Pec = 0.15; S.Dor.Add({0.3, 0.74, 0.32, EKeysFin::Spiny}); S.Anal.Add({0.64, 0.77, 0.3, EKeysFin::Round}); }
	{ FKeysShape& S = Add(TEXT("yellowtail")); S.Peak = 0.36; S.Nose = 0.9; S.Ped = 0.15; S.Tail = EKeysTail::Fork; S.TL = 0.32; S.TH = 0.85; S.Pec = 0.15; S.Dor.Add({0.32, 0.74, 0.28, EKeysFin::Spiny}); S.Anal.Add({0.64, 0.76, 0.25, EKeysFin::Tri}); }
	{ FKeysShape& S = Add(TEXT("mutton")); S.Peak = 0.4; S.Nose = 0.75; S.Ped = 0.2; S.Tail = EKeysTail::Fork; S.TL = 0.22; S.TH = 0.55; S.Pec = 0.16; S.Dor.Add({0.3, 0.75, 0.32, EKeysFin::Spiny}); S.Anal.Add({0.64, 0.78, 0.35, EKeysFin::Tri}); }
	{ FKeysShape& S = Add(TEXT("hogfish")); S.Peak = 0.42; S.Nose = 0.55; S.Ped = 0.24; S.Tail = EKeysTail::Lyre; S.TL = 0.24; S.TH = 0.6; S.Pec = 0.14; S.Dor.Add({0.18, 0.3, 0.85, EKeysFin::Filament}); S.Dor.Add({0.3, 0.76, 0.3, EKeysFin::Long}); S.Anal.Add({0.55, 0.78, 0.3, EKeysFin::Long}); S.Hump = 0.25; }
	{ FKeysShape& S = Add(TEXT("grouper")); S.Peak = 0.4; S.Nose = 0.6; S.Ped = 0.32; S.Tail = EKeysTail::Round; S.TL = 0.17; S.TH = 0.5; S.Pec = 0.18; S.Dor.Add({0.28, 0.78, 0.25, EKeysFin::Spiny}); S.Anal.Add({0.64, 0.8, 0.28, EKeysFin::Round}); S.Belly = 1.05; }
	{ FKeysShape& S = Add(TEXT("parrotfish")); S.Peak = 0.42; S.Nose = 0.55; S.Ped = 0.28; S.Tail = EKeysTail::Truncate; S.TL = 0.18; S.TH = 0.6; S.Pec = 0.16; S.Dor.Add({0.25, 0.8, 0.22, EKeysFin::Long}); S.Anal.Add({0.6, 0.8, 0.22, EKeysFin::Long}); }
	{ FKeysShape& S = Add(TEXT("angelfish")); S.Peak = 0.45; S.Nose = 0.5; S.Ped = 0.22; S.Tail = EKeysTail::Truncate; S.TL = 0.2; S.TH = 0.55; S.Pec = 0.14; S.Dor.Add({0.32, 0.85, 0.35, EKeysFin::Trail}); S.Anal.Add({0.45, 0.85, 0.35, EKeysFin::Trail}); }
	{ FKeysShape& S = Add(TEXT("mahi")); S.Peak = 0.12; S.Nose = 0.32; S.Ped = 0.1; S.Tail = EKeysTail::Fork; S.TL = 0.3; S.TH = 0.65; S.Pec = 0.12; S.Dor.Add({0.05, 0.88, 0.32, EKeysFin::Long}); S.Anal.Add({0.48, 0.88, 0.25, EKeysFin::Long}); S.Hump = 0.35; }
	{ FKeysShape& S = Add(TEXT("blackfin")); S.Peak = 0.43; S.Nose = 0.9; S.Ped = 0.06; S.Tail = EKeysTail::Lunate; S.TL = 0.24; S.TH = 1.2; S.Pec = 0.2; S.Dor.Add({0.3, 0.4, 0.4, EKeysFin::Tri}); S.Dor.Add({0.52, 0.58, 0.35, EKeysFin::Sickle}); S.Anal.Add({0.54, 0.6, 0.35, EKeysFin::Sickle}); S.bFinlets = true; }
	{ FKeysShape& S = Add(TEXT("yellowfin")); S.Peak = 0.43; S.Nose = 0.9; S.Ped = 0.06; S.Tail = EKeysTail::Lunate; S.TL = 0.26; S.TH = 1.25; S.Pec = 0.2; S.Dor.Add({0.3, 0.4, 0.42, EKeysFin::Tri}); S.bFinlets = true; }
	{ FKeysShape& S = Add(TEXT("bluefin")); S.Peak = 0.43; S.Nose = 0.9; S.Ped = 0.06; S.Tail = EKeysTail::Lunate; S.TL = 0.22; S.TH = 1.05; S.Pec = 0.14; S.Dor.Add({0.3, 0.4, 0.38, EKeysFin::Tri}); S.bFinlets = true; }
	{ FKeysShape& S = Add(TEXT("albacore")); S.Peak = 0.43; S.Nose = 0.9; S.Ped = 0.06; S.Tail = EKeysTail::Lunate; S.TL = 0.24; S.TH = 1.2; S.Pec = 0.42; S.Dor.Add({0.3, 0.4, 0.4, EKeysFin::Tri}); S.bFinlets = true; }
	{ FKeysShape& S = Add(TEXT("wahoo")); S.Peak = 0.46; S.Nose = 1.6; S.Ped = 0.14; S.Tail = EKeysTail::Lunate; S.TL = 0.15; S.TH = 1.1; S.Pec = 0.08; S.Dor.Add({0.12, 0.58, 0.22, EKeysFin::Long}); S.Anal.Add({0.58, 0.66, 0.25, EKeysFin::Tri}); S.bFinlets = true; }
	{ FKeysShape& S = Add(TEXT("sailfish")); S.Peak = 0.34; S.Nose = 1.2; S.Ped = 0.1; S.Tail = EKeysTail::Lunate; S.TL = 0.18; S.TH = 1.3; S.Pec = 0.12; S.Dor.Add({0.12, 0.78, 1.9, EKeysFin::Sail}); S.Anal.Add({0.6, 0.68, 0.4, EKeysFin::Sickle}); }
	{ FKeysShape& S = Add(TEXT("marlin")); S.Peak = 0.36; S.Nose = 1; S.Ped = 0.1; S.Tail = EKeysTail::Lunate; S.TL = 0.2; S.TH = 1.3; S.Pec = 0.16; S.Dor.Add({0.14, 0.6, 0.75, EKeysFin::Sickle}); S.Anal.Add({0.58, 0.66, 0.45, EKeysFin::Sickle}); }
	{ FKeysShape& S = Add(TEXT("blackmarlin")); S.Peak = 0.35; S.Nose = 1; S.Ped = 0.1; S.Tail = EKeysTail::Lunate; S.TL = 0.21; S.TH = 1.3; S.Pec = 0.24; S.Dor.Add({0.14, 0.5, 0.45, EKeysFin::Sickle}); S.Anal.Add({0.58, 0.66, 0.42, EKeysFin::Sickle}); }
	{ FKeysShape& S = Add(TEXT("gag")); S.Peak = 0.42; S.Nose = 0.65; S.Ped = 0.28; S.Tail = EKeysTail::Truncate; S.TL = 0.15; S.TH = 0.5; S.Pec = 0.17; S.Dor.Add({0.28, 0.8, 0.22, EKeysFin::Spiny}); S.Anal.Add({0.64, 0.8, 0.24, EKeysFin::Round}); }
	{ FKeysShape& S = Add(TEXT("redgrouper")); S.Peak = 0.42; S.Nose = 0.6; S.Ped = 0.3; S.Tail = EKeysTail::Round; S.TL = 0.16; S.TH = 0.5; S.Pec = 0.18; S.Dor.Add({0.28, 0.78, 0.26, EKeysFin::Spiny}); S.Anal.Add({0.64, 0.8, 0.26, EKeysFin::Round}); S.Belly = 1.05; }
	{ FKeysShape& S = Add(TEXT("swordfish")); S.Peak = 0.36; S.Nose = 0.95; S.Ped = 0.1; S.Tail = EKeysTail::Lunate; S.TL = 0.2; S.TH = 1.25; S.Pec = 0.15; S.Dor.Add({0.2, 0.32, 0.8, EKeysFin::Sickle}); S.Anal.Add({0.6, 0.66, 0.35, EKeysFin::Sickle}); }
	{ FKeysShape& S = Add(TEXT("flyingfish")); S.Peak = 0.38; S.Nose = 0.9; S.Ped = 0.18; S.Tail = EKeysTail::Fork; S.TL = 0.3; S.TH = 0.9; S.Pec = 1.05; S.Dor.Add({0.62, 0.72, 0.3, EKeysFin::Tri}); S.Anal.Add({0.62, 0.72, 0.3, EKeysFin::Tri}); S.bWings = true; }
	{ FKeysShape& S = Add(TEXT("blacktip")); S.Peak = 0.38; S.Nose = 1.3; S.Ped = 0.12; S.Tail = EKeysTail::Hetero; S.TL = 0.26; S.TH = 1; S.Pec = 0.55; S.Dor.Add({0.33, 0.46, 0.9, EKeysFin::Sickle}); S.Dor.Add({0.7, 0.75, 0.25, EKeysFin::Tri}); S.Anal.Add({0.72, 0.77, 0.2, EKeysFin::Tri}); S.bShark = true; }
	{ FKeysShape& S = Add(TEXT("nurse")); S.Peak = 0.3; S.Nose = 0.6; S.Ped = 0.2; S.Tail = EKeysTail::Hetero; S.TL = 0.3; S.TH = 0.7; S.Pec = 0.35; S.Dor.Add({0.55, 0.64, 0.5, EKeysFin::Round}); S.Dor.Add({0.7, 0.77, 0.4, EKeysFin::Round}); S.Anal.Add({0.7, 0.76, 0.25, EKeysFin::Round}); S.Belly = 0.7; S.bShark = true; }
	{ FKeysShape& S = Add(TEXT("cobia")); S.Peak = 0.32; S.Nose = 1; S.Ped = 0.18; S.Tail = EKeysTail::Fork; S.TL = 0.2; S.TH = 0.7; S.Pec = 0.16; S.Dor.Add({0.32, 0.42, 0.15, EKeysFin::Spiny}); S.Dor.Add({0.48, 0.7, 0.35, EKeysFin::Sickle}); S.Anal.Add({0.55, 0.72, 0.3, EKeysFin::Sickle}); S.Belly = 0.9; }
	{ FKeysShape& S = Add(TEXT("amberjack")); S.Peak = 0.38; S.Nose = 0.75; S.Ped = 0.12; S.Tail = EKeysTail::Fork; S.TL = 0.26; S.TH = 0.85; S.Pec = 0.12; S.Dor.Add({0.4, 0.72, 0.35, EKeysFin::Sickle}); S.Anal.Add({0.55, 0.72, 0.3, EKeysFin::Sickle}); }
	{ FKeysShape& S = Add(TEXT("kingfish")); S.Peak = 0.4; S.Nose = 1.4; S.Ped = 0.12; S.Tail = EKeysTail::Lunate; S.TL = 0.18; S.TH = 1.15; S.Pec = 0.1; S.Dor.Add({0.18, 0.38, 0.22, EKeysFin::Spiny}); S.Dor.Add({0.48, 0.58, 0.3, EKeysFin::Tri}); S.Anal.Add({0.56, 0.64, 0.3, EKeysFin::Tri}); S.bFinlets = true; }
	{ FKeysShape& S = Add(TEXT("cero")); S.Peak = 0.4; S.Nose = 1.3; S.Ped = 0.12; S.Tail = EKeysTail::Lunate; S.TL = 0.2; S.TH = 1.1; S.Pec = 0.1; S.Dor.Add({0.2, 0.4, 0.25, EKeysFin::Spiny}); S.Dor.Add({0.48, 0.58, 0.3, EKeysFin::Tri}); S.Anal.Add({0.56, 0.64, 0.3, EKeysFin::Tri}); S.bFinlets = true; }
	{ FKeysShape& S = Add(TEXT("jackcrevalle")); S.Peak = 0.36; S.Nose = 0.5; S.Ped = 0.1; S.Tail = EKeysTail::Fork; S.TL = 0.3; S.TH = 0.75; S.Pec = 0.22; S.Dor.Add({0.32, 0.4, 0.3, EKeysFin::Tri}); S.Dor.Add({0.44, 0.74, 0.3, EKeysFin::Sickle}); S.Anal.Add({0.52, 0.74, 0.28, EKeysFin::Sickle}); S.Hump = 0.15; }
	{ FKeysShape& S = Add(TEXT("ladyfish")); S.Peak = 0.42; S.Nose = 1.2; S.Ped = 0.16; S.Tail = EKeysTail::Fork; S.TL = 0.3; S.TH = 0.8; S.Pec = 0.1; S.Dor.Add({0.42, 0.54, 0.5, EKeysFin::Tri}); S.Anal.Add({0.7, 0.78, 0.25, EKeysFin::Tri}); }
	{ FKeysShape& S = Add(TEXT("sheepshead")); S.Peak = 0.4; S.Nose = 0.55; S.Ped = 0.22; S.Tail = EKeysTail::Fork; S.TL = 0.18; S.TH = 0.5; S.Pec = 0.16; S.Dor.Add({0.25, 0.78, 0.3, EKeysFin::Spiny}); S.Anal.Add({0.6, 0.78, 0.3, EKeysFin::Round}); S.Hump = 0.12; }
	{ FKeysShape& S = Add(TEXT("pompano")); S.Peak = 0.42; S.Nose = 0.4; S.Ped = 0.1; S.Tail = EKeysTail::Fork; S.TL = 0.32; S.TH = 0.85; S.Pec = 0.14; S.Dor.Add({0.48, 0.72, 0.35, EKeysFin::Sickle}); S.Anal.Add({0.5, 0.72, 0.35, EKeysFin::Sickle}); }
	{ FKeysShape& S = Add(TEXT("tripletail")); S.Peak = 0.42; S.Nose = 0.6; S.Ped = 0.3; S.Tail = EKeysTail::Round; S.TL = 0.16; S.TH = 0.55; S.Pec = 0.14; S.Dor.Add({0.3, 0.78, 0.35, EKeysFin::Round}); S.Anal.Add({0.55, 0.8, 0.38, EKeysFin::Round}); }
	{ FKeysShape& S = Add(TEXT("lionfish")); S.Peak = 0.38; S.Nose = 0.6; S.Ped = 0.25; S.Tail = EKeysTail::Round; S.TL = 0.2; S.TH = 0.6; S.Pec = 0.75; S.Dor.Add({0.2, 0.6, 1.4, EKeysFin::Spiny}); S.Dor.Add({0.62, 0.8, 0.6, EKeysFin::Round}); S.Anal.Add({0.62, 0.8, 0.5, EKeysFin::Round}); S.bWings = true; }
	{ FKeysShape& S = Add(TEXT("goliath")); S.Peak = 0.42; S.Nose = 0.55; S.Ped = 0.32; S.Tail = EKeysTail::Round; S.TL = 0.16; S.TH = 0.45; S.Pec = 0.18; S.Dor.Add({0.3, 0.8, 0.22, EKeysFin::Spiny}); S.Anal.Add({0.65, 0.82, 0.25, EKeysFin::Round}); S.Belly = 1.08; }
	{ FKeysShape& S = Add(TEXT("graytrigger")); S.Peak = 0.42; S.Nose = 0.55; S.Ped = 0.2; S.Tail = EKeysTail::Lyre; S.TL = 0.2; S.TH = 0.55; S.Pec = 0.12; S.Dor.Add({0.25, 0.33, 0.5, EKeysFin::Tri}); S.Dor.Add({0.48, 0.78, 0.35, EKeysFin::Sickle}); S.Anal.Add({0.5, 0.78, 0.35, EKeysFin::Sickle}); }
	{ FKeysShape& S = Add(TEXT("bullshark")); S.Peak = 0.36; S.Nose = 0.8; S.Ped = 0.12; S.Tail = EKeysTail::Hetero; S.TL = 0.26; S.TH = 1; S.Pec = 0.5; S.Dor.Add({0.32, 0.44, 0.8, EKeysFin::Sickle}); S.Dor.Add({0.7, 0.75, 0.25, EKeysFin::Tri}); S.Anal.Add({0.72, 0.77, 0.2, EKeysFin::Tri}); S.bShark = true; }
	{ FKeysShape& S = Add(TEXT("lemonshark")); S.Peak = 0.38; S.Nose = 1; S.Ped = 0.12; S.Tail = EKeysTail::Hetero; S.TL = 0.26; S.TH = 1; S.Pec = 0.5; S.Dor.Add({0.36, 0.47, 0.6, EKeysFin::Sickle}); S.Dor.Add({0.6, 0.7, 0.55, EKeysFin::Sickle}); S.Anal.Add({0.68, 0.74, 0.25, EKeysFin::Tri}); S.bShark = true; }
	{ FKeysShape& S = Add(TEXT("hammerhead")); S.Peak = 0.38; S.Nose = 1.2; S.Ped = 0.1; S.Tail = EKeysTail::Hetero; S.TL = 0.3; S.TH = 1.1; S.Pec = 0.4; S.Dor.Add({0.3, 0.42, 1.5, EKeysFin::Sickle}); S.Dor.Add({0.72, 0.76, 0.3, EKeysFin::Tri}); S.Anal.Add({0.72, 0.77, 0.3, EKeysFin::Tri}); S.bShark = true; }
	{ FKeysShape& S = Add(TEXT("dolphin")); S.Peak = 0.38; S.Nose = 0.7; S.Ped = 0.14; S.Tail = EKeysTail::Flukes; S.TL = 0.2; S.TH = 1; S.Pec = 0.2; S.Dor.Add({0.42, 0.55, 0.7, EKeysFin::Sickle}); S.Hump = 0.18; }
	{ FKeysShape& S = Add(TEXT("pilotwhale")); S.Peak = .3; S.Nose = .5; S.Ped = .16; S.Tail = EKeysTail::Flukes; S.TL = .22; S.TH = 1.1; S.Pec = .16; S.Dor.Add({.28, .4, .4, EKeysFin::Sickle}); S.Hump = .5; }
	{ FKeysShape& S = Add(TEXT("humpback")); S.Peak = .3; S.Nose = .6; S.Ped = .2; S.Tail = EKeysTail::Flukes; S.TL = .26; S.TH = 1.15; S.Pec = .34; S.Dor.Add({.56, .63, .15, EKeysFin::Round}); S.bWings = true; S.Hump = .18; }
	// and their shapes (SHAPE); sole and halibut borrow the flounder's
	{ FKeysShape& S = Add(TEXT("flounder")); S.Peak = 0.46; S.Nose = 0.35; S.Ped = 0.13; S.Tail = EKeysTail::Round; S.TL = 0.15; S.TH = 0.5; S.Pec = 0.1; S.Dor.Add({0.1, 0.93, 0.3, EKeysFin::Long}); S.Anal.Add({0.36, 0.93, 0.26, EKeysFin::Long}); S.Belly = 0.95; }
	{ FKeysShape& S = Add(TEXT("sole")); S.Peak = 0.46; S.Nose = 0.35; S.Ped = 0.13; S.Tail = EKeysTail::Round; S.TL = 0.15; S.TH = 0.5; S.Pec = 0.1; S.Dor.Add({0.1, 0.93, 0.3, EKeysFin::Long}); S.Anal.Add({0.36, 0.93, 0.26, EKeysFin::Long}); S.Belly = 0.95; }
	{ FKeysShape& S = Add(TEXT("halibut")); S.Peak = 0.46; S.Nose = 0.35; S.Ped = 0.13; S.Tail = EKeysTail::Round; S.TL = 0.15; S.TH = 0.5; S.Pec = 0.1; S.Dor.Add({0.1, 0.93, 0.3, EKeysFin::Long}); S.Anal.Add({0.36, 0.93, 0.26, EKeysFin::Long}); S.Belly = 0.95; }
	{ FKeysShape& S = Add(TEXT("bluerunner")); S.Peak = 0.38; S.Nose = 0.7; S.Ped = 0.1; S.Tail = EKeysTail::Fork; S.TL = 0.3; S.TH = 0.8; S.Pec = 0.18; S.Dor.Add({0.34, 0.74, 0.28, EKeysFin::Sickle}); S.Anal.Add({0.56, 0.74, 0.26, EKeysFin::Sickle}); }
	{ FKeysShape& S = Add(TEXT("schoolmaster")); S.Peak = 0.38; S.Nose = 0.8; S.Ped = 0.2; S.Tail = EKeysTail::Truncate; S.TL = 0.2; S.TH = 0.55; S.Pec = 0.15; S.Dor.Add({0.3, 0.74, 0.32, EKeysFin::Spiny}); S.Anal.Add({0.64, 0.77, 0.3, EKeysFin::Round}); }
	{ FKeysShape& S = Add(TEXT("lanesnapper")); S.Peak = 0.38; S.Nose = 0.8; S.Ped = 0.18; S.Tail = EKeysTail::Fork; S.TL = 0.24; S.TH = 0.6; S.Pec = 0.15; S.Dor.Add({0.3, 0.74, 0.3, EKeysFin::Spiny}); S.Anal.Add({0.64, 0.77, 0.28, EKeysFin::Round}); }
	{ FKeysShape& S = Add(TEXT("vermilion")); S.Peak = 0.36; S.Nose = 0.85; S.Ped = 0.16; S.Tail = EKeysTail::Fork; S.TL = 0.3; S.TH = 0.75; S.Pec = 0.16; S.Dor.Add({0.3, 0.75, 0.28, EKeysFin::Spiny}); S.Anal.Add({0.64, 0.78, 0.24, EKeysFin::Tri}); }
	{ FKeysShape& S = Add(TEXT("lookdown")); S.Peak = 0.26; S.Nose = 0.32; S.Ped = 0.1; S.Tail = EKeysTail::Fork; S.TL = 0.3; S.TH = 0.78; S.Pec = 0.16; S.Dor.Add({0.3, 0.84, 0.5, EKeysFin::Trail}); S.Anal.Add({0.42, 0.84, 0.42, EKeysFin::Trail}); }
	{ FKeysShape& S = Add(TEXT("palometa")); S.Peak = 0.42; S.Nose = 0.4; S.Ped = 0.1; S.Tail = EKeysTail::Fork; S.TL = 0.32; S.TH = 0.9; S.Pec = 0.14; S.Dor.Add({0.44, 0.72, 0.78, EKeysFin::Trail}); S.Anal.Add({0.48, 0.74, 0.7, EKeysFin::Trail}); }
	{ FKeysShape& S = Add(TEXT("spanishmack")); S.Peak = 0.4; S.Nose = 1.3; S.Ped = 0.12; S.Tail = EKeysTail::Lunate; S.TL = 0.2; S.TH = 1.1; S.Pec = 0.1; S.Dor.Add({0.2, 0.4, 0.25, EKeysFin::Spiny}); S.Dor.Add({0.48, 0.58, 0.3, EKeysFin::Tri}); S.Anal.Add({0.56, 0.64, 0.3, EKeysFin::Tri}); S.bFinlets = true; }
	{ FKeysShape& S = Add(TEXT("bigeye")); S.Peak = 0.42; S.Nose = 0.85; S.Ped = 0.06; S.Tail = EKeysTail::Lunate; S.TL = 0.25; S.TH = 1.2; S.Pec = 0.22; S.Dor.Add({0.3, 0.4, 0.44, EKeysFin::Tri}); S.bFinlets = true; }
	{ FKeysShape& S = Add(TEXT("skipjack")); S.Peak = 0.44; S.Nose = 0.9; S.Ped = 0.06; S.Tail = EKeysTail::Lunate; S.TL = 0.24; S.TH = 1.15; S.Pec = 0.18; S.Dor.Add({0.3, 0.4, 0.36, EKeysFin::Tri}); S.bFinlets = true; }
	{ FKeysShape& S = Add(TEXT("whaleshark")); S.Peak = 0.22; S.Nose = 0.35; S.Ped = 0.16; S.Tail = EKeysTail::Hetero; S.TL = 0.26; S.TH = 1.15; S.Pec = 0.5; S.Dor.Add({0.42, 0.56, 0.6, EKeysFin::Tri}); S.Dor.Add({0.72, 0.79, 0.25, EKeysFin::Tri}); S.Anal.Add({0.74, 0.8, 0.22, EKeysFin::Tri}); S.Belly = 0.8; S.bShark = true; }
	{ FKeysShape& S = Add(TEXT("giantmanta")); S.Peak = 0.3; S.Nose = 0.3; S.Ped = 0.05; S.Tail = EKeysTail::Truncate; S.TL = 0.9; S.TH = 0.08; S.Pec = 0.1; }
	{ FKeysShape& S = Add(TEXT("sawfish")); S.Peak = 0.34; S.Nose = 0.9; S.Ped = 0.14; S.Tail = EKeysTail::Hetero; S.TL = 0.26; S.TH = 0.85; S.Pec = 0.34; S.Dor.Add({0.56, 0.68, 0.6, EKeysFin::Tri}); S.Dor.Add({0.74, 0.82, 0.45, EKeysFin::Tri}); S.Anal.Add({0.76, 0.83, 0.2, EKeysFin::Tri}); S.bShark = true; }
	{ FKeysShape& S = Add(TEXT("tigershark")); S.Peak = 0.3; S.Nose = 0.75; S.Ped = 0.12; S.Tail = EKeysTail::Hetero; S.TL = 0.3; S.TH = 1.2; S.Pec = 0.42; S.Dor.Add({0.32, 0.44, 1.1, EKeysFin::Tri}); S.Dor.Add({0.74, 0.8, 0.3, EKeysFin::Tri}); S.Anal.Add({0.74, 0.8, 0.3, EKeysFin::Tri}); S.bShark = true; }
	{ FKeysShape& S = Add(TEXT("mako")); S.Peak = 0.34; S.Nose = 1.1; S.Ped = 0.08; S.Tail = EKeysTail::Lunate; S.TL = 0.24; S.TH = 1.25; S.Pec = 0.36; S.Dor.Add({0.33, 0.44, 0.95, EKeysFin::Tri}); S.Dor.Add({0.74, 0.79, 0.22, EKeysFin::Tri}); S.Anal.Add({0.74, 0.79, 0.22, EKeysFin::Tri}); S.bShark = true; }
	{ FKeysShape& S = Add(TEXT("greatwhite")); S.Peak = 0.3; S.Nose = 0.8; S.Ped = 0.1; S.Tail = EKeysTail::Hetero; S.TL = 0.28; S.TH = 1.3; S.Pec = 0.46; S.Dor.Add({0.33, 0.45, 1.25, EKeysFin::Tri}); S.Dor.Add({0.76, 0.81, 0.28, EKeysFin::Tri}); S.Anal.Add({0.75, 0.8, 0.28, EKeysFin::Tri}); S.bShark = true; }
	{ FKeysShape& S = Add(TEXT("spermwhale")); S.Peak = 0.18; S.Nose = 0.3; S.Ped = 0.2; S.Tail = EKeysTail::Flukes; S.TL = 0.24; S.TH = 1.3; S.Pec = 0.12; S.Dor.Add({0.62, 0.7, 0.12, EKeysFin::Round}); S.Hump = 0.62; }
	{ FKeysShape& S = Add(TEXT("mola")); S.Peak = 0.45; S.Nose = 0.4; S.Ped = 0.62; S.Tail = EKeysTail::Truncate; S.TL = 0.06; S.TH = 1.4; S.Pec = 0.1; S.Dor.Add({0.3, 0.52, 1.5, EKeysFin::Tri}); S.Anal.Add({0.3, 0.52, 1.5, EKeysFin::Tri}); }
	{ FKeysShape& S = Add(TEXT("warsaw")); S.Peak = 0.4; S.Nose = 0.5; S.Ped = 0.34; S.Tail = EKeysTail::Round; S.TL = 0.15; S.TH = 0.5; S.Pec = 0.2; S.Dor.Add({0.26, 0.8, 0.26, EKeysFin::Spiny}); S.Anal.Add({0.64, 0.82, 0.26, EKeysFin::Round}); S.Belly = 1.1; }
	{ FKeysShape& S = Add(TEXT("cubera")); S.Peak = 0.38; S.Nose = 0.8; S.Ped = 0.22; S.Tail = EKeysTail::Truncate; S.TL = 0.2; S.TH = 0.62; S.Pec = 0.17; S.Dor.Add({0.3, 0.76, 0.3, EKeysFin::Spiny}); S.Anal.Add({0.64, 0.78, 0.3, EKeysFin::Round}); S.Belly = 1.02; }
	{ FKeysShape& S = Add(TEXT("goldbluefin")); S.Peak = .43; S.Nose = .88; S.Ped = .06; S.Tail = EKeysTail::Lunate; S.TL = .23; S.TH = 1.1; S.Pec = .15; S.Dor.Add({.3, .4, .42, EKeysFin::Tri}); S.bFinlets = true; }
		}
		const FKeysShape* S = M.Find(K);
		return S ? *S : M[TEXT("mangrove")];
	}

	// web entities/fish/behavior.ts: how far each kind of fish stands off a diver or a near-miss shaft, and how fast it bolts
	struct FLifeFlee { double DiverR, SpearR, SpeedMul; };
	FLifeFlee LifeFleeParams(const FString& Key, const FKeysVis* V)
	{
		static const TSet<FString> Apex = {TEXT("barracuda"), TEXT("grouper"), TEXT("gag"), TEXT("redgrouper"), TEXT("goliath"), TEXT("marlin"), TEXT("blackmarlin"), TEXT("swordfish"),
			TEXT("sailfish"), TEXT("wahoo"), TEXT("kingfish"), TEXT("amberjack"), TEXT("cobia"), TEXT("jackcrevalle"), TEXT("tripletail"), TEXT("lionfish")};
		static const TSet<FString> Glide = {TEXT("turtle"), TEXT("manatee"), TEXT("stingray"), TEXT("eagleray"), TEXT("dolphin"), TEXT("pilotwhale"), TEXT("humpback")};
		if (Key == TEXT("flyingfish")) return {9, 12, 3.2};
		if (Glide.Contains(Key)) return {5, 8, 1.15};
		if (V->Kind == EKeysKind::Shark || Apex.Contains(Key)) return {3, 6, 1.35};
		if (V->SMax <= 2) return {7, 10, 2.2};
		return {9, 12, 3.2};
	}

	// near-field schools around a diver (web entities/fish/spawn.ts nearFieldForCell): 18 m cells within 72 m
	constexpr double LifeNearCell = 18, LifeNearRadius = 72;
	constexpr uint32 LifeNearSeed = 0x4b52u;

	// what you can see swimming in each zone (JS ZONE_LIFE)
	const TArray<FZoneRow>& LifeZoneLife(const FString& Zone)
	{
		static TMap<FString, TArray<FZoneRow>> T;
		if (!T.Num())
		{
			auto Z = [](const TCHAR* Name, std::initializer_list<FZoneRow> Rows) { T.Add(FString(Name), TArray<FZoneRow>(Rows)); };
		Z(TEXT("Creek"), {{TEXT("flounder"), 2}, {TEXT("sole"), 1.6}, {TEXT("snook"), 3}, {TEXT("redfish"), 2.5}, {TEXT("tarpon"), 1.5}, {TEXT("manatee"), 1}, {TEXT("mangrove"), 3.5}, {TEXT("ladyfish"), 2.5}, {TEXT("trout"), 1.5}, {TEXT("jackcrevalle"), 1}});
		Z(TEXT("Flats"), {{TEXT("flounder"), 2.5}, {TEXT("sole"), 1.2}, {TEXT("sawfish"), 0.012}, {TEXT("bonefish"), 5}, {TEXT("permit"), 2.2}, {TEXT("stingray"), 3}, {TEXT("redfish"), 1.5}, {TEXT("barracuda"), 2}, {TEXT("blacktip"), 1.5}, {TEXT("lemonshark"), 1}, {TEXT("eagleray"), 1}, {TEXT("ladyfish"), 3}, {TEXT("pompano"), 1.2}, {TEXT("palometa"), 3}, {TEXT("bluerunner"), 2}, {TEXT("manatee"), 0.7}});
		Z(TEXT("Backcountry"), {{TEXT("flounder"), 2.5}, {TEXT("sole"), 1.8}, {TEXT("sawfish"), 0.01}, {TEXT("redfish"), 4}, {TEXT("snook"), 3}, {TEXT("trout"), 3}, {TEXT("mangrove"), 2.5}, {TEXT("tarpon"), 2}, {TEXT("bonefish"), 1.5}, {TEXT("permit"), 1}, {TEXT("barracuda"), 1.2}, {TEXT("manatee"), 1.2}, {TEXT("stingray"), 2.2}, {TEXT("jackcrevalle"), 2}, {TEXT("ladyfish"), 3}, {TEXT("pompano"), 1.5}, {TEXT("tripletail"), 0.6}, {TEXT("dolphin"), 1.5}});
		Z(TEXT("Bridge"), {{TEXT("flounder"), 1.5}, {TEXT("tarpon"), 5}, {TEXT("snook"), 2}, {TEXT("mangrove"), 3}, {TEXT("eagleray"), 1}, {TEXT("sheepshead"), 3}, {TEXT("goliath"), 0.6}, {TEXT("jackcrevalle"), 1.5}, {TEXT("nurse"), 1}, {TEXT("cobia"), 1}, {TEXT("barracuda"), 1}, {TEXT("dolphin"), 1.2}, {TEXT("lookdown"), 3.5}, {TEXT("bluerunner"), 4}, {TEXT("schoolmaster"), 2}, {TEXT("manatee"), 0.9}});
		Z(TEXT("Hawk Channel"), {{TEXT("mangrove"), 4}, {TEXT("eagleray"), 2}, {TEXT("turtle"), 1.5}, {TEXT("barracuda"), 2}, {TEXT("nurse"), 1.5}, {TEXT("mutton"), 2}, {TEXT("dolphin"), 1.8}, {TEXT("cero"), 2}, {TEXT("pompano"), 1.5}, {TEXT("cobia"), 1}, {TEXT("graytrigger"), 1.2}, {TEXT("jackcrevalle"), 1.8}, {TEXT("yellowtail"), 1.5}, {TEXT("sheepshead"), 1.2}, {TEXT("tarpon"), 1.6}, {TEXT("goliath"), 0.5}, {TEXT("bullshark"), 0.35}, {TEXT("bluerunner"), 4.5}, {TEXT("lanesnapper"), 3}, {TEXT("spanishmack"), 3}, {TEXT("lookdown"), 1.5}, {TEXT("schoolmaster"), 2}});
		Z(TEXT("Reef"), {{TEXT("cubera"), 0.5}, {TEXT("yellowtail"), 5}, {TEXT("parrotfish"), 3}, {TEXT("angelfish"), 2}, {TEXT("hogfish"), 2}, {TEXT("grouper"), 2}, {TEXT("gag"), 1.2}, {TEXT("redgrouper"), 1.2}, {TEXT("nurse"), 1}, {TEXT("turtle"), 1.5}, {TEXT("barracuda"), 1.5}, {TEXT("mutton"), 2}, {TEXT("lionfish"), 1.5}, {TEXT("graytrigger"), 2}, {TEXT("cero"), 2}, {TEXT("goliath"), 1.1}, {TEXT("hammerhead"), 0.3}, {TEXT("bullshark"), 0.3}, {TEXT("tarpon"), 1.2}, {TEXT("dolphin"), 1}, {TEXT("schoolmaster"), 4.5}, {TEXT("lanesnapper"), 3.5}, {TEXT("bluerunner"), 4}, {TEXT("vermilion"), 2}, {TEXT("spanishmack"), 2}});
		Z(TEXT("Offshore"), {{TEXT("halibut"), 1.1}, {TEXT("mako"), 0.5}, {TEXT("mola"), 0.03}, {TEXT("greatwhite"), 0.025}, {TEXT("whaleshark"), 0.04}, {TEXT("giantmanta"), 0.04}, {TEXT("tigershark"), 0.14}, {TEXT("mahi"), 5}, {TEXT("flyingfish"), 4}, {TEXT("dolphin"), 2.5}, {TEXT("blackfin"), 3}, {TEXT("sailfish"), 1.5}, {TEXT("wahoo"), 1}, {TEXT("turtle"), 0.8}, {TEXT("marlin"), 0.6}, {TEXT("blackmarlin"), 0.25}, {TEXT("swordfish"), 0.3}, {TEXT("yellowfin"), 2.5}, {TEXT("albacore"), 1.2}, {TEXT("bluefin"), 0.8}, {TEXT("kingfish"), 1.5}, {TEXT("amberjack"), 1.5}, {TEXT("hammerhead"), 0.3}, {TEXT("pilotwhale"), 0.12}, {TEXT("humpback"), 0.02}, {TEXT("skipjack"), 4}, {TEXT("bigeye"), 0.7}});
		}
		const TArray<FZoneRow>* R = T.Find(Zone);
		return R ? *R : T[TEXT("Hawk Channel")];
	}

	// fishing tables the JS keeps in ZONE_TABLE but KeysData doesn't have (index.html 528-529)
	const TArray<FZoneRow>& LifeRigFishing()
	{
		// the web game's ZONE_TABLE['Oil Rig']
		static const TArray<FZoneRow> L{{TEXT("amberjack"), 6}, {TEXT("blackfin"), 3}, {TEXT("cobia"), 3}, {TEXT("grouper"), 1.5}, {TEXT("gag"), 1.2}, {TEXT("barracuda"), 2},
			{TEXT("kingfish"), 2.5}, {TEXT("mahi"), 1.5}, {TEXT("wahoo"), 1.5}, {TEXT("yellowfin"), 1.2}, {TEXT("sailfish"), .4}, {TEXT("goliath"), .5}, {TEXT("bullshark"), .35}};
		return L;
	}
	const TArray<FZoneRow>& LifeWreckFishing()
	{
		// the web game's ZONE_TABLE['Wreck']: steel on the bottom is where the big structure-holders live
		static const TArray<FZoneRow> L{{TEXT("cubera"), 2}, {TEXT("warsaw"), .8}, {TEXT("tigershark"), .3}, {TEXT("amberjack"), 6}, {TEXT("grouper"), 3.6}, {TEXT("cobia"), 3.2},
			{TEXT("gag"), 2.6}, {TEXT("goliath"), 2.6}, {TEXT("barracuda"), 2.5}, {TEXT("jackcrevalle"), 2.5}, {TEXT("kingfish"), 2.5}, {TEXT("bullshark"), .9}, {TEXT("mutton"), .5}, {TEXT("blacktip"), .3}};
		return L;
	}
	const TArray<FZoneRow>& LifeWeedFishing()
	{
		static const TArray<FZoneRow> L{{TEXT("mahi"), 8}, {TEXT("tripletail"), 3}, {TEXT("blackfin"), 2}, {TEXT("graytrigger"), 2}, {TEXT("wahoo"), 2}, {TEXT("sailfish"), 1.6}, {TEXT("jackcrevalle"), 1.5},
			{TEXT("yellowfin"), .7}, {TEXT("marlin"), .3}};
		return L;
	}
	// schools that hang around each kind of structure (JS ensureAnchors, 2146)
	const TArray<FZoneRow>& LifeRigSchools()
	{
		static const TArray<FZoneRow> L{{TEXT("amberjack"), 5}, {TEXT("blackfin"), 3}, {TEXT("cobia"), 2.5}, {TEXT("barracuda"), 2}, {TEXT("kingfish"), 2}, {TEXT("mahi"), 1.5}, {TEXT("wahoo"), 1.5}};
		return L;
	}
	const TArray<FZoneRow>& LifeWeedSchools()
	{
		static const TArray<FZoneRow> L{{TEXT("mahi"), 6}, {TEXT("tripletail"), 2}, {TEXT("blackfin"), 1.5}, {TEXT("graytrigger"), 1.5}, {TEXT("jackcrevalle"), 1}};
		return L;
	}

	FString LifeWeighted(const TArray<FZoneRow>& L)
	{
		if (!L.Num()) return FString();
		double Tot = 0;
		for (const FZoneRow& E : L) Tot += E.W;
		double R = FMath::FRand() * Tot;
		for (const FZoneRow& E : L) { R -= E.W; if (R <= 0) return E.Key; }
		return L[0].Key;
	}

	double LifeActDur(EKeysAct A)
	{
		switch (A) { case EKeysAct::Tail: return 2.6; case EKeysAct::Roll: return 1.4; case EKeysAct::Breathe: return 3.5; case EKeysAct::Porpoise: return 1.2; case EKeysAct::Bust: return .7; case EKeysAct::Blow: return 4.5; default: return 1; }
	}
	double LifeActCooldown(EKeysAct A)
	{
		switch (A) { case EKeysAct::Tail: return LRand(3, 9); case EKeysAct::Roll: return LRand(4, 12); case EKeysAct::Breathe: return LRand(8, 20); case EKeysAct::Porpoise: return LRand(1.5, 4); case EKeysAct::Bust: return LRand(2, 8); case EKeysAct::Blow: return LRand(24, 65); default: return 5; }
	}

	// ---------------------------------------------------------------- model templates, in the JS frame (game meters)
	struct FLifeTpl
	{
		TArray<FVector> P, N;
		TArray<FLinearColor> C;     // linear
		TArray<float> Wig, WigY, Wph;
		TArray<int32> T;

		int32 Add(const FVector& Pos, const FLinearColor& Srgb) { P.Add(Pos); C.Add(LLin(Srgb)); return P.Num() - 1; }
		void Tri(const FVector& A, const FVector& B, const FVector& Cc, const FLinearColor& Col)
		{
			const int32 I = Add(A, Col); Add(B, Col); Add(Cc, Col);
			T.Append({I, I + 1, I + 2});
		}
		void Quad(const FVector& A, const FVector& B, const FVector& Cc, const FVector& D, const FLinearColor& Col)
		{
			const int32 I = Add(A, Col); Add(B, Col); Add(Cc, Col); Add(D, Col);
			T.Append({I, I + 1, I + 2, I, I + 2, I + 3});
		}
		// triangle fan around Ctr (the outlines used here are star-shaped from it)
		void FanFrom(const FVector& Ctr, const TArray<FVector>& Pts, const FLinearColor& Col)
		{
			const int32 I = Add(Ctr, Col);
			for (const FVector& Q : Pts) Add(Q, Col);
			for (int32 k = 0; k + 1 < Pts.Num(); k++) T.Append({I, I + 1 + k, I + 2 + k});
		}
		void Fan(const TArray<FVector>& Pts, const FLinearColor& Col)
		{
			const int32 I = P.Num();
			for (const FVector& Q : Pts) Add(Q, Col);
			for (int32 k = 1; k + 1 < Pts.Num(); k++) T.Append({I, I + k, I + k + 1});
		}
		// lat-long ellipsoid; ColorFn sees the undeformed unit-sphere point and Deform bends it before scaling (JS mergeParts)
		void Ellipsoid(const FVector& Ctr, const FVector& Rad, const FLifeR3& Rot, int32 Segs, int32 Rings, TFunctionRef<FLinearColor(const FVector&)> ColorFn, TFunctionRef<void(FVector&)> Deform)
		{
			const int32 I0 = P.Num();
			for (int32 i = 0; i <= Rings; i++)
			{
				const double Th = PI * i / Rings;
				for (int32 j = 0; j <= Segs; j++)
				{
					const double Ph = 2 * PI * j / Segs;
					const FVector U(-FMath::Cos(Ph) * FMath::Sin(Th), FMath::Cos(Th), FMath::Sin(Ph) * FMath::Sin(Th));
					FVector D = U;
					Deform(D);
					Add(Ctr + Rot * FVector(D.X * Rad.X, D.Y * Rad.Y, D.Z * Rad.Z), ColorFn(U));
				}
			}
			for (int32 i = 0; i < Rings; i++)
				for (int32 j = 0; j < Segs; j++)
				{
					const int32 A = I0 + i * (Segs + 1) + j, B = A + Segs + 1;
					T.Append({A, B, A + 1, B, B + 1, A + 1});
				}
		}
		void Blob(const FVector& Ctr, const FVector& Rad, int32 Segs, int32 Rings, const FLinearColor& Col)
		{
			Ellipsoid(Ctr, Rad, FLifeR3::Ident(), Segs, Rings, [Col](const FVector&) { return Col; }, [](FVector&) {});
		}
		void Box(const FVector& Ctr, const FVector& Size, const FLifeR3& Rot, const FLinearColor& Col)
		{
			const FVector H = Size * .5;
			auto Pt = [&](double X, double Y, double Z) { return Ctr + Rot * FVector(X * H.X, Y * H.Y, Z * H.Z); };
			Quad(Pt(1, -1, -1), Pt(1, 1, -1), Pt(1, 1, 1), Pt(1, -1, 1), Col);
			Quad(Pt(-1, -1, -1), Pt(-1, -1, 1), Pt(-1, 1, 1), Pt(-1, 1, -1), Col);
			Quad(Pt(-1, 1, -1), Pt(-1, 1, 1), Pt(1, 1, 1), Pt(1, 1, -1), Col);
			Quad(Pt(-1, -1, -1), Pt(1, -1, -1), Pt(1, -1, 1), Pt(-1, -1, 1), Col);
			Quad(Pt(-1, -1, 1), Pt(1, -1, 1), Pt(1, 1, 1), Pt(-1, 1, 1), Col);
			Quad(Pt(-1, -1, -1), Pt(-1, 1, -1), Pt(1, 1, -1), Pt(1, -1, -1), Col);
		}
		// cone or tapered tube from A (radius RA) to B (radius RB)
		void Tube(const FVector& A, const FVector& B, double RA, double RB, int32 Sides, const FLinearColor& Col)
		{
			const FVector Dir = (B - A).GetSafeNormal();
			if (Dir.IsNearlyZero()) return;
			FVector U = FVector::CrossProduct(Dir, FVector(0, 1, 0));
			if (U.SizeSquared() < 1e-6) U = FVector::CrossProduct(Dir, FVector(1, 0, 0));
			U.Normalize();
			const FVector W = FVector::CrossProduct(Dir, U);
			for (int32 i = 0; i < Sides; i++)
			{
				const double A0 = 2 * PI * i / Sides, A1 = 2 * PI * (i + 1) / Sides;
				const FVector O0 = U * FMath::Cos(A0) + W * FMath::Sin(A0), O1 = U * FMath::Cos(A1) + W * FMath::Sin(A1);
				Quad(A + O0 * RA, B + O0 * RB, B + O1 * RB, A + O1 * RA, Col);
			}
		}
		void Octa(const FVector& Ctr, const FVector& Rad, const FLinearColor& Col)
		{
			for (int32 Sx = -1; Sx <= 1; Sx += 2)
				for (int32 Sy = -1; Sy <= 1; Sy += 2)
					for (int32 Sz = -1; Sz <= 1; Sz += 2)
						Tri(Ctr + FVector(Rad.X * Sx, 0, 0), Ctr + FVector(0, Rad.Y * Sy, 0), Ctr + FVector(0, 0, Rad.Z * Sz), Col);
		}
		// smooth normals, then the per-vertex swim weights the JS puts in shader attributes (addSwimAttrs, 2425)
		void Finish(const FKeysVis* V)
		{
			N.Init(FVector::ZeroVector, P.Num());
			for (int32 i = 0; i + 2 < T.Num(); i += 3)
			{
				const FVector F = FVector::CrossProduct(P[T[i + 1]] - P[T[i]], P[T[i + 2]] - P[T[i]]);
				N[T[i]] += F; N[T[i + 1]] += F; N[T[i + 2]] += F;
			}
			for (FVector& Nn : N) { Nn = Nn.GetSafeNormal(); if (Nn.IsNearlyZero()) Nn = FVector(0, 1, 0); }
			Wig.Init(0.f, P.Num()); WigY.Init(0.f, P.Num()); Wph.Init(0.f, P.Num());
			if (!V) return;
			const double L = V->Len;
			const bool bLat = V->Kind == EKeysKind::Fish || V->Kind == EKeysKind::Tuna || V->Kind == EKeysKind::Shark;
			const bool bVert = V->Kind == EKeysKind::Dolphin || V->Kind == EKeysKind::Manatee;
			const double Amp = V->Kind == EKeysKind::Tuna ? .05 : V->Kind == EKeysKind::Shark ? .08 : .1;
			for (int32 i = 0; i < P.Num(); i++)
			{
				const double U = LClamp(P[i].Z / L + .5, 0, 1.25), W = FMath::Square(FMath::Max(0.0, U - .28) / .72);
				if (bLat) Wig[i] = float(W * L * Amp);
				if (bVert) WigY[i] = float(W * L * .07);
				if (V->Kind == EKeysKind::Ray) WigY[i] = float(FMath::Square(FMath::Min(1.0, FMath::Abs(P[i].X) / (V->Wing / 2))) * V->Wing * .16);
				if (V->Kind == EKeysKind::Turtle) WigY[i] = FMath::Abs(P[i].X) > L * .32 ? float(.07 * L) : 0.f;
				Wph[i] = float(U * 2.6);
			}
		}
	};

	// JS bodyColor (2215): countershading plus each species' markings, v normalized to the body (-1..1)
	FLinearColor LifeBodyColor(const FKeysVis& V, const FVector& v)
	{
		FLinearColor C = LMix(V.Belly, V.Back, LClamp((v.Y + .35) / .9, 0, 1));
		const double Hs = LHash2(v.X * 9.1 + v.Z * 3.3, v.Y * 7.7 + v.Z * 5.1);
		if ((V.Kind == EKeysKind::Fish || V.Kind == EKeysKind::Tuna) && v.Z > -.6 && v.Z < -.545 && FMath::Abs(v.Y) < .62) C *= .78f;
		if (V.Kind == EKeysKind::Shark && v.Z > -.55 && v.Z < -.35 && FMath::Abs(v.Y) < .35 && FMath::Sin(v.Z * 95) > .75) C *= .6f;
		switch (V.Pattern)
		{
		case EKeysPat::Line: if (FMath::Abs(v.Y) < .08) C = LHex(0x26241d); break;
		case EKeysPat::Yline: if (FMath::Abs(v.Y - .05) < .14) C = V.Fin; break;
		case EKeysPat::Spot: if (v.Z > .62 && FMath::Abs(v.Y - .12) < .2) C = LHex(0x111111); break;
		case EKeysPat::Spots: if (Hs > .8 && v.Y > -.2) C *= .6f; break;
		case EKeysPat::Mottle: if (Hs > .6) C *= .75f; break;
		case EKeysPat::Bars: if (FMath::Sin(v.Z * 14) > .55 && v.Y > -.3) C *= .55f; break;
		case EKeysPat::Lbars: if (FMath::Sin(v.Z * 12) > .6 && v.Y > -.3) C = LMix(C, LHex(0x9fd2ff), .45); break;
		case EKeysPat::Ystripe: if (FMath::Abs(v.Y - .08) < .09 && v.Z < .35) C = LMix(C, LHex(0xf2c62e), .55); break;
		case EKeysPat::Gspots: if (Hs > .78 && v.Y > -.3 && v.Y < .4) C = LMix(C, LHex(0xd8b84a), .6); break;
		case EKeysPat::Wbars: if (FMath::Sin(v.Z * 22) > .2) C = LMix(C, LHex(0xf4ece4), .65); break;
		case EKeysPat::Dots: if (Hs > .8 && v.Y > 0) C = LHex(0xf2f2f2); break;
		default: break;
		}
		C.A = 1;
		return C;
	}

	double LifeFinEdge(EKeysFin Style, double U)
	{
		switch (Style)
		{
		case EKeysFin::Tri: return U < .28 ? U / .28 : 1 - (U - .28) / .72 * .88;
		case EKeysFin::Sickle: return U < .18 ? U / .18 : FMath::Pow(FMath::Max(0.0, 1 - (U - .18) / .82), 2.3) * .95 + .05;
		case EKeysFin::Long: return (U < .08 ? U / .08 : 1) * (.75 + .25 * FMath::Sin(PI * U)) * (U > .92 ? (1 - U) / .08 : 1);
		case EKeysFin::Sail: return FMath::Pow(FMath::Max(0.0, FMath::Sin(PI * FMath::Min(1.0, U * 1.15))), .55) * (1 - .35 * U);
		case EKeysFin::Spiny: return (U < .15 ? U / .15 : 1 - (U - .15) * .5) * ((FMath::FloorToInt(U * 14) % 2) ? .82 : 1);
		case EKeysFin::Round: return FMath::Sin(PI * U);
		case EKeysFin::Filament: return U < .5 ? .4 + U * 1.2 : 1 - (U - .5) * 1.6;
		case EKeysFin::Trail: return (U < .1 ? U / .1 : 1) * (.7 + .6 * U);
		default: return FMath::Sin(PI * U);
		}
	}

	// JS buildFishGeo (2361): a lofted super-ellipse body with shaped fins, at lower resolution
	void LifeBuildFish(const FKeysVis& V, FLifeTpl& M)
	{
		const FKeysShape& S = LifeShapeOf(V.Key);
		const double L = V.Len, Hh = V.H > 0 ? V.H : L * .17, W = V.W > 0 ? V.W : L * .16, NE = 2.3;
		const int32 NS = 16, NR = 10;
		const FLinearColor Fin = V.Fin, Back = V.Back;
		const bool bDolphin = V.Key == TEXT("dolphin"), bCet = V.Kind == EKeysKind::Dolphin;
		auto Prof = [&S](double t) { return t < S.Peak ? FMath::Pow(FMath::Sin(PI / 2 * t / S.Peak), S.Nose) : LLerp(1, S.Ped, FMath::Pow((t - S.Peak) / (1 - S.Peak), 1.25)); };
		auto HumpF = [&S](double t) { return 1 + S.Hump * FMath::Max(0.0, 1 - t / .35); };
		auto TopY = [&](double t) { return Hh / 2 * Prof(t) * HumpF(t); };
		auto BotY = [&](double t) { return -Hh / 2 * Prof(t) * S.Belly; };

		// body
		const int32 I0 = M.P.Num();
		for (int32 i = 0; i <= NS; i++)
		{
			const double t = double(i) / NS, z = -L / 2 + t * L, k = Prof(t);
			for (int32 j = 0; j <= NR; j++)
			{
				const double a = double(j) / NR * PI * 2, ca = FMath::Cos(a), sa = FMath::Sin(a);
				const double x = W / 2 * k * FMath::Sign(ca) * FMath::Pow(FMath::Abs(ca), 2 / NE);
				double y = Hh / 2 * k * FMath::Sign(sa) * FMath::Pow(FMath::Abs(sa), 2 / NE);
				y *= sa < 0 ? S.Belly : HumpF(t);
				M.Add(FVector(x, y, z), LifeBodyColor(V, FVector(x / (W / 2 + 1e-6), y / (Hh / 2), z / (L / 2))));
			}
		}
		for (int32 i = 0; i < NS; i++)
			for (int32 j = 0; j < NR; j++)
			{
				const int32 A = I0 + i * (NR + 1) + j, B = A + NR + 1;
				M.T.Append({A, B, A + 1, B, B + 1, A + 1});
			}

		// dorsal and anal fins along the body outline, in the midplane
		auto MidFin = [&](const FKeysFin& F, bool bTop)
		{
			const int32 NSeg = 8;
			auto Pt = [&](double U, bool bEdge)
			{
				const double t = LLerp(F.T0, F.T1, U), z = -L / 2 + t * L, yb = bTop ? TopY(t) - .004 : BotY(t) + .004;
				const double E = bEdge ? LifeFinEdge(F.Style, U) * F.H * Hh : 0;
				return FVector(0, yb + (bTop ? E : -E), z + E * .35);
			};
			for (int32 i = 0; i < NSeg; i++)
			{
				const double U0 = double(i) / NSeg, U1 = double(i + 1) / NSeg;
				M.Quad(Pt(U0, false), Pt(U1, false), Pt(U1, true), Pt(U0, true), Fin);
			}
		};
		for (const FKeysFin& F : S.Dor) MidFin(F, true);
		for (const FKeysFin& F : S.Anal) MidFin(F, false);

		// tail
		const double Z0 = L / 2 - .01, Pp = Hh / 2 * S.Ped, Tl = L * S.TL, Th = Hh / 2 * S.TH * 1.6;
		TArray<FVector2D> Tp;
		auto T2 = [&Tp](double A, double B) { Tp.Add(FVector2D(A, B)); };
		switch (S.Tail)
		{
		case EKeysTail::Lunate: T2(0, Pp * .6); T2(Tl * .5, Th * .55); T2(Tl, Th); T2(Tl * .66, Th * .3); T2(Tl * .5, 0); T2(Tl * .66, -Th * .3); T2(Tl, -Th); T2(Tl * .5, -Th * .55); T2(0, -Pp * .6); break;
		case EKeysTail::Truncate: T2(0, Pp); T2(Tl * .85, Th * .8); T2(Tl, Th * .62); T2(Tl * .97, 0); T2(Tl, -Th * .62); T2(Tl * .85, -Th * .8); T2(0, -Pp); break;
		case EKeysTail::Round:
			T2(0, Pp);
			for (int32 i = 0; i <= 8; i++) { const double A = PI / 2 - i / 8.0 * PI; T2(Tl * FMath::Cos(A) * .95 + Tl * .05, Th * .72 * FMath::Sin(A)); }
			T2(0, -Pp);
			break;
		case EKeysTail::Lyre: T2(0, Pp); T2(Tl * .7, Th * .55); T2(Tl * 1.15, Th * 1.05); T2(Tl * .8, Th * .35); T2(Tl * .75, 0); T2(Tl * .8, -Th * .35); T2(Tl * 1.15, -Th * 1.05); T2(Tl * .7, -Th * .55); T2(0, -Pp); break;
		case EKeysTail::Hetero: T2(0, Pp); T2(Tl * .6, Th * .75); T2(Tl * 1.15, Th * 1.25); T2(Tl * .95, Th * .55); T2(Tl * .6, -Th * .05); T2(Tl * .55, -Th * .62); T2(Tl * .3, -Th * .45); T2(0, -Pp); break;
		case EKeysTail::Flukes: break;
		default: T2(0, Pp * .8); T2(Tl * .6, Th * .6); T2(Tl, Th); T2(Tl * .55, 0); T2(Tl, -Th); T2(Tl * .6, -Th * .6); T2(0, -Pp * .8); break;
		}
		if (S.Tail != EKeysTail::Flukes)
		{
			TArray<FVector> Pts;
			for (const FVector2D& Q : Tp) Pts.Add(FVector(0, Q.Y, Z0 + Q.X));
			M.FanFrom(FVector(0, 0, Z0), Pts, Fin);
		}
		else
		{
			// dolphin flukes, flat and horizontal
			const double Wf = W * 2.4;
			const FVector2D Fl[] = {FVector2D(-.02, 0), FVector2D(-Wf * .55, -Tl * .4), FVector2D(-Wf, -Tl * .95), FVector2D(-Wf * .45, -Tl * .75), FVector2D(0, -Tl * .55),
				FVector2D(Wf * .45, -Tl * .75), FVector2D(Wf, -Tl * .95), FVector2D(Wf * .55, -Tl * .4), FVector2D(.02, 0)};
			TArray<FVector> Pts;
			for (const FVector2D& Q : Fl) Pts.Add(FVector(Q.X, 0, Z0 - Q.Y));
			M.FanFrom(FVector(0, 0, Z0), Pts, Back);
		}

		// finlets (tunas, mackerels, wahoo)
		if (S.bFinlets)
		{
			for (int32 i = 0; i < 7; i++)
			{
				const double t = .68 + i * .042, z = -L / 2 + t * L, Yt = TopY(t), Yb = BotY(t), Sz = Hh * .07;
				M.Tri(FVector(0, Yt - .003, z), FVector(0, Yt + Sz * .8, z + Sz * .9), FVector(0, Yt - .003, z + Sz * 1.1), V.Finlet);
				M.Tri(FVector(0, Yb + .003, z), FVector(0, Yb - Sz * .8, z + Sz * .9), FVector(0, Yb + .003, z + Sz * 1.1), V.Finlet);
			}
		}

		// pectoral and pelvic fins
		auto PFin = [&M](double Len, double Wid, const FLifeR3& R, const FVector& At, const FLinearColor& Col)
		{
			const FVector2D Sh[] = {FVector2D(0, 0), FVector2D(Len * .35, Wid * .55), FVector2D(Len, Wid * .15), FVector2D(Len * .6, -Wid * .3), FVector2D(Len * .25, -Wid * .35)};
			TArray<FVector> Pts;
			for (const FVector2D& Q : Sh) Pts.Add(At + R * FVector(Q.X, 0, -Q.Y));
			M.Fan(Pts, Col);
		};
		const double Pl = L * S.Pec, TP = .22;
		for (int32 Sx = -1; Sx <= 1; Sx += 2)
		{
			const FLifeR3 R1 = FLifeR3::RZ(Sx * (S.bShark ? -.35 : -.15)) * FLifeR3::RY(FMath::Atan2(-.88, Sx * .48));
			PFin(Pl, Pl * (S.bWings ? .55 : .38), R1, FVector(Sx * W / 2 * Prof(TP) * .82, -Hh * .12, -L / 2 + TP * L), S.bShark ? Back : Fin);
			if (!S.bShark && !bCet)
				PFin(Pl * .55, Pl * .25, FLifeR3::RY(FMath::Atan2(-.95, Sx * .3)), FVector(Sx * W * .12, BotY(.36) + .01, -L / 2 + .36 * L), Fin);
		}

		// eyes, bill, beak, hammer
		const double Te = .09, Ke = Prof(Te), Ex = W / 2 * Ke * .86, Ey = Hh * .1 * Ke + Hh * .04, Ez = -L / 2 + Te * L, Er = FMath::Max(.008, FMath::Min(Hh, W) * .085);
		for (int32 Sx = -1; Sx <= 1; Sx += 2)
		{
			M.Octa(FVector(Sx * Ex, Ey, Ez), FVector(Er * .6, Er, Er), (S.bShark || bCet) ? LHex(0x1a1a1a) : LHex(0xe8dca0));
			if (!S.bShark && !bCet) M.Octa(FVector(Sx * (Ex + Er * .35), Ey, Ez), FVector(Er * .3, Er * .6, Er * .6), LHex(0x0a0a0c));
		}
		if (V.Bill > 0) M.Tube(FVector(0, Hh * .02, -L / 2 + .04), FVector(0, Hh * .02, -L / 2 - L * V.Bill + .04), .03, 0, 6, Back);
		if (bDolphin) M.Tube(FVector(0, -Hh * .12, -L / 2 + L * .01), FVector(0, -Hh * .12, -L / 2 - L * .11), .045, 0, 8, Back);
		if (V.Key == TEXT("hammerhead")) M.Box(FVector(0, 0, -L * .47), FVector(L * .3, Hh * .16, L * .07), FLifeR3::Ident(), Back);
	}

	// JS buildCreatureGeoOld (2240): rays, turtles and manatees
	void LifeBuildOld(const FKeysVis& V, FLifeTpl& M)
	{
		const double L = V.Len;
		const FLinearColor Back = V.Back, Belly = V.Belly;
		const FLifeR3 I = FLifeR3::Ident();
		auto Body = [&V](const FVector& U) { return LifeBodyColor(V, U); };
		auto NoDeform = [](FVector&) {};
		if (V.Kind == EKeysKind::Ray)
		{
			M.Ellipsoid(FVector::ZeroVector, FVector(V.Wing / 2, L * .07, L / 2), I, 14, 6, Body, [](FVector& D) { D.Z *= 1 - .35 * FMath::Abs(D.X); });
			M.Tube(FVector(0, 0, L / 2), FVector(0, 0, L / 2 + L * 1.2), .03, .012, 4, Back);
			if (V.bFlap) M.Blob(FVector(0, 0, -L * .5), FVector(L * .12, L * .08, L * .14), 8, 5, Back);
		}
		else if (V.Kind == EKeysKind::Turtle)
		{
			M.Ellipsoid(FVector::ZeroVector, FVector(L * .42, L * .18, L * .55), I, 12, 6, Body, NoDeform);
			M.Blob(FVector(0, 0, -L * .62), FVector(L * .12, L * .1, L * .15), 8, 5, Belly);
			for (int32 Sx = -1; Sx <= 1; Sx += 2)
			{
				M.Box(FVector(Sx * L * .45, -.02, -L * .22), FVector(L * .55, .035, L * .17), FLifeR3::XYZ(0, -Sx * .45, 0), Belly);
				M.Box(FVector(Sx * L * .25, -.02, L * .45), FVector(L * .22, .03, L * .12), FLifeR3::XYZ(0, Sx * .5, 0), Belly);
			}
		}
		else
		{
			M.Ellipsoid(FVector::ZeroVector, FVector(L * .32, L * .27, L * .5), I, 14, 8, Body, [](FVector& D) { const double K = 1 - .35 * FMath::Max(0.0, D.Z); D.X *= K; D.Y *= K; });
			M.Blob(FVector(0, 0, L * .58), FVector(L * .28, .04, L * .17), 10, 4, Back);
			M.Blob(FVector(0, -L * .03, -L * .52), FVector(L * .13, L * .11, L * .1), 8, 5, Back);
			for (int32 Sx = -1; Sx <= 1; Sx += 2) M.Box(FVector(Sx * L * .32, -L * .08, -L * .2), FVector(L * .2, .04, L * .09), FLifeR3::XYZ(0, 0, Sx * .5), Back);
		}
	}

	const FLifeTpl& LifeCreatureTpl(const FKeysVis* V)
	{
		static TMap<const FKeysVis*, TUniquePtr<FLifeTpl>> Cache;
		TUniquePtr<FLifeTpl>& Slot = Cache.FindOrAdd(V);
		if (!Slot)
		{
			Slot = MakeUnique<FLifeTpl>();
			if (V->Kind == EKeysKind::Ray || V->Kind == EKeysKind::Turtle || V->Kind == EKeysKind::Manatee) LifeBuildOld(*V, *Slot);
			else LifeBuildFish(*V, *Slot);
			Slot->Finish(V);
		}
		return *Slot;
	}

	// ---------------------------------------------------------------- seabirds (JS BIRD_KINDS / birdGeos, 1990)
	struct FLifeBirdTpl
	{
		FLifeTpl Body;
		TArray<FVector> Inner, Outer, Tip;   // right wing panels, bird-local
		FLinearColor WingC, TipC;           // linear
		double Ei = .3;
	};

	const FLifeBirdTpl& LifeBirdTpl(uint8 Kind)
	{
		static FLifeBirdTpl K[4];
		static bool bBuilt = false;
		if (!bBuilt)
		{
			bBuilt = true;
			struct FKindDef { double Span, Body; uint32 BodyC, WingC, TipC, HeadC, BeakC, TailC; int32 Tail; bool bPelican, bAngular; };   // Tail: 0 square, 1 fork, 2 deep fork
			const FKindDef Defs[4] = {
				{1.25, .42, 0xf6f6f3, 0xaeb5bc, 0x1b1d20, 0xf8f8f6, 0xe2b23a, 0xf6f6f3, 0, false, false},   // gull
				{.95, .34, 0xf8f8f6, 0xd3d8dd, 0x8a9096, 0x15171a, 0xe4572e, 0xf8f8f6, 1, false, false},    // tern
				{2.1, .55, 0x16181c, 0x1b1d22, 0x111216, 0x16181c, 0x8a8f96, 0x16181c, 2, false, true},     // frigatebird
				{2.1, .85, 0x6f6556, 0x5d5448, 0x2a2622, 0xf2e6c4, 0xc9a25a, 0x5d5448, 0, true, false}};    // brown pelican
			for (int32 k = 0; k < 4; k++)
			{
				const FKindDef& D = Defs[k];
				FLifeBirdTpl& B = K[k];
				FLifeTpl& M = B.Body;
				const double L = D.Body;
				M.Blob(FVector::ZeroVector, FVector(L * .21, L * .2, L * .5), 6, 4, LHex(D.BodyC));
				if (D.bPelican)
				{
					M.Blob(FVector(0, .1, L * .42), FVector(L * .16, L * .16, L * .16), 5, 3, LHex(D.HeadC));
					M.Tube(FVector(0, .08, L * .45), FVector(0, .08, L * 1.15), .045, 0, 4, LHex(D.BeakC));
					M.Blob(FVector(0, .03, L * .72), FVector(.05, .045, .16), 4, 3, LHex(0xb08d5a));
					M.Blob(FVector(0, .05, L * .3), FVector(L * .11, L * .143, L * .11), 5, 3, LHex(D.HeadC));
				}
				else
				{
					M.Blob(FVector(0, .03, L * .52), FVector(L * .17, L * .1615, L * .187), 5, 3, LHex(D.HeadC));
					M.Tube(FVector(0, .02, L * .62), FVector(0, .02, L * .94), .022, 0, 4, LHex(D.BeakC));
				}
				// tail, trailing behind (-z)
				const double Tw = L * .22, Tlen = D.Tail == 2 ? L * .9 : D.Tail == 1 ? L * .55 : L * .32, Tz = -L * .42;
				TArray<FVector> Tp;
				if (D.Tail == 0) Tp = {FVector(Tw * .5, 0, Tz), FVector(Tw * .6, 0, Tz - Tlen), FVector(-Tw * .6, 0, Tz - Tlen), FVector(-Tw * .5, 0, Tz)};
				else Tp = {FVector(Tw * .5, 0, Tz), FVector(Tw * .7, 0, Tz - Tlen), FVector(0, 0, Tz - Tlen * .45), FVector(-Tw * .7, 0, Tz - Tlen), FVector(-Tw * .5, 0, Tz)};
				M.FanFrom(FVector(0, 0, Tz), Tp, LHex(D.TailC));
				M.Finish(nullptr);
				// wings: inner panel shoulder to elbow, outer panel (hand) elbow to tip with dark tips; leading edge forward (+z)
				const double S = D.Span / 2, Ei = S * .46, Ch = L * (D.bPelican ? .55 : .42), Eo = S - Ei, Sw = D.bAngular ? Ch * .5 : Ch * .2;
				B.Ei = Ei;
				B.Inner = {FVector(0, 0, Ch * .55), FVector(Ei, 0, Ch * .45), FVector(Ei, 0, -Ch * .5), FVector(0, 0, -Ch * .45)};
				B.Outer = {FVector(0, 0, Ch * .45), FVector(Eo * .7, 0, Ch * .2 - Sw * .5), FVector(Eo, 0, -Sw), FVector(Eo * .75, 0, -Ch * .35 - Sw * .4), FVector(0, 0, -Ch * .5)};
				B.Tip = {FVector(Eo * .62, .003, Ch * .24 - Sw * .45), FVector(Eo, .003, -Sw), FVector(Eo * .8, .003, -Ch * .32 - Sw * .4), FVector(Eo * .62, .003, -Ch * .3 - Sw * .3)};
				B.WingC = LLin(LHex(D.WingC));
				B.TipC = LLin(LHex(D.TipC));
			}
		}
		return K[FMath::Min<int32>(Kind, 3)];
	}

	// flat triangle into a frame buffer; positions in game meters
	void LifeBufTri(FKeysLifeBuf& B, const FVector& A, const FVector& Bb, const FVector& Cc, const FLinearColor& LinCol)
	{
		FVector Nn = FVector::CrossProduct(Bb - A, Cc - A).GetSafeNormal();
		if (Nn.Y < 0) Nn = -Nn;
		if (Nn.IsNearlyZero()) Nn = FVector(0, 1, 0);
		const int32 I = B.V.Num();
		const FVector UA = LG2U(A), UB = LG2U(Bb), UC = LG2U(Cc), NU = LN2U(Nn);
		// The game -> Unreal axis swap mirrors the triangle, and the normal above was flipped to face up, so the winding
		// can end up opposite the normal. Unreal then draws it as a back face and shades it dark; keep the two in agreement.
		const bool bSwap = FVector::DotProduct(FVector::CrossProduct(UB - UA, UC - UA), NU) > 0;   // Unreal: front faces wind clockwise against the normal
		B.V.Add(UA); B.V.Add(bSwap ? UC : UB); B.V.Add(bSwap ? UB : UC);
		for (int32 k = 0; k < 3; k++) { B.N.Add(NU); B.UV.Add(FVector2D::ZeroVector); B.C.Add(LinCol); }
		B.T.Append({I, I + 1, I + 2});
	}

	// a triangle with a chosen normal (game axes), wound so Unreal draws it front-facing
	void LifeBufTriN(FKeysLifeBuf& B, const FVector& A, const FVector& Bb, const FVector& Cc, const FVector& Nrm, const FLinearColor& LinCol)
	{
		const int32 I = B.V.Num();
		const FVector UA = LG2U(A), UB = LG2U(Bb), UC = LG2U(Cc), NU = LN2U(Nrm.GetSafeNormal());
		const bool bSwap = FVector::DotProduct(FVector::CrossProduct(UB - UA, UC - UA), NU) > 0;   // Unreal: front faces wind clockwise against the normal
		B.V.Add(UA); B.V.Add(bSwap ? UC : UB); B.V.Add(bSwap ? UB : UC);
		for (int32 k = 0; k < 3; k++) { B.N.Add(NU); B.UV.Add(FVector2D::ZeroVector); B.C.Add(LinCol); }
		B.T.Append({I, I + 1, I + 2});
	}

	void LifeBufBox(FKeysLifeBuf& B, const FVector& Ctr, const FVector& Size, const FLifeR3& Rot, const FLinearColor& Col)
	{
		const FVector H = Size * .5;
		auto Pt = [&](double X, double Y, double Z) { return Ctr + Rot * FVector(X * H.X, Y * H.Y, Z * H.Z); };
		const int32 F[6][4][3] = {
			{{1, -1, -1}, {1, 1, -1}, {1, 1, 1}, {1, -1, 1}}, {{-1, -1, -1}, {-1, -1, 1}, {-1, 1, 1}, {-1, 1, -1}},
			{{-1, 1, -1}, {-1, 1, 1}, {1, 1, 1}, {1, 1, -1}}, {{-1, -1, -1}, {1, -1, -1}, {1, -1, 1}, {-1, -1, 1}},
			{{-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}}, {{-1, -1, -1}, {-1, 1, -1}, {1, 1, -1}, {1, -1, -1}}};
		for (int32 f = 0; f < 6; f++)
		{
			const FVector P0 = Pt(F[f][0][0], F[f][0][1], F[f][0][2]), P1 = Pt(F[f][1][0], F[f][1][1], F[f][1][2]);
			const FVector P2 = Pt(F[f][2][0], F[f][2][1], F[f][2][2]), P3 = Pt(F[f][3][0], F[f][3][1], F[f][3][2]);
			LifeBufTri(B, P0, P1, P2, Col);
			LifeBufTri(B, P0, P2, P3, Col);
		}
	}

	void LifeBufOcta(FKeysLifeBuf& B, const FVector& Ctr, const FVector& Rad, const FLifeR3& Rot, const FLinearColor& Col)
	{
		for (int32 Sx = -1; Sx <= 1; Sx += 2)
			for (int32 Sy = -1; Sy <= 1; Sy += 2)
				for (int32 Sz = -1; Sz <= 1; Sz += 2)
					LifeBufTri(B, Ctr + Rot * FVector(Rad.X * Sx, 0, 0), Ctr + Rot * FVector(0, Rad.Y * Sy, 0), Ctr + Rot * FVector(0, 0, Rad.Z * Sz), Col);
	}
}

// ================================================================ actor
AKeysLife::AKeysLife()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	auto Make = [this](const TCHAR* Name)
	{
		UProceduralMeshComponent* M = CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		M->SetupAttachment(Root);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCastShadow(false);
		M->SetUsingAbsoluteLocation(true);
		M->SetUsingAbsoluteRotation(true);
		M->SetUsingAbsoluteScale(true);
		return M;
	};
	BirdMesh = Make(TEXT("Birds"));
	SeaMesh = Make(TEXT("SeaLife"));
	WeedMesh = Make(TEXT("Weeds"));
	DropMesh = Make(TEXT("Spray"));
}

AKeysLife* AKeysLife::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	TActorIterator<AKeysLife> It(W);
	return It ? *It : nullptr;
}

void AKeysLife::BeginPlay()
{
	Super::BeginPlay();
	KeysGeo::Init();
	for (const auto& KV : LifeVisTable()) LifeCreatureTpl(&KV.Value);
	LifeBirdTpl(0);
	ReadViewer(0);

	struct FHotspotCount { const TCHAR* Zone; int32 N; };
	const FHotspotCount Counts[] = {{TEXT("Creek"), 3}, {TEXT("Flats"), 7}, {TEXT("Backcountry"), 6}, {TEXT("Bridge"), 4}, {TEXT("Hawk Channel"), 5}, {TEXT("Reef"), 6}, {TEXT("Offshore"), 9}};
	for (const FHotspotCount& C : Counts)
		for (int32 i = 0; i < C.N; i++) SpawnHotspot(C.Zone);
	BuildWeeds();
	BuildRigs();
}

void AKeysLife::ReadViewer(double Dt)
{
	if (AKeysBoatPawn* P = Cast<AKeysBoatPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		const double NX = P->BoatX(), NZ = P->BoatZ();
		if (bHaveBoat && Dt > 0)
		{
			const double V = LHyp(NX - BoatX, NZ - BoatZ) / Dt;
			if (V < 60) BoatSpd = LLerp(BoatSpd, V, FMath::Min(1.0, Dt * 5));   // ignore teleports (boat swaps, unstick)
		}
		BoatX = NX;
		BoatZ = NZ;
		bHaveBoat = true;
	}
	if (APlayerCameraManager* C = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector L = C->GetCameraLocation();
		CamX = L.X / 100;
		CamZ = L.Y / 100;
		CamY = L.Z / 100;
	}
	else { CamX = BoatX; CamZ = BoatZ; }
}

void AKeysLife::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Dt = FMath::Clamp(double(DeltaSeconds), 0.0, .1);
	const double T = GetWorld()->GetTimeSeconds();
	NowT = T;
	ReadViewer(Dt);
	bTunaClosing = false;
	BirdBuf.Reset(); SeaBuf.Reset(); WeedBuf.Reset(); DropBuf.Reset();
	if (bHaveBoat)
	{
		if (!Pelicans.Num()) InitPelicans();
		UpdatePelicans(Dt, T);
		UpdateCreatures(Dt, T);
	}
	UpdateHotspots(Dt, T);
	UpdateWeeds(Dt, T);
	UpdateRigs(Dt, T);
	UpdateDrops(Dt, T);
	Flush(BirdMesh, BirdBuf);
	Flush(SeaMesh, SeaBuf);
	Flush(WeedMesh, WeedBuf);
	Flush(DropMesh, DropBuf);
}

void AKeysLife::Flush(UProceduralMeshComponent* M, FKeysLifeBuf& B)
{
	if (!M) return;
	if (!B.V.Num())
	{
		if (B.LastV != 0) { M->ClearMeshSection(0); B.LastV = 0; }
		return;
	}
	if (B.V.Num() != B.LastV || B.Sig != B.LastSig)
	{
		M->CreateMeshSection_LinearColor(0, B.V, B.T, B.N, B.UV, B.C, TArray<FProcMeshTangent>(), false);
		M->SetMaterial(0, AKeysWorld::VertexColorMaterial());
		B.LastV = B.V.Num();
		B.LastSig = B.Sig;
	}
	else M->UpdateMeshSection_LinearColor(0, B.V, B.N, B.UV, B.C, TArray<FProcMeshTangent>());
}

void AKeysLife::Toast(const FString& Msg)
{
	if (Toasts.Num() < 4 && !Toasts.Contains(Msg)) Toasts.Add(Msg);
}

FString AKeysLife::PopToast()
{
	if (!Toasts.Num()) return FString();
	const FString S = Toasts[0];
	Toasts.RemoveAt(0);
	return S;
}

// ---------------------------------------------------------------- birds
void AKeysLife::PoseBird(FKeysBird& B, double Dt, double Flap, double Tuck, double Rate)
{
	B.Ph += Dt * Rate * FMath::Max(.15, Flap);
	const double F = FMath::Sin(B.Ph), F2 = FMath::Sin(B.Ph - .7);
	B.Up = (.12 + F * .75 * Flap) * (1 - Tuck) + .35 * Tuck;
	B.Hand = (.05 + F2 * .55 * Flap) * (1 - Tuck) - .25 * Tuck;
	B.Tuck = Tuck;
}

void AKeysLife::MoveBird(FKeysBird& B, double X, double Y, double Z, double Dt)
{
	if (B.bPrev)
	{
		const double VX = X - B.PX, VZ = Z - B.PZ, Sp = LHyp(VX, VZ);
		if (Sp > 1e-4)
		{
			const double Dy = LWrapPi(FMath::Atan2(VX, VZ) - B.Yaw);
			B.Yaw += Dy * FMath::Min(1.0, Dt * 6);
			B.Roll = LLerp(B.Roll, LClamp(-Dy / FMath::Max(Dt, 1e-3) * .05, -.9, .9), FMath::Min(1.0, Dt * 3));
		}
		B.Pitch = LClamp(FMath::Atan2(-(Y - B.PY), FMath::Max(.01, Sp)), -1.3, 1.3);
	}
	B.PX = X; B.PY = Y; B.PZ = Z;
	B.bPrev = true;
}

void AKeysLife::ShootableBirds(double X, double Z, double R, TArray<FKeysBirdTarget>& Out)
{
	auto Try = [&](FKeysBird& B)
	{
		if (!B.bPrev || B.DownUntil > NowT) return;
		if (FMath::Square(B.PX - X) + FMath::Square(B.PZ - Z) > R * R) return;
		Out.Add({&B, FVector(B.PX, B.PY, B.PZ), B.Kind});
	};
	for (FKeysHotspot& H : Hotspots) for (FKeysBird& B : H.Birds) Try(B);
	for (FKeysWeedline& W : Weeds) for (FKeysBird& B : W.Birds) Try(B);
	for (FKeysPelicanFlock& F : Pelicans) for (FKeysBird& B : F.Birds) Try(B);
}

void AKeysLife::DownBird(FKeysBird* B, double Secs)
{
	if (B) B->DownUntil = NowT + Secs;
}

void AKeysLife::EmitBird(const FKeysBird& B)
{
	if (B.DownUntil > NowT) return;
	const FLifeBirdTpl& K = LifeBirdTpl(B.Kind);
	const FLifeR3 R = FLifeR3::YXZ(B.Pitch, B.Yaw, B.Roll);
	const FVector Pos(B.PX, B.PY, B.PZ);
	const double S = B.Scale;
	FKeysLifeBuf& Buf = BirdBuf;
	const int32 I0 = Buf.V.Num();
	const FLifeTpl& Body = K.Body;
	for (int32 i = 0; i < Body.P.Num(); i++)
	{
		Buf.V.Add(LG2U(Pos + R * (Body.P[i] * S)));
		Buf.N.Add(LN2U(R * Body.N[i]));
		Buf.UV.Add(FVector2D::ZeroVector);
		Buf.C.Add(Body.C[i]);
	}
	for (const int32 Ix : Body.T) Buf.T.Add(I0 + Ix);
	// two-part wings: shoulder (flap + fold) and elbow (hand), three.js Euler XYZ
	for (int32 Sx = -1; Sx <= 1; Sx += 2)
	{
		const FLifeR3 Rsh = FLifeR3::XYZ(0, -Sx * 1.15 * B.Tuck, Sx * B.Up), Rel = FLifeR3::XYZ(0, -Sx * .9 * B.Tuck, Sx * (B.Hand - B.Up * .25));
		const FVector Tsh(Sx * .06, .03, .03), Tel(Sx * K.Ei, 0, 0);
		auto Panel = [&](const TArray<FVector>& Pts, bool bOuter, const FLinearColor& Col)
		{
			TArray<FVector, TInlineAllocator<6>> W;
			for (const FVector& Q : Pts)
			{
				const FVector M(Q.X * Sx, Q.Y, Q.Z);
				const FVector Local = bOuter ? Tsh + Rsh * (Tel + Rel * M) : Tsh + Rsh * M;
				W.Add(Pos + R * (Local * S));
			}
			for (int32 k = 1; k + 1 < W.Num(); k++) LifeBufTri(Buf, W[0], W[k], W[k + 1], Col);
		};
		Panel(K.Inner, false, K.WingC);
		Panel(K.Outer, true, K.WingC);
		Panel(K.Tip, true, K.TipC);
	}
	Buf.Sig = Buf.Sig * 31 + B.Kind + 1;
}

void AKeysLife::SpawnFlock(FKeysPelicanFlock& F, bool bNear)
{
	const double A = FMath::FRand() * PI * 2, D = bNear ? LRand(150, 350) : LRand(450, 750);
	double X = BoatX + FMath::Cos(A) * D, Z = BoatZ + FMath::Sin(A) * D;
	for (int32 k = 0; k < 12 && LandH(X, Z) > -.5; k++) { const double B = FMath::FRand() * 6.28; X = BoatX + FMath::Cos(B) * D; Z = BoatZ + FMath::Sin(B) * D; }
	F.X = X; F.Z = Z;
	F.H = FMath::Atan2(BoatX - X, BoatZ - Z) + LRand(-1.1, 1.1);
	F.Sp = LRand(9, 12); F.Alt = LRand(2.5, 5); F.bVee = FMath::FRand() < .4; F.FlapT = LRand(0, 4); F.Turn = LRand(-.05, .05);
	for (FKeysBird& B : F.Birds) { B.bDive = false; B.bPrev = false; }
}

void AKeysLife::InitPelicans()
{
	for (int32 f = 0; f < 4; f++)
	{
		FKeysPelicanFlock F;
		const int32 N = 3 + FMath::RandRange(0, 4);
		for (int32 i = 0; i < N; i++) { FKeysBird B; B.Kind = 3; B.Scale = 1.15; B.Ph = FMath::FRand() * 6.28; F.Birds.Add(B); }
		SpawnFlock(F, f < 2);
		Pelicans.Add(MoveTemp(F));
	}
}

// brown pelicans: lines of 3-7 gliding low over the water, flapping in a ripple down the line; now and then one plunges (JS 1891)
void AKeysLife::UpdatePelicans(double Dt, double T)
{
	for (FKeysPelicanFlock& F : Pelicans)
	{
		if (LHyp(F.X - BoatX, F.Z - BoatZ) > 1100) SpawnFlock(F, false);
		const double Fx = FMath::Sin(F.H), Fz = FMath::Cos(F.H);
		if (LandH(F.X + Fx * 60, F.Z + Fz * 60) > -.3) F.H += Dt * .9; else F.H += F.Turn * Dt;   // curl away from land
		F.X += Fx * F.Sp * Dt;
		F.Z += Fz * F.Sp * Dt;
		F.FlapT -= Dt;
		const bool bFlapping = F.FlapT < 0;
		if (F.FlapT < -2.2) F.FlapT = LRand(2.5, 5);
		if (LHyp(F.X - CamX, F.Z - CamZ) >= 1300) { for (FKeysBird& B : F.Birds) B.bPrev = false; continue; }
		for (int32 i = 0; i < F.Birds.Num(); i++)
		{
			FKeysBird& B = F.Birds[i];
			const double Back = i * 5.5, Side = (i % 2 ? 1 : -1) * FMath::CeilToDouble(i / 2.0) * 2.2 * (F.bVee ? 1 : .25);
			double X = F.X - Fx * Back + FMath::Cos(F.H) * Side, Z = F.Z - Fz * Back - FMath::Sin(F.H) * Side;
			double Y = F.Alt + WaveH(X, Z, T, AmpAt(X, Z), false) + FMath::Sin(T * .6 + i) * .3;
			double Flap = bFlapping && FMath::Fmod(T, 3.0) > i * .18 ? 1 : 0, Tuck = 0;
			if (B.bDive)
			{
				B.DT += Dt;
				const double P = B.DT;
				if (P < 1.2) { Y = LLerp(Y, Y + 7, P / 1.2); Flap = .6; }                                   // climb and look
				else if (P < 1.9) { const double Q = (P - 1.2) / .7; Y = LLerp(Y + 7, .2, Q * Q); Tuck = FMath::Min(1.0, Q * 1.6); X += Fx * Q * 6; Z += Fz * Q * 6; }   // fold up and plunge
				else if (P < 4.2)                                                                              // sitting, swallowing
				{
					if (!B.bWX) { B.bWX = true; B.WX = X + Fx * 6; B.WZ = Z + Fz * 6; }
					X = B.WX; Z = B.WZ;
					if (!B.bSpl) { B.bSpl = true; Splash(X, Z, 16, 1.2); }
					Y = .15 + WaveH(X, Z, T, AmpAt(X, Z), false); Flap = 0; Tuck = .3;
				}
				else if (P < 6) { const double Q = (P - 4.2) / 1.8; Y = LLerp(.2, F.Alt, Q); Flap = 1; }
				else B.bDive = false;
			}
			else if (FMath::FRand() < Dt * .012) { B.bDive = true; B.DT = 0; B.bSpl = false; B.bWX = false; }
			PoseBird(B, Dt, Flap, Tuck, 5.5);
			MoveBird(B, X, Y, Z, Dt);
			EmitBird(B);
		}
	}
}

// ---------------------------------------------------------------- hotspots: bird piles over feeding fish (JS 2039)
void AKeysLife::SpawnHotspot(const FString& Zone)
{
	double X = 0, Z = 0;
	bool bOk = false;
	auto FarFromBoat = [this](double PX, double PZ) { return !bHaveBoat || LHyp(PX - BoatX, PZ - BoatZ) > 120; };
	if (Zone == TEXT("Creek"))
	{
		// the creeks are private to KeysGeo, so sample around their end points (see GCreeks in KeysGeo.cpp) and test CreekDist
		struct FC { double X0, Z0, X1, Z1, A; };
		static const FC Cs[] = {{620, -400, 700, 400, 70}, {-700, 360, -640, 790, 38}, {3450, -320, 3560, 320, 42}, {2150, -2620, 2650, -4300, 170}, {-2400, -2620, -2950, -4100, 150}, {3650, -2640, 3950, -3700, 70}};
		const int32 NC = int32(UE_ARRAY_COUNT(Cs));
		for (int32 k = 0; k < 400 && !bOk; k++)
		{
			const FC& C = Cs[FMath::RandRange(0, NC - 1)];
			const double U = LRand(.2, .8);
			X = LLerp(C.X0, C.X1, U) + LRand(-C.A, C.A);
			Z = LLerp(C.Z0, C.Z1, U) + LRand(-C.A, C.A);
			bOk = X > WBX0 && X < WBX1 && Z > WBZ0 && Z < WBZ1 && CreekDist(X, Z) < -1 && FarFromBoat(X, Z);
		}
	}
	if (!bOk && Zone == TEXT("Offshore") && FMath::FRand() < .45)
	{
		TArray<const FHump*> Big;
		for (const FHump& H : Humps()) if (!H.bPatch) Big.Add(&H);
		if (Big.Num())
		{
			const FHump* H = Big[FMath::FRand() < .55 ? 0 : FMath::Min(1, Big.Num() - 1)];
			for (int32 k = 0; k < 40 && !bOk; k++)
			{
				X = H->X + LRand(-260, 260);
				Z = ChainZ(H->X) + H->DZ + LRand(-260, 260);
				bOk = FarFromBoat(X, Z);
			}
		}
	}
	for (int32 k = 0; k < 500 && !bOk; k++)
	{
		X = LRand(WBX0 + 100, WBX1 - 100);
		Z = LRand(WBZ0 + 100, WBZ1 - 100);
		if (!FarFromBoat(X, Z)) continue;
		// cheap rejections before the full zone test
		const double DZ = Z - ChainZ(X);
		if (Zone == TEXT("Offshore") && DZ < 1500) continue;
		if (Zone == TEXT("Reef") && (DZ < 1150 || DZ > 1800)) continue;
		if (Zone == TEXT("Hawk Channel") && (DZ < -10 || DZ > 1350)) continue;
		if (Zone == TEXT("Backcountry") && DZ > 10) continue;
		if (ZoneAt(X, Z) == Zone && ShoreInfo(X, Z).D > 10) bOk = true;
	}
	if (!bOk) return;

	FKeysHotspot H;
	H.Id = NextId++;
	H.Zone = Zone;
	H.X = X; H.Z = Z;
	H.Fish = 2 + FMath::RandRange(0, 2);
	H.Amp = AmpFor(DepthAt(X, Z));
	H.FT = LRand(3, 20);
	for (int32 i = 0; i < 11; i++)
	{
		FKeysBird B;
		B.Kind = i % 4 == 3 ? 1 : 0;
		B.Scale = 1.25;
		B.Ph = FMath::FRand() * 6.28;
		B.R = LRand(6, 13); B.H = LRand(10, 18); B.Sp = LRand(.5, .9) * (FMath::FRand() < .5 ? -1 : 1); B.Ph2 = FMath::FRand() * 6.28;
		H.Birds.Add(B);
	}
	if (Zone == TEXT("Offshore"))
	{
		const double Ang = FMath::FRand() * PI;
		for (int32 i = 0; i < 14; i++)
		{
			const double Tt = (i - 7) * 5.5;
			H.Weeds.Add(FVector4(FMath::Cos(Ang) * Tt + LRand(-1.5, 1.5), FMath::Sin(Ang) * Tt + LRand(-1.5, 1.5), -Ang + LRand(-.3, .3), LRand(4, 8)));
			H.WeedW.Add(LRand(1, 2));
		}
	}
	Hotspots.Add(MoveTemp(H));
}

void AKeysLife::UpdateHotspots(double Dt, double T)
{
	const FLinearColor WeedC = LLin(LHex(0xc69a33));
	for (FKeysHotspot& H : Hotspots)
	{
		// feeding frenzies come and go: the bait gets pushed up, the birds pile in, then it goes quiet for a while
		H.FT -= Dt;
		if (H.FT <= 0) { H.bActive = !H.bActive; H.FT = H.bActive ? LRand(14, 28) : LRand(10, 26); }
		H.Fr = LLerp(H.Fr, H.bActive ? 1 : 0, FMath::Min(1.0, Dt * .7));
		const double DC = LHyp(H.X - CamX, H.Z - CamZ), Fr = H.Fr;
		const bool bNear = DC < 900, bVis = DC < 1100;
		if (!bVis) { for (FKeysBird& B : H.Birds) B.bPrev = false; continue; }
		for (FKeysBird& B : H.Birds)
		{
			const double A = T * B.Sp * (1 + Fr * .6) + B.Ph2, R = B.R * (1 - .35 * Fr), Hh = B.H * (1 - .4 * Fr);
			const double CX = FMath::Cos(A) * R, CY = Hh + FMath::Sin(T * 1.3 + B.Ph2), CZ = FMath::Sin(A) * R;
			if (!B.bDive && Fr > .45 && FMath::FRand() < Dt * .3) { B.bDive = true; B.DT = 0; B.DX = LRand(-9, 9); B.DZ = LRand(-9, 9); B.bSpl = false; }
			if (B.bDive)
			{
				B.DT += Dt;
				const double P = B.DT / 1.8;
				double X, Y, Z, Flap = 0, Tuck = 0;
				if (P < .45) { const double Q = P / .45; X = LLerp(CX, B.DX, Q); Z = LLerp(CZ, B.DZ, Q); Y = LLerp(CY, .3, Q * Q); Tuck = FMath::Min(1.0, Q * 1.6); }   // fold the wings and plunge
				else if (P < .62) { X = B.DX; Z = B.DZ; Y = .05; Tuck = 1; if (!B.bSpl) { B.bSpl = true; if (bNear) Splash(H.X + B.DX, H.Z + B.DZ, 8, .8); } }   // in the water
				else { const double Q = (P - .62) / .38; X = LLerp(B.DX, CX, Q); Z = LLerp(B.DZ, CZ, Q); Y = LLerp(.3, CY, Q); Flap = 1; }   // beat back up
				if (P >= 1) B.bDive = false;
				PoseBird(B, Dt, Flap, Tuck, 12);
				MoveBird(B, H.X + X, Y, H.Z + Z, Dt);
			}
			else
			{
				const double Flap = (FMath::Sin(T * .8 + B.Ph2) > .1 || Fr > .5) ? 1 : 0;
				PoseBird(B, Dt, Flap, 0, 9 + Fr * 3);
				MoveBird(B, H.X + CX, CY, H.Z + CZ, Dt);
			}
			EmitBird(B);
		}
		for (int32 i = 0; i < H.Weeds.Num(); i++)
		{
			const FVector4& W = H.Weeds[i];
			const double X = H.X + W.X, Z = H.Z + W.Y;
			LifeBufBox(WeedBuf, FVector(X, WaveH(X, Z, T, H.Amp, false) + .05, Z), FVector(W.W, .12, H.WeedW[i]), FLifeR3::RY(W.Z), WeedC);
		}
		if (!bNear) continue;
		// the surface boils: fish busting bait, white water, little silver baitfish spraying out
		if (FMath::FRand() < Dt * (.8 + 7 * Fr))
		{
			const double A = FMath::FRand() * 2 * PI, R = FMath::Sqrt(FMath::FRand()) * 10;
			Splash(H.X + FMath::Cos(A) * R, H.Z + FMath::Sin(A) * R, 4 + int32(Fr * 10), .6 + Fr * .7);
		}
		if (DC < 500)
		{
			const double NB = Fr * Dt * 24, NF = Fr * Dt * 28;
			const int32 NBi = int32(NB) + (FMath::FRand() < FMath::Frac(NB) ? 1 : 0), NFi = int32(NF) + (FMath::FRand() < FMath::Frac(NF) ? 1 : 0);
			for (int32 k = 0; k < NBi; k++)
			{
				const double A = FMath::FRand() * 2 * PI, R = FMath::Sqrt(FMath::FRand()) * 9, V = LRand(1.5, 3.5);
				const double X = H.X + FMath::Cos(A) * R, Z = H.Z + FMath::Sin(A) * R;
				SpawnDrop(FVector(X, WaveH(X, Z, T, H.Amp, false) + .15, Z), FVector(FMath::Cos(A) * V, LRand(2, 4.5), FMath::Sin(A) * V), LRand(.4, .8), LRand(.06, .1), true);
			}
			for (int32 k = 0; k < NFi; k++)
			{
				const double A = FMath::FRand() * 2 * PI, R = FMath::Sqrt(FMath::FRand()) * 11;
				const double X = H.X + FMath::Cos(A) * R, Z = H.Z + FMath::Sin(A) * R;
				SpawnDrop(FVector(X, WaveH(X, Z, T, H.Amp, false) + .05, Z), FVector(LRand(-.3, .3), 0, LRand(-.3, .3)), LRand(1.5, 3), LRand(.2, .45), false);
			}
		}
		if (Fr > .6 && !H.bTold && bHaveBoat && LHyp(H.X - BoatX, H.Z - BoatZ) < 320)
		{
			H.bTold = true;
			Toast(TEXT("Birds diving and fish busting bait - that's a hot spot! Cast into it."));
		}
	}
}

// ---------------------------------------------------------------- floating sargassum: shade and bait for mahi, tripletail, jacks and triggerfish (JS 2081)
void AKeysLife::BuildWeeds()
{
	TArray<FVector2D> Spots;
	for (int32 k = 0; k < 400 && Spots.Num() < 18; k++)
	{
		const double X = LRand(-3800, 3800), Z = ChainZ(X) + LRand(1550, 3700);
		if (DepthAt(X, Z) > 35) Spots.Add(FVector2D(X, Z));
	}
	for (const FHump& H : Humps())
		if (!H.bPatch)
			for (int32 k = 0; k < 2; k++) Spots.Add(FVector2D(H.X + LRand(-300, 300), ChainZ(H.X) + H.DZ + LRand(-300, 300)));
	for (const FVector2D& S : Spots)
	{
		FKeysWeedline W;
		W.X = S.X; W.Z = S.Y;
		W.Len = LRand(50, 170); W.Wid = LRand(3, 9); W.Ang = FMath::FRand() * PI;
		W.Amp = AmpFor(DepthAt(W.X, W.Z));
		const int32 N = FMath::RoundToInt(W.Len / 2.2);
		const double CA = FMath::Cos(W.Ang), SA = FMath::Sin(W.Ang);
		for (int32 i = 0; i < N; i++)
		{
			const double U = (double(i) / N - .5) * W.Len, V = (FMath::FRand() - .5) * W.Wid * (1 - FMath::Abs(U / W.Len) * 1.2);
			FKeysWeedBit Bit;
			Bit.OX = CA * U - SA * V; Bit.OZ = SA * U + CA * V; Bit.S = LRand(.7, 2.2); Bit.Rot = FMath::FRand() * 6; Bit.Col = i % 6;
			W.Bits.Add(Bit);
		}
		for (int32 i = 0; i < 3; i++)
		{
			FKeysBird B;
			B.Kind = i == 2 ? 0 : 2;
			B.Scale = 1.2;
			B.Ph = FMath::FRand() * 6.28;
			B.R = LRand(15, 30); B.H = LRand(18, 30); B.Sp = LRand(.3, .5) * (FMath::FRand() < .5 ? -1 : 1); B.Ph2 = FMath::FRand() * 6;
			W.Birds.Add(B);
		}
		Weeds.Add(MoveTemp(W));
	}
}

void AKeysLife::UpdateWeeds(double Dt, double T)
{
	static const uint32 WC[] = {0xc69a33, 0xb4862a, 0xd8b04a, 0x9c7424, 0xe0bf5a, 0xa98a3a};
	FLinearColor Cols[6];
	for (int32 i = 0; i < 6; i++) Cols[i] = LLin(LHex(WC[i]));
	for (FKeysWeedline& W : Weeds)
	{
		W.X += .22 * Dt;   // drifting with the current
		W.Z += .05 * Dt;
		const double D = LHyp(W.X - CamX, W.Z - CamZ);
		if (D < 800)
			for (const FKeysWeedBit& Bit : W.Bits)
			{
				const double X = W.X + Bit.OX, Z = W.Z + Bit.OZ;
				LifeBufOcta(WeedBuf, FVector(X, WaveH(X, Z, T, W.Amp, false) + .04, Z), FVector(Bit.S, .18, Bit.S * .8), FLifeR3::RY(Bit.Rot), Cols[Bit.Col % 6]);
			}
		if (D < 1100)
			for (FKeysBird& B : W.Birds)
			{
				const double A = T * B.Sp + B.Ph2;
				PoseBird(B, Dt, FMath::Sin(T * .35 + B.Ph2) > .88 ? 1 : 0, 0, B.Kind == 2 ? 5 : 9);
				MoveBird(B, W.X + FMath::Cos(A) * B.R, B.H + FMath::Sin(T + B.Ph2), W.Z + FMath::Sin(A) * B.R, Dt);
				EmitBird(B);
			}
		else for (FKeysBird& B : W.Birds) B.bPrev = false;
		if (!W.bTold && bHaveBoat && LHyp(W.X - BoatX, W.Z - BoatZ) < 250)
		{
			W.bTold = true;
			Toast(TEXT("Weedline ahead - mahi love to hang under the weeds."));
		}
	}
}

// ---------------------------------------------------------------- offshore oil platforms: steel jackets in blue water (JS 2097)
void AKeysLife::BuildRigs()
{
	const double Spots[4][2] = {{-3250, 2950}, {-900, 3150}, {1550, 2800}, {3350, 3100}};
	auto C = [](uint32 H) { return LLin(LHex(H)); };
	const FLinearColor Steel = C(0xc9a227), Rust = C(0x7a4a2a), Deck = C(0x6b7178), White = C(0xeef0f1), Orange = C(0xe4572e), Heli = C(0x2f6e4a), RingC = C(0xf2c14e);
	for (int32 Ri = 0; Ri < 4; Ri++)
	{
		const double X = Spots[Ri][0], Z = ChainZ(X) + Spots[Ri][1], Dep = DepthAt(X, Z);
		if (Dep < 20) continue;
		const double Rot = Ri * .6;
		FKeysMesh M;
		// JS local (x, y up, z) -> FKeysMesh (x, y, z up), meters
		auto V = [](double A, double B, double Cc) { return FVector(A, Cc, B); };
		auto Beam = [&M](const FVector& A, const FVector& B, double R, const FLinearColor& Col) { M.AddTube(A, B, R, R, 8, Col); };
		auto Box = [&](double W, double H, double D, const FLinearColor& Col, double PX, double PY, double PZ) { M.AddBox(V(PX, PY, PZ), FVector(W, D, H), Col); };
		const double Hl = 12, Base = -FMath::Min(40.0, Dep), Top = 18;
		const double Legs[4][2] = {{-Hl, -Hl}, {Hl, -Hl}, {Hl, Hl}, {-Hl, Hl}};
		// jacket legs, splayed slightly, rust below the waterline, painted above
		for (int32 i = 0; i < 4; i++)
		{
			const double LX = Legs[i][0], LZ = Legs[i][1];
			Beam(V(LX * 1.25, Base, LZ * 1.25), V(LX * 1.04, -.6, LZ * 1.04), .95, Rust);
			Beam(V(LX * 1.04, -.6, LZ * 1.04), V(LX, Top, LZ), .9, Steel);
		}
		// horizontal and X bracing between the legs
		for (const double Y : {-16.0, -6.0, 6.0, 13.0})
		{
			const double K = Y < 0 ? 1.04 + (-.6 - Y) / (-.6 - Base) * .21 : 1.0;
			for (int32 i = 0; i < 4; i++)
			{
				const int32 j = (i + 1) % 4;
				Beam(V(Legs[i][0] * K, Y, Legs[i][1] * K), V(Legs[j][0] * K, Y, Legs[j][1] * K), .35, Y < 0 ? Rust : Steel);
			}
		}
		for (int32 i = 0; i < 4; i++)
		{
			const int32 j = (i + 1) % 4;
			const double AX = Legs[i][0], AZ = Legs[i][1], BX = Legs[j][0], BZ = Legs[j][1];
			Beam(V(AX * 1.02, 1, AZ * 1.02), V(BX, 12.5, BZ), .25, Steel);
			Beam(V(BX * 1.02, 1, BZ * 1.02), V(AX, 12.5, AZ), .25, Steel);
		}
		// decks and modules
		Box(34, 1.2, 34, Deck, 0, Top, 0);
		Box(30, 1, 30, Deck, 0, Top + 6.5, 0);
		for (int32 i = 0; i < 4; i++) Beam(V(Legs[i][0] / Hl * 14, Top + .6, Legs[i][1] / Hl * 14), V(Legs[i][0] / Hl * 14, Top + 6, Legs[i][1] / Hl * 14), .45, Steel);
		Box(14, 5.5, 10, White, -6, Top + 3.3, -8);
		Box(14, 1, 10, Orange, -6, Top + 6.2, -8);
		Box(8, 4, 8, Orange, 8, Top + 2.6, 6);
		Box(6, 9, 6, White, 9, Top + 11, -9);
		// helideck with its yellow ring
		{
			const FVector Lo = V(-8, Top + 12.35, 6), Hi = V(-8, Top + 12.85, 6);
			M.AddTube(Lo, Hi, 8, 8, 24, Heli);
			for (int32 i = 0; i < 24; i++)
			{
				const double A0 = 2 * PI * i / 24, A1 = 2 * PI * (i + 1) / 24;
				const FVector O0(FMath::Cos(A0) * 8, FMath::Sin(A0) * 8, 0), O1(FMath::Cos(A1) * 8, FMath::Sin(A1) * 8, 0);
				M.AddTri(Hi, Hi + O0, Hi + O1, Heli, FVector::UpVector);
				M.AddTri(Lo, Lo + O1, Lo + O0, Heli, -FVector::UpVector);
			}
			Beam(V(-8, Top + 7, 6), V(-8, Top + 12.3, 6), .6, Steel);
			for (int32 i = 0; i < 32; i++)
			{
				const double A0 = 2 * PI * i / 32, A1 = 2 * PI * (i + 1) / 32;
				M.AddTube(V(-8 + 5 * FMath::Cos(A0), Top + 12.9, 6 + 5 * FMath::Sin(A0)), V(-8 + 5 * FMath::Cos(A1), Top + 12.9, 6 + 5 * FMath::Sin(A1)), .18, .18, 4, RingC);
			}
		}
		// crane and flare boom
		Beam(V(12, Top + 7, 12), V(12, Top + 12, 12), .7, Steel);
		Beam(V(12, Top + 12, 12), V(-4, Top + 22, 20), .35, Orange);
		Beam(V(17, Top + 6, -6), V(36, Top + 16, -12), .4, Steel);
		// stairs down to the boat landing
		Box(6, .4, 3, Deck, Hl + 3, 1.6, 0);
		Beam(V(Hl + 3, 1.8, -1.5), V(Hl, Top, -1.5), .15, Steel);
		Beam(V(Hl + 3, 1.8, 1.5), V(Hl, Top, 1.5), .15, Steel);

		UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this, *FString::Printf(TEXT("Rig%d"), Ri));
		PM->SetupAttachment(Root);
		PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PM->RegisterComponent();
		PM->SetWorldLocationAndRotation(ToUE(X, 0, Z), FRotator(0.0, -FMath::RadiansToDegrees(Rot), 0.0));
		M.Commit(PM, 0, AKeysWorld::VertexColorMaterial());

		// the flare: a flickering flame on the end of the boom
		FKeysMesh FM;
		const FLinearColor Flame = LLin(FLinearColor(1.f, .62f, .2f)), Core = LLin(FLinearColor(1.f, .9f, .55f));
		FM.AddTube(FVector(0, 0, -1.5), FVector(0, 0, 5), 1.8, 0, 7, Flame);
		FM.AddTube(FVector(0, 0, -1.5), FVector(0, 0, -2.6), 1.8, 0, 7, Flame);
		FM.AddTube(FVector(.3, 0, -1), FVector(.3, 0, 2.8), 1.0, 0, 5, Core);
		UProceduralMeshComponent* FC = NewObject<UProceduralMeshComponent>(this, *FString::Printf(TEXT("RigFlame%d"), Ri));
		FC->SetupAttachment(PM);
		FC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FC->SetCastShadow(false);
		FC->SetRelativeLocation(V(37, Top + 17.5, -12.5) * 100.0);
		FC->RegisterComponent();
		FM.Commit(FC, 0, AKeysWorld::VertexColorMaterial());

		FKeysRig R;
		R.X = X; R.Z = Z; R.Rot = Rot;
		R.MeshIndex = RigMeshes.Num();
		RigMeshes.Add(PM);
		FlameMeshes.Add(FC);
		Rigs.Add(R);
		FKeysRig Twin;
		Twin.X = X + 18; Twin.Z = Z - 14; Twin.Rot = Rot; Twin.bTwin = true;
		Rigs.Add(Twin);

		// collisions on the four legs at the waterline
		const double CR = FMath::Cos(Rot), SR = FMath::Sin(Rot);
		for (int32 i = 0; i < 4; i++)
		{
			const double LX = Legs[i][0] * 1.04, LZ = Legs[i][1] * 1.04;
			KeysGeo::FPillar Leg;
			Leg.X = X + LX * CR + LZ * SR; Leg.Z = Z - LX * SR + LZ * CR; Leg.R = 2; Leg.bOld = false;
			RigLegs.Add(Leg);
		}
	}
}

void AKeysLife::UpdateRigs(double Dt, double T)
{
	for (FKeysRig& R : Rigs)
	{
		if (R.bTwin || !RigMeshes.IsValidIndex(R.MeshIndex)) continue;
		const bool bNear = LHyp(R.X - CamX, R.Z - CamZ) < 2500;
		if (UProceduralMeshComponent* PM = RigMeshes[R.MeshIndex]) PM->SetVisibility(bNear, true);
		if (!bNear) continue;
		if (FlameMeshes.IsValidIndex(R.MeshIndex) && FlameMeshes[R.MeshIndex])
			FlameMeshes[R.MeshIndex]->SetRelativeScale3D(FVector(1 + .15 * FMath::Sin(T * 9 + R.X), 1 + .15 * FMath::Sin(T * 8.1 + R.Z), 1 + .25 * FMath::Sin(T * 7.3)));
		if (!R.bTold && bHaveBoat && LHyp(R.X - BoatX, R.Z - BoatZ) < 450)
		{
			R.bTold = true;
			Toast(TEXT("Oil rig ahead - amberjack, cobia and tuna stack up around the legs."));
		}
	}
}

// ---------------------------------------------------------------- spray
void AKeysLife::SpawnDrop(const FVector& P, const FVector& V, double Life, double Size, bool bGrav)
{
	if (Drops.Num() >= 600) return;
	FKeysDrop D;
	D.P = P; D.Vel = V; D.Life = Life; D.Size = Size; D.bGrav = bGrav;
	Drops.Add(D);
}

void AKeysLife::Splash(double X, double Z, int32 N, double Power)
{
	if (LHyp(X - CamX, Z - CamZ) > 500) return;
	const double Surf = WaveH(X, Z, NowT, AmpAt(X, Z), false);
	N = FMath::Min(N, 70);
	// a crown of spray: big splashes throw bigger, higher drops
	const double SizeK = FMath::Pow(FMath::Max(Power, .3), .7);
	for (int32 i = 0; i < N; i++)
	{
		const double A = FMath::FRand() * PI * 2, S = LRand(.5, 1.2) * Power;
		SpawnDrop(FVector(X + FMath::Cos(A) * .3 * Power, Surf + .15, Z + FMath::Sin(A) * .3 * Power),
			FVector(FMath::Cos(A) * S, LRand(2, 5.5) * Power * .75, FMath::Sin(A) * S), LRand(.7, 1.3), LRand(.6, 1.2) * .2 * SizeK, true);
	}
	// and a patch of white water left behind on the surface
	if (Power >= .9)
	{
		static int32 SplashId = 0;
		const int32 Src = 900000 + (SplashId++ % 5000);
		const double W = .6 + Power * .8;
		EmitTrail(Src, X - .2, Z, W, FMath::Min(1.0, .5 + Power * .25), NowT);
		EmitTrail(Src, X + .2, Z, W, FMath::Min(1.0, .5 + Power * .25), NowT);
	}
}

void AKeysLife::UpdateDrops(double Dt, double T)
{
	for (int32 i = Drops.Num() - 1; i >= 0; i--)
	{
		FKeysDrop& D = Drops[i];
		D.Age += Dt;
		if (D.bGrav) D.Vel.Y -= 9.8 * Dt;
		D.P += D.Vel * Dt;
		if (D.Age >= D.Life || (D.bGrav && D.Vel.Y < 0 && D.P.Y < -.8)) Drops.RemoveAtSwap(i);
	}
	const FLinearColor Spray = LLin(LHex(0xf4f8fb)), Foam = LLin(LHex(0xe6eef2));
	const FVector Up(0, 1, 0);
	for (const FKeysDrop& D : Drops)
	{
		const double S = D.Size * (1 - .5 * D.Age / D.Life);
		const FVector P = D.P;
		const FLinearColor& Col = D.bGrav ? Spray : Foam;
		if (D.bGrav)
		{
			// spray: a little diamond turned to face the camera and lit like the sky-facing water, so it reads as bright
			// white water from any angle (a solid shape shades its sunless side gray and looks like gravel)
			FVector ToCam(CamX - P.X, CamY - P.Y, CamZ - P.Z);
			ToCam = ToCam.GetSafeNormal(UE_SMALL_NUMBER, FVector(1, 0, 0));
			const FVector R = FVector::CrossProduct(Up, ToCam).GetSafeNormal(UE_SMALL_NUMBER, FVector(1, 0, 0));
			const FVector V = FVector::CrossProduct(ToCam, R).GetSafeNormal();
			// stretched along the direction it's flying, like a streak of water
			const FVector Vel = D.Vel.GetSafeNormal(UE_SMALL_NUMBER, Up);
			const FVector Along = (Vel - ToCam * FVector::DotProduct(Vel, ToCam)).GetSafeNormal(UE_SMALL_NUMBER, V);
			const FVector Side = FVector::CrossProduct(ToCam, Along).GetSafeNormal(UE_SMALL_NUMBER, R);
			const FVector T0 = P + Along * S * 1.6, B0 = P - Along * S * .9, L0 = P - Side * S * .6, R0 = P + Side * S * .6;
			const FVector N = (Up + ToCam * .5).GetSafeNormal();
			LifeBufTriN(DropBuf, T0, L0, B0, N, Col);
			LifeBufTriN(DropBuf, T0, B0, R0, N, Col);
		}
		else
		{
			// foam lies flat on the water
			const FVector F0 = P + FVector(S, 0, 0), F1 = P + FVector(0, 0, S), F2 = P + FVector(-S, 0, 0), F3 = P + FVector(0, 0, -S);
			LifeBufTriN(DropBuf, F0, F1, F2, Up, Col); LifeBufTriN(DropBuf, F0, F2, F3, Up, Col);
		}
	}
}

// ---------------------------------------------------------------- sea life: schools, sharks, rays, turtles, manatees, dolphins (JS 2487-2600)
int32 AKeysLife::FreeSlots(const FKeysVis* V) const { return V->Cap() - Used.FindRef(V); }

bool AKeysLife::HasGroup(int32 Id) const
{
	for (const FKeysSchool& G : Schools) if (G.Id == Id) return true;
	return false;
}

const FKeysHotspot* AKeysLife::FindHotspot(int32 Id) const
{
	for (const FKeysHotspot& H : Hotspots) if (H.Id == Id) return &H;
	return nullptr;
}

FKeysSchool* AKeysLife::SpawnGroupAt(const FString& Type, double X, double Z, double H, bool bIgnoreCap)
{
	const FKeysVis* V = LifeVis(Type);
	if (!V) return nullptr;
	const double D = DepthAt(X, Z);
	if (D < V->DMin || D > V->DMax || ShoreInfo(X, Z).D < 4) return nullptr;
	const int32 N = V->SMin + FMath::RandRange(0, V->SMax - V->SMin);
	if (!bIgnoreCap && FreeSlots(V) < N) return nullptr;
	FKeysSchool& G = Schools.AddDefaulted_GetRef();
	G.Id = NextId++;
	G.Type = Type;
	G.V = V;
	G.CX = X; G.CZ = Z;
	G.H = H > 1e8 ? FMath::FRand() * PI * 2 : H;
	G.Ph = FMath::FRand() * 100;
	G.DvT = LRand(4, 14);
	const double Of = OffshoreF(X, Z);
	for (int32 i = 0; i < N; i++)
	{
		const double A = FMath::FRand() * PI * 2, R = FMath::Sqrt(FMath::FRand()) * V->Spread;
		FKeysSwimmer M;
		M.OX = FMath::Cos(A) * R; M.OZ = FMath::Sin(A) * R; M.OY = LRand(-1, 1); M.Ph = FMath::FRand() * 100;
		const double Rr = LRand(.8, 1.15);
		M.S = LifeVSC * Rr * (V->DMin >= 25 ? 1 + .45 * Of : 1);
		M.SizeU = (Rr - .8) / .35;
		M.ActT = LRand(1, 8);
		M.Shade = LRand(.84, 1.08);   // no two fish quite the same shade
		G.Members.Add(M);
	}
	Used.FindOrAdd(V) += N;
	return &G;
}

void AKeysLife::FreeGroup(FKeysSchool& G)
{
	if (int32* U = Used.Find(G.V)) *U = FMath::Max(0, *U - G.Members.Num());
	G.Members.Reset();
}

void AKeysLife::TrySpawn(double RMin)
{
	FString Type;
	double X, Z;
	TArray<const FKeysHotspot*> Near;
	for (const FKeysHotspot& H : Hotspots) if (LHyp(H.X - BoatX, H.Z - BoatZ) < LifePopR) Near.Add(&H);
	if (Near.Num() && FMath::FRand() < .35)
	{
		const FKeysHotspot* H = Near[FMath::RandRange(0, Near.Num() - 1)];
		X = H->X + LRand(-18, 18);
		Z = H->Z + LRand(-18, 18);
		Type = LifeWeighted(KeysData::ZoneTable(H->Zone));
	}
	else
	{
		const double A = FMath::FRand() * PI * 2, R = LRand(RMin, LifePopR);
		X = BoatX + FMath::Cos(A) * R;
		Z = BoatZ + FMath::Sin(A) * R;
		if (X < WBX0 || X > WBX1 || Z < WBZ0 || Z > WBZ1) return;
		Type = LifeWeighted(LifeZoneLife(ZoneAt(X, Z)));
		// the web game's Humps life table: pilot whales hang round the offshore humps
		if (const FHump* Hm = NearHump(X, Z); Hm && !Hm->bPatch && FMath::FRand() < .35)
		{
			static const TArray<FZoneRow> Humps{{TEXT("spermwhale"), 0.3}, {TEXT("warsaw"), 1.2}, {TEXT("mako"), 0.35}, {TEXT("amberjack"), 4}, {TEXT("grouper"), 2.5}, {TEXT("gag"), 1.5}, {TEXT("redgrouper"), 1.5}, {TEXT("cobia"), 2}, {TEXT("kingfish"), 1.5}, {TEXT("pilotwhale"), 2.6}, {TEXT("vermilion"), 3}, {TEXT("bluerunner"), 3}, {TEXT("skipjack"), 2}};
			Type = LifeWeighted(Humps);
		}
	}
	SpawnGroupAt(Type, X, Z);
}

bool AKeysLife::DebugSpawn(const FString& Type, double X, double Z) { return SpawnGroupAt(Type, X, Z, 1e9, true) != nullptr; }

void AKeysLife::SpawnTunaNear(double X, double Z)
{
	const TArray<FZoneRow> L{{TEXT("yellowfin"), 5}, {TEXT("albacore"), 2.5}, {TEXT("bluefin"), 1.2 * (1 + 4 * OffshoreF(X, Z))}};
	const FString Type = LifeWeighted(L);
	for (int32 k = 0; k < 6; k++)
	{
		const double A = FMath::FRand() * PI * 2, R = LRand(170, 240), GX = X + FMath::Cos(A) * R, GZ = Z + FMath::Sin(A) * R;
		if (GX < WBX0 || GX > WBX1 || GZ < WBZ0 || GZ > WBZ1) continue;
		if (FKeysSchool* G = SpawnGroupAt(Type, GX, GZ, FMath::Atan2(-(X - GX), -(Z - GZ))))
		{
			G->KeepT = 60;
			Toast(TEXT("Birds are working in the distance - something big is busting bait."));
			return;
		}
	}
}

bool AKeysLife::AnchorPos(const FKeysSchool& G, double& OX, double& OZ) const
{
	if (G.AnchorKind == 1 && Rigs.IsValidIndex(G.AnchorRef)) { OX = Rigs[G.AnchorRef].X; OZ = Rigs[G.AnchorRef].Z; return true; }
	if (G.AnchorKind == 2) { if (const FKeysHotspot* H = FindHotspot(G.AnchorRef)) { OX = H->X; OZ = H->Z; return true; } }
	if (G.AnchorKind == 3 && Weeds.IsValidIndex(G.AnchorRef)) { OX = Weeds[G.AnchorRef].X; OZ = Weeds[G.AnchorRef].Z; return true; }
	if (G.AnchorKind == 4) { OX = G.AnchorX; OZ = G.AnchorZ; return true; }
	return false;
}

// schools that hang on the weeds, the rigs and in the frenzy instead of wandering off (JS 2145)
void AKeysLife::EnsureAnchors()
{
	struct FA { uint8 Kind; int32 Ref; double X, Z, R; const TArray<FZoneRow>* Types; int32* Grp; };
	TArray<FA> L;
	for (int32 i = 0; i < Rigs.Num(); i++) L.Add({uint8(1), i, Rigs[i].X, Rigs[i].Z, Rigs[i].bTwin ? 14.0 : 24.0, &LifeRigSchools(), &Rigs[i].GrpId});
	for (FKeysHotspot& H : Hotspots) L.Add({uint8(2), H.Id, H.X, H.Z, 10.0, &KeysData::ZoneTable(H.Zone), &H.GrpId});
	for (int32 i = 0; i < Weeds.Num(); i++) L.Add({uint8(3), i, Weeds[i].X, Weeds[i].Z, FMath::Max(12.0, Weeds[i].Len * .3), &LifeWeedSchools(), &Weeds[i].GrpId});
	for (const FA& A : L)
	{
		if (LHyp(A.X - BoatX, A.Z - BoatZ) > 340) continue;
		if (*A.Grp >= 0 && HasGroup(*A.Grp)) continue;
		const double D = DepthAt(A.X, A.Z);
		TArray<FZoneRow> Ok;
		for (const FZoneRow& Row : *A.Types)
			if (const FKeysVis* V = LifeVis(Row.Key)) if (D >= V->DMin && D <= V->DMax) Ok.Add(Row);
		if (!Ok.Num()) continue;
		for (int32 Tr = 0; Tr < 3; Tr++)
			if (FKeysSchool* G = SpawnGroupAt(LifeWeighted(Ok), A.X + LRand(-6, 6), A.Z + LRand(-6, 6)))
			{
				G->AnchorKind = A.Kind; G->AnchorRef = A.Ref; G->AnchorR = A.R; G->KeepT = 30;
				*A.Grp = G->Id;
				break;
			}
	}
}

void AKeysLife::ManagePopulation(double RMin)
{
	for (int32 i = Schools.Num() - 1; i >= 0; i--)
	{
		FKeysSchool& G = Schools[i];
		if (G.KeepT > 0) G.KeepT -= .4;
		if (!(G.KeepT > 0) && LHyp(G.CX - BoatX, G.CZ - BoatZ) > LifePopR + 40) { FreeGroup(G); Schools.RemoveAt(i); }
	}
	for (int32 k = 0; k < 8 && Schools.Num() < LifePopTarget; k++) TrySpawn(RMin);
	EnsureAnchors();
}

void AKeysLife::UpdateCreatures(double Dt, double T)
{
	PopT -= Dt;
	if (PopT <= 0) { PopT = .4; ManagePopulation(70); }
	NearT -= Dt;
	if (NearT <= 0) { NearT = .5; UpdateNearField(); }
	const double Bs = BoatSpd;
	const FLinearColor Water = LLin(FLinearColor(.03f, .22f, .30f));
	for (FKeysSchool& G : Schools)
	{
		const FKeysVis* V = G.V;
		if (!V) continue;
		const double Bdx = G.CX - BoatX, Bdz = G.CZ - BoatZ, Bd = LHyp(Bdx, Bdz);
		// spooked by the boat
		if ((Bs > 2 && Bd < 14 + Bs * .6) || Bd < 6)
		{
			if (G.Flee <= 0 && V->Act == EKeysAct::Glide && G.Glide <= 0) { G.Glide = LRand(1.6, 3.4); for (const FKeysSwimmer& M : G.Members) Splash(G.CX + M.OX, G.CZ + M.OZ, 2, .4); }
			G.Flee = 1.5;
			G.FleeH = FMath::Atan2(-Bdx, -Bdz);
		}
		// a diver finning hard, or a shaft hissing past, spooks fish by species class (web entities/fish/behavior.ts)
		if (bDiver || Spooks.Num())
		{
			const FLifeFlee FP = LifeFleeParams(G.Type, V);
			double BestD = 1e9, TX = 0, TZ = 0;
			if (bDiver)
			{
				const double Dd = LHyp(G.CX - DiverX, G.CZ - DiverZ);
				const double K = LClamp((DiverSpeed - .5) / (2 - .5), 0, 1);
				if (Dd < 1.6 + (FP.DiverR - 1.6) * K) { BestD = Dd; TX = DiverX; TZ = DiverZ; }
			}
			for (const FVector2D& Sp : Spooks)
			{
				const double Dd = LHyp(G.CX - Sp.X, G.CZ - Sp.Y);
				if (Dd < FP.SpearR && Dd < BestD) { BestD = Dd; TX = Sp.X; TZ = Sp.Y; }
			}
			if (BestD < 1e8)
			{
				G.Flee = 1.5;
				G.FleeH = FMath::Atan2(-(G.CX - TX), -(G.CZ - TZ));
				G.FleeMul = FP.SpeedMul;
			}
		}
		if (V->Act == EKeysAct::Glide && G.Glide <= 0 && FMath::FRand() < Dt * .03) G.Glide = LRand(1.6, 3.4);
		double Sp = V->Speed;
		if (G.Flee > 0) { G.Flee -= Dt; G.H = LAngLerp(G.H, G.FleeH, Dt * 4); Sp *= G.FleeMul > 0 ? G.FleeMul : V->Level == 2 ? 1.8 : 3.2; }
		else { G.FleeMul = 0; G.H += (FMath::Sin(T * .37 + G.Ph) * .35 + FMath::Sin(T * .11 + G.Ph * 2) * .2) * Dt; }
		// tuna run down a bait sitting in blue water
		if (V->IsTuna() && !G.bChased && bBait && G.Members.Num())
		{
			const double Dx = BaitX - G.CX, Dz = BaitZ - G.CZ, Dd = LHyp(Dx, Dz);
			if (Dd < 320)
			{
				G.Flee = 0;
				G.H = LAngLerp(G.H, FMath::Atan2(-Dx, -Dz), Dt * 2);
				Sp = V->Speed * 1.9;
				bTunaClosing = true;
				if (Dd < 9)
				{
					G.Members.RemoveAt(G.Members.Num() - 1);
					if (int32* U = Used.Find(V)) *U = FMath::Max(0, *U - 1);
					Pending.Key = G.Type; Pending.FX = G.CX; Pending.FZ = G.CZ;
					bStrikePending = true;
					bBait = false;
					Splash(BaitX, BaitZ, 26, 1.3);
					G.bChased = true; G.Flee = 2; G.FleeH = G.H;
				}
			}
		}
		if (G.AnchorKind && G.Flee <= 0)
		{
			double AX0, AZ0;
			if (AnchorPos(G, AX0, AZ0))
			{
				const double Ax = G.CX - AX0, Az = G.CZ - AZ0, D = FMath::Max(1e-3, LHyp(Ax, Az)), R = G.AnchorR, Dir = FMath::Fmod(G.Ph, 2.0) < 1 ? 1 : -1;
				const double Rc = LClamp((R - D) / R, -1, 1) * .9, Vx = -Az / D * Dir + Ax / D * Rc, Vz = Ax / D * Dir + Az / D * Rc;
				G.H = LAngLerp(G.H, FMath::Atan2(-Vx, -Vz), Dt * 2);
				Sp *= .75;
			}
			else G.AnchorKind = 0;
		}
		if (G.Glide > 0)
		{
			G.Glide -= Dt;
			Sp = 12;
			if (G.Glide <= 0) { G.Glide = 0; for (const FKeysSwimmer& M : G.Members) Splash(G.CX + M.OX, G.CZ + M.OZ, 3, .5); }
		}
		double Fx = -FMath::Sin(G.H), Fz = -FMath::Cos(G.H);
		const double La = 5 + Sp * 2, Lx = G.CX + Fx * La, Lz = G.CZ + Fz * La, Ld = DepthFast(Lx, Lz);
		if (Ld < V->DMin || Ld > V->DMax * 1.5 || ShoreInfo(Lx, Lz).D < 4)
		{
			if (!G.Turn) G.Turn = FMath::FRand() < .5 ? -1 : 1;
			G.H += G.Turn * 1.8 * Dt;
			Sp *= .6;
			Fx = -FMath::Sin(G.H); Fz = -FMath::Cos(G.H);
		}
		else G.Turn = 0;
		G.CX += Fx * Sp * Dt;
		G.CZ += Fz * Sp * Dt;
		const double D = DepthFast(G.CX, G.CZ), Amp = AmpFor(D), Floor = KeysSea::SeafloorHeightAt(G.CX, G.CZ), Rx = FMath::Cos(G.H), Rz = -FMath::Sin(G.H);
		// dive cycles: sound for a while, then come back up; spooked fish head down
		G.DvT -= Dt;
		if (G.DvT <= 0) { G.DvTarget = G.DvTarget > .3 ? 0 : (D > 3 ? LRand(.4, 1) : 0); G.DvT = G.DvTarget > 0 ? LRand(5, 14) : LRand(6, 18); }
		G.Dv = LLerp(G.Dv, G.Flee > 0 && D > 3 ? .9 : G.DvTarget, FMath::Min(1.0, Dt * .35));
		const bool bOld = V->Kind == EKeysKind::Ray || V->Kind == EKeysKind::Turtle || V->Kind == EKeysKind::Manatee;
		const double Wf = 4 + Sp * 3, Wa = bOld ? .03 : (V->Kind == EKeysKind::Fish ? .04 : .03);
		const bool bDraw = LHyp(G.CX - CamX, G.CZ - CamZ) < 350;
		const FLifeTpl& Tpl = LifeCreatureTpl(V);
		for (FKeysSwimmer& M : G.Members)
		{
			const double Ox = M.OX + FMath::Sin(T * .6 + M.Ph) * .3, Oz = M.OZ + FMath::Cos(T * .5 + M.Ph) * .3;
			const double Mx = G.CX + Rx * Ox + Fx * Oz, Mz = G.CZ + Rz * Ox + Fz * Oz;
			const double Surf = WaveH(Mx, Mz, T, Amp, false);
			double Y;
			if (V->Act == EKeysAct::Blow) Y = Surf - 3.4 - (M.OY + 1) * .6;   // whales cruise at a real depth and come up to blow
			else if (V->Level == 0) Y = Surf - .5 - (M.OY + 1) * .15;
			else if (V->Level == 2) Y = Floor + .25 + (M.OY + 1) * .05;
			else Y = LClamp(LLerp(Floor + .35, Surf - .45, .55 + M.OY * .2), Floor + .2, Surf - .35);
			if (G.Dv > 0 && V->Level != 2) Y = LLerp(Y, FMath::Max(Floor + .5, Surf - FMath::Min(14.0, (Surf - Floor) * .85)), G.Dv);
			double Pitch = 0, Roll = 0;
			if (V->Act != EKeysAct::None && V->Act != EKeysAct::Glide)
			{
				M.ActT -= Dt;
				if (!M.Act && M.ActT <= 0 && G.Flee <= 0 && (V->Act != EKeysAct::Tail || D < 2.6)) { M.Act = uint8(V->Act); M.AP = 0; M.bSpl = false; }
				if (M.Act)
				{
					const EKeysAct A = EKeysAct(M.Act);
					M.AP += Dt / LifeActDur(A);
					const double P = FMath::Min(1.0, M.AP), K = FMath::Sin(P * PI);
					switch (A)
					{
					case EKeysAct::Tail:   // tailing in skinny water
						Pitch = -.95 * K; Y = LLerp(Y, Surf - .08, K);
						if (bDraw && K > .6 && FMath::FRand() < Dt * 4) SpawnDrop(FVector(Mx - Fx * .5, Surf + .05, Mz - Fz * .5), FVector::ZeroVector, 1.2, .25, false);
						break;
					case EKeysAct::Roll: Y = Surf - .9 + K * 1.15; Pitch = FMath::Cos(P * PI) * .7; if (P > .45 && !M.bSpl) { M.bSpl = true; Splash(Mx, Mz, 7, .6); } break;
					case EKeysAct::Bust: Y = Surf - .5 + K * .75; Pitch = FMath::Cos(P * PI) * .6; if (P > .4 && !M.bSpl) { M.bSpl = true; Splash(Mx, Mz, 9, .8); } break;
					case EKeysAct::Breathe: Y = LLerp(Y, Surf - .1, K); Pitch = .22 * K; break;
					case EKeysAct::Porpoise: Y = Surf - .6 + K * 1.35; Pitch = FMath::Cos(P * PI) * .8; if (P > .85 && !M.bSpl) { M.bSpl = true; Splash(Mx, Mz, 6, .6); } break;
					case EKeysAct::Blow:
						// school.ts: a nose-up rise with the blow near the peak, then the flukes break the surface as it sounds
						Y = LLerp(Y, Surf - .25 + .35, K); Pitch = FMath::Cos(P * PI) * .5;
						if (P > .35 && P < .6 && !M.bSpl) { M.bSpl = true; if (AKeysFinal* Fn = AKeysFinal::Get(this)) Fn->Spout(Mx, Surf, Mz); }
						if (P > .82 && M.AP - Dt / LifeActDur(A) <= .82) Splash(Mx, Mz, 9, .7);
						break;
					default: break;
					}
					if (M.AP >= 1) { M.Act = 0; M.ActT = LifeActCooldown(V->Act); }
				}
			}
			if (G.Glide > 0) { Y = Surf + .45 + FMath::Sin(T * 3 + M.Ph) * .12; Pitch = .05; }
			if (V->bFlap) Roll = FMath::Sin(T * 2.2 + M.Ph) * .28;
			else if (V->Kind == EKeysKind::Ray) Roll = FMath::Sin(T * 1.5 + M.Ph) * .05;
			else if (V->bFlat) Roll = PI / 2 + FMath::Sin(T * 1.1 + M.Ph) * .04;   // a flatfish lies over on one side (creatures.ts flat)
			M.Yaw = G.H + FMath::Sin(T * Wf + M.Ph) * (G.Glide > 0 ? 0 : Wa);
			M.WX = Mx; M.WY = Y; M.WZ = Mz; M.Pitch = Pitch; M.Roll = Roll;
			M.Sz = V->Len * M.S;
			if (!bDraw) continue;

			// CPU swim: the JS vertex shader's tail beat (addSwimShader, 2436), plus the deep-water haze
			const FLifeR3 R = FLifeR3::YXZ(Pitch, M.Yaw, Roll);
			const FVector Pos(Mx, Y, Mz);
			const double S = M.S, Swp0 = T * 7.5 + Mx * .37 + Mz * .23;
			// the see-through sea (and the underwater post-process) absorb with depth now, so no painted-in haze
			const float Haze = 0.f;
			const int32 I0 = SeaBuf.V.Num();
			for (int32 i = 0; i < Tpl.P.Num(); i++)
			{
				FVector P = Tpl.P[i];
				const double Sw = Swp0 - Tpl.Wph[i];
				if (Tpl.Wig[i] != 0) P.X += Tpl.Wig[i] * FMath::Sin(Sw);
				if (Tpl.WigY[i] != 0) P.Y += Tpl.WigY[i] * FMath::Sin(Sw * .55);
				SeaBuf.V.Add(LG2U(Pos + R * (P * S)));
				SeaBuf.N.Add(LN2U(R * Tpl.N[i]));
				SeaBuf.UV.Add(FVector2D::ZeroVector);
				FLinearColor Cc = LMix(Tpl.C[i] * float(M.Shade), Water, Haze);
				Cc.A = 1;
				SeaBuf.C.Add(Cc);
			}
			for (const int32 Ix : Tpl.T) SeaBuf.T.Add(I0 + Ix);
			SeaBuf.Sig = SeaBuf.Sig * 31 + uint32(Tpl.P.Num()) * 7 + uint32(V->Kind);
		}
	}
	Spooks.Reset();
}

// ================================================================ fishing hooks
const FKeysWeedline* AKeysLife::NearWeedline(double X, double Z, double Extra) const
{
	for (const FKeysWeedline& W : Weeds)
	{
		const double Dx = X - W.X, Dz = Z - W.Z, CA = FMath::Cos(W.Ang), SA = FMath::Sin(W.Ang);
		const double U = Dx * CA + Dz * SA, V = -Dx * SA + Dz * CA;
		if (FMath::Abs(U) < W.Len / 2 + Extra && FMath::Abs(V) < W.Wid / 2 + Extra) return &W;
	}
	return nullptr;
}

const FKeysRig* AKeysLife::NearRigPtr(double X, double Z, double R) const
{
	for (const FKeysRig& Rg : Rigs) if (!Rg.bTwin && LHyp(Rg.X - X, Rg.Z - Z) < R) return &Rg;
	return nullptr;
}

bool AKeysLife::NearHotspot(double X, double Z, double R) const
{
	for (const FKeysHotspot& H : Hotspots) if (LHyp(H.X - X, H.Z - Z) < R) return true;
	return false;
}

bool AKeysLife::NearWeed(double X, double Z, double Extra) const { return NearWeedline(X, Z, Extra) != nullptr; }
bool AKeysLife::NearRig(double X, double Z, double R) const { return NearRigPtr(X, Z, R) != nullptr; }

FKeysFishSpot AKeysLife::SpotAt(double X, double Z) const
{
	FKeysFishSpot S;
	S.Zone = ZoneAt(X, Z);
	for (const FKeysHotspot& H : Hotspots)
		if (LHyp(H.X - X, H.Z - Z) < 50) { S.HotspotId = H.Id; S.bHot = true; S.bFrenzy = H.Fr > .5; break; }
	if (!S.bHot && NearWeedline(X, Z, 18)) { S.bHot = true; S.Zone = TEXT("Weedline"); }
	if (NearRigPtr(X, Z, 70)) { S.bHot = true; S.Zone = TEXT("Oil Rig"); S.HotspotId = -1; S.bFrenzy = false; }
	else if (KeysWrecks::Near(X, Z)) { S.bHot = true; S.Zone = TEXT("Wreck"); S.HotspotId = -1; S.bFrenzy = false; }
	return S;
}

FString AKeysLife::SpeciesBias(double X, double Z) const { return SpotAt(X, Z).Zone; }

const TArray<FZoneRow>& AKeysLife::FishTable(const FString& Zone)
{
	if (Zone == TEXT("Oil Rig")) return LifeRigFishing();
	if (Zone == TEXT("Weedline")) return LifeWeedFishing();
	if (Zone == TEXT("Wreck")) return LifeWreckFishing();
	return KeysData::ZoneTable(Zone);
}

FString AKeysLife::PickSpecies(const FKeysFishSpot& S, double BobX, double BobZ) const
{
	const FHump* Hump = NearHump(BobX, BobZ);
	const TArray<FZoneRow>& Tbl = FishTable(S.Zone);
	const double Of = S.Zone == TEXT("Offshore") ? OffshoreF(BobX, BobZ) : 0;
	auto Big = [](const FString& K) { return K == TEXT("marlin") || K == TEXT("swordfish") || K == TEXT("bluefin") ? 1.0 : K == TEXT("yellowfin") ? .6 : K == TEXT("wahoo") ? .5 : K == TEXT("sailfish") ? .3 : 0.0; };
	TArray<double> W;
	double Tot = 0;
	for (const FZoneRow& R : Tbl)
	{
		double V = S.bHot && R.W < 2.5 ? R.W * 2.5 : R.W;   // a school evens the odds for the rarer fish
		const double B = Big(R.Key);
		V *= B > 0 ? 1 + 4 * Of * B : 1 - .45 * Of;
		if (Hump && (R.Key == TEXT("amberjack") || R.Key == TEXT("blackfin") || R.Key == TEXT("kingfish"))) V *= 4;
		Tot += V;
		W.Add(V);
	}
	if (!Tbl.Num()) return FString();
	double Rr = FMath::FRand() * Tot;
	for (int32 i = 0; i < Tbl.Num(); i++) { Rr -= W[i]; if (Rr <= 0) return Tbl[i].Key; }
	return Tbl[0].Key;
}

void AKeysLife::OnCastLanded(const FKeysFishSpot& S, double BobX, double BobZ)
{
	if (S.Zone == TEXT("Offshore") && FMath::FRand() < .6) SpawnTunaNear(BobX, BobZ);
}

void AKeysLife::OnCatchLanded(const FKeysFishSpot& S)
{
	if (S.HotspotId < 0) return;
	for (int32 i = 0; i < Hotspots.Num(); i++)
	{
		if (Hotspots[i].Id != S.HotspotId) continue;
		if (--Hotspots[i].Fish <= 0)
		{
			const FString Zone = Hotspots[i].Zone;
			Hotspots.RemoveAt(i);
			SpawnHotspot(Zone);
			Toast(TEXT("The birds moved on. Check your chart for a new school."));
		}
		return;
	}
}

void AKeysLife::SetBait(bool bActive, double BobX, double BobZ)
{
	if (!bActive) bStrikePending = false;   // the line came in before the pawn picked the strike up
	bBait = bActive && !bStrikePending;
	BaitX = BobX;
	BaitZ = BobZ;
}

bool AKeysLife::PollTunaStrike(FKeysTunaStrike& Out)
{
	if (!bStrikePending) return false;
	Out = Pending;
	bStrikePending = false;
	return true;
}

double AKeysLife::TunaWeight(const KeysData::FSpecies& S, double BobX, double BobZ)
{
	const double W = S.Min + (S.Max - S.Min) * FMath::Min(1.0, FMath::Pow(double(FMath::FRand()), 1.7 - 1.2 * OffshoreF(BobX, BobZ)));
	return FMath::RoundToDouble(W * 10) / 10;
}

FString AKeysLife::AreaLabel(double X, double Z) const
{
	if (NearRigPtr(X, Z, 90)) return TEXT("Oil Rig");
	if (NearWeedline(X, Z, 30)) return TEXT("Weedline");
	if (const KeysWrecks::FWreck* W = KeysWrecks::Near(X, Z, 60)) return FString::Printf(TEXT("Wreck: %s"), W->Name);
	for (const FKeysHotspot& H : Hotspots) if (H.Fr > .5 && LHyp(H.X - X, H.Z - Z) < 120) return TEXT("Feeding frenzy!");
	return FString();
}

void AKeysLife::GetMarkers(TArray<FKeysMarker>& Out) const
{
	for (const FKeysRig& R : Rigs) if (!R.bTwin) { FKeysMarker M; M.Kind = 0; M.X = R.X; M.Z = R.Z; Out.Add(M); }
	for (const FKeysWeedline& W : Weeds) { FKeysMarker M; M.Kind = 1; M.X = W.X; M.Z = W.Z; M.Ang = W.Ang; M.Len = W.Len; Out.Add(M); }
	for (const KeysWrecks::FWreck& W : KeysWrecks::Wrecks()) { FKeysMarker M; M.Kind = 3; M.X = W.X; M.Z = W.Z; M.Ang = W.Heading; M.Len = W.Len; Out.Add(M); }
	for (const FKeysHotspot& H : Hotspots) { FKeysMarker M; M.Kind = 2; M.X = H.X; M.Z = H.Z; M.Frenzy = H.Fr; Out.Add(M); }
}

void AKeysLife::FishNear(double X, double Z, double R, TArray<FVector>& OutPos, TArray<double>& OutLen) const
{
	for (const FKeysSchool& G : Schools)
	{
		if (FMath::Abs(G.CX - X) > R + 30 || FMath::Abs(G.CZ - Z) > R + 30) continue;
		for (const FKeysSwimmer& M : G.Members)
			if (LHyp(M.WX - X, M.WZ - Z) < R) { OutPos.Add(FVector(M.WX, M.WY, M.WZ)); OutLen.Add(M.Sz); }
	}
}

bool AKeysLife::BuildCreatureMesh(const FString& Key, FKeysMesh& Out, double Scale)
{
	const FKeysVis* V = LifeVis(Key);
	if (!V) return false;
	const FLifeTpl& Tp = LifeCreatureTpl(V);
	const int32 I0 = Out.V.Num();
	// JS frame (nose toward -z) -> FKeysMesh frame (nose along +X, Z up), centimeters like FKeysMesh::AddVert
	for (int32 i = 0; i < Tp.P.Num(); i++)
	{
		const FVector P = Tp.P[i] * Scale, N = Tp.N[i];
		Out.V.Add(FVector(-P.Z, P.X, P.Y) * 100.0);
		Out.N.Add(FVector(-N.Z, N.X, N.Y));
		Out.UV.Add(FVector2D(P.X, P.Y));
		Out.C.Add(Tp.C[i]);
	}
	for (const int32 Ix : Tp.T) Out.T.Add(I0 + Ix);
	return true;
}

// ================================================================ diving
void AKeysLife::SetDiver(bool bOn, double X, double Z, double RelSpeed)
{
	bDiver = bOn; DiverX = X; DiverZ = Z; DiverSpeed = RelSpeed;
}

void AKeysLife::SpookAt(double X, double Z) { Spooks.Add(FVector2D(X, Z)); }

void AKeysLife::UpdateNearField()
{
	// free near-field schools once the diver is well away (or out of the water)
	for (auto It = NearCells.CreateIterator(); It; ++It)
	{
		const int32 CX = int32(It.Key() >> 32), CZ = int32(uint32(It.Key() & 0xffffffff));
		const double D = LHyp((CX + .5) * LifeNearCell - DiverX, (CZ + .5) * LifeNearCell - DiverZ);
		const int32 Idx = Schools.IndexOfByPredicate([&](const FKeysSchool& G) { return G.Id == It.Value(); });
		if (!bDiver || D > LifeNearRadius * 1.3 || Idx == INDEX_NONE)
		{
			if (Idx != INDEX_NONE) { FreeGroup(Schools[Idx]); Schools.RemoveAt(Idx); }
			It.RemoveCurrent();
		}
	}
	if (!bDiver) return;
	static const TMap<FString, double> Density = {{TEXT("Reef"), 1.0}, {TEXT("Bridge"), 1.0}, {TEXT("Hawk Channel"), .55}, {TEXT("Flats"), .5},
		{TEXT("Backcountry"), .4}, {TEXT("Creek"), .4}, {TEXT("Offshore"), .12}};
	const int32 R = FMath::CeilToInt32(LifeNearRadius / LifeNearCell);
	const int32 IX = FMath::FloorToInt32(DiverX / LifeNearCell), IZ = FMath::FloorToInt32(DiverZ / LifeNearCell);
	for (int32 j = -R; j <= R; j++)
		for (int32 i = -R; i <= R; i++)
		{
			const int32 CX = IX + i, CZ = IZ + j;
			const double X0 = (CX + .5) * LifeNearCell, Z0 = (CZ + .5) * LifeNearCell;
			if (LHyp(X0 - DiverX, Z0 - DiverZ) > LifeNearRadius) continue;
			const int64 K = (int64(CX) << 32) ^ int64(uint32(CZ));
			if (NearCells.Contains(K)) continue;
			NearCells.Add(K, -1);   // rolled; -1 if it holds nothing
			const FString Zone = ZoneAt(X0, Z0);
			const double* Dn = Density.Find(Zone);
			if (KeysSea::HashCell(LifeNearSeed, CX, CZ, 9701) >= (Dn ? *Dn : .4)) continue;
			// a minority of cells hold one big fish rather than a shoal
			const bool bBig = KeysSea::HashCell(LifeNearSeed, CX, CZ, 9708) < .22;
			TArray<FZoneRow> Table;
			for (const FZoneRow& Row : LifeZoneLife(Zone))
			{
				const FKeysVis* V = LifeVis(Row.Key);
				if (!V || !KeysData::Species(Row.Key)) continue;
				if (bBig ? V->Len >= .85 : V->SMax >= 4) Table.Add(Row);
			}
			if (!Table.Num())
				for (const FZoneRow& Row : LifeZoneLife(Zone))
					if (const FKeysVis* V = LifeVis(Row.Key)) if (KeysData::Species(Row.Key) && V->SMax >= 4) Table.Add(Row);
			if (!Table.Num()) continue;
			double Tot = 0;
			for (const FZoneRow& Row : Table) Tot += Row.W;
			double Pick = KeysSea::HashCell(LifeNearSeed, CX, CZ, 9702) * Tot;
			FString Type = Table.Last().Key;
			for (const FZoneRow& Row : Table) { Pick -= Row.W; if (Pick <= 0) { Type = Row.Key; break; } }
			const double AX = X0 + (KeysSea::HashCell(LifeNearSeed, CX, CZ, 9704) - .5) * LifeNearCell;
			const double AZ = Z0 + (KeysSea::HashCell(LifeNearSeed, CX, CZ, 9705) - .5) * LifeNearCell;
			if (LandH(AX, AZ) >= 0) continue;
			if (FKeysSchool* G = SpawnGroupAt(Type, AX, AZ, KeysSea::HashCell(LifeNearSeed, CX, CZ, 9706) * PI * 2, true))
			{
				G->AnchorKind = 4; G->AnchorX = AX; G->AnchorZ = AZ;
				G->AnchorR = 3 + 6 * KeysSea::HashCell(LifeNearSeed, CX, CZ, 9707);
				G->KeepT = 1e12;
				G->NearCell = K;
				NearCells[K] = G->Id;
			}
		}
}

void AKeysLife::SpearTargetsNear(double X, double Y, double Z, double R, TArray<KeysDive::FTarget>& Out) const
{
	const double R2 = R * R;
	for (const FKeysSchool& G : Schools)
	{
		const FKeysVis* V = G.V;
		if (!V || LHyp(G.CX - X, G.CZ - Z) > R + 40) continue;
		const KeysData::FSpecies* Sp = KeysData::Species(G.Type);
		if (!Sp) continue;   // ambient-only creatures have no catch
		const bool bCatchable = V->Kind != EKeysKind::Dolphin && V->Kind != EKeysKind::Turtle && V->Kind != EKeysKind::Manatee && !V->bNoCatch;
		const double Fx = -FMath::Sin(G.H), Fz = -FMath::Cos(G.H);
		double Spd = V->Speed;
		if (G.Flee > 0) Spd *= G.FleeMul > 0 ? G.FleeMul : 3.2;
		for (int32 i = 0; i < G.Members.Num(); i++)
		{
			const FKeysSwimmer& M = G.Members[i];
			const FVector C(M.WX, M.WY, M.WZ);
			if ((C - FVector(X, Y, Z)).SizeSquared() > R2) continue;
			// the body as a capsule along the nose-tail axis (web entities/fish/hitbox.ts)
			const double L = M.Sz;
			const double Girth = FMath::Max(V->H > 0 ? V->H * M.S : V->Len * .17 * M.S, V->W > 0 ? V->W * M.S : V->Len * .16 * M.S) / 2;
			const double Rad = FMath::Min(L / 2, FMath::Max(.05 + .015 * L, Girth * 1.4));
			const double Reach = FMath::Max(0.0, L / 2 - Rad);
			const FVector N(-FMath::Sin(M.Yaw) * FMath::Cos(M.Pitch), FMath::Sin(M.Pitch), -FMath::Cos(M.Yaw) * FMath::Cos(M.Pitch));
			KeysDive::FTarget T;
			T.Id = G.Id * 1000 + i;
			T.Key = G.Type;
			T.Weight = FMath::RoundToDouble((Sp->Min + (Sp->Max - Sp->Min) * LClamp(M.SizeU, 0, 1)) * 10) / 10;
			T.A = C + N * Reach;
			T.B = C - N * (Reach + .1 * L);
			T.Radius = Rad;
			T.Vel = FVector(Fx * Spd, 0, Fz * Spd);
			T.bCatchable = bCatchable;
			Out.Add(T);
		}
	}
}

void AKeysLife::TakeSpeared(int32 TargetId)
{
	const int32 Gid = TargetId / 1000, Mi = TargetId % 1000;
	for (FKeysSchool& G : Schools)
		if (G.Id == Gid && G.Members.IsValidIndex(Mi))
		{
			G.Members.RemoveAt(Mi);
			if (int32* U = Used.Find(G.V)) *U = FMath::Max(0, *U - 1);
			G.Flee = 1.5;
			G.FleeH = FMath::Atan2(-(G.CX - DiverX), -(G.CZ - DiverZ));
			return;
		}
}

void AKeysLife::DebugBringSchool(double X, double Z)
{
	FKeysSchool* Best = nullptr;
	double BestD = 1e9;
	for (FKeysSchool& G : Schools)
	{
		if (!G.V || !G.Members.Num() || !KeysData::Species(G.Type) || G.V->Kind == EKeysKind::Dolphin || G.V->Kind == EKeysKind::Turtle || G.V->Kind == EKeysKind::Manatee || G.V->bNoCatch) continue;
		const double D = LHyp(G.CX - X, G.CZ - Z);
		if (D < BestD) { BestD = D; Best = &G; }
	}
	if (!Best) return;
	Best->CX = X; Best->CZ = Z; Best->Flee = 0; Best->FleeMul = 0;
	Best->AnchorKind = 4; Best->AnchorX = X; Best->AnchorZ = Z; Best->AnchorR = 3; Best->KeepT = 1e12;
	UE_LOG(LogTemp, Log, TEXT("KRLife brought a school of %s (%d) to %.0f, %.0f"), *Best->Type, Best->Members.Num(), X, Z);
}
