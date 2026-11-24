#include "KeysBait.h"
#include "KeysMesh.h"
#include "KeysLife.h"
#include "KeysData.h"

namespace KeysBaitImpl
{
	TArray<KeysBait::FBall> GBalls;
	TArray<FVector4> GSonar;

	// sRGB hex to the linear colours the meshes carry (as KeysLife's LLin)
	FLinearColor Lin(uint32 Hex) { return FLinearColor(FColor((Hex >> 16) & 0xff, (Hex >> 8) & 0xff, Hex & 0xff)); }

	// repaint a school body by height on the body: back above, silver flank, white belly, its fins in their own colour
	void Repaint(FKeysMesh& M, uint32 Back, uint32 Flank, uint32 Belly, uint32 Fin)
	{
		double MaxZ = 1e-3, MinX = 1e9, MaxX = -1e9;
		for (const FVector& V : M.V) { MaxZ = FMath::Max(MaxZ, FMath::Abs(V.Z)); MinX = FMath::Min(MinX, V.X); MaxX = FMath::Max(MaxX, V.X); }
		const FLinearColor CB = Lin(Back), CF = Lin(Flank), CW = Lin(Belly), CFin = Lin(Fin);
		M.SyncKinds();
		for (int32 i = 0; i < M.V.Num(); i++)
		{
			if (M.K[i].X == SurfFin) { M.C[i] = CFin; continue; }
			const double V = M.V[i].Z / MaxZ;
			FLinearColor C = V > .1 ? FMath::Lerp(CF, CB, float(FMath::Clamp((V - .1) / .35, 0.0, 1.0))) : FMath::Lerp(CF, CW, float(FMath::Clamp((-V + .1) / .5, 0.0, 1.0)));
			M.C[i] = FLinearColor(C.R, C.G, C.B, 1);
		}
	}

	// a seabird, 1 long (nose +X, back +Z, wings out along Y), both faces of every panel so it shows from below
	void Bird(FKeysMesh& M, bool bFrigate)
	{
		const FLinearColor Up = bFrigate ? Lin(0x17171a) : Lin(0xc4cad0), Under = bFrigate ? Lin(0x1d1d20) : Lin(0xf4f5f6);
		const FLinearColor Body = bFrigate ? Lin(0x141416) : Lin(0xf1f2f3), Cap = bFrigate ? Lin(0x141416) : Lin(0x111111);
		const FLinearColor Bill = bFrigate ? Lin(0x8a8f95) : Lin(0xe07a2a);
		M.Surf = SurfFish;
		// body and head
		M.AddTube(FVector(-.42, 0, 0), FVector(.18, 0, .01), .025, .065, 6, Body);
		M.AddTube(FVector(.18, 0, .01), FVector(.32, 0, .03), .065, .04, 6, Cap);
		M.AddTube(FVector(.32, 0, .03), FVector(bFrigate ? .5 : .46, 0, .02), .014, .004, 4, Bill);
		// wings: inner panel root to wrist, outer panel wrist to tip; the frigatebird's long and bent back at the wrist
		for (int32 s = -1; s <= 1; s += 2)
		{
			const double Sp = bFrigate ? 1.15 : 1.1;
			const FVector R0(.1, s * .04, .02), R1(-.12, s * .04, .02);
			const FVector W0(bFrigate ? .16 : .06, s * .45, bFrigate ? .1 : .14), W1(bFrigate ? -.04 : -.1, s * .45, bFrigate ? .1 : .14);
			const FVector Tip(bFrigate ? -.38 : -.22, s * Sp, bFrigate ? .02 : .06);
			M.AddQuad(R0, W0, W1, R1, Up, FVector(0, 0, 1));
			M.AddQuad(R0, W0, W1, R1, Under, FVector(0, 0, -1));
			M.AddTri(W0, Tip, W1, Up, FVector(0, 0, 1));
			M.AddTri(W0, Tip, W1, Under, FVector(0, 0, -1));
		}
		// the tail: a deep fork on both birds, long scissors on the frigatebird
		const double TL = bFrigate ? .55 : .32, TW = bFrigate ? .1 : .12;
		for (int32 s = -1; s <= 1; s += 2)
		{
			const FVector A(-.36, 0, 0), B(-.36 - TL, s * TW, 0), C(-.36 - TL * .45, s * TW * .25, 0);
			M.AddTri(A, B, C, Up, FVector(0, 0, 1));
			M.AddTri(A, B, C, Under, FVector(0, 0, -1));
		}
		// (AddVert keeps centimetres, as the school bodies)
		M.SyncKinds();
	}
}

void KeysBait::Publish(TArray<FBall>&& Balls) { KeysBaitImpl::GBalls = MoveTemp(Balls); }
const TArray<KeysBait::FBall>& KeysBait::Balls() { return KeysBaitImpl::GBalls; }
void KeysBait::PublishSonar(TArray<FVector4>&& Fish) { KeysBaitImpl::GSonar = MoveTemp(Fish); }

FString KeysBait::CatchKey(const FString& K)
{
	static const TSet<FString> Caught = {TEXT("blackfin"), TEXT("skipjack"), TEXT("mahi"), TEXT("sailfish"), TEXT("kingfish"), TEXT("wahoo"), TEXT("bluerunner"), TEXT("jackcrevalle")};
	return Caught.Contains(K) ? K : FString();
}

namespace KeysBaitImpl
{
	const KeysBait::FBall* WorkedNear(double X, double Z)
	{
		const KeysBait::FBall* Best = nullptr;
		double BD = 1e9;
		for (const KeysBait::FBall& B : GBalls)
		{
			if (B.Work < .25) continue;
			const double D = FMath::Sqrt(FMath::Square(B.X - X) + FMath::Square(B.Z - Z));
			if (D < B.R + 30 && D < BD) { BD = D; Best = &B; }
		}
		return Best;
	}
}

FString KeysBait::PickAt(double X, double Z, double Rand)
{
	const FBall* B = KeysBaitImpl::WorkedNear(X, Z);
	if (!B) return FString();
	// most bites at a worked ball are the fish working it; the rest are whatever else the water holds
	if (Rand > .5 + .4 * B->Work) return FString();
	// how readily each takes a bait or a lure in the middle of it (a sailfish is pickier than a skipjack)
	auto Bite = [](const FString& K) { return K == TEXT("sailfish") ? .5 : K == TEXT("wahoo") ? .7 : K == TEXT("mahi") ? 1.3 : 1.0; };
	TArray<TPair<FString, double>> Rows;
	double Tot = 0;
	for (const TPair<FString, int32>& P : B->Preds)
	{
		const FString K = CatchKey(P.Key);
		if (K.IsEmpty() || !KeysData::Species(K)) continue;
		// (a pack of twelve does not bite twelve times as often as a single sailfish: the first fish to the bait takes it)
		const double W = Bite(K) * FMath::Sqrt(double(FMath::Max(1, P.Value)));
		Rows.Add({K, W});
		Tot += W;
	}
	if (Tot <= 0) return FString();
	double R = FMath::FRand() * Tot;
	for (const TPair<FString, double>& Rw : Rows) { R -= Rw.Value; if (R <= 0) return Rw.Key; }
	return Rows.Last().Key;
}

bool KeysBait::WorkedAt(double X, double Z, bool* bFrenzy)
{
	const FBall* B = KeysBaitImpl::WorkedNear(X, Z);
	if (bFrenzy) *bFrenzy = B && B->Work > .6;
	return B != nullptr;
}

void KeysBait::SonarNear(double X, double Z, double R, TArray<FVector>& OutPos, TArray<double>& OutLen)
{
	for (const FVector4& F : KeysBaitImpl::GSonar)
		if (FMath::Square(F.X - X) + FMath::Square(F.Z - Z) < R * R) { OutPos.Add(FVector(F.X, F.Y, F.Z)); OutLen.Add(F.W); }
}

bool KeysBait::BuildMesh(const FString& Key, FKeysMesh& Out, int32 Lod)
{
	using namespace KeysBaitImpl;
	// Ballyhoo (Hemiramphus brasiliensis): slender, 20-35 cm, dark blue-green back, silver sides, white belly
	if (Key == TEXT("ballyhoo")) { if (!AKeysLife::BuildSchoolMesh(TEXT("barracuda"), Out, Lod)) return false; Repaint(Out, 0x24465c, 0xbfcad2, 0xf0f2f3, 0x7d8e99); return true; }
	// Spanish sardine (Sardinella aurita): 12-20 cm, greenish-blue back, bright silver sides
	if (Key == TEXT("sardine")) { if (!AKeysLife::BuildSchoolMesh(TEXT("herring"), Out, Lod)) return false; Repaint(Out, 0x3a6a6c, 0xcdd6da, 0xf1f4f5, 0xb7c3c4); return true; }
	// Round scad, the cigar minnow (Decapterus punctatus): 15-20 cm, a small slim jack, blue-olive back, silver below
	if (Key == TEXT("scad")) { if (!AKeysLife::BuildSchoolMesh(TEXT("bluerunner"), Out, Lod)) return false; Repaint(Out, 0x4b6b78, 0xc9d1d4, 0xeef1f1, 0xc4cccc); return true; }
	// Silky shark (Carcharhinus falciformis): 1.8-2.5 m here, bronze-grey back, white belly, no black fin tips
	if (Key == TEXT("silky")) { if (!AKeysLife::BuildSchoolMesh(TEXT("blacktip"), Out, Lod)) return false; Repaint(Out, 0x66625a, 0xa29d94, 0xecedec, 0x5b5852); return true; }
	// Royal or sandwich tern; magnificent frigatebird
	if (Key == TEXT("tern") || Key == TEXT("frigate")) { Bird(Out, Key == TEXT("frigate")); return true; }
	return false;
}
