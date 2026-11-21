// The boats and the clutter of the dry stack and the boatyard: the invented fleet (flats skiffs, bay boats, centre
// consoles and one sportfisher, all the game's own lofted hull in different proportions), shrink wrap, jack stands and
// keel blocks, and the props a working yard collects (paint buckets, old outboards on stands, a trailer, drums, the
// travel lift). KeysYards.cpp places them; nothing here knows where the yard is.
#include "KeysYardProps.h"
#include "KeysHuman.h"

namespace KeysYardProps
{
	using KeysMesh::FHullDef;
	using KeysMesh::FHullSta;

	FLinearColor Hex(uint32 C) { return FLinearColor(FColor(uint8((C >> 16) & 255), uint8((C >> 8) & 255), uint8(C & 255))); }
	double Hash(double A, double B) { const double H = FMath::Sin(A * 127.1 + B * 311.7) * 43758.5453; return H - FMath::FloorToDouble(H); }

	void Append(FKeysMesh& Dst, const FKeysMesh& Src, const FTransform& T)
	{
		Dst.SyncKinds();
		const int32 Base = Dst.V.Num();
		const FVector Tr = T.GetTranslation() * 100.0;
		const FQuat Q = T.GetRotation();
		const FVector Sc = T.GetScale3D();
		for (int32 i = 0; i < Src.V.Num(); i++)
		{
			Dst.V.Add(Tr + Q.RotateVector(Src.V[i] * Sc));
			Dst.N.Add(Q.RotateVector(Src.N[i]));
			Dst.UV.Add(Src.UV[i]);
			Dst.C.Add(Src.C[i]);
			Dst.K.Add(Src.K.IsValidIndex(i) ? Src.K[i] : FVector2D::ZeroVector);
		}
		for (int32 i : Src.T) Dst.T.Add(Base + i);
	}

	void Box(FKeysMesh& M, double CX, double CY, double CZ, double SX, double SY, double SZ, const FLinearColor& C, double YawRad)
	{
		M.AddBox(FVector(CX, CY, CZ), FVector(SX, SY, SZ), C, YawRad != 0 ? FQuat(FVector::ZAxisVector, YawRad) : FQuat::Identity);
	}

	void Wheel(FKeysMesh& M, const FVector& A, const FVector& B, double R, const FLinearColor& C, int32 Sides)
	{
		const FVector Ax = (B - A).GetSafeNormal() * .002;
		M.AddTube(A, B, R, R, Sides, C);
		M.AddTube(A, A - Ax, R, 0, Sides, C);
		M.AddTube(B, B + Ax, R, 0, Sides, C);
	}

	namespace
	{
		void Tube(FKeysMesh& M, const FVector& A, const FVector& B, double R, const FLinearColor& C, int32 Sides = 6) { M.AddTube(A, B, R, R, Sides, C); }
		// a flat strap from A to B, W wide across Side
		void Strap(FKeysMesh& M, const FVector& A, const FVector& B, const FVector& Side, double W, const FLinearColor& C)
		{
			const FVector S = Side.GetSafeNormal() * W * .5;
			const FVector Nn = FVector::CrossProduct(B - A, S).GetSafeNormal();
			M.AddQuad(A - S, A + S, B + S, B - S, C, Nn);
			M.AddQuad(A - S, A + S, B + S, B - S, C, -Nn);
		}
		FHullSta StaAtX(const FHullDef& H, const FBoat& B, double X) { return KeysMesh::HullStationAtZ(H, B.L, B.B, -X); }
	}

	// ---------------------------------------------------------------- the fleet
	FHullDef HullOf(const FBoat& B)
	{
		FHullDef H;
		switch (B.Kind)
		{
		case BoatSkiff: H.F = .46; H.Spring = .1; H.YK = .2; H.Dr0 = 5; H.Dr1 = 32; H.Rake = .8; H.Entry = .5; break;
		case BoatBay: H.F = .72; H.Spring = .22; H.YK = .3; H.Dr0 = 13; H.Dr1 = 42; H.Rake = 1.0; H.Entry = .45; break;
		case BoatSport: H.F = 1.75; H.Spring = .5; H.YK = .75; H.Dr0 = 16; H.Dr1 = 46; H.Rake = 1.6; H.Entry = .4; H.Flare = .06; break;
		default: H.F = 1.1; H.Spring = .35; H.YK = .5; H.Dr0 = 20; H.Dr1 = 48; H.Rake = 1.2; H.Entry = .42; break;
		}
		H.Hull = Hex(B.Hull); H.Cap = Hex(B.Hull); H.Boot = Hex(B.Accent); H.Bottom = Hex(B.Bottom);
		H.bCove = B.Kind == BoatCC || B.Kind == BoatSport;
		H.Cove = Hex(B.Accent);
		return H;
	}

	FBoat MakeBoat(int32 Seed, double MaxL, bool bNoSkiff)
	{
		static const uint32 HullC[] = {0xffffff, 0xf6f1e4, 0xe9eef2, 0xa7e6da, 0xbfd9ee, 0x1f2a44, 0xf2c14e, 0xd9dde2, 0xfbfaf6, 0x9ec7b4, 0xffffff, 0x2f4f6f, 0xf3d9c4, 0xffffff};
		static const uint32 AccC[] = {0x23466e, 0x2bb3a8, 0xe4572e, 0x14181f, 0xc8102e, 0x0f6e7a, 0xc8a24a, 0x1f4f8c, 0x3f8a5a, 0x15171a};
		static const uint32 BotC[] = {0x1f3a5c, 0x15171a, 0x8a2a2a, 0x1f4f8c, 0x23513f, 0x2a2d31};
		static const uint32 CowlC[] = {0xf2f2f2, 0x15171a, 0xf2f2f2, 0x2a2d31, 0xd9dde2};
		// invented makers and boat names
		static const TCHAR* Brands[] = {TEXT("Reefwright"), TEXT("Tidewell"), TEXT("Gumbo Limbo"), TEXT("Cowfish"), TEXT("Buttonwood"), TEXT("Hardbottom")};
		static const TCHAR* Names[] = {TEXT("Knot Today"), TEXT("Slack Tide"), TEXT("Second Wind"), TEXT("Lazy Mullet"), TEXT("Grunt Work"), TEXT("Low Key"), TEXT("Pin Fish"),
			TEXT("Shallow Minded"), TEXT("Sea Grape"), TEXT("Bight Me"), TEXT("Tarpon Tax"), TEXT("Mangrove Mule"), TEXT("Conch Out"), TEXT("Skinny Dip"), TEXT("Chum Bucket"),
			TEXT("Wrasse Hole"), TEXT("Permit Pending"), TEXT("Ballyhoo"), TEXT("Tide Loan"), TEXT("Snook Nook"), TEXT("Goliath Creek Kid"), TEXT("Marl Barker"), TEXT("Day Rate"),
			TEXT("Back Country"), TEXT("Sandspur"), TEXT("Hog Wild"), TEXT("Patch Reef"), TEXT("Yellowtail Fever"), TEXT("Sunday Money"), TEXT("Bar Jack"), TEXT("Rip Line"),
			TEXT("Coral Fee"), TEXT("Two Stroke Joe"), TEXT("Gulf Rascal"), TEXT("Fly Rod Fred"), TEXT("Ebb Lasso"), TEXT("Mudflat Mary"), TEXT("Poling Around"), TEXT("Bait Thief"),
			TEXT("Seven Mile Smile"), TEXT("Noseeum"), TEXT("Lobster Mobster"), TEXT("Reel Quiet"), TEXT("Channel Five"), TEXT("Stone Crab Stan"), TEXT("Wahoo Dunnit")};
		FBoat B;
		const double R = Hash(Seed, 1.7), R2 = Hash(Seed * 1.3, 4.1), R3 = Hash(Seed, 9.3);
		if (R < .24 && !bNoSkiff) { B.Kind = BoatSkiff; B.L = 5.1 + .5 * R2; B.B = 1.95; B.Engines = 1; B.bTop = false; }
		else if (R < .55) { B.Kind = BoatBay; B.L = 6.5 + .9 * R2; B.B = 2.45; B.Engines = 1; B.bTop = R3 < .5; }
		else { B.Kind = BoatCC; B.L = FMath::Min(MaxL, 7.3 + 1.9 * R2); B.B = FMath::Min(2.85, B.L * .32); B.Engines = B.L > 8.5 ? 3 : 2; B.bTop = R3 < .85; }
		B.Hull = HullC[int32(Hash(Seed, 2.9) * 14) % 14];
		B.Accent = AccC[int32(Hash(Seed, 5.3) * 10) % 10];
		B.Bottom = BotC[int32(Hash(Seed, 6.1) * 6) % 6];
		B.Cowl = CowlC[int32(Hash(Seed, 7.7) * 5) % 5];
		B.Top = Hash(Seed, 8.9) < .6 ? 0xfbfaf6 : B.Accent;
		B.Name = Names[int32(Hash(Seed, 3.3) * 46) % 46];
		B.Brand = Brands[int32(Hash(Seed, 11.1) * 6) % 6];
		return B;
	}

	double KeelZ(const FBoat& B) { const FHullDef H = HullOf(B); return KeysMesh::HullStation(H, B.L, B.B, .3).YK; }
	double BottomZ(const FBoat& B, double Off)
	{
		const FHullDef H = HullOf(B);
		const FHullSta S = KeysMesh::HullStation(H, B.L, B.B, .3);
		return S.YK + (S.YC - S.YK) * FMath::Min(1.0, FMath::Abs(Off) / FMath::Max(.05, S.Bb));
	}
	double SheerZ(const FBoat& B) { return HullOf(B).F; }
	double TopZ(const FBoat& B)
	{
		const FHullDef H = HullOf(B);
		const double Sole = KeysMesh::HullSole(B.L, KeysMesh::HullStation(H, B.L, B.B, .45));
		if (B.bWrapped) return H.F + (B.Kind == BoatSkiff ? .7 : 1.5);
		switch (B.Kind)
		{
		case BoatSkiff: return H.F + 1.25;
		case BoatSport: return H.F + 3.4;
		case BoatBay: return B.bTop ? Sole + 2.15 : Sole + 1.5;
		default: return B.bTop ? Sole + 2.2 : Sole + 1.75;
		}
	}

	namespace
	{
		// KeysMesh::BuildHullLoft leaves the top edge of the topsides (the rub rail's two corners of each panel) with
		// no beam: its tumblehome term takes a root of sin(pi), which comes out a hair below zero. Those corners belong
		// at the sheer's half-beam; put them there and give the panel its normal back, or the mesh has no bounds.
		void MendSheer(FKeysMesh& M, int32 V0, const FHullDef& H, const FBoat& B)
		{
			auto Bad = [&](int32 i) { return M.V[i].ContainsNaN(); };
			for (int32 q = V0; q + 3 < M.V.Num(); q += 4)
			{
				if (!Bad(q + 1) && !Bad(q + 2)) continue;
				if (Bad(q) || Bad(q + 3)) continue;
				const int32 Tq = M.T.Find(q);
				if (Tq == INDEX_NONE || Tq + 5 >= M.T.Num() || M.T[Tq + 3] != q) continue;   // not one of the loft's panels
				const double Side = M.V[q].Y + M.V[q + 3].Y >= 0 ? 1 : -1;
				for (int32 i = q + 1; i <= q + 2; i++)
				{
					// the station whose sheer stands at this X
					const double X = M.V[i].X / 100;
					double T0 = 0, T1 = 1;
					for (int32 It = 0; It < 24; It++)
					{
						const double Tm = (T0 + T1) / 2;
						const FHullSta S = KeysMesh::HullStation(H, B.L, B.B, Tm);
						if (-KeysMesh::HullZRake(H, S, S.YS) < X) T0 = Tm; else T1 = Tm;
					}
					M.V[i].Y = Side * KeysMesh::HullStation(H, B.L, B.B, (T0 + T1) / 2).Bs * 100;
				}
				FVector Nn = FVector::CrossProduct(M.V[q + 2] - M.V[q], M.V[q + 1] - M.V[q + 3]).GetSafeNormal();
				if (Nn.Y * Side < 0) Nn = -Nn;
				if (Nn.IsNearlyZero()) Nn = FVector(0, Side, 0);
				for (int32 i = q; i <= q + 3; i++) M.N[i] = Nn;
				// wind it to face out, as AddQuad would have
				const int32 Ti = Tq;
				{
					const bool bFlip = FVector::DotProduct(FVector::CrossProduct(M.V[q + 1] - M.V[q], M.V[q + 2] - M.V[q]), Nn) > 0;
					const int32 Idx[6] = {q, bFlip ? q + 2 : q + 1, bFlip ? q + 1 : q + 2, q, bFlip ? q + 3 : q + 2, bFlip ? q + 2 : q + 3};
					for (int32 k = 0; k < 6; k++) M.T[Ti + k] = Idx[k];
				}
			}
			// and the two corners of the transom that share that edge
			const double Bs0 = KeysMesh::HullStation(H, B.L, B.B, 0).Bs * 100;
			for (int32 t = 0; t + 2 < M.T.Num(); t += 3)
			{
				if (M.T[t] < V0) continue;
				for (int32 k = 0; k < 3; k++)
				{
					const int32 i = M.T[t + k];
					if (!Bad(i)) continue;
					double Sum = 0;
					for (int32 j = 0; j < 3; j++) if (j != k && !Bad(M.T[t + j])) Sum += M.V[M.T[t + j]].Y;
					M.V[i].Y = (Sum >= 0 ? 1 : -1) * Bs0;
					for (int32 j = 0; j < 3; j++) M.N[M.T[t + j]] = FVector(-1, 0, 0);
				}
			}
		}

		void Engines(FKeysMesh& M, const FBoat& B, const FHullDef& H)
		{
			const double XT = -B.L / 2;
			const int32 NE = FMath::Max(1, B.Engines);
			const double EW = B.Kind == BoatSkiff ? .44 : B.Kind == BoatBay ? .56 : .6;
			const double ESp = FMath::Min(.66, B.B * .8 / NE);
			const double CavZ = -H.YK * .95 - .1, Zm = CavZ + .76;
			for (int32 e = 0; e < NE; e++)
			{
				const double Y = (e - (NE - 1) * .5) * ESp;
				if (B.bNoEngines)
				{
					// where she hung: a cleaner square of gelcoat, four bolt holes, and the rigging tube left hanging
					M.Surf = SurfDefault;
					Box(M, XT - .006, Y, Zm - .12, .012, EW * .62, .5, Hex(B.Hull) * .9f);
					for (int32 a = -1; a <= 1; a += 2) for (int32 b = -1; b <= 1; b += 2) Box(M, XT - .014, Y + a * EW * .2, Zm - .12 + b * .17, .012, .035, .035, Hex(0x15171a));
					const FVector Top(XT + .12, Y + EW * .12, H.F - .02);
					Tube(M, Top, FVector(XT - .12, Y + EW * .1, H.F - .1), .035, Hex(0x1c1c1e), 6);
					Tube(M, FVector(XT - .12, Y + EW * .1, H.F - .1), FVector(XT - .16, Y + EW * .05, H.F - .75), .035, Hex(0x1c1c1e), 6);
					Tube(M, FVector(XT - .16, Y + EW * .05, H.F - .75), FVector(XT - .13, Y - EW * .1, H.F - .95), .012, Hex(0xb8322a), 4);
					Tube(M, FVector(XT - .16, Y + EW * .05, H.F - .75), FVector(XT - .2, Y + EW * .2, H.F - 1.0), .012, Hex(0x2a2d31), 4);
					continue;
				}
				KeysMesh::FOutboard O;
				O.Mount = FVector(XT, Y, Zm);
				O.W = EW;
				O.CavZ = CavZ;
				O.TopZ = Zm + .08 + FMath::Clamp(EW / .7, .8, 1.05) * 1.15;
				O.Cowl = Hex(B.Cowl);
				O.Panel = Hex(0x101114);
				O.Accent = Hex(B.Accent);
				O.Lower = B.Cowl == 0xf2f2f2 ? Hex(0xe6e6e4) : Hex(0x17181b);
				KeysMesh::BuildOutboard(M, O);
			}
			M.Surf = SurfDefault;
		}

		void TTop(FKeysMesh& M, double X, double Sole, double TopZ0, double Lt, double Wt, double LegX, double LegY, const FLinearColor& Top)
		{
			M.Surf = SurfSteel;
			for (int32 a = -1; a <= 1; a += 2) for (int32 b = -1; b <= 1; b += 2)
				Tube(M, FVector(X + a * LegX, b * LegY, Sole + .1), FVector(X + a * LegX * 1.1, b * Wt * .42, TopZ0), .03, Hex(0xd9dde2), 6);
			M.Surf = SurfGelcoat;
			M.AddRoundBox(FVector(X, 0, TopZ0 + .04), FVector(Lt, Wt, .09), .035, Top);
			M.Surf = SurfDefault;
		}

		void ShrinkWrap(FKeysMesh& M, const FBoat& B, const FHullDef& H)
		{
			// pulled tight from the rub rail up over a ridge pole: highest over the console, falling to the bow and stern
			const FLinearColor W = Hex(0xf3f6f9), Seam = Hex(0xd8dee6);
			const double Peak = B.Kind == BoatSkiff ? .7 : 1.5;
			const int32 NS = 14;
			TArray<FVector> Pts;
			const int32 NU = 7;
			for (int32 i = 0; i <= NS; i++)
			{
				const double T = .0 + double(i) / NS;
				const FHullSta S = KeysMesh::HullStation(H, B.L, B.B, T);
				const double Xr = -KeysMesh::HullZRake(H, S, S.YS);
				const double Hump = FMath::Pow(FMath::Max(0.0, FMath::Sin(PI * FMath::Clamp(.12 + T * .88, 0.0, 1.0))), .7);
				const double Rz = S.YS + .12 + Peak * Hump;
				const double Bs = S.Bs + .03;
				for (int32 u = 0; u < NU; u++)
				{
					const double A = double(u) / (NU - 1);           // 0 port rail .. 1 starboard rail
					const double Sd = A * 2 - 1;
					// straight panels from the rail to the shoulder, then over the ridge
					const double Y = FMath::Abs(Sd) > .67 ? Bs * FMath::Sign(Sd) : Bs * Sd / .67 * .9;
					const double Zz = FMath::Abs(Sd) > .99 ? S.YS - .14 : FMath::Abs(Sd) > .6 ? S.YS + .1 + (Rz - S.YS) * .45 : Rz - (Rz - S.YS) * .2 * FMath::Abs(Sd) / .6;
					Pts.Add(FVector(Xr, Y, Zz));
				}
			}
			M.Surf = SurfVinyl;
			M.AddGrid(Pts, NU, NS + 1, false, W, FVector(0, 0, -.5));
			// the transom end, closed flat
			{
				const FHullSta S = KeysMesh::HullStation(H, B.L, B.B, 0);
				const double X = -B.L / 2 - .02;
				for (int32 u = 0; u < NU - 1; u++)
					M.AddTri(Pts[u], Pts[u + 1], FVector(X, 0, S.YS - .14), W, FVector(-1, 0, 0));
			}
			// tape seams round her and a blue zip door on the port quarter, two vents
			M.Surf = SurfDefault;
			const FHullSta Sm = KeysMesh::HullStation(H, B.L, B.B, .3);
			for (double T : {.25, .5, .75})
			{
				const int32 i = int32(T * NS);
				for (int32 u = 0; u < NU - 1; u++)
				{
					const FVector A = Pts[i * NU + u], Bq = Pts[i * NU + u + 1];
					const FVector Out = FVector(0, (A.Y + Bq.Y) * .5, .4).GetSafeNormal() * .012;
					Strap(M, A + Out, Bq + Out, FVector(1, 0, 0), .07, Seam);
				}
			}
			const double Dx = -B.L * .18;
			Box(M, Dx, -(Sm.Bs + .045), Sm.YS + .32, .62, .02, .72, Hex(0x2f6fd0));
			Box(M, Dx, -(Sm.Bs + .055), Sm.YS + .32, .5, .02, .6, W);
			for (double Vx : {-B.L * .32, B.L * .2}) Box(M, Vx, Sm.Bs * .86, Sm.YS + .32 + Peak * .22, .16, .05, .12, Hex(0xb9c0c8));
		}
	}

	void BuildBoat(FKeysMesh& M, const FBoat& B)
	{
		const FHullDef H = HullOf(B);
		const int32 V0 = M.V.Num();
		M.Surf = SurfGelcoat;
		KeysMesh::BuildHullLoft(M, H, B.L, B.B, !B.bWrapped);
		M.Surf = SurfDefault;
		if (B.bSanded)
		{
			// the bottom paint sanded back: bands of grey primer, old paint gone chalky, and gelcoat showing through
			const FLinearColor Bot = Hex(B.Bottom), Primer = Hex(0xb4b0a6), Gel = Hex(B.Hull) * .92f, Chalk = FMath::Lerp(Bot, FLinearColor::White, .42f);
			M.SyncKinds();
			for (int32 i = V0; i < M.V.Num(); i++)
			{
				if (M.V[i].Z > 1.5) continue;
				const FLinearColor Cc = M.C[i];
				if (FMath::Abs(Cc.R - Bot.R) + FMath::Abs(Cc.G - Bot.G) + FMath::Abs(Cc.B - Bot.B) > .06f) continue;
				const double Hh = Hash(FMath::FloorToDouble(M.V[i].X / 38.0), FMath::FloorToDouble((FMath::Abs(M.V[i].Y) + M.V[i].Z * 2) / 30.0) + (M.V[i].Y > 0 ? 7 : 0));
				M.C[i] = Hh > .58 ? Primer : Hh > .34 ? Chalk : Hh < .13 ? Gel : Bot * .8f;
				M.K[i].X = SurfDefault;
			}
		}
		MendSheer(M, V0, H, B);
		if (B.bWrapped) { ShrinkWrap(M, B, H); return; }
		auto Sole = [&](double X) { return KeysMesh::HullSole(B.L, StaAtX(H, B, X)); };
		const FLinearColor White = Hex(0xf4f4f2), Steel = Hex(0xd9dde2), Cush = Hex(0xf1eee6), Dark = Hex(0x1b232b);
		switch (B.Kind)
		{
		case BoatSkiff:
		{
			// a side console, a casting deck forward, a poling platform over the engine and the push pole along the gunwale
			const double S0 = Sole(-B.L * .08);
			M.Surf = SurfGelcoat;
			M.AddRoundBox(FVector(-B.L * .06, .32, S0 + .36), FVector(.42, .5, .72), .06, White);
			Box(M, B.L * .2, 0, H.F - .04, B.L * .3, B.B * .62, .06, Hex(B.Hull) * .96f);
			M.Surf = SurfVinyl;
			Box(M, -B.L * .2, 0, S0 + .42, .34, .9, .1, Cush);
			M.Surf = SurfSteel;
			const double Px = -B.L / 2 + .15, Pz = H.F + 1.15;
			for (int32 a = -1; a <= 1; a += 2) for (int32 b = -1; b <= 1; b += 2) Tube(M, FVector(Px + .3 + a * .42, b * .55, H.F), FVector(Px + .1 + a * .3, b * .36, Pz), .022, Steel, 5);
			M.Surf = SurfNonSkid;
			Box(M, Px + .1, 0, Pz + .02, .72, .86, .04, White);
			M.Surf = SurfDefault;
			Tube(M, FVector(-B.L * .42, -B.B * .4, H.F + .08), FVector(B.L * .44, -B.B * .16, H.F + .2), .016, Hex(0x1c1c1e), 5);
			break;
		}
		case BoatBay:
		{
			const double Cx = B.L * .02, S0 = Sole(Cx);
			M.Surf = SurfGelcoat;
			M.AddRoundBox(FVector(Cx, 0, S0 + .5), FVector(B.L * .1, B.B * .3, 1.0), .07, White);
			Box(M, B.L * .26, 0, H.F - .05, B.L * .26, B.B * .6, .06, Hex(B.Hull) * .96f);
			Box(M, -B.L * .36, 0, H.F - .05, B.L * .16, B.B * .78, .06, Hex(B.Hull) * .96f);
			M.Surf = SurfGlass;
			Box(M, Cx + B.L * .045, 0, S0 + 1.16, .03, B.B * .28, .32, Dark, 0);
			M.Surf = SurfSteel;
			for (int32 a = -1; a <= 1; a += 2) Tube(M, FVector(Cx - B.L * .11, a * .32, S0), FVector(Cx - B.L * .11, a * .32, S0 + .88), .028, Steel, 6);
			M.Surf = SurfVinyl;
			M.AddRoundBox(FVector(Cx - B.L * .11, 0, S0 + .93), FVector(.34, .86, .12), .04, Cush);
			M.Surf = SurfDefault;
			if (B.bTop) TTop(M, Cx - B.L * .03, S0, S0 + 2.0, B.L * .24, B.B * .66, B.L * .07, B.B * .17, Hex(B.Top));
			// a trolling motor stowed on the bow
			Tube(M, FVector(B.L * .4, .25, H.F + .16), FVector(B.L * .14, .5, H.F + .16), .022, Hex(0x15171a), 5);
			Box(M, B.L * .4, .25, H.F + .16, .26, .16, .16, Hex(0x15171a));
			break;
		}
		case BoatSport:
		{
			// an express sportfisher: the house with its dark window band, a bridge with a hardtop on pipework, a cockpit aft
			const double Hx = B.L * .08, Hl = B.L * .34, Hw = B.B * .78;
			M.Surf = SurfGelcoat;
			M.AddRoundBox(FVector(Hx, 0, H.F + .62), FVector(Hl, Hw, 1.3), .12, White);
			M.AddRoundBox(FVector(Hx + Hl * .62, 0, H.F + .28), FVector(Hl * .5, Hw * .86, .6), .12, White);
			M.Surf = SurfGlass;
			M.AddRoundBox(FVector(Hx + .02, 0, H.F + .9), FVector(Hl + .04, Hw + .03, .42), .1, Dark);
			M.Surf = SurfGelcoat;
			Box(M, Hx - Hl * .1, 0, H.F + 1.32, Hl * .7, Hw * .9, .1, White);
			M.AddRoundBox(FVector(Hx - Hl * .02, 0, H.F + 1.75), FVector(.7, Hw * .5, .8), .08, White);
			TTop(M, Hx - Hl * .1, H.F + 1.3, H.F + 3.3, Hl * .66, Hw * .92, Hl * .26, Hw * .4, White);
			M.Surf = SurfSteel;
			for (int32 a = -1; a <= 1; a += 2) Tube(M, FVector(Hx - Hl * .5, a * Hw * .5, H.F + 1.3), FVector(B.L * .42, a * B.B * .2, H.F + .75), .02, Steel, 5);   // bow rail
			M.Surf = SurfDefault;
			break;
		}
		default:
		{
			const double Cx = B.L * .04, S0 = Sole(Cx);
			M.Surf = SurfGelcoat;
			M.AddRoundBox(FVector(Cx, 0, S0 + .6), FVector(B.L * .12, B.B * .34, 1.2), .08, White);
			M.AddRoundBox(FVector(Cx + B.L * .1, 0, S0 + .28), FVector(B.L * .08, B.B * .3, .56), .07, White);
			M.Surf = SurfGlass;
			Box(M, Cx + B.L * .05, 0, S0 + 1.42, .03, B.B * .32, .42, Dark);
			M.Surf = SurfSteel;
			for (int32 a = -1; a <= 1; a += 2) Tube(M, FVector(Cx - B.L * .12, a * .36, S0), FVector(Cx - B.L * .12, a * .36, S0 + .9), .03, Steel, 6);
			M.Surf = SurfVinyl;
			M.AddRoundBox(FVector(Cx - B.L * .12, 0, S0 + .96), FVector(.4, .96, .14), .05, Cush);
			M.AddRoundBox(FVector(-B.L / 2 + .42, 0, S0 + .34), FVector(.42, B.B * .56, .12), .04, Cush);
			M.Surf = SurfDefault;
			if (B.bTop) TTop(M, Cx - B.L * .03, S0, S0 + 2.08, B.L * .28, B.B * .72, B.L * .08, B.B * .19, Hex(B.Top));
			break;
		}
		}
		Engines(M, B, H);
	}

	// ---------------------------------------------------------------- ashore: keel blocks and jack stands
	double BlockedHeight(const FBoat& B) { return (B.Kind == BoatSport ? .62 : .46) - KeelZ(B); }

	void JackStand(FKeysMesh& M, const FVector& At, double Hh, double TiltYaw)
	{
		static const uint32 Paint[] = {0xd9772b, 0x2f5fa8, 0xc9b23a};
		const FLinearColor C = Hex(Paint[int32(Hash(At.X, At.Y) * 3) % 3]) * .85f, Steel = Hex(0x8d9499);
		M.Surf = SurfMetalPaint;
		const double Collar = FMath::Max(.3, Hh - .32);
		const FVector Top = At + FVector(0, 0, Collar);
		for (int32 k = 0; k < 3; k++)
		{
			const double A = TiltYaw + 2 * PI * k / 3;
			const FVector Foot = At + FVector(FMath::Cos(A), FMath::Sin(A), 0) * (.3 + Hh * .08);
			Tube(M, Foot, Top, .022, C, 5);
			const double A2 = TiltYaw + 2 * PI * (k + 1) / 3;
			const FVector Foot2 = At + FVector(FMath::Cos(A2), FMath::Sin(A2), 0) * (.3 + Hh * .08);
			Tube(M, FMath::Lerp(Foot, Top, .3), FMath::Lerp(Foot2, Top, .3), .012, C, 4);
		}
		M.Surf = SurfSteel;
		Tube(M, Top - FVector(0, 0, .12), At + FVector(0, 0, Hh - .04), .018, Steel, 5);
		Tube(M, Top + FVector(-.1, 0, .02), Top + FVector(.1, 0, .02), .01, Steel, 4);
		M.Surf = SurfWood;
		M.AddBox(At + FVector(0, 0, Hh - .02), FVector(.26, .2, .05), Hex(0x8c6f4a), FQuat(FVector::ZAxisVector, TiltYaw));
		M.Surf = SurfDefault;
	}

	void JackStandStack(FKeysMesh& M, const FVector& At, int32 Num)
	{
		for (int32 i = 0; i < Num; i++) JackStand(M, At + FVector(0, 0, i * .17), .95, i * .35);
	}

	void BuildStands(FKeysMesh& M, const FBoat& B, double GroundZ)
	{
		const FHullDef H = HullOf(B);
		const FLinearColor Timber = Hex(0x7d6648), Timber2 = Hex(0x6a5740);
		// keel blocks: stacked timbers, crossed, under the keel
		M.Surf = SurfWood;
		for (double Fx : {-.34, -.04, .24})
		{
			const double X = B.L * Fx;
			const FHullSta S = StaAtX(H, B, X);
			const double Top = S.YK - .005, Hh = Top - GroundZ;
			const int32 Layers = FMath::Max(2, FMath::RoundToInt32(Hh / .16));
			for (int32 k = 0; k < Layers; k++)
			{
				const double Z0 = GroundZ + Hh * k / Layers, Z1 = GroundZ + Hh * (k + 1) / Layers;
				const bool bAcross = k % 2 == 0;
				if (k == Layers - 1) Box(M, X, 0, (Z0 + Z1) / 2, .34, .5, Z1 - Z0, Timber2);
				else for (int32 a = -1; a <= 1; a += 2)
					Box(M, X + (bAcross ? a * .12 : 0), bAcross ? 0 : a * .22, (Z0 + Z1) / 2, bAcross ? .2 : .62, bAcross ? .72 : .2, Z1 - Z0, k % 3 == 1 ? Timber2 : Timber);
			}
		}
		M.Surf = SurfDefault;
		// jack stands along the chines, a chain under the keel between each pair
		const bool bBig = B.Kind == BoatSport || B.L > 8.6;
		TArray<double> Fx = {-.36, .2};
		if (B.Kind != BoatSkiff) Fx.Insert(-.06, 1);
		if (bBig) Fx.Add(.33);
		for (double F : Fx)
		{
			const double X = B.L * F;
			const FHullSta S = StaAtX(H, B, X);
			const double Off = S.Bb * .8;
			const double Zh = S.YK + (S.YC - S.YK) * .8;
			if (Off < .25) continue;
			for (int32 a = -1; a <= 1; a += 2) JackStand(M, FVector(X, a * Off, GroundZ), Zh - GroundZ, a > 0 ? 0 : PI);
			M.Surf = SurfSteel;
			Tube(M, FVector(X, -Off, GroundZ + (Zh - GroundZ) * .45), FVector(X, Off, GroundZ + (Zh - GroundZ) * .45), .008, Hex(0x5b6168), 3);
			M.Surf = SurfDefault;
		}
	}

	// ---------------------------------------------------------------- clutter
	void PaintBucket(FKeysMesh& M, const FVector& At, uint32 Label, bool bOpen)
	{
		M.Surf = SurfMetalPaint;
		M.AddTube(At, At + FVector(0, 0, .37), .145, .155, 10, Hex(0xdfe2e4));
		M.AddTube(At + FVector(0, 0, .09), At + FVector(0, 0, .27), .15, .155, 10, Hex(Label));
		M.AddTube(At + FVector(0, 0, .37), At + FVector(0, 0, .375), .158, bOpen ? .15 : .0, 10, bOpen ? Hex(0xb9bec4) : Hex(0xcfd3d6));
		if (bOpen) M.AddTube(At + FVector(0, 0, .3), At + FVector(0, 0, .301), .14, .0, 10, Hex(Label));
		M.Surf = SurfDefault;
		// drips down the side
		for (int32 k = 0; k < 2; k++)
		{
			const double A = Hash(At.X + k, At.Y) * 2 * PI;
			Box(M, At.X + FMath::Cos(A) * .153, At.Y + FMath::Sin(A) * .153, At.Z + .3, .03, .03, .1 + .06 * k, Hex(Label), A);
		}
	}

	void OutboardOnStand(FKeysMesh& M, const FVector& At, double Yaw, uint32 Cowl, bool bCowlOff)
	{
		FKeysMesh L;
		const FLinearColor Wood = Hex(0x8a7556), Steel = Hex(0x6d747b);
		// a timber stand: two A-frames, a clamp board across the top, a brace and a pair of casters
		L.Surf = SurfWood;
		const double Hb = 1.42;
		for (int32 a = -1; a <= 1; a += 2)
		{
			Tube(L, FVector(.45, a * .34, 0), FVector(.03, a * .3, Hb), .04, Wood, 4);
			Tube(L, FVector(-.55, a * .34, 0), FVector(.03, a * .3, Hb), .04, Wood, 4);
			Tube(L, FVector(.3, a * .33, .34), FVector(-.36, a * .33, .34), .03, Wood, 4);
		}
		Box(L, .03, 0, Hb - .12, .07, .74, .34, Wood);
		Tube(L, FVector(.45, -.34, .04), FVector(.45, .34, .04), .03, Wood, 4);
		Tube(L, FVector(-.55, -.34, .04), FVector(-.55, .34, .04), .03, Wood, 4);
		L.Surf = SurfDefault;
		KeysMesh::FOutboard O;
		O.Mount = FVector(0, 0, Hb - .08);
		O.W = .56;
		O.CavZ = O.Mount.Z - .76;
		O.TopZ = O.Mount.Z + .08 + 1.05;
		// years in the sun: chalky cowl, a rusty lower unit
		O.Cowl = FMath::Lerp(Hex(Cowl), Hex(0xa9aeb0), .45f);
		O.Panel = Hex(0x33363a);
		O.Accent = Hex(0x7a4a2e);
		O.Lower = FMath::Lerp(Hex(0x5a5d60), Hex(0x7a4a2e), float(Hash(At.X, At.Y)) * .6f);
		O.CowlKind = SurfDefault;
		KeysMesh::BuildOutboard(L, O);
		if (bCowlOff)
		{
			// the cowl is off this one and sits on the ground beside it; a rag over the powerhead
			L.Surf = SurfCloth;
			L.AddRoundBox(FVector(-.52, 0, O.TopZ + .03), FVector(.5, .5, .06), .02, Hex(0x9aa7b5));
			L.Surf = SurfDefault;
		}
		Append(M, L, FTransform(FQuat(FVector::ZAxisVector, Yaw), At));
	}

	void Trailer(FKeysMesh& M, const FVector& At, double Yaw, double Len)
	{
		FKeysMesh L;
		const FLinearColor Galv = Hex(0xaab0b4), Tire = Hex(0x17181a), Carpet = Hex(0x3c4250);
		L.Surf = SurfSteel;
		const double Hz = .52, Wd = .98;
		for (int32 a = -1; a <= 1; a += 2)
		{
			Box(L, -Len * .18, a * Wd, Hz, Len * .64, .09, .13, Galv);
			// the rails come in to the tongue
			L.AddTube(FVector(Len * .14, a * Wd, Hz), FVector(Len * .42, a * .06, Hz), .05, .05, 4, Galv);
		}
		Box(L, Len * .46, 0, Hz, Len * .16, .1, .12, Galv);
		for (double Fx : {-.48, -.28, -.06, .12}) Box(L, Len * Fx, 0, Hz - .02, .08, Wd * 2, .09, Galv);
		// the coupler, the tongue jack and the winch post with its bow stop
		Box(L, Len * .55, 0, Hz + .02, .18, .12, .1, Hex(0x5b6168));
		Tube(L, FVector(Len * .44, .12, Hz + .3), FVector(Len * .44, .12, .1), .03, Galv, 6);
		Wheel(L, FVector(Len * .44, .08, .1), FVector(Len * .44, .16, .1), .1, Tire, 8);
		Tube(L, FVector(Len * .36, 0, Hz), FVector(Len * .33, 0, Hz + .85), .05, Galv, 4);
		Box(L, Len * .33, 0, Hz + .8, .2, .16, .16, Hex(0x5b6168));
		Box(L, Len * .31, 0, Hz + .95, .1, .22, .14, Tire);
		// tandem axles, fenders, lights
		for (double Ax : {-Len * .22, -Len * .22 + .86})
		{
			Tube(L, FVector(Ax, -Wd - .2, .34), FVector(Ax, Wd + .2, .34), .04, Galv, 5);
			for (int32 a = -1; a <= 1; a += 2)
			{
				L.Surf = SurfRubber;
				Wheel(L, FVector(Ax, a * (Wd + .14), .34), FVector(Ax, a * (Wd + .34), .34), .34, Tire, 12);
				L.Surf = SurfSteel;
				Wheel(L, FVector(Ax, a * (Wd + .33), .34), FVector(Ax, a * (Wd + .355), .34), .16, Galv, 8);
			}
		}
		for (int32 a = -1; a <= 1; a += 2)
		{
			Box(L, -Len * .22 + .43, a * (Wd + .24), .74, 1.9, .26, .04, Galv);
			Box(L, -Len * .5, a * (Wd - .02), Hz + .0, .05, .14, .09, Hex(0xb8322a));
		}
		// carpeted bunks, tipped in
		L.Surf = SurfCloth;
		for (int32 a = -1; a <= 1; a += 2)
			L.AddBox(FVector(-Len * .16, a * .58, Hz + .2), FVector(Len * .6, .24, .07), Carpet, FQuat(FVector::XAxisVector, a * -.3));
		L.Surf = SurfSteel;
		for (int32 a = -1; a <= 1; a += 2) for (double Fx : {-.4, .08}) Tube(L, FVector(Len * Fx, a * .7, Hz), FVector(Len * Fx, a * .6, Hz + .17), .025, Galv, 4);
		L.Surf = SurfDefault;
		Append(M, L, FTransform(FQuat(FVector::ZAxisVector, Yaw), At));
	}

	void SignBoard(FKeysMesh& M, const FVector& At, double Yaw, double W, double Hh, double PostH, uint32 Face)
	{
		const FQuat Q(FVector::ZAxisVector, Yaw);
		const FVector Side = Q.RotateVector(FVector(0, 1, 0));
		M.Surf = SurfWood;
		if (PostH > 0) for (int32 a = -1; a <= 1; a += 2) M.AddBox(At + Side * (a * (W / 2 - .08)) + FVector(0, 0, (PostH + Hh) / 2), FVector(.1, .1, PostH + Hh), Hex(0x6e5a44), Q);
		M.Surf = SurfDefault;
		M.AddBox(At + FVector(0, 0, PostH + Hh / 2), FVector(.07, W, Hh), Hex(Face), Q);
		M.AddBox(At + FVector(0, 0, PostH + Hh / 2), FVector(.05, W + .08, Hh + .08), Hex(0x2a2d31), Q);
	}

	void FuelPump(FKeysMesh& M, const FVector& At, double Yaw, uint32 Body)
	{
		FKeysMesh L;
		L.Surf = SurfConcrete;
		Box(L, 0, 0, .06, .7, 1.2, .12, Hex(0xbdb9ae));
		L.Surf = SurfMetalPaint;
		L.AddRoundBox(FVector(0, 0, 1.0), FVector(.46, .92, 1.76), .05, Hex(Body));
		L.AddRoundBox(FVector(0, 0, 1.95), FVector(.52, 1.0, .16), .05, Hex(0xf2f2f0));
		L.Surf = SurfGlass;
		for (int32 a = -1; a <= 1; a += 2) Box(L, a * .235, 0, 1.4, .012, .6, .42, Hex(0x14181c));
		L.Surf = SurfDefault;
		for (int32 a = -1; a <= 1; a += 2) Box(L, a * .236, 0, .85, .012, .66, .3, Hex(0xf2f2f0));
		// the hose looped up to its hook, the nozzle hung on the side
		L.Surf = SurfRubber;
		for (int32 a = -1; a <= 1; a += 2)
		{
			const FVector P0(0, a * .47, .5), P1(.12, a * .62, .3), P2(.14, a * .66, 1.2), P3(0, a * .5, 1.5);
			Tube(L, P0, P1, .02, Hex(0x15171a), 5); Tube(L, P1, P2, .02, Hex(0x15171a), 5); Tube(L, P2, P3, .02, Hex(0x15171a), 5);
			L.Surf = SurfSteel;
			Box(L, 0, a * .5, 1.42, .1, .06, .22, Hex(0x8d9499));
			L.Surf = SurfRubber;
		}
		L.Surf = SurfDefault;
		Append(M, L, FTransform(FQuat(FVector::ZAxisVector, Yaw), At));
	}

	void PressureWasher(FKeysMesh& M, const FVector& At, double Yaw)
	{
		FKeysMesh L;
		L.Surf = SurfMetalPaint;
		L.AddRoundBox(FVector(0, 0, .42), FVector(.62, .46, .42), .06, Hex(0xc8302e));
		L.AddRoundBox(FVector(.05, 0, .7), FVector(.36, .36, .2), .05, Hex(0x1c1c1e));
		L.Surf = SurfSteel;
		Tube(L, FVector(-.3, -.22, .25), FVector(-.5, -.22, .98), .018, Hex(0x2a2d31), 5);
		Tube(L, FVector(-.3, .22, .25), FVector(-.5, .22, .98), .018, Hex(0x2a2d31), 5);
		Tube(L, FVector(-.5, -.22, .98), FVector(-.5, .22, .98), .018, Hex(0x2a2d31), 5);
		L.Surf = SurfRubber;
		for (int32 a = -1; a <= 1; a += 2) Wheel(L, FVector(-.18, a * .25, .16), FVector(-.18, a * .32, .16), .16, Hex(0x17181a), 10);
		Tube(L, FVector(.3, 0, .1), FVector(.3, 0, .22), .03, Hex(0x2a2d31), 5);
		L.Surf = SurfDefault;
		Append(M, L, FTransform(FQuat(FVector::ZAxisVector, Yaw), At));
	}

	void Drum(FKeysMesh& M, const FVector& At, uint32 C)
	{
		M.Surf = SurfMetalPaint;
		M.AddTube(At, At + FVector(0, 0, .88), .29, .29, 12, Hex(C));
		for (double Zr : {.02, .3, .59, .86}) M.AddTube(At + FVector(0, 0, Zr), At + FVector(0, 0, Zr + .025), .3, .3, 12, Hex(C) * .8f);
		M.AddTube(At + FVector(0, 0, .86), At + FVector(0, 0, .865), .29, 0, 12, Hex(C) * .7f);
		M.Surf = SurfDefault;
	}

	void Ladder(FKeysMesh& M, const FVector& Foot, const FVector& Top)
	{
		const FVector D = Top - Foot;
		FVector Side = FVector::CrossProduct(D, FVector::UpVector).GetSafeNormal();
		if (Side.IsNearlyZero()) Side = FVector(0, 1, 0);
		M.Surf = SurfSteel;
		for (int32 a = -1; a <= 1; a += 2) Tube(M, Foot + Side * (a * .21), Top + Side * (a * .21), .022, Hex(0xc4c9cd), 4);
		const int32 Rungs = FMath::Max(2, int32(D.Size() / .3));
		for (int32 k = 1; k < Rungs; k++) { const FVector P = Foot + D * (double(k) / Rungs); Tube(M, P - Side * .21, P + Side * .21, .014, Hex(0xb4b9bd), 4); }
		M.Surf = SurfDefault;
	}

	void Dumpster(FKeysMesh& M, const FVector& At, double Yaw)
	{
		const FQuat Q(FVector::ZAxisVector, Yaw);
		M.Surf = SurfMetalPaint;
		M.AddBox(At + FVector(0, 0, .68), FVector(1.9, 1.25, 1.2), Hex(0x2f4a3a), Q);
		M.AddBox(At + FVector(0, 0, 1.31), FVector(1.96, 1.3, .06), Hex(0x15171a), Q * FQuat(FVector::YAxisVector, .06));
		for (int32 a = -1; a <= 1; a += 2) M.AddBox(At + Q.RotateVector(FVector(a * .98, 0, .8)), FVector(.08, .5, .1), Hex(0x24382c), Q);
		M.Surf = SurfDefault;
	}

	void Pallet(FKeysMesh& M, const FVector& At, double Yaw)
	{
		const FQuat Q(FVector::ZAxisVector, Yaw);
		M.Surf = SurfWood;
		for (int32 k = 0; k < 5; k++) M.AddBox(At + Q.RotateVector(FVector(-.5 + k * .25, 0, .13)), FVector(.16, 1.0, .02), Hex(k % 2 ? 0x9a8463 : 0x8a7556), Q);
		for (int32 k = -1; k <= 1; k++) M.AddBox(At + Q.RotateVector(FVector(0, k * .45, .06)), FVector(1.16, .09, .12), Hex(0x7d6648), Q);
		M.Surf = SurfDefault;
	}

	// ---------------------------------------------------------------- the travel lift
	void TravelLift(FKeysMesh& M, const FTravelLift& T)
	{
		const FLinearColor Blue = Hex(0x2a5d9c), BlueD = Hex(0x1f477a), Tire = Hex(0x17181a), Yellow = Hex(0xe8b923), Steel = Hex(0x8d9499), Strapc = Hex(0xd8c12c);
		const double Hw = T.W / 2, Hl = T.L / 2, WheelR = .78;
		for (int32 a = -1; a <= 1; a += 2)
		{
			const double Y = a * Hw;
			M.Surf = SurfMetalPaint;
			// legs, the top beam they carry, and the low beam between the wheels
			for (int32 b = -1; b <= 1; b += 2)
			{
				Box(M, b * (Hl - .3), Y, (T.H + WheelR * 2) / 2 + .2, .46, .42, T.H - WheelR * 2 - .1, Blue);
				// hazard bands low on each leg
				for (int32 k = 0; k < 3; k++) Box(M, b * (Hl - .3), Y, WheelR * 2 + .5 + k * .34, .47, .43, .17, k % 2 ? Tire : Yellow);
				// the knee brace up to the top beam
				M.AddTube(FVector(b * (Hl - .3), Y, T.H - 2.2), FVector(b * (Hl - 2.0), Y, T.H - .3), .11, .11, 4, BlueD);
			}
			Box(M, 0, Y, T.H - .05, T.L + .5, .5, .6, Blue);
			Box(M, 0, Y, WheelR * 2 + .12, T.L - .2, .36, .34, BlueD);
			// wheels, their forks and hubs
			for (int32 b = -1; b <= 1; b += 2)
			{
				const double X = b * (Hl - .3);
				M.Surf = SurfRubber;
				Wheel(M, FVector(X, Y - .3, WheelR), FVector(X, Y + .3, WheelR), WheelR, Tire, 16);
				M.Surf = SurfMetalPaint;
				Wheel(M, FVector(X, Y - .32, WheelR), FVector(X, Y + .32, WheelR), .36, Yellow, 10);
				for (int32 c = -1; c <= 1; c += 2) Box(M, X, Y + c * .38, WheelR + .5, .3, .08, 1.1, BlueD);
			}
			// the hoists on the top beam, their cables down to the sling ends, and the slings round her
			for (int32 b = -1; b <= 1; b += 2)
			{
				const double X = b * T.SlingX;
				M.Surf = SurfMetalPaint;
				Box(M, X, Y - a * .1, T.H + .42, .9, .6, .42, BlueD);
				M.Surf = SurfSteel;
				Wheel(M, FVector(X - .25, Y - a * .1, T.H + .42), FVector(X + .25, Y - a * .1, T.H + .42), .24, Steel, 10);
				const FVector Hook(X, a * (T.SheerHalf + .3), T.SheerZ + .7);
				for (double Dx : {-.1, .1}) Tube(M, FVector(X + Dx, Y - a * .3, T.H - .3), Hook + FVector(Dx, 0, .2), .014, Hex(0x3a3d40), 4);
				Box(M, Hook.X, Hook.Y, Hook.Z + .1, .34, .1, .3, Yellow);
				M.Surf = SurfCloth;
				const FVector Sheer(X, a * (T.SheerHalf + .03), T.SheerZ), Chine(X, a * (T.ChineHalf + .02), T.ChineZ - .02), Keel(X, 0, T.KeelZ - .03);
				const FVector Fwd(1, 0, 0);
				Strap(M, Hook, Sheer, Fwd, .3, Strapc);
				Strap(M, Sheer, Chine, Fwd, .3, Strapc);
				Strap(M, Chine, Keel, Fwd, .3, Strapc);
			}
		}
		M.Surf = SurfMetalPaint;
		// the cross beam at the closed end, and a lighter tie low at the back
		Box(M, -Hl + .3, 0, T.H - .05, .5, T.W + .5, .6, Blue);
		Box(M, -Hl + .3, 0, T.H - 1.3, .24, T.W, .24, BlueD);
		// the operator's cab and the engine on the starboard side, a ladder up to it
		M.AddRoundBox(FVector(-Hl + 1.6, Hw + .75, 3.3), FVector(1.4, 1.1, 1.9), .08, Hex(0xe9ecee));
		M.Surf = SurfGlass;
		Box(M, -Hl + 1.6, Hw + 1.31, 3.55, 1.1, .02, .9, Hex(0x1b232b));
		Box(M, -Hl + 2.31, Hw + .75, 3.55, .02, .8, .9, Hex(0x1b232b));
		M.Surf = SurfMetalPaint;
		Box(M, -Hl + 1.6, Hw + .75, 2.28, 1.6, 1.3, .1, BlueD);
		M.AddRoundBox(FVector(Hl - 2.2, Hw + .62, 2.9), FVector(1.8, .9, 1.1), .06, BlueD);
		M.Surf = SurfDefault;
		Ladder(M, FVector(-Hl + 2.6, Hw + 1.1, .1), FVector(-Hl + 2.45, Hw + 1.1, 2.3));
	}
}
