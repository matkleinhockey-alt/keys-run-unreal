// What lives in the water under the mangroves and on the mud (part of the ecosystem, KeysMangroveEco.cpp):
//  - schools of small fish (silversides, pilchards, juvenile snappers and grunts) that mill in the shade of the prop
//    roots, scatter from the boat, a diver, a bird's strike or a predator and close up again; on the ebb they leave
//    the roots for the deeper edge or the creek's mouth, on the flood they go back in
//  - what hunts them: snook lying under the edge of the roots, a pack of jacks working along it, juvenile tarpon
//    that roll, a small lemon shark, a barracuda hanging still. When one is hungry and bait is in reach it rushes it,
//    and the bait showers out of the water
//  - needlefish at the surface, mullet that jump, a nurse shark asleep on the bottom, a stingray on the shallow edge,
//    upside-down jellyfish on the floor of the calm ponds, a horseshoe crab
//  - mangrove tree crabs on the roots (they drop off when the boat comes near) and fiddler crabs on the mud the low
//    tide bares (they go down their burrows)
// Everything is an instance of one small mesh per kind, shaded by the school fish's material (M_KR_Boid: it beats the
// tail), made near the camera only: fish to 70-80 m, crabs to 60 m, the big ones to 150 m.
//
// From a boat's deck, low and tens of metres off, the water mirrors the sky and nothing under it shows. So what is in
// it is told at the surface (EmitWaterMarks, drawn into the birds' moving mesh): a school is a dark patch of nervous
// water with silver flashes on it, a fish under way near the top draws a V of wake behind it, and the splashes of a
// mullet's jump, a shower of bait and a predator's rush are thrown high enough to catch the eye at 40 m.
#include "KeysMangroveEcoInt.h"
#include "KeysGeo.h"
#include "KeysGeoReal.h"
#include "KeysLife.h"
#include "KeysFeed.h"
#include "KeysMesh.h"
#include "KeysTown.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace KeysMangroveEco
{
	namespace
	{
		// Key: a KeysLife species model ("+": its full creature model), or one of the shapes made here ("*")
		struct FPoolSpec { const TCHAR* Key; double LenMin, LenMax, Cull; };
		const FPoolSpec PoolSpecs[NumPools] = {
			{TEXT("silverside"), .11, .16, 70},      // silverside
			{TEXT("herring"), .14, .20, 80},         // pilchard
			{TEXT("mangrove"), .14, .24, 80},        // juvenile snapper
			{TEXT("grunt"), .12, .20, 80},           // juvenile grunt
			{TEXT("schoolmaster"), .10, .17, 80},    // juvenile schoolmaster (root life)
			{TEXT("lanesnapper"), .09, .15, 80},     // juvenile lane snapper (root life)
			{TEXT("*sergeant"), .11, .16, 70},       // sergeant major (root life)
			{TEXT("*needle"), .38, .58, 70},         // needlefish
			{TEXT("bonefish"), .40, .55, 130},       // mullet
			{TEXT("barracuda"), .60, .90, 150},      // barracuda
			{TEXT("lemonshark"), .85, 1.25, 160},    // juvenile lemon shark
			{TEXT("nurse"), .95, 1.45, 160},         // nurse shark
			{TEXT("+stingray"), .75, 1.0, 140},      // stingray
			{TEXT("*jelly"), .18, .30, 55},          // upside-down jellyfish
			{TEXT("snook"), .55, .85, 150},          // snook
			{TEXT("jackcrevalle"), .45, .65, 150},   // jack
			{TEXT("tarpon"), .75, 1.15, 160},        // juvenile tarpon
			{TEXT("mangrove"), .24, .36, 90},        // gray snapper, grown (root life)
			{TEXT("*treecrab"), .14, .18, 60},       // mangrove tree crab (drawn several times life, like the kingfisher: at life size it is two pixels from 8 m)
			{TEXT("*fiddler"), .10, .13, 55},        // fiddler crab
			{TEXT("*horseshoe"), .42, .55, 60},      // horseshoe crab
		};
		const FPoolSpec& SpecOf(int32 Kind) { return PoolSpecs[Kind - Silverside]; }
		FLinearColor Hex(uint32 C) { return KeysTownPaint::Hex(C); }

		void BuildShape(const FString& Key, FKeysMesh& M)
		{
			if (Key == TEXT("*needle"))
			{
				// long, thin and green-backed, with a beak
				const FLinearColor Back = Hex(0x6f9a7a), Side = Hex(0xcfe0d6);
				M.AddTube(FVector(-.4, 0, 0), FVector(-.1, 0, 0), .012, .03, 6, Side);
				M.AddTube(FVector(-.1, 0, 0), FVector(.26, 0, 0), .03, .022, 6, Back);
				M.AddTube(FVector(.26, 0, 0), FVector(.5, 0, 0), .014, .003, 5, Back);
				M.AddTri(FVector(-.4, 0, 0), FVector(-.5, 0, .06), FVector(-.5, 0, -.06), Back, FVector(0, 1, 0));
				M.AddTri(FVector(-.4, 0, 0), FVector(-.5, 0, -.06), FVector(-.5, 0, .06), Back, FVector(0, -1, 0));
				M.AddTri(FVector(-.3, 0, .02), FVector(-.2, 0, .02), FVector(-.3, 0, .07), Back, FVector(0, 1, 0));
				M.AddTri(FVector(-.3, 0, .02), FVector(-.3, 0, .07), FVector(-.2, 0, .02), Back, FVector(0, -1, 0));
			}
			else if (Key == TEXT("*jelly"))
			{
				// Cassiopea: it lies on its bell with its frilled arms up
				const FLinearColor Bell = Hex(0x7d7a52), Rim = Hex(0xcfc9a0), Arm = Hex(0x8fa07a), Frill = Hex(0xd9e2c8);
				M.AddTube(FVector(0, 0, 0), FVector(0, 0, .07), .5, .46, 12, Rim);
				M.AddTube(FVector(0, 0, .07), FVector(0, 0, .13), .46, .05, 12, Bell);
				for (int32 a = 0; a < 8; a++)
				{
					const double An = a * PI / 4 + .3, Cx = FMath::Cos(An), Sy = FMath::Sin(An);
					M.AddTube(FVector(Cx * .1, Sy * .1, .1), FVector(Cx * .3, Sy * .3, .24), .07, .05, 5, Arm);
					M.AddTube(FVector(Cx * .3, Sy * .3, .24), FVector(Cx * .37, Sy * .37, .33), .08, .01, 5, Frill);
				}
			}
			else if (Key == TEXT("*sergeant"))
			{
				// the sergeant major: a small, deep, flat-sided damselfish, yellow on the back, silvery white below, with
				// five black bars down its sides and a forked tail (nose at +X)
				const int32 I0 = M.V.Num();
				const int32 NS = 16;
				for (int32 i = 0; i < NS; i++)
				{
					const double X0 = .5 - .85 * i / NS, X1 = .5 - .85 * (i + 1) / NS;
					auto R = [](double X) { const double U = (X - .05) / .47; return FMath::Max(.035, .2 * FMath::Sqrt(FMath::Max(0.0, 1 - U * U))); };
					M.AddTube(FVector(X0, 0, 0), FVector(X1, 0, 0), R(X0), R(X1), 8, FLinearColor::White);
				}
				for (const double Sd : {1.0, -1.0})
				{
					M.AddTri(FVector(-.33, 0, 0), FVector(-.5, 0, .2), FVector(-.44, 0, 0), FLinearColor::White, FVector(0, Sd, 0));
					M.AddTri(FVector(-.33, 0, 0), FVector(-.44, 0, 0), FVector(-.5, 0, -.2), FLinearColor::White, FVector(0, Sd, 0));
					M.AddTri(FVector(.15, 0, .16), FVector(-.3, 0, .1), FVector(-.05, 0, .32), FLinearColor::White, FVector(0, Sd, 0));
				}
				const FLinearColor Yel = Hex(0xd8c848), Sil = Hex(0xe6ece8), Bar = Hex(0x1a1c1e), Fin = Hex(0xb8b070);
				for (int32 i = I0; i < M.V.Num(); i++)
				{
					FVector& P = M.V[i];
					P = FVector(P.X, P.Y * .5, P.Z * 1.45);   // (flat-sided and deep)
					const double X = P.X / 100, Z = P.Z / 100;
					FLinearColor C = FMath::Lerp(Sil, Yel, float(FMath::Clamp((Z + .02) / .2, 0.0, 1.0)));
					if (X < -.33 || Z > .26) C = Fin;
					for (int32 b = 0; b < 5; b++) if (X < .33 && FMath::Abs(X - (.26 - b * .14)) < .03) C = Bar;
					M.C[i] = C;
				}
			}
			else if (Key == TEXT("*horseshoe"))
			{
				const FLinearColor Shell = Hex(0x6a5a3a), Dark = Hex(0x4f4229);
				M.AddTube(FVector(.22, 0, 0), FVector(.22, 0, .1), .3, .22, 10, Shell);
				M.AddTube(FVector(.22, 0, .1), FVector(.2, 0, .16), .22, .02, 10, Shell);
				M.AddTube(FVector(.02, 0, .05), FVector(-.3, 0, .04), .2, .09, 8, Dark);
				M.AddTube(FVector(-.3, 0, .04), FVector(-.9, 0, .03), .025, .004, 4, Dark);
			}
			else
			{
				// a crab: a wide shell, eight legs, two claws (a fiddler's one great pale claw)
				const bool bFid = Key == TEXT("*fiddler");
				// (a tree crab is dark olive with rust-red legs and claws, against grey-brown roots; a fiddler is dark, almost
				// purple, against pale mud, with its one great claw orange)
				const FLinearColor Shell = Hex(bFid ? 0x2a2230 : 0x2c3020), Leg = Hex(bFid ? 0x5a3a2e : 0xb04a28), Claw = Hex(bFid ? 0xf08a20 : 0xd8502a);
				M.AddRoundBox(FVector(0, 0, .22), FVector(.62, .9, .3), .1, Shell, FQuat::Identity, 1);
				for (const double Sd : {-1.0, 1.0})
					for (int32 l = 0; l < 4; l++)
					{
						const double X = .22 - l * .16;
						M.AddTube(FVector(X, Sd * .42, .22), FVector(X * 1.2, Sd * .78, .34), .05, .04, 4, Leg);
						M.AddTube(FVector(X * 1.2, Sd * .78, .34), FVector(X * 1.4, Sd * 1.0, 0), .04, .015, 4, Leg);
					}
				M.AddRoundBox(FVector(.44, -.3, .2), FVector(.3, .18, .14), .05, Claw, FQuat::Identity, 1);
				if (bFid) M.AddRoundBox(FVector(.5, .34, .3), FVector(.7, .3, .26), .1, Claw, FQuat(FVector(0, 1, 0), -.5), 1);
				else M.AddRoundBox(FVector(.44, .3, .2), FVector(.3, .18, .14), .05, Claw, FQuat::Identity, 1);
				for (const double Sd : {-1.0, 1.0}) M.AddTube(FVector(.26, Sd * .16, .34), FVector(.3, Sd * .18, .52), .025, .03, 4, Hex(0x1a1a1a));
			}
		}

		// one kind's mesh and its instances, made the first time one of that kind comes near
		UInstancedStaticMeshComponent* EnsurePool(AKeysMangroveEco* A, FEco& E, int32 Ix)
		{
			if (A->Pools.Num() < NumPools) A->Pools.SetNum(NumPools);
			if (A->Pools[Ix] || E.Pools[Ix].bTried || !A->MID) return A->Pools[Ix];
			E.Pools[Ix].bTried = true;
			const double T0 = FPlatformTime::Seconds();
			const FString Key = PoolSpecs[Ix].Key;
			FKeysMesh M;
			if (Key.StartsWith(TEXT("*"))) BuildShape(Key, M);
			else if (Key.StartsWith(TEXT("+")) ? !AKeysLife::BuildCreatureMesh(Key.Mid(1), M, 1) : !AKeysLife::BuildSchoolMesh(Key, M, Ix <= LaneSnapper - Silverside ? 2 : 1))   // (the small fish, in their hundreds, take the school fish's cheaper far model)
			{
				UE_LOG(LogTemp, Warning, TEXT("KREco: no model for %s"), *Key);
				return nullptr;
			}
			M.SyncKinds();
			// normalize as the school fish are: nose at +X 50, tail at -50, whatever its length
			double MinX = 1e9, MaxX = -1e9;
			for (const FVector& V : M.V) { MinX = FMath::Min(MinX, V.X); MaxX = FMath::Max(MaxX, V.X); }
			const double Kk = 100.0 / FMath::Max(1.0, MaxX - MinX), Cx = (MinX + MaxX) * .5;
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
				Pos[V] = FVector3f((M.V[i] - FVector(Cx, 0, 0)) * Kk);
				const FVertexInstanceID I = MD.CreateVertexInstance(V);
				Nrm[I] = FVector3f(M.N[i]);
				Col[I] = FVector4f(M.C[i].R, M.C[i].G, M.C[i].B, 1);
				UVs.Set(I, 0, FVector2f(M.UV.IsValidIndex(i) ? M.UV[i] : FVector2D::ZeroVector));
				UVs.Set(I, 1, FVector2f(M.K[i]));
				Inst[i] = I;
			}
			int32 Tris = 0;
			for (int32 i = 0; i + 2 < M.T.Num(); i += 3)
			{
				const int32 Va = M.T[i], Vb = M.T[i + 1], Vc = M.T[i + 2];
				const FVector Fn = FVector::CrossProduct(M.V[Vb] - M.V[Va], M.V[Vc] - M.V[Va]);
				if (Fn.SizeSquared() < 1e-12) continue;
				const bool bFlip = FVector::DotProduct(Fn, M.N[Va] + M.N[Vb] + M.N[Vc]) > 0;
				MD.CreateTriangle(Grp, {Inst[Va], Inst[bFlip ? Vc : Vb], Inst[bFlip ? Vb : Vc]});
				Tris++;
			}
			UStaticMesh* SM = NewObject<UStaticMesh>(A, *FString::Printf(TEXT("Eco_%d"), Ix));
			SM->GetStaticMaterials().Add(FStaticMaterial(A->MID, FName(TEXT("Mat"))));
			SM->SetPositiveBoundsExtension(FVector(40));
			SM->SetNegativeBoundsExtension(FVector(40));
			UStaticMesh::FBuildMeshDescriptionsParams Bp;
			Bp.bFastBuild = true;
			Bp.bMarkPackageDirty = false;
			Bp.bBuildSimpleCollision = false;
			SM->BuildFromMeshDescriptions({&MD}, Bp);
			SM->CalculateExtendedBounds();
			A->Meshes.Add(SM);
			UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(A, *FString::Printf(TEXT("EcoPool_%d"), Ix));
			C->SetStaticMesh(SM);
			C->SetupAttachment(A->Root);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->NumCustomDataFloats = 8;   // tail phase, its rate, when, amplitude, body bend, flash, shade, seed (M_KR_Boid)
			C->SetCastShadow(false);
			C->SetMaterial(0, A->MID);
			C->SetCullDistances(0, int32(PoolSpecs[Ix].Cull * 100));
			C->bAffectDistanceFieldLighting = false;
			C->RegisterComponent();
			A->Pools[Ix] = C;
			E.Pools[Ix].Tris = Tris;
			UE_LOG(LogTemp, Log, TEXT("KREco: %s mesh, %d triangles, drawn to %.0f m, made in %.1f ms"), KindName(Ix + Silverside), Tris, PoolSpecs[Ix].Cull, (FPlatformTime::Seconds() - T0) * 1000);
			return C;
		}

		// an instance: where, which way it heads (game axes), how long, and what its tail is doing
		void Put(FEco& E, int32 Kind, const FVector& P, const FVector& Fwd, double Roll, double Len, double Phase, double Freq, double Amp, double Bend, double Flash, double Shade, double Seed, const FVector& Scale = FVector(1))
		{
			FPoolE& Pl = E.Pools[Kind - Silverside];
			FRotator R = FRotationMatrix::MakeFromX(D2U(Fwd.GetSafeNormal(.0001, FVector(1, 0, 0)))).Rotator();
			R.Roll = FMath::RadiansToDegrees(Roll);
			Pl.Xf.Add(FTransform(R, G2U(P), Scale * Len));
			const float D[8] = {float(FMath::Fmod(Phase, 2 * PI)), float(Freq), float(E.Now), float(Amp), float(Bend), float(FMath::Clamp(Flash, 0.0, 1.0)), float(Shade), float(Seed)};
			Pl.Data.Append(D, 8);
		}

		bool IsHunter(int32 K) { return K == Snook || K == Jack || K == Tarpon || K == LemonShark || K == Barracuda; }
		// how much water a kind wants under it to lie somewhere
		double NeedDepth(int32 K) { return K == Tarpon || K == Jack ? .7 : K == LemonShark || K == NurseShark || K == Barracuda || K == Snook ? .45 : .25; }

		// the bait goes up: a quarter of the school leaves the water, and everything with wings hears of it
		void Shower(FEco& E, FSchoolE& Sc, const FVector2D& From, uint8 Hunter)
		{
			Sc.Scatter = 1;
			Sc.ThreatP = From;
			Sc.HitT = E.Now;
			int32 Up = 0;
			for (FFishE& F : Sc.Fish)
			{
				if (Up >= 48 || FMath::FRand() > .5) continue;
				const FVector2D Away = (FVector2D(F.P.X, F.P.Z) - From).GetSafeNormal() + FVector2D(FMath::FRandRange(-.6, .6), FMath::FRandRange(-.6, .6));
				F.Air = 1;
				F.P.Y = E.Sea + .02;
				F.V = FVector(Away.X * FMath::FRandRange(1.0, 2.6), FMath::FRandRange(2.6, 5.2), Away.Y * FMath::FRandRange(1.0, 2.6));
				Up++;
			}
			// (white water thrown up over the whole school, and where the hunter went through it a boil with a column of spray)
			if (E.Life)
			{
				for (int32 k = 0; k < 6; k++) E.Life->Splash(Sc.C.X + FMath::FRandRange(-1.0, 1.0) * (Sc.R + .6), Sc.C.Y + FMath::FRandRange(-1.0, 1.0) * (Sc.R + .6), 12, k < 2 ? 1.2 : .8);
				E.Life->Splash(From.X, From.Y, 30, 1.6);
			}
			E.Sound(TEXT("shower"), Sc.Kind, FVector(Sc.C.X, E.Sea, Sc.C.Y), FMath::Min(1.f, Sc.N / 80.f));
			FShower& Sh = E.Showers.AddDefaulted_GetRef();
			Sh.Id = E.NextShower++; Sh.P = Sc.C; Sh.T = E.Now; Sh.Prey = Sc.Kind; Sh.Hunter = Hunter;
		}
	}

	FSchoolE* NearestSchool(FEco& E, const FVector2D& P, double R, double* OutD)
	{
		FSchoolE* Best = nullptr;
		double BestD = R * R;
		for (TPair<int64, FTileE*>& T : E.Tiles)
		{
			if (FMath::Abs(T.Value->C.X - P.X) > 400 + R || FMath::Abs(T.Value->C.Y - P.Y) > 400 + R) continue;
			for (FSchoolE& S : T.Value->Schools)
			{
				const double D = FVector2D::DistSquared(S.C, P);
				if (D < BestD) { BestD = D; Best = &S; }
			}
		}
		if (OutD && Best) *OutD = FMath::Sqrt(BestD);
		return Best;
	}

	FSchoolE* FindSchool(FEco& E, int32 Id)
	{
		for (TPair<int64, FTileE*>& T : E.Tiles)
			for (FSchoolE& S : T.Value->Schools) if (S.Id == Id) return &S;
		return nullptr;
	}

	int32 BaitNear(FEco& E, const FVector2D& P, double R)
	{
		int32 N = 0;
		for (TPair<int64, FTileE*>& T : E.Tiles)
		{
			if (FMath::Abs(T.Value->C.X - P.X) > 400 + R || FMath::Abs(T.Value->C.Y - P.Y) > 400 + R) continue;
			for (FSchoolE& S : T.Value->Schools) if (FVector2D::DistSquared(S.C, P) < R * R) N += S.N;
		}
		return N;
	}

	FGroup* NearestGroup(FEco& E, const FVector2D& P, double R, int32 Kind)
	{
		FGroup* Best = nullptr;
		double BestD = R * R;
		for (TPair<int64, FTileE*>& T : E.Tiles)
		{
			if (FMath::Abs(T.Value->C.X - P.X) > 400 + R || FMath::Abs(T.Value->C.Y - P.Y) > 400 + R) continue;
			for (FGroup& G : T.Value->Groups)
			{
				if (G.Kind != Kind) continue;
				const double D = FVector2D::DistSquared(G.C, P);
				if (D < BestD) { BestD = D; Best = &G; }
			}
		}
		return Best;
	}

	// ---------------------------------------------------------------- making a tile's water
	void SpawnWater(FEco& E, FTileE& T, const FPlan& P)
	{
		for (const FSpot& Sp : P.Spots)
		{
			const FVector2D P2(Sp.P.X, Sp.P.Z);
			const double R1 = EHash(Sp.Seed * 1e-6, 1.0), R2 = EHash(Sp.Seed * 1e-6, 2.0);
			if (Sp.Kind >= Silverside && Sp.Kind <= Sergeant)
			{
				FSchoolE& S = T.Schools.AddDefaulted_GetRef();
				S.Kind = Sp.Kind; S.Id = E.NextSchool++; S.N = Sp.N; S.Seed = Sp.Seed;
				S.Home = P2; S.Mouth = Sp.Mouth; S.Out = Sp.Out; S.bBank = Sp.Site.Bank != 0;
				// each school turns at its own height of tide: all are out by dead low and all in by high water
				S.Th = -.55 + .6 * R1;
				S.bOut = E.Tide < S.Th;
				S.C = S.bOut ? S.Mouth : S.Home;
				S.R = .5 + .08 * FMath::Sqrt(double(S.N)) * (Sp.Kind == Silverside ? 1.2 : 1);
				if (RootLife())
				{
					// (root life: a band along the root line, the small snappers, grunts and sergeants hanging in it head to
					// the current, the silversides and pilchards weaving round)
					S.R = .45 + .06 * FMath::Sqrt(double(S.N));
					S.Tan = FVector2D(-S.Out.Y, S.Out.X);
					S.bHang = Sp.Kind != Silverside && Sp.Kind != Pilchard;
				}
				S.Mill = R2 * 6;
				S.Floor = FloorAt(S.C.X, S.C.Y);
			}
			else if (Sp.Kind == TreeCrab || Sp.Kind == Fiddler)
			{
				FCrabs& C = T.Crabs.AddDefaulted_GetRef();
				C.Kind = Sp.Kind; C.Site = Sp.Site; C.C = Sp.Kind == TreeCrab ? FVector2D(Sp.Site.X, Sp.Site.Z) : P2; C.Out = Sp.Out; C.N = Sp.N; C.Seed = Sp.Seed;
				C.FloorY = FloorAt(P2.X, P2.Y);
			}
			else if (Sp.Kind >= Needlefish && Sp.Kind <= Horseshoe)
			{
				FGroup& G = T.Groups.AddDefaulted_GetRef();
				G.Kind = Sp.Kind; G.Seed = Sp.Seed; G.Home = G.C = P2; G.Out = Sp.Out; G.bBank = Sp.Site.Bank != 0;
				G.Yaw = 2 * PI * R1;
				G.Hunger = 8 + 50 * R2;
				const FPoolSpec& K = SpecOf(Sp.Kind);
				for (int32 i = 0; i < Sp.N; i++)
				{
					FSwim& M = G.M.AddDefaulted_GetRef();
					const double A = 2 * PI * EHash(Sp.Seed * 1e-6 + i, 3.0), Rr = (Sp.Kind == Jelly ? 4.5 : Sp.Kind == Mullet ? 2.2 : Sp.Kind == Needlefish ? 1.4 : 1.1) * FMath::Sqrt(EHash(Sp.Seed * 1e-6 + i, 4.0)) * (Sp.N > 1 ? 1 : 0);
					M.Off = FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rr;
					M.Len = FMath::Lerp(K.LenMin, K.LenMax, EHash(Sp.Seed * 1e-6 + i, 5.0));
					M.Ph = 10 * EHash(Sp.Seed * 1e-6 + i, 6.0);
					M.T = 4 + 30 * EHash(Sp.Seed * 1e-6 + i, 7.0);
					M.P = FVector(P2.X + M.Off.X, -.3, P2.Y + M.Off.Y);
					M.Fwd = FVector(FMath::Cos(G.Yaw + A * (Sp.Kind == Jelly ? 1 : .1)), 0, FMath::Sin(G.Yaw + A * (Sp.Kind == Jelly ? 1 : .1)));
				}
				if (RootLife() && (Sp.Kind == Snook || Sp.Kind == GraySnapper || Sp.Kind == Barracuda))
				{
					// (root life) lying along the root line: snook side by side and a little behind one another, the gray
					// snapper loose about their place, all heading along the shore
					const FVector2D Tn(-Sp.Out.Y, Sp.Out.X);
					G.Yaw = FMath::Atan2(Tn.Y, Tn.X) + ((Sp.Seed & 1) ? PI : 0);
					for (int32 i = 0; i < G.M.Num(); i++)
					{
						FSwim& M = G.M[i];
						const double H1 = EHash(Sp.Seed * 1e-6 + i, 8.0), H2 = EHash(Sp.Seed * 1e-6 + i, 9.0);
						M.Off = Sp.Kind == Snook ? FVector2D(-.55 * i + .2 * H1, (i % 2 ? .35 : -.1) + .2 * H2) : Sp.Kind == GraySnapper ? FVector2D((H1 - .5) * 2.8, (H2 - .5) * 1.4) : FVector2D::ZeroVector;
						M.Fwd = FVector(FMath::Cos(G.Yaw), 0, FMath::Sin(G.Yaw));
					}
				}
			}
		}
	}

	// ---------------------------------------------------------------- every frame
	void UpdateWater(FEco& E, FTileE& T)
	{
		const double Dt = E.Dt;
		const FVector2D Cam2(E.Cam.X, E.Cam.Z);
		const bool bBoatMoving = E.BoatSpd > .8;

		// ---- the schools of small fish
		for (FSchoolE& S : T.Schools)
		{
			const double DCam = FVector2D::Distance(S.C, Cam2);
			if (DCam > 600) continue;
			// the tide says where it belongs: in under the roots, or out at the edge (and never where the water has gone)
			const bool bRL = RootLife();
			// (root life: they hold in the roots only while there is 0.4 m of water there)
			const bool bWantOut = E.Tide < S.Th || E.Sea - FloorAt(S.Home.X, S.Home.Y) < (bRL ? .4 : .1);
			const FVector2D Goal = bWantOut ? S.Mouth : S.Home;
			const FVector2D ToG = Goal - S.C;
			const double Lg = ToG.Size();
			if (Lg > .05) S.C += ToG / Lg * FMath::Min(Lg, 1.3 * Dt);
			S.bOut = FVector2D::DistSquared(S.C, S.Home) > 4;
			const bool bWasAwake = S.bAwake;
			if (!S.bAwake && DCam < 105) S.bAwake = true;
			else if (S.bAwake && DCam > 130) { S.bAwake = false; S.Fish.Empty(); }
			if (!S.bAwake) continue;
			S.Floor = FloorAt(S.C.X, S.C.Y);
			const double Depth = FMath::Max(.06, E.Sea - S.Floor), Thick = bRL ? FMath::Clamp(Depth * .38, .03, .6) : FMath::Clamp(Depth * .36, .03, .4);
			S.Y = E.Sea - FMath::Clamp(Depth * .5, .05, bRL ? .8 : .55);
			const FPoolSpec& K = SpecOf(S.Kind);
			if (!bWasAwake)
			{
				S.Fish.SetNum(S.N);
				for (int32 i = 0; i < S.N; i++)
				{
					FFishE& F = S.Fish[i];
					const double A = 2 * PI * EHash(S.Seed * 1e-6 + i, 1.0), Rr = FMath::Sqrt(EHash(S.Seed * 1e-6 + i, 2.0));
					F.Slot = FVector(FMath::Cos(A) * Rr, EHash(S.Seed * 1e-6 + i, 3.0) * 2 - 1, FMath::Sin(A) * Rr);
					F.Len = float(FMath::Lerp(K.LenMin, K.LenMax, EHash(S.Seed * 1e-6 + i, 4.0)));
					F.Seed = float(EHash(S.Seed * 1e-6 + i, 5.0));
					F.Shade = float(.85 + .25 * EHash(S.Seed * 1e-6 + i, 6.0));
					F.Ph = F.Seed * 6;
					F.P = FVector(S.C.X + F.Slot.X * S.R, S.Y + F.Slot.Y * Thick, S.C.Y + F.Slot.Z * S.R);
					F.V = FVector::ZeroVector;
					F.Fwd = FVector(-F.Slot.Z, 0, F.Slot.X).GetSafeNormal(.01, FVector(1, 0, 0));
				}
			}
			E.SchoolsAwake++;
			// what frightens them: the hull under way, a diver
			S.Scatter = FMath::Max(0.0, S.Scatter - Dt * .38);
			const double DB = FVector2D::Distance(S.C, E.Boat);
			if (DB < 9 && bBoatMoving) { S.Scatter = FMath::Max(S.Scatter, FMath::Min(1.0, .25 + (1 - DB / 9) * (.4 + E.BoatSpd * .2))); S.ThreatP = E.Boat; }
			// (root life: a diver does not scatter the school; the fish near him part round him, fish by fish, below)
			if (E.bDiver && FVector2D::Distance(S.C, FVector2D(E.Diver.X, E.Diver.Z)) < (bRL ? 0 : 3.2)) { S.Scatter = FMath::Max(S.Scatter, .6); S.ThreatP = FVector2D(E.Diver.X, E.Diver.Z); }
			S.Mill += Dt * (bRL && S.bHang ? .035 : .22 + S.Scatter * .6) * ((S.Seed & 1) ? 1 : -1);
			const double Cm = FMath::Cos(S.Mill), Sm = FMath::Sin(S.Mill), Spread = S.R * (1 + S.Scatter * 1.6);
			const bool bDraw = DCam < K.Cull + 10;
			// (root life) how many of its fish are moved and drawn: all within 18 m of the camera, a tenth at 70 m (from the
			// deck nothing under the water shows that far off; under it the water hides it); the rest wait where they are
			int32 Live = S.Fish.Num();
			if (bRL)
			{
				const double Kd = DCam < 18 ? 1 : FMath::Lerp(1.0, .1, FMath::Clamp((DCam - 18) / 52, 0.0, 1.0));
				Live = bDraw ? FMath::Clamp(int32(S.Fish.Num() * Kd + .5), FMath::Min(S.Fish.Num(), 6), S.Fish.Num()) : 0;
				E.FishCap += Live; E.FishThinned += S.Fish.Num() - Live;
			}
			const double La = S.R * (S.bBank ? 2.3 : 2.0) * (1 + S.Scatter * 1.2), Lb = S.R * .8 * (1 + S.Scatter * 1.2);
			const FVector2D Tn = S.Tan, On = S.Out;
			// (the hanging ones face the current: along the shore, turned about with the tide)
			const double Cur = ((S.Seed & 2) ? 1 : -1) * (E.TideRate < -1e-4 ? -1 : 1);
			auto WantOf = [&](const FFishE& F)
			{
				if (!bRL) return FVector(S.C.X + (F.Slot.X * Cm - F.Slot.Z * Sm) * Spread, S.Y + F.Slot.Y * Thick, S.C.Y + (F.Slot.X * Sm + F.Slot.Z * Cm) * Spread);
				// a band along the root line, the fish going round it (the hanging ones barely), weaving in and out
				const double Rr = FMath::Sqrt(F.Slot.X * F.Slot.X + F.Slot.Z * F.Slot.Z), Th = FMath::Atan2(F.Slot.Z, F.Slot.X) + S.Mill * (S.bHang ? 1 : .6 + .8 * F.Seed);
				const double Wv = (S.bHang ? .05 : .16) * FMath::Sin(E.Now * (1.1 + F.Seed) + F.Seed * 40);
				const double U = La * Rr * FMath::Cos(Th), V = Lb * Rr * FMath::Sin(Th) + Wv;
				return FVector(S.C.X + Tn.X * U + On.X * V, S.Y + F.Slot.Y * Thick + .04 * FMath::Sin(E.Now * .8 + F.Seed * 23), S.C.Y + Tn.Y * U + On.Y * V);
			};
			for (int32 i = Live; i < S.Fish.Num() && bRL; i++) S.Fish[i].V = FVector::ZeroVector;
			for (int32 i = 0; i < Live; i++)
			{
				FFishE& F = S.Fish[i];
				// (one that has been waiting comes back in its place)
				if (bRL && i >= S.Live && F.Air <= 0) F.P = WantOf(F);
				if (F.Air > 0)
				{
					// out of the water: it falls back
					F.V.Y -= 9.8 * Dt;
					F.P += F.V * Dt;
					if (F.P.Y < E.Sea - .02 && F.V.Y < 0) { F.Air = 0; F.V *= .3; }
				}
				else
				{
					const FVector Want = WantOf(F);
					FVector Acc = (Want - F.P) * 5.0 - F.V * 3.2;
					if (S.Scatter > .05)
					{
						const FVector2D Away = (FVector2D(F.P.X, F.P.Z) - S.ThreatP).GetSafeNormal();
						Acc += FVector(Away.X, 0, Away.Y) * S.Scatter * 10 + FVector(FMath::FRandRange(-1.0, 1.0), 0, FMath::FRandRange(-1.0, 1.0)) * S.Scatter * 5;
					}
					if (bRL && E.bDiver)
					{
						// they part round the diver and close behind him
						const FVector Dd = F.P - (E.Diver + FVector(0, .1, 0));
						const double Ld = Dd.Size();
						if (Ld < 1.7 && Ld > .01) Acc += Dd / Ld * (1.7 - Ld) * 18;
					}
					F.V += Acc * Dt;
					const double Sp = F.V.Size();
					if (Sp > 3.6) F.V *= 3.6 / Sp;
					F.P += F.V * Dt;
					F.P.Y = FMath::Clamp(F.P.Y, FMath::Min(S.Floor + .03, E.Sea - .04), E.Sea - .03);
				}
				const double Sp = F.V.Size();
				if (bRL && S.bHang && Sp < .3 && F.Air <= 0)
				{
					// hanging head to the current, each a little off the line
					const double Ja = (F.Seed - .5) * .5 + .15 * FMath::Sin(E.Now * .5 + F.Seed * 17);
					const FVector2D Hd = Tn * Cur;
					const FVector Want(Hd.X * FMath::Cos(Ja) - Hd.Y * FMath::Sin(Ja), 0, Hd.X * FMath::Sin(Ja) + Hd.Y * FMath::Cos(Ja));
					F.Fwd = FMath::Lerp(F.Fwd, Want, FMath::Min(1.0, Dt * 2)).GetSafeNormal(.001, F.Fwd);
				}
				else if (Sp > .04) F.Fwd = FMath::Lerp(F.Fwd, F.V / Sp, FMath::Min(1.0, Dt * 9)).GetSafeNormal(.001, F.Fwd);
				F.Ph += float((7 + Sp * 16) * Dt);
				F.Flash = float(FMath::Max(0.0, F.Flash - Dt * 3));
				// (it turns on its side and shows silver: all the time now and then, more when they are frightened)
				if (FMath::FRand() < Dt * (S.Scatter > .4 ? 2 : .14)) F.Flash = 1;
				if (bDraw) Put(E, S.Kind, F.P, F.Fwd, F.Flash * .9, F.Len * (F.Air > 0 ? 1.8 : 1), F.Ph, 7 + Sp * 16, FMath::Min(1.0, .3 + Sp * .5), 0, F.Flash, F.Shade, F.Seed);
			}
			S.Live = Live;
		}

		// ---- the ones that swim alone or in a few
		const double FeedNow = E.Phase == 1 || E.Phase == 3 ? 1.7 : 1, TideRun = FMath::Abs(E.Tide) < .7 ? 1.3 : 1;
		for (FGroup& G : T.Groups)
		{
			const double DCam = FVector2D::Distance(G.C, Cam2);
			const FPoolSpec& K = SpecOf(G.Kind);
			G.bActive = DCam < K.Cull + 20;
			if (!G.bActive) continue;
			G.T += Dt;
			const double DB = FVector2D::Distance(G.C, E.Boat);
			// --- where it lies: off the small fish it is working, wherever they are on this tide; else at home
			FVector2D Station = G.Home;
			// (root life) snook, gray snapper and a barracuda keep their place in and off the roots where a diver sees them;
			// the gray snapper ease back into the roots as he comes near, and in among them when he is close
			const bool bLie = RootLife() && (G.Kind == Snook || G.Kind == Barracuda || G.Kind == GraySnapper);
			const double DDiver = E.bDiver ? FVector2D::Distance(G.C, FVector2D(E.Diver.X, E.Diver.Z)) : 1e9;
			if (bLie && G.Kind == GraySnapper) Station = G.Home - G.Out * (DDiver < 2.5 ? 1.1 : DDiver < 7 ? .35 : 0);
			if (!bLie && (IsHunter(G.Kind) || G.Kind == Mullet || G.Kind == Needlefish))
			{
				G.Wander -= Dt;
				if (G.Wander <= 0)
				{
					G.Wander = 1.5;
					double Ds = 0;
					FSchoolE* Sc = IsHunter(G.Kind) ? NearestSchool(E, G.State == GHold ? G.C : G.Home, 60, &Ds) : nullptr;
					G.Prey = Sc ? Sc->Id : -1;
					FVector2D St = Sc ? Sc->C + Sc->Out * (G.Kind == Snook ? 2.2 : G.Kind == Barracuda ? 4.5 : 9) : G.Home;
					// (somewhere with water enough for it)
					for (int32 k = 0; k < 8 && (E.Sea - FloorAt(St.X, St.Y) < NeedDepth(G.Kind) || !Wet(St.X, St.Y)); k++) St += G.Out * 3;
					G.Aim = St;
				}
				Station = G.Aim.IsNearlyZero() ? G.Home : G.Aim;
				if (G.Kind == Jack || G.Kind == Mullet || G.Kind == LemonShark)
				{
					// they work along the edge, to and fro
					const FVector2D Tn(-G.Out.Y, G.Out.X);
					const double Sw = (G.Kind == LemonShark ? 26 : 16) * FMath::Sin(E.Now * (G.Kind == Mullet ? .05 : .08) + (G.Seed & 255));
					const FVector2D Q = Station + Tn * Sw;
					if (Wet(Q.X, Q.Y) && E.Sea - FloorAt(Q.X, Q.Y) > NeedDepth(G.Kind) * .8) Station = Q;
				}
			}
			// --- hunger, the bait in reach, the rush
			double Speed = G.Kind == Jack ? 1.3 : G.Kind == Mullet ? .7 : G.Kind == LemonShark ? .8 : G.Kind == Horseshoe ? .05 : G.Kind == Stingray ? .35 : .5;
			if (IsHunter(G.Kind))
			{
				if (G.State == GHold)
				{
					G.Hunger -= Dt * FeedNow * TideRun;
					if (G.Hunger <= 0)
					{
						double Ds = 0;
						FSchoolE* Sc = NearestSchool(E, G.C, 30, &Ds);
						if (Sc && Sc->bAwake && E.Now - Sc->HitT > 18 && Sc->Fish.Num())
						{
							G.State = GRush; G.Prey = Sc->Id; G.Aim = Sc->C; G.T = 0; G.RushD = Ds;
						}
						else G.Hunger = 4;   // hungry, and nothing in reach: it waits where it is
					}
				}
				else if (G.State == GRush)
				{
					Speed = 6.5;
					// (its rush tears the surface: spray along its track)
					if (E.Life && DCam < 120 && FMath::FRand() < Dt * 9) E.Life->Splash(G.C.X, G.C.Y, 5, .9);
					FSchoolE* Sc = FindSchool(E, G.Prey);
					if (Sc) G.Aim = Sc->C;
					Station = G.Aim;
					if (!Sc || G.T > 6) { G.State = GReturn; G.T = 0; }
					else if (FVector2D::Distance(G.C, G.Aim) < 1.3)
					{
						Shower(E, *Sc, G.C - (G.Aim - G.C).GetSafeNormal() * 1.5, G.Kind);
						G.State = GReturn; G.T = 0;
						G.Hunger = (G.Kind == Barracuda || G.Kind == LemonShark ? 70 : 32) + 45 * FMath::FRand();
						if (LinkDue(E, LinkRush, 2))
							UE_LOG(LogTemp, Log, TEXT("KREco link rush: %d %s, hungry (%s, %s), with %d %s %.0f m off: they rush them at %.0f, %.0f and the bait showers out of the water"),
								G.M.Num(), KindName(G.Kind), PhaseWord(E), TideWord(E), Sc->N, KindName(Sc->Kind), G.RushD, Sc->C.X, Sc->C.Y);
					}
				}
				else if (G.State == GReturn)
				{
					// it carries on through and comes round
					Speed = 2.2;
					if (G.T > 2.5) G.State = GHold;
				}
			}
			// --- the boat: it gives way to a hull under way
			if (DB < 6 && bBoatMoving && G.Kind != Jelly && G.Kind != Horseshoe && G.State != GRush) { G.State = GFlee; G.FleeT = 2.2; }
			if (G.State == GFlee)
			{
				G.FleeT -= Dt;
				Speed = G.Kind == Stingray || G.Kind == Barracuda ? 5 : 3.5;
				const FVector2D Away = (G.C - E.Boat).GetSafeNormal();
				const FVector2D Q = G.C + Away * 6;
				Station = Wet(Q.X, Q.Y) ? Q : G.C;
				if (G.FleeT <= 0) G.State = GHold;
			}
			if (G.Kind == Horseshoe)
			{
				// it trundles over the bottom, turning now and then
				G.Yaw += FMath::Sin(E.Now * .05 + (G.Seed & 63)) * .1 * Dt;
				const FVector2D Q = G.C + FVector2D(FMath::Cos(G.Yaw), FMath::Sin(G.Yaw)) * 2;
				Station = Wet(Q.X, Q.Y) && FVector2D::Distance(Q, G.Home) < 12 ? Q : G.Home;
			}
			else if (G.Kind == Stingray && G.State == GHold)
			{
				const double A = E.Now * .03 + (G.Seed & 127);
				const FVector2D Q = G.Home + FVector2D(FMath::Cos(A), FMath::Sin(A * 1.3)) * 9;
				if (Wet(Q.X, Q.Y)) Station = Q;
			}
			else if (G.Kind == NurseShark && E.Phase != 0) Speed = 0;   // asleep on the bottom by day; it cruises at night
			else if (G.Kind == NurseShark)
			{
				const double A = E.Now * .04 + (G.Seed & 127);
				const FVector2D Q = G.Home + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 10;
				if (Wet(Q.X, Q.Y)) Station = Q;
			}
			const FVector2D ToS = Station - G.C;
			const double Ls = ToS.Size();
			FVector2D GV = FVector2D::ZeroVector;
			if (Ls > .3 && Speed > 0)
			{
				GV = ToS / Ls * FMath::Min(Speed, Ls * 1.5);
				G.C += GV * Dt;
				// (gray snapper back off facing what they are wary of)
				if (!(bLie && G.Kind == GraySnapper)) G.Yaw += WrapPi(FMath::Atan2(GV.Y, GV.X) - G.Yaw) * FMath::Min(1.0, Dt * 2.5);
			}
			else if (bLie)
			{
				// (root life) lying still in the shade, nose to the current: along the shore, turned about with the tide
				const FVector2D Tn(-G.Out.Y, G.Out.X);
				const double Cur = ((G.Seed & 2) ? 1 : -1) * (E.TideRate < -1e-4 ? -1 : 1);
				G.Yaw += WrapPi(FMath::Atan2(Tn.Y * Cur, Tn.X * Cur) - G.Yaw) * FMath::Min(1.0, Dt * .8);
			}
			else if (G.Kind == Snook || G.Kind == Barracuda)
			{
				// lying still, head to the small fish
				if (const FSchoolE* Sc = FindSchool(E, G.Prey)) G.Yaw += WrapPi(FMath::Atan2(Sc->C.Y - G.C.Y, Sc->C.X - G.C.X) - G.Yaw) * FMath::Min(1.0, Dt);
			}
			const double Floor = FloorAt(G.C.X, G.C.Y), Depth = FMath::Max(.05, E.Sea - Floor);
			G.Depth = Depth;
			const bool bDraw = DCam < K.Cull;
			const double Gs = GV.Size();
			G.Spd = Gs;
			for (int32 i = 0; i < G.M.Num(); i++)
			{
				FSwim& M = G.M[i];
				M.Ph += Dt * (3 + Gs * 5);
				M.T -= Dt;
				// its place in the group, and how deep it swims
				double Y = E.Sea - FMath::Min(Depth * .55, .6);
				double Pitch = 0, Roll = 0, Amp = FMath::Min(1.0, .12 + Gs * .35), Freq = 3 + Gs * 5;
				FVector Scale(1);
				if (G.Kind == Needlefish) Y = E.Sea - .05;
				else if (G.Kind == NurseShark || G.Kind == Stingray || G.Kind == Horseshoe || G.Kind == Jelly) Y = FloorAt(G.Home.X + M.Off.X, G.Home.Y + M.Off.Y) + (G.Kind == Jelly ? .01 : G.Kind == Horseshoe ? .02 : .07);
				else if (G.Kind == LemonShark) Y = FMath::Max(Floor + .16, E.Sea - .32);
				else if (bLie && G.Kind == Snook) Y = Floor + FMath::Clamp(Depth * .35, .12, .55);
				else if (bLie && G.Kind == GraySnapper) Y = Floor + FMath::Clamp(Depth * (.3 + .35 * EHash(i + 3.0, G.Seed * 1e-6)), .15, FMath::Max(.15, Depth - .12));
				else if (bLie && G.Kind == Barracuda) Y = E.Sea - FMath::Min(.3, Depth * .4);
				else if (G.Kind == Snook) Y = FMath::Max(Floor + .12, E.Sea - .45);
				if (G.Kind == Jelly)
				{
					// it pulses where it lies
					const double Pu = FMath::Sin(E.Now * 1.6 + M.Ph);
					Scale = FVector(1 + .06 * Pu, 1 + .06 * Pu, 1 - .12 * Pu);
					M.P = FVector(G.Home.X + M.Off.X, Y, G.Home.Y + M.Off.Y);
					if (bDraw) Put(E, G.Kind, M.P, M.Fwd, 0, M.Len, 0, 0, 0, 0, 0, .9 + .2 * EHash(M.Ph, 1.0), M.Ph, Scale);
					continue;
				}
				// a mullet jumps: more when something is after it, or the boat goes by
				if (G.Kind == Mullet && M.Air <= 0 && M.T <= 0)
				{
					M.T = FMath::FRandRange(5.0, 40.0) / (DB < 25 && bBoatMoving ? 5 : NearestGroup(E, G.C, 14, Jack) || NearestGroup(E, G.C, 14, Snook) ? 4 : 1);
					M.Air = 1;
					M.V = FVector(M.Fwd.X * 2.4, 4.3 + FMath::FRand(), M.Fwd.Z * 2.4);   // (a metre and more clear of the water)
					M.P.Y = E.Sea;
					if (E.Life && DCam < 130) E.Life->Splash(M.P.X, M.P.Z, 6, .6);
				}
				// a tarpon rolls: up to the top, its back out, and down
				if (G.Kind == Tarpon && M.T <= 0) { M.T = FMath::FRandRange(14.0, 40.0) / FeedNow; M.Roll = 1.6; E.Sound(TEXT("roll"), G.Kind, M.P, .5f); if (E.Life && DCam < 110) E.Life->Splash(M.P.X, M.P.Z, 8, .9); }
				if (M.Air > 0)
				{
					M.V.Y -= 9.8 * Dt;
					M.P += M.V * Dt;
					Pitch = FMath::Atan2(M.V.Y, 2.2);
					if (M.P.Y < E.Sea - .05 && M.V.Y < 0) { M.Air = 0; E.Sound(TEXT("jump"), G.Kind, M.P, .5f); if (E.Life && DCam < 130) E.Life->Splash(M.P.X, M.P.Z, 18, 1.5); }   // (it lands flat: a column of white water and a patch of foam left behind)
				}
				else
				{
					if (M.Roll > 0) { M.Roll -= Dt; const double U = 1 - M.Roll / 1.6; Y = FMath::Lerp(Y, E.Sea + .04, FMath::Sin(PI * U)); Pitch = .5 * FMath::Cos(PI * U); }
					const double Cy = FMath::Cos(G.Yaw), Sy = FMath::Sin(G.Yaw);
					FVector Want(G.C.X + M.Off.X * Cy - M.Off.Y * Sy, Y, G.C.Y + M.Off.X * Sy + M.Off.Y * Cy);
					// (gray snapper drift about their places, finning)
					if (bLie && G.Kind == GraySnapper) Want += FVector(FMath::Sin(E.Now * .31 + i * 1.7), .3 * FMath::Sin(E.Now * .5 + i), FMath::Cos(E.Now * .27 + i * 2.3)) * .14;
					const FVector D = Want - M.P;
					const double Follow = G.State == GRush ? 6 : 2;
					M.P += D * FMath::Min(1.0, Dt * Follow);
					if (FMath::Abs(M.P.Y - Y) > 1.5) M.P.Y = Y;
					// a needlefish darts a body length now and then
					if (G.Kind == Needlefish && FMath::FRand() < Dt * .25) M.P += M.Fwd * .5;
				}
				FVector Head = FVector(FMath::Cos(G.Yaw), 0, FMath::Sin(G.Yaw));
				// (and each turns to eye a diver who comes within 7 m)
				if (bLie && G.Kind == GraySnapper && DDiver < 7) Head = FVector(E.Diver.X - M.P.X, 0, E.Diver.Z - M.P.Z).GetSafeNormal(.001, Head);
				FVector Fw = FMath::Lerp(M.Fwd, Head, FMath::Min(1.0, Dt * 3)).GetSafeNormal(.001, Head);
				M.Fwd = Fw;
				if (Pitch != 0) Fw = (FVector(Fw.X, 0, Fw.Z).GetSafeNormal() * FMath::Cos(Pitch) + FVector(0, FMath::Sin(Pitch), 0));
				if (G.Kind == NurseShark || G.Kind == Horseshoe || G.Kind == Stingray) { Amp *= G.Kind == Stingray ? 0 : .5; }
				if (bDraw) Put(E, G.Kind, M.P, Fw, Roll, M.Len, M.Ph, Freq, G.Kind == Horseshoe ? 0 : Amp, 0, 0, .9 + .2 * EHash(i + 1.0, G.Seed * 1e-6), EHash(i + 2.0, G.Seed * 1e-6));
			}
		}

		// ---- the crabs
		for (FCrabs& C : T.Crabs)
		{
			const FPoolSpec& K = SpecOf(C.Kind);
			if (FVector2D::DistSquared(C.C, Cam2) > FMath::Square(K.Cull + 6)) continue;
			const double DB = FVector2D::Distance(C.C, E.Boat);
			if (C.Kind == TreeCrab)
			{
				// up the root above the tide; when the boat comes close they let go and drop, and climb out again later
				if (!C.bGone && DB < 10 && E.BoatSpd > .3) { C.bGone = true; C.T = 0; E.Sound(TEXT("crabdrop"), C.Kind, FVector(C.C.X, E.Sea, C.C.Y), .2f); }
				else if (C.bGone && DB > 15) { C.T += Dt; if (C.T > 16) { C.bGone = false; C.Hide = 1; } }
				if (C.bGone) C.Hide = FMath::Min(2.0, C.Hide + Dt * 2.4); else C.Hide = FMath::Max(0.0, C.Hide - Dt * .12);
				for (int32 i = 0; i < C.N; i++)
				{
					const double H1 = EHash(C.Seed * 1e-6 + i, 1.0), H2 = EHash(C.Seed * 1e-6 + i, 2.0);
					const int32 Root = i % 2;
					// its height on the root: a slow wander, and never under water (down the root is toward the mud)
					double U = .12 + .3 * H1 + .08 * FMath::Sin(E.Now * .12 + H2 * 20);
					for (int32 k = 0; k < 6 && RootPoint(C.Site, Root, U).Y < E.Sea + .1; k++) U -= .06;
					U = FMath::Max(.02, U);
					const FVector On = RootPoint(C.Site, Root, U), Next = RootPoint(C.Site, Root, U + .05);
					FVector P = On;
					double Scale = 1;
					if (C.bGone)
					{
						// falling, each a moment after the last, into the water and gone
						const double Tf = FMath::Max(0.0, C.Hide * .42 - i * .08);
						P.Y = On.Y - 4.9 * Tf * Tf;
						if (P.Y < E.Sea) { if (P.Y > E.Sea - .12 && E.Life && C.Hide < 1.9) E.Life->Splash(P.X, P.Z, 2, .18); continue; }
					}
					else if (C.Hide > 0)
					{
						// climbing back out from the waterline
						double Uw = U;
						for (int32 k = 0; k < 12 && RootPoint(C.Site, Root, Uw).Y > E.Sea + .03; k++) Uw += .06;
						P = RootPoint(C.Site, Root, FMath::Lerp(U, FMath::Min(Uw, .95), FMath::Clamp(C.Hide, 0.0, 1.0)));
					}
					Put(E, C.Kind, P, (Next - On).GetSafeNormal(.001, FVector(1, 0, 0)) * (H2 < .5 ? 1 : -1), 0, FMath::Lerp(K.LenMin, K.LenMax, H1) * Scale, 0, 0, 0, 0, 0, .85 + .3 * H2, H1);
				}
			}
			else
			{
				// fiddlers: out on the mud while it is bare, down their burrows when the boat is near or the water is back
				const bool bBare = C.FloorY > E.Sea + .006;
				const bool bScared = DB < 11 && E.BoatSpd > .6;
				if (bScared || !bBare) { C.T = 0; C.Hide = FMath::Min(1.0, C.Hide + Dt * (bScared ? 3.5 : .5)); }
				else { C.T += Dt; if (C.T > 5) C.Hide = FMath::Max(0.0, C.Hide - Dt * .5); }
				if (C.Hide >= 1) continue;
				for (int32 i = 0; i < C.N; i++)
				{
					const double H1 = EHash(C.Seed * 1e-6 + i, 1.0), H2 = EHash(C.Seed * 1e-6 + i, 2.0), H3 = EHash(C.Seed * 1e-6 + i, 3.0);
					const double A = 2 * PI * H1, R = 1.7 * FMath::Sqrt(H2);
					// (the nearest to the boat go first, the rest a moment after)
					const double Mine = FMath::Clamp(C.Hide * 1.6 - H3 * .6, 0.0, 1.0);
					if (Mine >= 1) continue;
					const double Step = .05 * FMath::Sin(E.Now * .7 + H3 * 30);
					const FVector P(C.C.X + FMath::Cos(A) * R + Step, C.FloorY + .008 - Mine * .05, C.C.Y + FMath::Sin(A) * R);
					const double Wave = FMath::Max(0.0, FMath::Sin(E.Now * 3 + H3 * 40));   // it rears up to wave the big claw
					const double Yaw = 2 * PI * H3;
					Put(E, C.Kind, P + FVector(0, Wave * .012, 0), FVector(FMath::Cos(Yaw), Wave * .35, FMath::Sin(Yaw)), 0, FMath::Lerp(K.LenMin, K.LenMax, H2) * (1 - Mine), 0, 0, 0, 0, 0, .85 + .3 * H1, H1);
				}
			}
		}
	}

	// ---------------------------------------------------------------- what shows at the surface
	// Written into the birds' moving mesh (rebuilt every frame anyway). Every school awake and every fish near the top
	// writes the same number of triangles whatever it is doing (a wake with no way on is drawn shut), so the mesh's
	// layout only changes when one wakes or goes back to sleep.
	void EmitWaterMarks(FEco& E, FTileE& T)
	{
		FSoup& B = E.Live;
		const FVector2D Cam2(E.Cam.X, E.Cam.Z);
		auto Tri = [&B](const FVector& P0, const FVector& P1, const FVector& P2, const FLinearColor& C0, const FLinearColor& C1, const FLinearColor& C2)
		{
			const int32 I = B.V.Num();
			B.V.Add(G2U(P0)); B.V.Add(G2U(P1)); B.V.Add(G2U(P2));
			for (int32 k = 0; k < 3; k++) { B.N.Add(FVector(0, 0, 1)); B.T.Add(I + k); }
			B.C.Add(C0); B.C.Add(C1); B.C.Add(C2);
		};
		auto Surf = [&E](double X, double Z, double Amp) { return KeysGeo::WaveH(X, Z, E.Now, Amp, false); };
		static const FLinearColor Dark = Hex(0x173c46), Edge = Hex(0x3a7a84), Silver = Hex(0xf2f8fa), Foam = Hex(0xe4f0f2);
		// ---- a school: a dark, ragged patch of nervous water, and silver flashes on it
		for (const FSchoolE& S : T.Schools)
		{
			if (!S.bAwake) continue;
			const bool bShow = FVector2D::DistSquared(S.C, Cam2) < 115.0 * 115.0 && E.Sea - S.Floor > .1;
			const double Amp = bShow ? KeysGeo::AmpAt(S.C.X, S.C.Y) : 0;
			const double R = bShow ? S.R * (1 + S.Scatter * 1.3) * 1.2 + .35 : 0;
			const FVector Mid(S.C.X, bShow ? Surf(S.C.X, S.C.Y, Amp) + .035 : E.Sea, S.C.Y);
			FVector Rim[12];
			for (int32 i = 0; i < 12; i++)
			{
				const double A = i * PI / 6, Rr = R * (.72 + .4 * EHash(i + 1.0, S.Seed * 1e-6) + .1 * FMath::Sin(E.Now * 1.3 + i * 2.1 + S.Mill));
				const double X = S.C.X + FMath::Cos(A) * Rr, Z = S.C.Y + FMath::Sin(A) * Rr;
				Rim[i] = FVector(X, bShow ? Surf(X, Z, Amp) + .03 : E.Sea, Z);
			}
			for (int32 i = 0; i < 12; i++) Tri(Mid, Rim[i], Rim[(i + 1) % 12], Dark, Edge, Edge);
			// (twelve places a fish may show its silver side this third of a second: each a sliver lying on the water)
			const double Beat = FMath::FloorToDouble(E.Now * 3.2);
			for (int32 j = 0; j < 12; j++)
			{
				const double H1 = EHash(Beat + j * 7.3, S.Seed * 1e-6 + j), H2 = EHash(Beat * 1.7 + j, S.Seed * 1e-6 + 3.0), H3 = EHash(Beat + j, 5.0 + S.Seed * 1e-6);
				const bool bOn = bShow && H3 < .45 + S.Scatter * .5;
				const double A = 2 * PI * H1, Rr = R * .85 * FMath::Sqrt(H2), L = bOn ? .22 + .2 * H2 : 0;
				const double X = S.C.X + FMath::Cos(A) * Rr, Z = S.C.Y + FMath::Sin(A) * Rr;
				const FVector P(X, bOn ? Surf(X, Z, Amp) + .07 : E.Sea, Z);
				const FVector Ax(FMath::Cos(A * 3.1) * L, 0, FMath::Sin(A * 3.1) * L), Sd(-Ax.Z * .4, .04, Ax.X * .4);
				Tri(P - Ax, P + Sd, P + Ax, Silver, Silver, Silver);
				Tri(P - Ax, P - Sd, P + Ax, Silver, Silver, Silver);
			}
		}
		// ---- a fish under way near the top: a V of wake from its nose, longer the faster it goes
		for (const FGroup& G : T.Groups)
		{
			if (!G.bActive || !(IsHunter(G.Kind) || G.Kind == Mullet || G.Kind == Needlefish)) continue;
			const double Amp = KeysGeo::AmpAt(G.C.X, G.C.Y);
			for (const FSwim& M : G.M)
			{
				const bool bOn = G.Spd > .35 && M.Air <= 0 && M.P.Y > E.Sea - .75;
				const double L = bOn ? FMath::Clamp(G.Spd * 1.1, .5, 6.5) * (G.Kind == Needlefish ? .5 : 1) : 0, W = bOn ? .07 + L * .035 : 0;
				const FVector F2 = FVector(M.Fwd.X, 0, M.Fwd.Z).GetSafeNormal(.001, FVector(1, 0, 0)), Rt(-F2.Z, 0, F2.X);
				const FVector Nose(M.P.X + F2.X * M.Len * .4, bOn ? Surf(M.P.X, M.P.Z, Amp) + .04 : E.Sea, M.P.Z + F2.Z * M.Len * .4);
				for (const double Sd : {-1.0, 1.0})
				{
					const FVector Tail = Nose - F2 * L + Rt * Sd * L * .36;
					const FVector Tl(Tail.X, bOn ? Surf(Tail.X, Tail.Z, Amp) + .04 : E.Sea, Tail.Z);
					Tri(Nose, Tl - Rt * Sd * W, Tl + Rt * Sd * W, Foam, Foam, Foam);
				}
			}
		}
	}

	// ---------------------------------------------------------------- the instances, laid out for this frame
	void FlushPools(AKeysMangroveEco* A, FEco& E)
	{
		// (the meshes are made one a frame from the start, before anything needs them: making one takes a few ms)
		for (int32 Ix = 0; Ix < NumPools; Ix++) if (!E.Pools[Ix].bTried) { EnsurePool(A, E, Ix); break; }
		for (int32 Ix = 0; Ix < NumPools; Ix++)
		{
			FPoolE& Pl = E.Pools[Ix];
			const int32 N = Pl.Xf.Num();
			if (!N && !Pl.Shown) continue;
			// laying a kind's instances out again costs the same however few they are, so each kind is laid out every
			// other frame (turn about), and the ones that hardly move (jellyfish, crabs, a sleeping shark) every eighth
			const int32 Kind = Ix + Silverside, Every = Kind == Jelly || Kind == TreeCrab || Kind == Fiddler || Kind == Horseshoe || Kind == NurseShark ? 8 : 2;
			if ((GFrameCounter + Ix) % Every != 0 && N <= Pl.Shown + 8) continue;
			UInstancedStaticMeshComponent* C = EnsurePool(A, E, Ix);
			if (!C) { Pl.Shown = 0; continue; }
			const int32 Have = C->GetInstanceCount();
			if (N > Have)
			{
				TArray<FTransform> More;
				More.Append(Pl.Xf.GetData() + Have, N - Have);
				C->AddInstances(More, false, true);
			}
			// what was drawn last frame and is not this frame is shrunk to nothing where it was
			const int32 Was = FMath::Min(Pl.Shown, C->GetInstanceCount());
			for (int32 i = N; i < Was; i++) Pl.Xf.Add(FTransform(FQuat::Identity, i < N ? FVector::ZeroVector : G2U(E.Cam), FVector(.0001)));
			if (Pl.Xf.Num()) C->BatchUpdateInstancesTransforms(0, Pl.Xf, true, false, false);
			for (int32 i = 0; i < N; i++) C->SetCustomData(i, TArrayView<const float>(Pl.Data.GetData() + i * 8, 8), false);
			C->MarkRenderStateDirty();
			Pl.Shown = N;
			E.FishDrawn += N;
		}
	}
}
