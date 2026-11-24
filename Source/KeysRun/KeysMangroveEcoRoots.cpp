// The life in and on the red mangrove's roots (the root life: part of the ecosystem, KeysMangroveEco.cpp).
//
// A real Florida Keys red mangrove edge is a crowded nursery. Below the high-tide line the prop roots carry a crust of
// barnacles at the top of the tide, mangrove and flat tree oysters through the middle of it, and under the low-tide line,
// where the roots never dry, fire sponge (orange and red), colonial tunicates (purple and blue), a green fuzz of algae
// and an anemone here and there; it is thickest where a creek's current runs past and thin on a shallow flat. In the
// water between the roots: clouds of silversides and pilchards, bands of juvenile grunts and snappers (gray,
// schoolmaster, lane) hanging in the shade, sergeant majors at the edge, snook lying under the outer roots nose to the
// current, gray snapper in loose groups just inside the root line, a small barracuda hanging off it.
//
//  - PlanRootLife: the rules for the fish, by tree (the tile's plan, off the game thread, KeysMangroveEco.cpp)
//  - UpdateGrowth: the growth, as instances, laid along the roots exactly as KeysMangrove.cpp draws them
//    (KeysMangroveEco::RootPieces), only within 44 m of the camera and again when it has moved 7 m: nothing of it goes
//    into the tile mesh, which is what the trees cost when they arrive
//  - FindWall / SafeEye: the test camera (eco:look roots, eco:dive roots, and every eco:look under the surface)
//
// -KRNoRootLife leaves all of it out (KeysMangroveEco::RootLife()).
#include "KeysMangroveEcoInt.h"
#include "KeysGeo.h"
#include "KeysGeoReal.h"
#include "KeysMesh.h"
#include "KeysTown.h"
#include "KeysWorld.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"
#include "Misc/CommandLine.h"

namespace KeysMangroveEco
{
	bool RootLife()
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("KRNoRootLife"));
		return !bOff && Enabled();
	}

	// the sea's height at the top and the bottom of the tide (KeysGeo: the tide is .32 m either side of the chart)
	static constexpr double HighW = .32, LowW = -.32;

	enum EGrow : uint8 { GBarnacle, GOyster, GSpongeRed, GSpongeOrange, GTunicPurple, GTunicBlue, GAlgae, GAnemone };
	const TCHAR* GrowName(int32 G)
	{
		static const TCHAR* N[NumGrow] = {TEXT("barnacles"), TEXT("oysters"), TEXT("fire sponge red"), TEXT("fire sponge orange"), TEXT("tunicates purple"), TEXT("tunicates blue"),
			TEXT("green algae"), TEXT("anemone")};
		return G >= 0 && G < NumGrow ? N[G] : TEXT("?");
	}
	// a sleeve is laid along the root (scaled to the piece's length along X); a cluster stands on it (scaled evenly)
	static bool IsSleeve(int32 G) { return G == GBarnacle; }

	namespace
	{
		FLinearColor RHex(uint32 C) { return KeysTownPaint::Hex(C); }
		double RHash(double A, double B) { return EHash(A, B); }
		FVector2D Norm2(const FVector2D& V, const FVector2D& Else) { const double L = V.Size(); return L > 1e-3 ? V / L : Else; }

		// ---------------------------------------------------------------- the shapes (in root radii: 1 is the root's own)
		// a ring-built sleeve round the root, X from -.5 to .5; Rad(u, a) its radius, Col(u, a, rad) its colour
		void Sleeve(FKeysMesh& M, int32 NU, int32 NA, TFunctionRef<double(double, double)> Rad, TFunctionRef<FLinearColor(double, double, double)> Col)
		{
			const int32 V0 = M.V.Num();
			for (int32 i = 0; i <= NU; i++)
			{
				const double U = double(i) / NU;
				for (int32 j = 0; j < NA; j++)
				{
					const double A = 2 * PI * j / NA;
					// (the ends hug the root, so it reads as grown on rather than slid over)
					const double End = FMath::Min(1.0, FMath::Min(U, 1 - U) * 6);
					const double R = FMath::Lerp(1.05, Rad(U, A), End);
					M.AddVert(FVector(U - .5, FMath::Cos(A) * R, FMath::Sin(A) * R), FVector((R - 1) * .0, FMath::Cos(A), FMath::Sin(A)), Col(U, A, R));
				}
			}
			for (int32 i = 0; i < NU; i++)
				for (int32 j = 0; j < NA; j++)
				{
					const int32 A = V0 + i * NA + j, B = V0 + i * NA + (j + 1) % NA, C = V0 + (i + 1) * NA + (j + 1) % NA, D = V0 + (i + 1) * NA + j;
					M.T.Append({A, B, C, A, C, D});
				}
		}
		// a low ellipsoid at C with half-axes along Ax (long), Ay, Az
		void Blob(FKeysMesh& M, const FVector& C, const FVector& Ax, const FVector& Ay, const FVector& Az, int32 NA, int32 NR, TFunctionRef<FLinearColor(double)> Col)
		{
			const int32 V0 = M.V.Num();
			for (int32 r = 0; r <= NR; r++)
			{
				const double La = -PI / 2 + PI * r / NR, CL = FMath::Cos(La), SL = FMath::Sin(La);
				for (int32 j = 0; j < NA; j++)
				{
					const double A = 2 * PI * j / NA;
					const FVector D(SL, CL * FMath::Cos(A), CL * FMath::Sin(A));
					M.AddVert(C + Ax * D.X + Ay * D.Y + Az * D.Z, (Ax.GetSafeNormal() * D.X + Ay.GetSafeNormal() * D.Y + Az.GetSafeNormal() * D.Z).GetSafeNormal(), Col(.5 + .5 * SL));
				}
			}
			for (int32 r = 0; r < NR; r++)
				for (int32 j = 0; j < NA; j++)
				{
					const int32 A = V0 + r * NA + j, B = V0 + r * NA + (j + 1) % NA, Cc = V0 + (r + 1) * NA + (j + 1) % NA, D = V0 + (r + 1) * NA + j;
					M.T.Append({A, B, Cc, A, Cc, D});
				}
		}
		FVector Rad(double A, double R, double X = 0) { return FVector(X, FMath::Cos(A) * R, FMath::Sin(A) * R); }

		void BuildGrow(int32 G, FKeysMesh& M)
		{
			M.Surf = G == GBarnacle || G == GOyster ? SurfConcrete : SurfCloth;
			const double Sd = G * 7.31;
			auto Noise = [Sd](double U, double A, double F) { return FMath::Sin(U * 9 * F + A * 3 + Sd) * FMath::Cos(A * 2 * F + U * 5 + Sd * 1.7); };
			// (every piece is small and sits on one side of the root, +Y, its foot on the bark at about one root radius: the
			// crust is built of many of them, scattered round and along the root, with bark showing between)
			if (G == GBarnacle)
			{
				// barnacles: a thin, pale, rough band, speckled dark where the shells' mouths are
				Sleeve(M, 2, 7, [&](double U, double A) { return 1.1 + .06 * Noise(U, A, 2.3); },
					[&](double U, double A, double R) { return RHash(U * 13 + Sd, A * 7) < .35 ? RHex(0x5e5a50) : FMath::Lerp(RHex(0xa8a292), RHex(0xc8c2b2), float(RHash(A * 3, U * 11))); });
			}
			else if (G == GOyster)
			{
				// a small clump of mangrove and flat tree oysters: three shells standing off the bark at odd angles, pale lips,
				// purple-black insides showing on the flat ones
				for (int32 k = 0; k < 3; k++)
				{
					const double A = PI / 2 + (RHash(k, 1.0) - .5) * 2.2, X = (RHash(k, 2.0) - .5) * 1.4, Tilt = (RHash(k, 3.0) - .5) * 1.2;
					const FVector Out = Rad(A, 1);
					const FVector Long = (Out * FMath::Cos(Tilt) + FVector(FMath::Sin(Tilt), 0, 0)).GetSafeNormal();
					const FVector Side = FVector::CrossProduct(Long, FVector(1, 0, 0)).GetSafeNormal(.01, FVector(0, 0, 1));
					const FVector Thin = FVector::CrossProduct(Long, Side).GetSafeNormal();
					const double Ls = .9 + .7 * RHash(k, 4.0);
					const uint32 Shell = k == 0 ? 0x5c4e56 : k == 1 ? 0x8f8878 : 0x9e9684;
					Blob(M, FVector(X, 0, 0) + Out * (.9 + Ls * .5), Long * Ls * .55, Side * Ls * .4, Thin * .25, 5, 2,
						[Shell](double T) { return FMath::Lerp(RHex(Shell) * .75f, RHex(0xc8c2b2), float(FMath::Square(T))); });
				}
			}
			else if (G == GSpongeRed || G == GSpongeOrange)
			{
				// fire sponge: a few soft irregular lobes grown together on the bark, darker in the folds, one raised vent
				const FLinearColor Hi = RHex(G == GSpongeRed ? 0xb8301e : 0xd8641e), Lo = RHex(G == GSpongeRed ? 0x6a1a12 : 0x8a4012);
				for (int32 k = 0; k < 3; k++)
				{
					const double A = PI / 2 + (RHash(k, Sd) - .5) * 1.8, X = (RHash(k + 3.0, Sd) - .5) * 1.5, Rr = .45 + .4 * RHash(k + 5.0, Sd);
					const FVector Out = Rad(A, 1);
					Blob(M, FVector(X, 0, 0) + Out * (.85 + Rr * .5), Out * Rr * .8, Rad(A + PI / 2, Rr), FVector(Rr * 1.2, 0, 0), 5, 2,
						[Hi, Lo](double T) { return FMath::Lerp(Lo, Hi, float(FMath::Clamp(T * 1.6 - .3, 0.0, 1.0))); });
				}
				Blob(M, FVector(.1, 0, 0) + Rad(PI / 2, 1.75), Rad(PI / 2, .35), Rad(PI, .2), FVector(.2, 0, 0), 4, 2, [Hi, Lo](double T) { return T > .85 ? Lo * .4f : Hi; });
			}
			else if (G == GTunicPurple || G == GTunicBlue)
			{
				// colonial tunicates: a little bunch of beads standing out of the bark, each with a pale ring at its mouth
				const FLinearColor Body = RHex(G == GTunicPurple ? 0x6c3c96 : 0x3a58b8), Tip = RHex(G == GTunicPurple ? 0xc4a0e4 : 0xa4c4f4);
				for (int32 k = 0; k < 5; k++)
				{
					const double A = PI / 2 + (RHash(k, Sd + 1) - .5) * 1.8, X = (RHash(k, Sd + 2) - .5) * 1.2;
					const FVector Out = Rad(A, 1), Side = Rad(A + PI / 2, 1), Ax(1, 0, 0);
					const double H = .45 + .35 * RHash(k, Sd + 3);
					Blob(M, FVector(X, 0, 0) + Out * (.9 + H * .5), Out * H * .5, Side * .22, Ax * .22, 4, 2, [Body, Tip](double T) { return T > .8 ? Tip : FMath::Lerp(Body * .7f, Body, float(T)); });
				}
			}
			else if (G == GAlgae)
			{
				// a tuft of fine green blades
				for (int32 k = 0; k < 8; k++)
				{
					const double A = PI / 2 + (RHash(k, 11.0) - .5) * 1.6, X = (RHash(k, 12.0) - .5) * .8, Ls = .6 + .9 * RHash(k, 13.0), W = .05;
					const FVector B0 = FVector(X, 0, 0) + Rad(A, .95), Tip = FVector(X + (RHash(k, 14.0) - .5) * .5, 0, 0) + Rad(A + (RHash(k, 15.0) - .5) * .7, 1.0 + Ls);
					const FVector Sd2 = FVector(W, 0, 0);
					const FLinearColor Cb = RHex(0x3c6a26), Ct = RHex(0x8ab84c);
					const FVector Nn = FVector::CrossProduct(Tip - B0, Sd2).GetSafeNormal();
					const int32 I0 = M.AddVert(B0 - Sd2, Nn, Cb), I1 = M.AddVert(B0 + Sd2, Nn, Cb), I2 = M.AddVert(Tip, Nn, Ct);
					const int32 J0 = M.AddVert(B0 - Sd2, -Nn, Cb), J1 = M.AddVert(B0 + Sd2, -Nn, Cb), J2 = M.AddVert(Tip, -Nn, Ct);
					M.T.Append({I0, I1, I2, J0, J2, J1});
				}
			}
			else
			{
				// an anemone on the root (Aiptasia, Bartholomea): a tan column and a ring of pale tentacles tipped lilac
				const FLinearColor Col = RHex(0x8a7a5c), Ten = RHex(0xd8ceb0), TipC = RHex(0xb48cd0);
				Blob(M, FVector(0, 1.7, 0), FVector(0, .8, 0), FVector(.55, 0, 0), FVector(0, 0, .55), 5, 2, [Col](double T) { return FMath::Lerp(Col * .7f, Col, float(T)); });
				for (int32 k = 0; k < 8; k++)
				{
					const double A = 2 * PI * k / 8 + .2 * RHash(k, 21.0), Ls = 1.1 + .7 * RHash(k, 22.0);
					const FVector Base(FMath::Cos(A) * .45, 2.4, FMath::Sin(A) * .45), Dir = FVector(FMath::Cos(A) * .8, .75 + .3 * RHash(k, 23.0), FMath::Sin(A) * .8).GetSafeNormal();
					const FVector S1 = FVector::CrossProduct(Dir, FVector(0, 1, 0)).GetSafeNormal(.01, FVector(1, 0, 0)), S2 = FVector::CrossProduct(Dir, S1).GetSafeNormal();
					Blob(M, Base + Dir * Ls * .5, Dir * Ls * .5, S1 * .12, S2 * .12, 3, 2, [Ten, TipC](double T) { return T > .75 ? TipC : Ten; });
				}
			}
			M.SyncKinds();
			for (int32 i = 0; i < M.K.Num(); i++) M.K[i].X = M.Surf;
		}

		// a vertex-coloured static mesh from an FKeysMesh (no normalising: one root radius is a metre at scale 1)
		UStaticMesh* GrowMesh(AKeysMangroveEco* A, const FKeysMesh& M, int32 G, int32& OutTris)
		{
			FMeshDescription MD;
			FStaticMeshAttributes Attr(MD);
			Attr.Register();
			TVertexAttributesRef<FVector3f> Pos = Attr.GetVertexPositions();
			TVertexInstanceAttributesRef<FVector3f> Nrm = Attr.GetVertexInstanceNormals();
			TVertexInstanceAttributesRef<FVector4f> Col = Attr.GetVertexInstanceColors();
			TVertexInstanceAttributesRef<FVector2f> UVs = Attr.GetVertexInstanceUVs();
			UVs.SetNumChannels(2);
			const FPolygonGroupID Grp = MD.CreatePolygonGroup();
			Attr.GetPolygonGroupMaterialSlotNames()[Grp] = FName(TEXT("Mat"));
			TArray<FVertexInstanceID> Inst;
			Inst.SetNum(M.V.Num());
			for (int32 i = 0; i < M.V.Num(); i++)
			{
				const FVertexID V = MD.CreateVertex();
				Pos[V] = FVector3f(M.V[i]);
				const FVertexInstanceID I = MD.CreateVertexInstance(V);
				Nrm[I] = FVector3f(M.N[i]);
				Col[I] = FVector4f(M.C[i].R, M.C[i].G, M.C[i].B, 1);
				UVs.Set(I, 0, FVector2f(0, 0));
				UVs.Set(I, 1, FVector2f(M.K.IsValidIndex(i) ? M.K[i] : FVector2D::ZeroVector));
				Inst[i] = I;
			}
			OutTris = 0;
			for (int32 i = 0; i + 2 < M.T.Num(); i += 3)
			{
				const int32 Va = M.T[i], Vb = M.T[i + 1], Vc = M.T[i + 2];
				const FVector Fn = FVector::CrossProduct(M.V[Vb] - M.V[Va], M.V[Vc] - M.V[Va]);
				if (Fn.SizeSquared() < 1e-12) continue;
				const bool bFlip = FVector::DotProduct(Fn, M.N[Va] + M.N[Vb] + M.N[Vc]) > 0;
				MD.CreateTriangle(Grp, {Inst[Va], Inst[bFlip ? Vc : Vb], Inst[bFlip ? Vb : Vc]});
				OutTris++;
			}
			UMaterialInterface* Mat = AKeysWorld::VertexColorMaterial();
			UStaticMesh* SM = NewObject<UStaticMesh>(A, *FString::Printf(TEXT("EcoGrow_%d"), G));
			SM->GetStaticMaterials().Add(FStaticMaterial(Mat, FName(TEXT("Mat"))));
			UStaticMesh::FBuildMeshDescriptionsParams Bp;
			Bp.bFastBuild = true;
			Bp.bMarkPackageDirty = false;
			Bp.bBuildSimpleCollision = false;
			SM->BuildFromMeshDescriptions({&MD}, Bp);
			SM->CalculateExtendedBounds();
			return SM;
		}

		int32 GGrowTris[NumGrow] = {};
		bool EnsureGrowPools(AKeysMangroveEco* A)
		{
			if (A->GrowPools.Num() == NumGrow) return true;
			const double T0 = FPlatformTime::Seconds();
			FString Tri;
			for (int32 G = 0; G < NumGrow; G++)
			{
				FKeysMesh M;
				BuildGrow(G, M);
				UStaticMesh* SM = GrowMesh(A, M, G, GGrowTris[G]);
				A->Meshes.Add(SM);
				UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(A, *FString::Printf(TEXT("EcoGrowPool_%d"), G));
				C->SetStaticMesh(SM);
				C->SetupAttachment(A->Root);
				C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				C->SetCastShadow(false);
				C->SetMaterial(0, AKeysWorld::VertexColorMaterial());
				C->SetCullDistances(1800, 2400);
				C->bAffectDistanceFieldLighting = false;
				C->RegisterComponent();
				A->GrowPools.Add(C);
				Tri += FString::Printf(TEXT(" %s %d,"), GrowName(G), GGrowTris[G]);
			}
			UE_LOG(LogTemp, Log, TEXT("KRRoot growth meshes (triangles):%s made in %.1f ms; drawn to 24 m"), *Tri, (FPlatformTime::Seconds() - T0) * 1000);
			return true;
		}

		// how much grows on a tree's roots (0 none): a creek bank's most, a deep fringe more than a shallow one
		double Cover(const FSite& S, const FVector2D& O)
		{
			const FVector2D W(S.X + O.X * 2.5, S.Z + O.Y * 2.5);
			const double D = KeysGeo::DepthAt(W.X, W.Y);
			if (KeysGeo::LandH(W.X, W.Y) > -.05 || D < .12) return 0;
			if (S.Bank) return .5;
			return D > 1.0 ? .42 : D > .5 ? .3 : .18;
		}

		// what grows on one tree's roots, into Out (per kind); Live: the tide's height now (a piece in the air is left
		// out only if it is above the high-tide line: barnacles and oysters dry at low water, as they do)
		int32 GrowTree(const FSite& S, double Cov, TArray<FTransform>* Out, TArray<FVector>* OutP, TArray<FVector2D>* OutO, TArray<FRootSeg>& Segs)
		{
			Segs.Reset();
			RootPieces(S, Segs);
			int32 NRoot = 0;
			const double Bias = S.Bank ? 1.0 : 0;   // (the creek edge: the best of it)
			for (int32 i = 0; i < Segs.Num(); i++)
			{
				const FRootSeg& Sg = Segs[i];
				// the piece's lower end and its upper, and where it meets the bottom and the top of the tide
				const FVector Hi = Sg.A.Y > Sg.B.Y ? Sg.A : Sg.B, Lo = Sg.A.Y > Sg.B.Y ? Sg.B : Sg.A;
				const double Floor = FloorAt(Lo.X, Lo.Z) + .03;
				if (Hi.Y < Floor || Lo.Y > HighW || Hi.Y - Lo.Y < .02) continue;
				const double Y1 = FMath::Min(Hi.Y, HighW), Y0 = FMath::Max(Lo.Y, Floor);
				if (Y1 - Y0 < .04) continue;
				const FVector Dir = (Hi - Lo).GetSafeNormal();
				const double T0 = (Y0 - Lo.Y) / (Hi.Y - Lo.Y), T1 = (Y1 - Lo.Y) / (Hi.Y - Lo.Y);
				const double L = (Hi - Lo).Size() * (T1 - T0);
				const FVector Top = FMath::Lerp(Lo, Hi, T1);
				NRoot++;
				// the way out from the root: away from the trunk, level
				const FVector2D Away = Norm2(FVector2D(Top.X - S.X, Top.Z - S.Z), FVector2D(1, 0));
				double T = .01 + .04 * RHash(S.X * .37 + i, S.Z * .53);
				const FVector UpOut = [&] { FVector U = FVector::CrossProduct(Dir, FVector(Away.Y, 0, -Away.X)).GetSafeNormal(.001, FVector(Away.X, 0, Away.Y)); return FVector::DotProduct(U, FVector(Away.X, 0, Away.Y)) < 0 ? -U : U; }();
				for (int32 n = 0; n < 48 && T < L; n++)
				{
					const double H1 = RHash(S.X * 1.13 + i * 3.7 + n * .91, S.Z * .77 - n * 1.3), H2 = RHash(S.Z * 1.31 + n * 2.3, S.X * .59 + i * 1.7), H3 = RHash(H1 * 91 + i, H2 * 37 + n);
					const double Y = (Top - Dir * T).Y;
					// how thick it is here: thin at the top of the tide, thickening toward the bottom, where the roots never dry
					const double Deep = FMath::Clamp((LowW + .06 - Y) / .6, 0.0, 1.0);
					const int32 Here = Y < LowW + .06 && H3 < .25 + .5 * Deep ? 2 : 1;   // (two pieces round the root at once, lower down)
					for (int32 h = 0; h < Here; h++)
					{
						const double Hh = h ? RHash(H2 * 53 + n, H1 * 29 + i) : H2, Hr = h ? RHash(H3 * 71 + n, H1 * 13) : H3;
						// the band: barnacles at the top of the tide, oysters through the middle, sponges, tunicates, algae and
						// the anemones below the low-tide line
						int32 G;
						double Pc = Cov;
						if (Y > HighW - .2) { G = Hh < .8 ? GBarnacle : GOyster; Pc = Cov * 1.3; }
						else if (Y > LowW + .06) { G = Hh < .62 ? GOyster : Hh < .78 ? GBarnacle : GAlgae; Pc = Cov * 1.1; }
						else
						{
							G = Hh < .14 ? GSpongeRed : Hh < .28 ? GSpongeOrange : Hh < .4 ? GTunicPurple : Hh < .48 ? GTunicBlue : Hh < .82 ? GAlgae : Hh < .86 + .03 * Bias ? GAnemone : GOyster;
							Pc = Cov * (.7 + .8 * Deep);
						}
						if ((h ? RHash(H1 * 17 + n, H3 * 41) : H1) >= Pc) continue;
						const double Rr = Sg.R;
						const double Lp = G == GBarnacle ? .04 + .06 * Hr : 0;
						const FVector Mid = Top - Dir * (T + Lp * .5);
						// (round the root: toward the open water as often as not, anywhere else the rest)
						const double Roll = h ? PI * (.7 + .6 * Hr) + (Hh - .5) : Hr < .45 ? (Hr - .22) * 2.5 : (Hr - .45) * 2 * PI / .55;
						const FVector Up = UpOut.RotateAngleAxis(FMath::RadiansToDegrees(Roll), Dir);
						const FRotator R = FRotationMatrix::MakeFromXY(D2U(-Dir), D2U(Up)).Rotator();
						const double Sc = Rr * (IsSleeve(G) ? 1 : G == GAnemone ? .45 + .2 * Hr : (.6 + .55 * Hr) * (.85 + .3 * Deep));
						Out[G].Add(FTransform(R, G2U(Mid), IsSleeve(G) ? FVector(Lp, Sc, Sc) : FVector(Sc)));
						if (OutP[G].Num() < 400) { OutP[G].Add(Mid); OutO[G].Add(Norm2(FVector2D(Up.X, Up.Z), Away)); }
					}
					T += .03 + .06 * H2;
				}
			}
			return NRoot;
		}
	}

	// ---------------------------------------------------------------- the growth near the camera
	void UpdateGrowth(AKeysMangroveEco* A, FEco& E)
	{
		if (!RootLife()) return;
		// (from high up nothing under the water shows: let it go)
		const bool bHigh = E.Cam.Y - E.Sea > 30;
		const bool bMoved = FVector::DistSquared(E.Cam, E.GrowC) > 7.0 * 7.0;
		if (!bMoved && !E.bGrowForce) return;
		if (bHigh && E.GrowC.X > 1e8) return;
		EnsureGrowPools(A);
		const double T0 = FPlatformTime::Seconds();
		E.bGrowForce = false;
		E.GrowC = bHigh ? FVector(1e9) : E.Cam;
		TArray<FTransform> Out[NumGrow];
		for (int32 G = 0; G < NumGrow; G++) { E.GrowP[G].Reset(); E.GrowO[G].Reset(); }
		E.GrowTrees = E.GrowRoots = 0;
		if (!bHigh)
		{
			// the trees within reach, nearest first, to a budget
			constexpr double Reach = 26;
			const FVector2D C(E.Cam.X, E.Cam.Z);
			TArray<TPair<double, const FSite*>> Near;
			for (const TPair<int64, FTileE*>& T : E.Tiles)
			{
				if (FMath::Abs(T.Value->C.X - C.X) > 300 + Reach || FMath::Abs(T.Value->C.Y - C.Y) > 300 + Reach) continue;
				for (const FSite& S : T.Value->Trees)
				{
					const double D = FVector2D::DistSquared(FVector2D(S.X, S.Z), C);
					if (D < Reach * Reach) Near.Add({D, &S});
				}
			}
			Near.Sort([](const TPair<double, const FSite*>& X, const TPair<double, const FSite*>& Y) { return X.Key < Y.Key; });
			TArray<FRootSeg> Segs;
			int32 Total = 0;
			for (const TPair<double, const FSite*>& N : Near)
			{
				const FSite& S = *N.Value;
				const double Cov = Cover(S, S.Bank ? S.In : Outward(S.X, S.Z));
				if (Cov <= 0) continue;
				E.GrowRoots += GrowTree(S, Cov, Out, E.GrowP, E.GrowO, Segs);
				E.GrowTrees++;
				Total = 0;
				for (int32 G = 0; G < NumGrow; G++) Total += Out[G].Num();
				if (Total > 3600) break;
			}
		}
		E.GrowTris = 0;
		for (int32 G = 0; G < NumGrow; G++)
		{
			UInstancedStaticMeshComponent* Pc = A->GrowPools.IsValidIndex(G) ? A->GrowPools[G] : nullptr;
			if (!Pc) continue;
			Pc->ClearInstances();
			if (Out[G].Num()) Pc->AddInstances(Out[G], false, true);
			E.GrowN[G] = Out[G].Num();
			E.GrowTris += Out[G].Num() * GGrowTris[G];
		}
		E.GrowMs = (FPlatformTime::Seconds() - T0) * 1000;
		if (E.bScript)
		{
			FString Who;
			for (int32 G = 0; G < NumGrow; G++) Who += FString::Printf(TEXT(" %s %d,"), GrowName(G), E.GrowN[G]);
			UE_LOG(LogTemp, Log, TEXT("KRRoot growth at %.0f, %.0f: %d roots of %d trees within 26 m:%s %d triangles; laid out in %.2f ms"), E.Cam.X, E.Cam.Z, E.GrowRoots, E.GrowTrees, *Who, E.GrowTris, E.GrowMs);
		}
	}

	// ---------------------------------------------------------------- the fish, by tree (PlanTile)
	void PlanRootLife(const FSite& S, const FVector2D& O, TFunctionRef<FSpot&(uint8, const FSite&, const FVector&, const FVector2D&, int32, double)> Add)
	{
		using namespace KeysGeo;
		auto At = [&](double D) { return FVector2D(S.X + O.X * D, S.Z + O.Y * D); };
		auto Water = [&](const FVector2D& Q) { return LandH(Q.X, Q.Y) < -.05 ? DepthAt(Q.X, Q.Y) : -1.0; };
		// the deepest water in among this tree's outer roots (1 to 2.6 m out from the trunk), and how deep it is at high water
		FVector2D Home = At(S.Bank ? 1.8 : 1.6);
		double Dh = Water(Home);
		for (const double D : {1.2, 2.0, 2.6})
		{
			const FVector2D Q = At(D);
			const double Dq = Water(Q);
			if (Dq > Dh + .05) { Dh = Dq; Home = Q; }
		}
		const double D4 = Water(At(4)), D8 = Water(At(8));
		if (Dh + HighW < .4 && D4 + HighW < .4) return;   // (never 0.4 m of water here, even at the top of the tide)
		// how much the edge holds: a creek bank, a channel's edge, a bank of fair depth, a shallow flat
		const double Rich = S.Bank ? 1.0 : D4 > 1.0 ? .8 : D4 > .5 ? .5 : .22;
		const FVector2D Tan(-O.Y, O.X);
		auto Hash = [&S](double Salt) { return SHash(S, Salt); };
		// ---- the schools in the roots: one tree in two or three on a creek bank, fewer on a flat
		if (Hash(101) < (S.Bank ? .5 : .4 * Rich))
		{
			const double Hk = Hash(102);
			const uint8 Kind = Hk < .34 ? Silverside : Hk < .52 ? Pilchard : Hk < .66 ? Snapper : Hk < .8 ? Grunt : Hk < .9 ? Schoolmaster : LaneSnapper;
			const double Big = .6 + .4 * Rich + .3 * Hash(103);
			const int32 N = int32((Kind == Silverside ? 140 : Kind == Pilchard ? 90 : Kind == Snapper || Kind == Grunt ? 34 : 26) * Big);
			FSpot& Sp = Add(Kind, S, FVector(Home.X, 0, Home.Y), O, N, 101);
			Sp.Depth = float(Dh);
			if (S.Bank)
			{
				FVector2D ToM(S.MouthX - S.CX, S.MouthZ - S.CZ);
				const double L = ToM.Size();
				Sp.Mouth = FVector2D(S.CX, S.CZ) + (L > 1 ? ToM / L * FMath::Min(L, 45.0) : FVector2D::ZeroVector);
			}
			else
			{
				FVector2D Best = At(12);
				double BestD = -1;
				for (double D = 4; D <= 40; D += 4)
				{
					const FVector2D Q = At(D);
					if (LandH(Q.X, Q.Y) > -.05 || KeysGeoReal::MangroveAt(Q.X, Q.Y)) continue;
					const double Dq = DepthAt(Q.X, Q.Y);
					if (Dq > BestD + .05) { BestD = Dq; Best = Q; }
					if (Dq >= 1.0) break;
				}
				Sp.Mouth = Best;
			}
		}
		// ---- sergeant majors at the edge of the roots, where there is a little depth
		if (D4 + HighW > .7 && Hash(104) < .1 * Rich)
		{
			FSpot& Sp = Add(Sergeant, S, FVector(At(3.2).X, 0, At(3.2).Y), O, 8 + int32(Hash(105) * 14), 104);
			Sp.Mouth = At(9);
		}
		// ---- snook lying in the shade under the outer roots (a creek's undercut bank), nose to the current, one to five
		if (Dh + HighW > .65 && Hash(106) < (S.Bank ? .085 : .05 * Rich))
			Add(Snook, S, FVector(Home.X, 0, Home.Y), O, 1 + int32(Hash(107) * 4.99), 106);
		// ---- gray snapper, grown, in a loose group just inside the root line
		if (Dh + HighW > .6 && Hash(108) < (S.Bank ? .11 : .07 * Rich))
		{
			const FVector2D Q = At(S.Bank ? 1.4 : 1.3);
			Add(GraySnapper, S, FVector(Q.X, 0, Q.Y), O, 5 + int32(Hash(109) * 10), 108);
		}
		// ---- a small barracuda hanging off the edge
		if (D4 + HighW > .8 && Hash(110) < .035 * Rich) Add(Barracuda, S, FVector(At(4.2).X, 0, At(4.2).Y), O, 1, 110);
		// ---- a tarpon or two rolling in a creek
		if (S.Bank && D8 + HighW > 1.0 && Hash(111) < .016)
		{
			const FVector2D Q(S.CX, S.CZ);
			Add(Tarpon, S, FVector(Q.X, 0, Q.Y), O, 1 + int32(Hash(112) * 1.99), 111);
		}
		(void)Tan; (void)D8;
	}

	// ---------------------------------------------------------------- the test camera
	bool FindWall(FEco& E, const FVector2D& Near, double Range, FVector& OutAt, FVector2D& OutOut, FVector2D& OutTan, bool bLog)
	{
		double Best = -1e9;
		const FSite* BestS = nullptr;
		FVector2D BestO(1, 0);
		for (const TPair<int64, FTileE*>& T : E.Tiles)
		{
			if (FMath::Abs(T.Value->C.X - Near.X) > 300 + Range || FMath::Abs(T.Value->C.Y - Near.Y) > 300 + Range) continue;
			for (const FSite& S : T.Value->Trees)
			{
				const double Dn = FVector2D::Distance(FVector2D(S.X, S.Z), Near);
				if (Dn > Range) continue;
				const FVector2D O = S.Bank ? S.In : Outward(S.X, S.Z);
				const FVector2D W(S.X + O.X * 3, S.Z + O.Y * 3), W6(S.X + O.X * 6, S.Z + O.Y * 6);
				if (!Wet(W.X, W.Y) || !Wet(W6.X, W6.Y)) continue;
				const double D = E.Sea - FloorAt(W.X, W.Y), D6 = E.Sea - FloorAt(W6.X, W6.Y);
				if (D < .55 || D6 < .6) continue;
				double Sc = FMath::Min(D, 1.6) + FMath::Min(D6, 1.8) * .5 + (S.Bank ? .6 : 0) - Dn * .004;
				// (life about it)
				for (const FSchoolE& Sc2 : T.Value->Schools) if (FVector2D::DistSquared(Sc2.Home, FVector2D(S.X, S.Z)) < 100) Sc += .5;
				for (const FGroup& G : T.Value->Groups)
					if ((G.Kind == Snook || G.Kind == GraySnapper) && FVector2D::DistSquared(G.Home, FVector2D(S.X, S.Z)) < 100) Sc += 2.5;
				for (const FSite& S2 : T.Value->Trees) Sc += FMath::Square(S2.X - S.X) + FMath::Square(S2.Z - S.Z) < 64 ? .12 : 0;
				if (Sc > Best) { Best = Sc; BestS = &S; BestO = O; }
			}
		}
		if (!BestS) { if (bLog) UE_LOG(LogTemp, Log, TEXT("KREco wall: no root wall with water off it within %.0f m"), Range); return false; }
		const FVector2D P(BestS->X + BestO.X * 1.4, BestS->Z + BestO.Y * 1.4);
		const double Fl = FloorAt(P.X, P.Y);
		OutAt = FVector(P.X, FMath::Max(Fl + .15, E.Sea - FMath::Max(.2, (E.Sea - Fl) * .5)), P.Y);
		OutOut = BestO;
		OutTan = FVector2D(-BestO.Y, BestO.X);
		if (bLog) UE_LOG(LogTemp, Log, TEXT("KREco wall: %s tree at %.1f, %.1f (%.0f m from %.0f, %.0f), %.1f m of water off it now, score %.1f"), BestS->Bank ? TEXT("a creek bank") : TEXT("a fringe"), BestS->X, BestS->Z,
			FVector2D::Distance(FVector2D(BestS->X, BestS->Z), Near), Near.X, Near.Y, E.Sea - FloorAt(P.X + BestO.X * 1.6, P.Y + BestO.Y * 1.6), Best);
		return true;
	}

	static bool InRoot(FEco& E, const FVector& P, double Clear)
	{
		TArray<FRootSeg> Segs;
		for (const TPair<int64, FTileE*>& T : E.Tiles)
		{
			if (FMath::Abs(T.Value->C.X - P.X) > 306 || FMath::Abs(T.Value->C.Y - P.Z) > 306) continue;
			for (const FSite& S : T.Value->Trees)
			{
				if (FMath::Square(S.X - P.X) + FMath::Square(S.Z - P.Z) > 6.0 * 6.0) continue;
				Segs.Reset();
				RootPieces(S, Segs);
				for (const FRootSeg& Sg : Segs)
					if (FMath::PointDistToSegment(P, Sg.A, Sg.B) < Sg.R + Clear) return true;
			}
		}
		return false;
	}

	FVector SafeEye(FEco& E, FVector Eye, const FVector& At, bool bUnder, FVector& OutAim)
	{
		OutAim = At;
		const FVector2D A2(At.X, At.Z);
		FVector2D Dir = FVector2D(Eye.X - At.X, Eye.Z - At.Z);
		const double Dist = FMath::Max(.3, Dir.Size());
		Dir = Norm2(Dir, FVector2D(1, 0));
		auto Room = [&E](double X, double Z) { return Wet(X, Z) && E.Sea - FloorAt(X, Z) >= .24; };
		if (bUnder && !Room(Eye.X, Eye.Z))
		{
			// out along the line of sight, then round it either way, until there is water to hold the camera
			bool bFound = false;
			for (int32 k = 0; k < 9 && !bFound; k++)
			{
				const double An = ((k + 1) / 2) * (k % 2 ? 1 : -1) * .35;
				const FVector2D D2(Dir.X * FMath::Cos(An) - Dir.Y * FMath::Sin(An), Dir.X * FMath::Sin(An) + Dir.Y * FMath::Cos(An));
				for (double L = Dist; L <= Dist + 10 && !bFound; L += .4)
				{
					const FVector2D Q = A2 + D2 * L;
					if (Room(Q.X, Q.Y)) { Eye.X = Q.X; Eye.Z = Q.Y; bFound = true; }
				}
			}
			if (!bFound) { bUnder = false; Eye.Y = E.Sea + .35; }
		}
		if (bUnder)
		{
			const double Fl = FloorAt(Eye.X, Eye.Z);
			// (the underwater view needs the eye well under: within about 0.9 m of the top of the tide the surface cuts the
			// picture in two. A subject that high, at the top of the tide, is taken that way, level with it)
			const double Lo = Fl + .1, Hi = E.Sea - FMath::Min(1.1, FMath::Max(.12, E.Sea - Fl - .15));
			if (At.Y > Hi + .3) Eye.Y = FMath::Clamp(At.Y - .05, FMath::Min(Lo, E.Sea - .12), E.Sea - .12);
			else Eye.Y = Lo <= Hi ? FMath::Clamp(Eye.Y, Lo, Hi) : (Fl + E.Sea) * .5;
		}
		else if (Wet(Eye.X, Eye.Z)) Eye.Y = FMath::Max(Eye.Y, E.Sea + .12);
		else Eye.Y = FMath::Max(Eye.Y, KeysGeo::LandH(Eye.X, Eye.Z) + .3);
		// and not inside a root: back it off along the line of sight
		for (int32 k = 0; k < 14 && InRoot(E, Eye, .1); k++)
		{
			const FVector2D Q = FVector2D(Eye.X, Eye.Z) + Dir * .22;
			Eye.X = Q.X; Eye.Z = Q.Y;
			if (bUnder) { const double Fl = FloorAt(Eye.X, Eye.Z); Eye.Y = FMath::Clamp(Eye.Y, FMath::Min(Fl + .1, E.Sea - .07), E.Sea - .07); }
		}
		// in thin water it looks along, not down into the mud
		if (bUnder)
		{
			const double H = FVector2D::Distance(FVector2D(Eye.X, Eye.Z), A2), K = E.Sea - FloorAt(Eye.X, Eye.Z) < .7 ? .12 : .35;
			OutAim.Y = FMath::Clamp(OutAim.Y, Eye.Y - H * K, Eye.Y + H * K);
		}
		return Eye;
	}
}
