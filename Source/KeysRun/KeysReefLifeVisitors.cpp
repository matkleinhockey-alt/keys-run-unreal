// Reef life, the visitors: southern stingray, green turtle, loggerhead. Three species of the Florida Keys reef tract
// that are neither fish-shaped nor fish-mannered, so this file brings its own bodies (FSpecies::Build), its own limbs
// (FSpecies::Limb: a ray's wings, a turtle's front flippers) and uses the traits Bury, DigEvery, BreathEvery and
// bBitesBottom. Those hooks are request R1 of the reefvisitors lane (.marshal/requests/reefvisitors/R1.patch); built
// against a framework without them this file compiles and registers nothing, rather than draw a turtle as a fish.
//
// REFERENCE (what the verifier reads the shots against)
//
// Southern stingray, Hypanus americanus (was Dasyatis americana)
//   Size      disc width: females 0.8 to 1.5 m, males 0.45 to 0.7 m (about half). The whip tail is about 1.3 times
//             the disc's length, so a 1.5 m female is some 3 m from snout to tail tip. Phases: female, male.
//   Shape     a flat diamond (rhomboid) disc, a little wider than long, widest just ahead of its middle; the front
//             edges nearly straight, the snout a blunt angle with a small tip, the outer corners narrowly rounded,
//             the rear edges convex and running into two small pelvic fins either side of the tail's base. A low hump
//             down the middle carries the eyes, raised, a third of the way back, each with a spiracle (a dark hole)
//             right behind it. The tail is a thin whip.
//   Colour    plain: grey-brown to olive-brown above with no marks, the edge a little paler; white below with a dusky
//             margin; the tail darkens to near black.
//   Where     sand: patches in and beside the reef, the grooves between spurs, the sand at the reef's foot, and
//             seagrass edges. 1 to 25 m. Alone, sometimes two.
//   Does      lies sunk in the sand with only the hump, eyes, spiracles and tail showing; holds still until a diver is
//             about 2 m off, then lifts off in a cloud of sand, glides away low and settles again. Feeds by digging:
//             beats its wings and jets water into the sand for clams, worms and crabs, leaving a pit and a drifting
//             cloud. Active day and night. Swims by a wave run along its wings.
//
// Green turtle, Chelonia mydas
//   Size      shell (carapace) 0.8 to 1.1 m. Phase: adult.
//   Shape     a smooth, low, oval shell a little heart-shaped; five vertebral scutes, FOUR pairs of costals (the first
//             does not touch the nuchal), the scutes not overlapping; a small blunt head (about a fifth of the shell's
//             length); long front flippers, short paddle hind flippers.
//   Colour    shell brown-olive, each scute with streaks radiating from its hind part, lighter and darker; head and
//             flipper scales dark brown with pale cream edges; throat, underside of the flippers and the plastron pale
//             yellowish white.
//   Where     turtle grass beside the reef (grazing), patch reefs and the reef top (resting against a coral head or
//             under a ledge, and asleep there at night). 1 to 18 m. Alone.
//   Does      swims with the front flippers together, a slow wing beat, the hind ones trailing and steering; grazes
//             seagrass nose-down; comes up to breathe every few minutes, a few breaths with the head out, and goes
//             back down. Unbothered by a slow diver. Protected: not a spear target.
//
// Loggerhead, Caretta caretta
//   Size      shell 0.8 to 1.0 m. Phase: adult.
//   Shape     heart-shaped shell, widest toward the front; five vertebrals and FIVE pairs of costals (the small first
//             one touches the nuchal); a very large, blocky head (about 0.28 of the shell's length, broad jaws);
//             front flippers shorter and thicker than the green's.
//   Colour    shell reddish brown; skin yellowish to red-brown above with yellow edges, the head's sides and the jaws
//             yellow-tan; plastron and underside pale yellow.
//   Where     hard bottom and reef: reef top, spurs, fore reef, patch reefs. 2 to 30 m. Alone.
//   Does      roves the bottom and crushes conch, crabs and urchins in its jaws (a crack you can hear); swims and
//             breathes as the green turtle does; sleeps wedged against the reef at night. Protected.
#include "KeysReefLife.h"
#include "KeysMesh.h"

namespace KeysReefLifeVisitors
{
	using namespace KeysReefLife;

	// the hooks of request R1: are they in this framework?
	template <typename T> concept CHooks = requires(T S) { S.Build; S.Limb; S.LimbWave; S.bBitesBottom; S.BiteTilt; S.BreathEvery; S.BreathSecs; S.Bury; S.DigEvery; };

	static FLinearColor Blend(const FLinearColor& A, const FLinearColor& B, double T) { return FMath::Lerp(A, B, float(FMath::Clamp(T, 0.0, 1.0))); }
	static double Hash2(double X, double Y) { const double H = FMath::Sin(X * 127.1 + Y * 311.7) * 43758.5453; return H - FMath::FloorToDouble(H); }
	static double Sgn(double X) { return X < 0 ? -1.0 : 1.0; }
	// two rows of points joined into a band (each row its own vertices already added, N long)
	static void Band(FKeysMesh& M, int32 Row0, int32 Row1, int32 N, bool bWrap)
	{
		for (int32 k = 0; k + 1 < N || (bWrap && k < N); k++)
		{
			const int32 A = Row0 + k, B = Row0 + (k + 1) % N, C = Row1 + (k + 1) % N, D = Row1 + k;
			M.T.Append({A, B, C, A, C, D});
		}
	}
	// Sub (already in mesh units) moved by Xf and added to M
	static void AppendXf(FKeysMesh& M, const FKeysMesh& Sub, const FTransform& Xf)
	{
		const int32 I0 = M.V.Num();
		M.SyncKinds();
		for (int32 i = 0; i < Sub.V.Num(); i++)
		{
			M.V.Add(Xf.TransformPosition(Sub.V[i]));
			M.N.Add(Xf.TransformVectorNoScale(Sub.N[i]));
			M.UV.Add(Sub.UV[i]);
			M.C.Add(Sub.C[i]);
			M.K.Add(Sub.K.IsValidIndex(i) ? Sub.K[i] : FVector2D::ZeroVector);
		}
		for (const int32 T : Sub.T) M.T.Add(I0 + T);
	}

	// ================================================================ southern stingray
	namespace Ray
	{
		// plan, in body lengths (snout +0.5, tail tip -0.5): the disc is 0.44 long and 0.49 wide
		constexpr double W = .245;        // half the disc's width
		constexpr double Xt = .302;       // where the outer corners are along the body
		constexpr double Y0 = .09;        // inboard of this the body mesh, outboard the wing (a limb)
		constexpr double C0 = .14;        // half the chord at Y0
		constexpr double WingRoot = .175, WingSpan = .675;   // the wing in its own frame: root at u = .35 of the material's wave, tip at u = -1

		static double ChordF(double Y)   // from the corner line forward to the front edge
		{
			Y = FMath::Abs(Y);
			if (Y >= Y0) return C0 * FMath::Pow(FMath::Max(0.0, (W - Y) / (W - Y0)), .8);
			return FMath::Lerp(.198, C0, Y / Y0) + .012 * FMath::Exp(-FMath::Square(Y / .02));
		}
		static double ChordB(double Y)   // and back to the rear edge (inboard: the pelvic fins)
		{
			Y = FMath::Abs(Y);
			if (Y >= Y0) return ChordF(Y);
			return C0 + (.215 - C0) * (1 - FMath::Pow(Y / Y0, 1.6));
		}
		static double Thick(double Y) { const double S = FMath::Abs(Y) / W; return .012 * FMath::Pow(FMath::Max(0.0, 1 - S), .7) + .04 * FMath::Exp(-FMath::Square(S / .2)); }
		static double Belly(double Y) { const double S = FMath::Abs(Y) / W; return .006 * FMath::Max(0.0, 1 - S) + .008 * FMath::Exp(-FMath::Square(S / .25)); }
		// Q 1 the front edge .. -1 the rear edge
		static FVector Pt(double Y, double Q, bool bTop)
		{
			const double X = Xt + (Q > 0 ? Q * ChordF(Y) : Q * ChordB(Y));
			const double Dome = FMath::Pow(FMath::Max(0.0, 1 - Q * Q), .55);
			// the hump is highest a little ahead of the corner line and runs out toward the tail
			const double Along = bTop ? 1 - .25 * FMath::Square(Q - .15) : 1;
			return FVector(X, Y, bTop ? Thick(Y) * Dome * Along : -Belly(Y) * Dome);
		}
		static FVector Nrm(double Y, double Q, bool bTop)
		{
			const double E = .004;
			const FVector Dq = Pt(Y, FMath::Min(1.0, Q + E), bTop) - Pt(Y, FMath::Max(-1.0, Q - E), bTop);
			const FVector Dy = Pt(Y + E, Q, bTop) - Pt(Y - E, Q, bTop);
			FVector N = FVector::CrossProduct(Dq, Dy).GetSafeNormal();
			if (N.IsNearlyZero()) N = FVector(0, 0, 1);
			if ((N.Z < 0) == bTop) N = -N;
			return N;
		}
		static FLinearColor Skin(double Y, double Q, bool bTop, bool bMale)
		{
			const double S = FMath::Abs(Y) / W, Edge = FMath::Max(FMath::Abs(Q), S);
			if (!bTop) return Blend(Hex(0xf0ece2), Hex(0xbdb5a4), FMath::Pow(Edge, 5.0));
			// grey-brown to olive-brown, plain; a shade darker down the middle, paler at the very edge
			FLinearColor C = bMale ? Hex(0x70684f) : Hex(0x6a6150);
			C = Blend(C, Hex(0x554d3f), .5 * FMath::Exp(-FMath::Square(S / .16)));
			C = Blend(C, Hex(0x91876d), .6 * FMath::Pow(Edge, 7.0));
			return C * float(.95 + .1 * Hash2(Y * 40, Q * 9));
		}
		// a patch of the disc: Y from YA to YB in NY steps, the whole chord in NQ; top and bottom
		static void Disc(FKeysMesh& M, double YA, double YB, int32 NY, int32 NQ, bool bMale)
		{
			for (int32 Side = 0; Side < 2; Side++)
			{
				const bool bTop = Side == 0;
				const int32 I0 = M.V.Num();
				for (int32 i = 0; i <= NY; i++)
					for (int32 j = 0; j <= NQ; j++)
					{
						const double Y = FMath::Lerp(YA, YB, double(i) / NY), Q = -FMath::Cos(PI * j / NQ);
						M.AddVert(Pt(Y, Q, bTop), Nrm(Y, Q, bTop), Skin(Y, Q, bTop, bMale));
					}
				for (int32 i = 0; i < NY; i++) Band(M, I0 + i * (NQ + 1), I0 + (i + 1) * (NQ + 1), NQ + 1, false);
			}
		}
		static void Tail(FKeysMesh& M, int32 Segs, int32 Sides)
		{
			const double X0 = Xt - .2, X1 = -.5;
			for (int32 k = 0; k < Segs; k++)
			{
				const double TA = double(k) / Segs, TB = double(k + 1) / Segs;
				const FLinearColor C = Blend(Hex(0x5d5545), Hex(0x1b1815), .5 * (TA + TB));
				M.AddTube(FVector(FMath::Lerp(X0, X1, TA), 0, .006 * (1 - TA)), FVector(FMath::Lerp(X0, X1, TB), 0, .006 * (1 - TB)), FMath::Lerp(.0125, .0016, FMath::Pow(TA, .7)), FMath::Lerp(.0125, .0016, FMath::Pow(TB, .7)), Sides, C);
			}
		}
		static void Eyes(FKeysMesh& M)
		{
			for (int32 Sd = -1; Sd <= 1; Sd += 2)
			{
				// the eye on its raised mound, and the spiracle right behind it
				const double Ey = Sd * .036, Q = .44;
				const FVector Base = Pt(Ey, Q, true);
				const int32 Top = M.AddVert(Base + FVector(0, 0, .016), FVector(0, 0, 1), Hex(0x5a5243));
				const int32 R0 = M.V.Num();
				for (int32 k = 0; k < 8; k++)
				{
					const double A = 2 * PI * k / 8;
					const FVector D(FMath::Cos(A), FMath::Sin(A), 0);
					// the outer face of the mound is the eye itself: dark, with a pale ring round it
					const bool bEye = D.Y * Sd > .5;
					M.AddVert(Base + FVector(D.X * .017, D.Y * .012, .007), (D + FVector(0, 0, .5)).GetSafeNormal(), bEye ? Hex(0x0a0907) : Hex(0x857a60));
				}
				const int32 R1 = M.V.Num();
				for (int32 k = 0; k < 8; k++)
				{
					const double A = 2 * PI * k / 8;
					const FVector D(FMath::Cos(A), FMath::Sin(A), 0);
					M.AddVert(Base + FVector(D.X * .021, D.Y * .015, -.002), D, Hex(0x6a6150));
				}
				for (int32 k = 0; k < 8; k++) { M.T.Append({Top, R0 + k, R0 + (k + 1) % 8}); }
				Band(M, R0, R1, 8, true);
				const FVector Sp0 = Pt(Ey * 1.05, Q - .2, true) + FVector(0, 0, .0025);
				const FLinearColor Hole = Hex(0x0d0b09);
				const int32 S0 = M.AddVert(Sp0 + FVector(.012, 0, 0), FVector(0, 0, 1), Hole);
				const int32 S1 = M.AddVert(Sp0 + FVector(0, Sd * .011, 0), FVector(0, 0, 1), Hole);
				const int32 S2 = M.AddVert(Sp0 + FVector(-.014, 0, 0), FVector(0, 0, 1), Hole);
				const int32 S3 = M.AddVert(Sp0 + FVector(0, -Sd * .008, 0), FVector(0, 0, 1), Hole);
				M.T.Append({S0, S1, S2, S0, S2, S3});
			}
		}
		// one wing in the limb's own frame: +X the root .. -X the tip (the material's wave grows that way), +Y up,
		// Z along the chord; the same mesh serves both sides because the outer disc is the same fore and aft
		static void Wing(FKeysMesh& M, bool bMale)
		{
			const int32 NS = 8, NQ = 6;
			for (int32 Side = 0; Side < 2; Side++)
			{
				const bool bTop = Side == 0;
				const int32 I0 = M.V.Num();
				for (int32 i = 0; i <= NS; i++)
					for (int32 j = 0; j <= NQ; j++)
					{
						const double S = double(i) / NS, Y = Y0 + S * (W - Y0), Q = -FMath::Cos(PI * j / NQ);
						const FVector P = Pt(Y, Q, bTop), N = Nrm(Y, Q, bTop);
						// (the limb's X is stretched over the wave's whole range and squeezed back by the limb's scale)
						M.AddVert(FVector(WingRoot - WingSpan * S, P.Z, -(P.X - Xt)), FVector(-N.Y * (W - Y0) / WingSpan, N.Z, -N.X).GetSafeNormal(), Skin(Y, Q, bTop, bMale));
					}
				for (int32 i = 0; i < NS; i++) Band(M, I0 + i * (NQ + 1), I0 + (i + 1) * (NQ + 1), NQ + 1, false);
			}
		}
		static void Build(FKeysMesh& M, int32 Phase, int32 Kind)
		{
			const bool bMale = Phase == 1;
			M.Surf = SurfSkin;   // (smooth skin: SurfFish would draw scales on it)
			if (Kind == 3) { Wing(M, bMale); M.Surf = 0; return; }
			if (Kind == 0) { Disc(M, -Y0, Y0, 8, 10, bMale); Tail(M, 6, 6); Eyes(M); }
			else if (Kind == 1) { Disc(M, -W, W, 12, 6, bMale); Tail(M, 3, 4); Eyes(M); }
			else { Disc(M, -W, W, 4, 2, bMale); Tail(M, 1, 3); }
			M.Surf = 0;
		}
		// the wings do not swing: they stand at the body's side and the material's wave runs out along them
		static FTransform Limb(int32 Phase, int32 Side, double Stroke, double Effort)
		{
			const double Sg = Side ? -1 : 1, Sx = (W - Y0) / WingSpan;
			const FVector Xa(0, -Sg, 0), Za(-Sg, 0, 0), Ya(0, 0, 1);
			FTransform T(FMatrix(Xa, Ya, Za, FVector(Xt * 100, Sg * (Y0 * 100 + Sx * WingRoot * 100), 0)));
			T.SetScale3D(FVector(Sx, 1, 1));
			return T;
		}
	}

	// ================================================================ the two turtles
	struct FTurtle
	{
		double CL = .7, Cx = -.05;         // shell length; where its middle is along the animal (body lengths)
		double HW = .28, Heart = .05;      // half its width; how much wider the front is than the back
		double H = .15, Hp = .075;         // the dome's height over the rim; the plastron's depth under it
		int32 Costals = 4;                 // 4 pairs the green turtle, 5 the loggerhead
		double HeadL = .075, HeadW = .05, HeadH = .048, Block = 0;   // the head's half-length, -width, -height; how square it is
		double NeckL = .05;
		double FlipL = .42, FlipC = .062;  // a front flipper: its length and greatest half-chord
		double StreakAmt = 1;
		FLinearColor Shell, Streak, Seam, Margin, Skin, SkinEdge, Under, Beak;
	};

	namespace Turtle
	{
		constexpr double SN = 2.4;         // the shell's outline is a superellipse of this power
		constexpr int32 NM = 26;           // marginal scutes round the rim
		constexpr double RimRho = .8;      // where the costals end and the marginals begin

		static double Rho(double U, double V) { return FMath::Pow(FMath::Pow(FMath::Abs(U), SN) + FMath::Pow(FMath::Abs(V), SN), 1 / SN); }
		static FVector2D RimPt(int32 J, double R)
		{
			const double A = (J + .5) * 2 * PI / NM, Cs = FMath::Cos(A), Sn = FMath::Sin(A);
			return FVector2D(R * Sgn(Cs) * FMath::Pow(FMath::Abs(Cs), 2 / SN), R * Sgn(Sn) * FMath::Pow(FMath::Abs(Sn), 2 / SN));
		}
		static FVector ShellP(const FTurtle& T, double U, double V)
		{
			const double R = FMath::Min(1.0, Rho(U, V));
			return FVector(T.Cx + U * T.CL * .5, V * T.HW * (1 + T.Heart * U), T.H * FMath::Pow(FMath::Max(0.0, 1 - FMath::Pow(R, 2.3)), .62));
		}
		static FVector ShellN(const FTurtle& T, double U, double V)
		{
			const double E = .01, K = FMath::Min(1.0, .985 / FMath::Max(1e-6, Rho(U, V)));
			U *= K; V *= K;   // (the very rim is vertical: take the slope a hair inside it)
			FVector N = FVector::CrossProduct(ShellP(T, U + E, V) - ShellP(T, U - E, V), ShellP(T, U, V + E) - ShellP(T, U, V - E)).GetSafeNormal();
			if (N.IsNearlyZero()) N = FVector(0, 0, 1);
			return N.Z < 0 ? -N : N;
		}
		// one scute: its own vertices, so the seams are sharp; a fan with streaks radiating to the rim
		static void Scute(FKeysMesh& M, const FTurtle& T, const TArray<FVector2D>& Poly, int32 Lod)
		{
			TArray<FVector2D> Rim;
			for (int32 k = 0; k < Poly.Num(); k++)
			{
				Rim.Add(Poly[k]);
				if (Lod == 0) Rim.Add((Poly[k] + Poly[(k + 1) % Poly.Num()]) * .5);
			}
			FVector2D Ctr(0, 0);
			for (const FVector2D& P : Poly) Ctr += P;
			Ctr /= Poly.Num();
			const double Sh = .9 + .2 * Hash2(Ctr.X * 9.1, FMath::Abs(Ctr.Y) * 7.3);
			const int32 N = Rim.Num();
			const int32 C = M.AddVert(ShellP(T, Ctr.X, Ctr.Y), ShellN(T, Ctr.X, Ctr.Y), Blend(T.Shell, T.Streak, .35 * T.StreakAmt) * float(Sh));
			int32 Mid = -1;
			if (Lod == 0)
			{
				Mid = M.V.Num();
				for (int32 k = 0; k < N; k++)
				{
					const FVector2D P = FMath::Lerp(Ctr, Rim[k], .58);
					// light and dark by turns: the streaks that radiate across each scute
					const double Lt = (k & 1) ? .85 : 0, Dk = (k & 1) ? 0 : .45;
					M.AddVert(ShellP(T, P.X, P.Y), ShellN(T, P.X, P.Y), Blend(Blend(T.Shell, T.Streak, Lt * T.StreakAmt), T.Seam, Dk * T.StreakAmt) * float(Sh));
				}
			}
			const int32 R0 = M.V.Num();
			for (int32 k = 0; k < N; k++)
			{
				const double Lt = (Lod == 0 && (k & 1)) ? .5 * T.StreakAmt : 0;
				M.AddVert(ShellP(T, Rim[k].X, Rim[k].Y), ShellN(T, Rim[k].X, Rim[k].Y), Blend(Blend(T.Shell, T.Seam, .62), T.Streak, Lt) * float(Sh));
			}
			if (Mid >= 0)
			{
				for (int32 k = 0; k < N; k++) M.T.Append({C, Mid + k, Mid + (k + 1) % N});
				Band(M, Mid, R0, N, true);
			}
			else for (int32 k = 0; k < N; k++) M.T.Append({C, R0 + k, R0 + (k + 1) % N});
		}
		static void Carapace(FKeysMesh& M, const FTurtle& T, int32 Lod)
		{
			M.Surf = SurfDefault;   // (matte: under water a wet shell does not shine)
			if (Lod == 2)
			{
				// far: one dome
				const int32 C = M.AddVert(ShellP(T, 0, 0), FVector(0, 0, 1), T.Shell);
				const int32 R0 = M.V.Num();
				for (int32 k = 0; k < 8; k++) { const double A = 2 * PI * k / 8; const FVector2D P(FMath::Cos(A), FMath::Sin(A) * .98); M.AddVert(ShellP(T, P.X, P.Y), ShellN(T, P.X * .8, P.Y * .8), Blend(T.Shell, T.Seam, .4)); }
				for (int32 k = 0; k < 8; k++) M.T.Append({C, R0 + k, R0 + (k + 1) % 8});
				return;
			}
			const FVector2D F0 = RimPt(0, RimRho), B0 = RimPt(NM / 2 - 1, RimRho);
			const double Uf = F0.X, Wb = F0.Y, Wm = .235, Step = 2 * Uf / 5;
			auto Ub = [&](int32 k) { return Uf - k * Step; };
			auto Um = [&](int32 i) { return Uf - (i + .5) * Step; };
			// ---- five vertebrals down the middle
			for (int32 i = 0; i < 5; i++)
			{
				TArray<FVector2D> P;
				if (i == 0) { P.Add(FVector2D(F0.X, -F0.Y)); P.Add(F0); } else { P.Add(FVector2D(Ub(i), -Wb)); P.Add(FVector2D(Ub(i), Wb)); }
				P.Add(FVector2D(Um(i), Wm));
				if (i == 4) { P.Add(B0); P.Add(FVector2D(B0.X, -B0.Y)); } else { P.Add(FVector2D(Ub(i + 1), Wb)); P.Add(FVector2D(Ub(i + 1), -Wb)); }
				P.Add(FVector2D(Um(i), -Wm));
				Scute(M, T, P, Lod);
			}
			// ---- the costals either side: between the vertebrals' zigzag edge and the marginals
			TArray<FVector2D> E;
			E.Add(F0);
			for (int32 i = 0; i < 5; i++) { E.Add(FVector2D(Um(i), Wm)); if (i < 4) E.Add(FVector2D(Ub(i + 1), Wb)); }
			E.Add(B0);
			// seams: (place on that edge, marginal point they run out to)
			TArray<FIntPoint> Seams;
			if (T.Costals >= 5) Seams.Add(FIntPoint(1, 2));
			Seams.Add(FIntPoint(3, 4)); Seams.Add(FIntPoint(5, 6)); Seams.Add(FIntPoint(7, 8));
			for (int32 Sd = 1; Sd >= -1; Sd -= 2)
				for (int32 m = 0; m <= Seams.Num(); m++)
				{
					const int32 EA = m == 0 ? 0 : Seams[m - 1].X, EB = m == Seams.Num() ? E.Num() - 1 : Seams[m].X;
					const int32 RA = m == 0 ? 1 : Seams[m - 1].Y, RB = m == Seams.Num() ? NM / 2 - 2 : Seams[m].Y;
					TArray<FVector2D> P;
					for (int32 k = EA; k <= EB; k++) P.Add(E[k]);
					for (int32 j = RB; j >= RA; j--) P.Add(RimPt(j, RimRho));
					if (Sd < 0) for (FVector2D& Q : P) Q.Y = -Q.Y;
					Scute(M, T, P, Lod);
				}
			// ---- the marginals: a ring of small scutes, each with its own corners
			for (int32 j = 0; j < NM; j++)
			{
				const FVector2D A = RimPt(j, RimRho), B = RimPt(j + 1, RimRho), C = RimPt(j + 1, 1), D = RimPt(j, 1);
				const double Sh = .9 + .2 * Hash2(j * 3.7, 1.3);
				const FLinearColor In = Blend(T.Shell, T.Seam, .35) * float(Sh), Out = T.Margin * float(Sh);
				const int32 I0 = M.AddVert(ShellP(T, A.X, A.Y), ShellN(T, A.X, A.Y), In);
				M.AddVert(ShellP(T, B.X, B.Y), ShellN(T, B.X, B.Y), In);
				M.AddVert(ShellP(T, C.X, C.Y), ShellN(T, C.X, C.Y), Out);
				M.AddVert(ShellP(T, D.X, D.Y), ShellN(T, D.X, D.Y), Out);
				M.T.Append({I0, I0 + 1, I0 + 2, I0, I0 + 2, I0 + 3});
			}
		}
		static void Plastron(FKeysMesh& M, const FTurtle& T, int32 Lod)
		{
			M.Surf = SurfDefault;   // (matte: under water a wet shell does not shine)
			const int32 N = Lod == 2 ? 8 : NM;
			const int32 R0 = M.V.Num();
			for (int32 Ring = 0; Ring < 2; Ring++)
				for (int32 k = 0; k < N; k++)
				{
					const double A = (k + .5) * 2 * PI / N, Cs = FMath::Cos(A), Sn = FMath::Sin(A), R = Ring ? .8 : 1.0;
					const double U = R * Sgn(Cs) * FMath::Pow(FMath::Abs(Cs), 2 / SN), V = R * Sgn(Sn) * FMath::Pow(FMath::Abs(Sn), 2 / SN);
					FVector P = ShellP(T, U, V);
					P.Z = Ring ? -T.Hp : 0;
					M.AddVert(P, FVector(Cs * (Ring ? .2 : 1), Sn * (Ring ? .2 : 1), Ring ? -1 : -.45).GetSafeNormal(), Ring ? T.Under : Blend(T.Under, T.Margin, .45));
				}
			Band(M, R0, R0 + N, N, true);
			const int32 C = M.AddVert(FVector(T.Cx, 0, -T.Hp * 1.06), FVector(0, 0, -1), T.Under * .96f);
			for (int32 k = 0; k < N; k++) M.T.Append({C, R0 + N + k, R0 + N + (k + 1) % N});
		}
		static void Head(FKeysMesh& M, const FTurtle& T, int32 Lod)
		{
			M.Surf = SurfSkin;
			const int32 NS = Lod == 0 ? 8 : Lod == 1 ? 5 : 3, NA = Lod == 0 ? 10 : Lod == 1 ? 6 : 4;
			const FVector Ctr(.5 - T.HeadL, 0, .012);
			const double Pw = FMath::Lerp(1.0, .55, T.Block);   // < 1: the section squarer, the head blockier
			// ---- the neck, from under the front of the shell to the back of the head
			{
				const double XA = T.Cx + T.CL * .5 * .86, XB = Ctr.X - T.HeadL * .2;
				const int32 I0 = M.V.Num();
				for (int32 Ring = 0; Ring < 2; Ring++)
					for (int32 j = 0; j < NA; j++)
					{
						const double Ps = 2 * PI * j / NA, Cs = FMath::Cos(Ps), Sn = FMath::Sin(Ps);
						const double Ry = T.HeadW * (Ring ? .8 : 1.05), Rz = T.HeadH * (Ring ? .78 : .95);
						M.AddVert(FVector(Ring ? XB : XA, Ry * Cs, Ctr.Z - .004 + Rz * Sn), FVector(0, Cs, Sn), Blend(T.Under, T.Skin, FMath::Clamp(Sn * 1.6 + .5, 0.0, 1.0)));
					}
				Band(M, I0, I0 + NA, NA, true);
			}
			// ---- the head: an egg, blunt in front, the jaws' muscle swelling the back
			const int32 I0 = M.V.Num();
			for (int32 i = 0; i <= NS; i++)
				for (int32 j = 0; j < NA; j++)
				{
					const double Th = PI * (.04 + .92 * i / NS), Ps = 2 * PI * j / NA;
					const double Ct = FMath::Cos(Th), St = FMath::Sin(Th), Cs = FMath::Cos(Ps), Sn = FMath::Sin(Ps);
					const double Cy = Sgn(Cs) * FMath::Pow(FMath::Abs(Cs), Pw), Cz = Sgn(Sn) * FMath::Pow(FMath::Abs(Sn), Pw);
					// narrower toward the beak; flat under the jaw
					const double Tp = Ct > 0 ? 1 - (.34 - .14 * T.Block) * Ct * Ct : 1 + .06 * T.Block * (-Ct);
					const FVector D(Ct * (Ct > 0 ? 1.0 : .85), St * Cy * Tp, St * Cz * Tp * (Cz < 0 ? .82 : 1.0));
					const FVector P = Ctr + FVector(D.X * T.HeadL, D.Y * T.HeadW, D.Z * T.HeadH);
					const FVector N = FVector(D.X / T.HeadL, D.Y / T.HeadW, D.Z / T.HeadH).GetSafeNormal();
					// scales above, dark with pale edges; pale below; the horny beak in front
					FLinearColor C = T.Skin;
					if (Lod == 0) C = Blend(T.Skin, T.SkinEdge, ((i + j) & 1) ? .55 : .05); else C = Blend(T.Skin, T.SkinEdge, .3);
					C = Blend(T.Under, C, FMath::Clamp(Cz * 2.2 + .75, 0.0, 1.0));
					if (Ct > .78) C = Blend(C, T.Beak, FMath::Clamp((Ct - .78) / .12, 0.0, 1.0));
					M.AddVert(P, N, C);
				}
			for (int32 i = 0; i < NS; i++) Band(M, I0 + i * NA, I0 + (i + 1) * NA, NA, true);
			const int32 Nose = M.AddVert(Ctr + FVector(T.HeadL, 0, -T.HeadH * .08), FVector(1, 0, 0), T.Beak);
			for (int32 j = 0; j < NA; j++) M.T.Append({Nose, I0 + j, I0 + (j + 1) % NA});
			if (Lod == 2) return;
			// ---- the eyes
			M.Surf = SurfEye;
			for (int32 Sd = -1; Sd <= 1; Sd += 2)
			{
				const FVector D(.42, Sd * .8, .34);
				const FVector P = Ctr + FVector(D.X * T.HeadL, D.Y * T.HeadW * (1 - .2 * .42 * .42), D.Z * T.HeadH) + FVector(0, Sd * .003, 0);
				const FVector N = FVector(.15, Sd, .2).GetSafeNormal(), T1 = FVector(1, 0, 0), T2 = FVector::CrossProduct(N, T1).GetSafeNormal();
				const double R = T.HeadH * .3;
				const int32 E0 = M.AddVert(P + N * .002, N, Hex(0x050505));
				for (int32 k = 0; k < 6; k++) { const double A = 2 * PI * k / 6; M.AddVert(P + (T1 * FMath::Cos(A) + T2 * FMath::Sin(A)) * R, N, Hex(0x1a130c)); }
				for (int32 k = 0; k < 6; k++) M.T.Append({E0, E0 + 1 + k, E0 + 1 + (k + 1) % 6});
			}
		}
		// a flat paddle, two faces: Pts are stations along it (X out along the limb, half-chord in Y); dark above, pale below
		static void Paddle(FKeysMesh& M, const FTurtle& T, const TArray<FVector2D>& St, double Thk, int32 Lod)
		{
			M.Surf = SurfSkin;
			for (int32 Face = 0; Face < 2; Face++)
			{
				const double Zs = Face ? -1 : 1;
				const int32 I0 = M.V.Num();
				for (int32 i = 0; i < St.Num(); i++)
					for (int32 j = -1; j <= 1; j++)
					{
						const double Tk = Thk * (1 - .6 * double(i) / FMath::Max(1, St.Num() - 1));
						FLinearColor C = Face ? T.Under : Blend(T.Skin, T.SkinEdge, j == 0 ? (Lod == 0 && (i & 1) ? .4 : .12) : .7);
						if (Face) C = Blend(C, T.Skin, j == 0 ? .12 : .3);
						M.AddVert(FVector(St[i].X, j * St[i].Y, j == 0 ? Zs * Tk : 0), FVector(0, j * .5, Zs).GetSafeNormal(), C);
					}
				for (int32 i = 0; i + 1 < St.Num(); i++) Band(M, I0 + i * 3, I0 + (i + 1) * 3, 3, false);
			}
		}
		static void Flipper(FKeysMesh& M, const FTurtle& T, int32 Lod)
		{
			// the front flipper in its own frame: root at the origin, out along +X, the same fore and aft so that one
			// mesh serves both sides (the sweep back is in the pose)
			TArray<FVector2D> St;
			const int32 N = Lod == 2 ? 2 : 6;
			for (int32 i = 0; i <= N; i++)
			{
				const double S = double(i) / N;
				const double C = S < .3 ? .5 + .5 * FMath::Sin(HALF_PI * S / .3) : 1 - .86 * FMath::Pow((S - .3) / .7, 1.7);
				St.Add(FVector2D(S * T.FlipL, T.FlipC * C));
			}
			Paddle(M, T, St, .011, Lod);
		}
		static FTransform Limb(const FTurtle& T, int32 Side, double Stroke, double Effort)
		{
			// both flippers together, a slow wing beat (a third of a beat a second at a cruise): up and a little
			// forward, then down and back with the leading edge turned down
			const double Sg = Side ? -1 : 1, C = FMath::Frac(Stroke * .17) * 2 * PI;
			const double Phi = FMath::DegreesToRadians(-6 + 34 * Effort * FMath::Sin(C));
			const double Theta = FMath::DegreesToRadians(34 - 14 * Effort * FMath::Sin(C));
			const double Tau = FMath::DegreesToRadians(-24 * Effort * FMath::Cos(C));
			const FVector D(-FMath::Sin(Theta), Sg * FMath::Cos(Theta) * FMath::Cos(Phi), FMath::Cos(Theta) * FMath::Sin(Phi));
			const FVector N0(0, -Sg * FMath::Sin(Phi), FMath::Cos(Phi));
			const FVector Lead = (FVector(1, 0, 0) - D * D.X - N0 * N0.X).GetSafeNormal();
			const FVector Nn = (N0 * FMath::Cos(Tau) + Lead * FMath::Sin(Tau)).GetSafeNormal();
			const FVector Ya = FVector::CrossProduct(Nn, D).GetSafeNormal();
			return FTransform(FMatrix(D, Ya, Nn, FVector(T.Cx + T.CL * .5 * .6, Sg * T.HW * .66, -T.Hp * .45) * 100.0));
		}
		static void Build(FKeysMesh& M, const FTurtle& T, int32 Kind)
		{
			if (Kind == 3) { Flipper(M, T, 0); M.Surf = 0; return; }
			Carapace(M, T, Kind);
			Plastron(M, T, Kind);
			Head(M, T, Kind);
			// the hind flippers: short paddles trailing either side of the tail, and the tail's stub
			for (int32 Sd = -1; Sd <= 1; Sd += 2)
			{
				FKeysMesh Sub;
				Paddle(Sub, T, {FVector2D(0, T.CL * .05), FVector2D(T.CL * .14, T.CL * .085), FVector2D(T.CL * .27, T.CL * .04)}, .008, Kind);
				const FVector D = FVector(-.86, Sd * .5, -.08).GetSafeNormal(), Up(0, 0, 1);
				const FVector Ya = FVector::CrossProduct(Up, D).GetSafeNormal(), Za = FVector::CrossProduct(D, Ya);
				AppendXf(M, Sub, FTransform(FMatrix(D, Ya, Za, FVector(T.Cx - T.CL * .5 * .7, Sd * T.HW * .5, -T.Hp * .5) * 100.0)));
			}
			if (Kind < 2)
			{
				M.Surf = SurfSkin;
				M.AddTube(FVector(T.Cx - T.CL * .5 * .9, 0, -T.Hp * .55), FVector(T.Cx - T.CL * .5 * 1.06, 0, -T.Hp * .6), .014, .003, 5, T.Skin);
			}
			if (Kind > 0)
			{
				// past the near level of detail the front flippers do not move: they are part of the body, at rest
				for (int32 Sd = 0; Sd < 2; Sd++)
				{
					FKeysMesh Sub;
					Flipper(Sub, T, Kind);
					AppendXf(M, Sub, Limb(T, Sd, 0, 0));
				}
			}
			M.Surf = 0;
		}
	}

	static const FTurtle& Green()
	{
		static const FTurtle T = []
		{
			FTurtle G;
			G.CL = .70; G.Cx = -.03; G.HW = .275; G.Heart = .06; G.H = .155; G.Hp = .07; G.Costals = 4;
			G.HeadL = .075; G.HeadW = .047; G.HeadH = .046; G.Block = 0; G.NeckL = .03;
			G.FlipL = .43; G.FlipC = .06; G.StreakAmt = 1;
			G.Shell = Hex(0x6a5836); G.Streak = Hex(0xb0965a); G.Seam = Hex(0x2a2318); G.Margin = Hex(0x8c7a4c);
			G.Skin = Hex(0x4a3c28); G.SkinEdge = Hex(0xd9cc9a); G.Under = Hex(0xe8dfb0); G.Beak = Hex(0x8a7a54);
			return G;
		}();
		return T;
	}
	static const FTurtle& Logger()
	{
		static const FTurtle T = []
		{
			FTurtle G;
			G.CL = .66; G.Cx = -.045; G.HW = .262; G.Heart = .16; G.H = .15; G.Hp = .075; G.Costals = 5;
			G.HeadL = .1; G.HeadW = .076; G.HeadH = .066; G.Block = 1; G.NeckL = .015;
			G.FlipL = .36; G.FlipC = .064; G.StreakAmt = .45;
			G.Shell = Hex(0x8c4a26); G.Streak = Hex(0xb8743c); G.Seam = Hex(0x4c2614); G.Margin = Hex(0xc09048);
			G.Skin = Hex(0x945a2c); G.SkinEdge = Hex(0xe0b860); G.Under = Hex(0xe2c880); G.Beak = Hex(0xc8a258);
			return G;
		}();
		return T;
	}

	// ================================================================ the species
	// (the fish builder's Paint is still asked for by the registry; with Build set it is never called)
	static FLinearColor PlainPaint(const FLinearColor& Top, const FLinearColor& Under, const FPaint& At) { return At.Part == PEye ? FLinearColor(.004f, .004f, .004f) : At.Part == PBody && At.V < -.2 ? Under : Top; }

	template <typename TS> static void RayHooks(TS& S)
	{
		if constexpr (CHooks<TS>)
		{
			S.Build = [](FKeysMesh& M, int32 Phase, int32 Kind) { Ray::Build(M, Phase, Kind); };
			S.Limb = [](int32 Phase, int32 Side, double Stroke, double Effort) { return Ray::Limb(Phase, Side, Stroke, Effort); };
			S.LimbWave = .7;
			S.Bury = .75;
			S.DigEvery = 150;
		}
	}
	static FSpecies MakeRay()
	{
		FSpecies S;
		S.Key = TEXT("southernstingray");
		S.Common = TEXT("Southern stingray");
		S.Scientific = TEXT("Hypanus americanus");
		S.Role = ERole::Sitter;
		FBody B;
		B.HalfH = .03; B.HalfW = Ray::W; B.Peak = .3; B.TailLen = .5; B.TailH = .01; B.DorH = 0; B.AnH = 0; B.PecLen = 0; B.PelLen = 0; B.EyeR = 0;
		// the whole animal is 1 long and its disc 0.49 wide: disc width 0.8 to 1.5 m in females, 0.45 to 0.7 m in males
		S.Phases = {FPhase{TEXT("female"), .8 / (2 * Ray::W), 1.5 / (2 * Ray::W), B}, FPhase{TEXT("male"), .45 / (2 * Ray::W), .7 / (2 * Ray::W), B}};
		S.Paint = [](int32 Phase, const FPaint& At) { return PlainPaint(Hex(0x6a6150), Hex(0xf0ece2), At); };
		FMix Female; Female.Weight = .6; Female.A = 0;
		FMix Male; Male.Weight = .4; Male.A = 1;
		S.Mixes = {Female, Male};
		// sand: patches by the reef, the grooves between spurs (fore reef), the sand at the reef's foot, seagrass edges
		S.Depth[0] = 3; S.Depth[1] = 4.5; S.Depth[2] = 16; S.Depth[3] = 25;   // (not on the flats: KeysLife's own stingray lives there)
		S.Zones = ZSand | ZFore | ZGrass;
		S.Habit = EHabit::Both;
		S.GroupsPerHa = 2.2; S.MaxAlive = 5; S.Weight = 3;
		S.Shy = 3.6;                       // a slow diver sets it off at about 2 m
		S.Cruise = .5; S.Burst = 3.2; S.Roam = 12; S.Alt[0] = .05; S.Alt[1] = .1; S.Spacing = 2; S.Cull = 60;
		S.bRows = true;                    // its wings are its limbs; the tail only trails
		S.BiteEvery = 400;                 // (a Sitter's BiteEvery is how often it shifts to a new place)
		S.FirstSight = TEXT("Southern stingray (Hypanus americanus). It lies buried in the sand: give it room and it glides off.");
		RayHooks(S);
		return S;
	}

	template <typename TS> static void TurtleHooks(TS& S, bool bLogger)
	{
		if constexpr (CHooks<TS>)
		{
			if (bLogger)
			{
				S.Build = [](FKeysMesh& M, int32 Phase, int32 Kind) { Turtle::Build(M, Logger(), Kind); };
				S.Limb = [](int32 Phase, int32 Side, double Stroke, double Effort) { return Turtle::Limb(Logger(), Side, Stroke, Effort); };
				S.BreathEvery = 300;
			}
			else
			{
				S.Build = [](FKeysMesh& M, int32 Phase, int32 Kind) { Turtle::Build(M, Green(), Kind); };
				S.Limb = [](int32 Phase, int32 Side, double Stroke, double Effort) { return Turtle::Limb(Green(), Side, Stroke, Effort); };
				S.BreathEvery = 240;
			}
			S.BreathSecs = 5;
			S.bBitesBottom = true;
			S.BiteTilt = 36;
		}
	}
	static FSpecies MakeGreen()
	{
		const FTurtle& T = Green();
		FSpecies S;
		S.Key = TEXT("greenturtle");
		S.Common = TEXT("Green turtle");
		S.Scientific = TEXT("Chelonia mydas");
		S.Role = ERole::Grazer;
		FBody B;
		B.HalfH = .12; B.HalfW = .18; B.Peak = .5; B.DorH = 0; B.AnH = 0; B.TailLen = .05; B.TailH = .05;
		S.Phases = {FPhase{TEXT("adult"), .8 / T.CL, 1.1 / T.CL, B}};   // shell 0.8 to 1.1 m
		S.Paint = [](int32 Phase, const FPaint& At) { return PlainPaint(Green().Shell, Green().Under, At); };
		FMix One;
		S.Mixes = {One};
		// turtle grass beside the reef, patch reefs and the reef top
		S.Depth[0] = 1; S.Depth[1] = 2; S.Depth[2] = 10; S.Depth[3] = 18;
		S.Zones = ZGrass | ZPatch | ZTop;
		S.Habit = EHabit::Day;
		S.GroupsPerHa = 1.2; S.MaxAlive = 4; S.Weight = 3;
		S.Shy = .7;                        // unbothered by a slow diver; a rush still sends it off
		S.Cruise = .45; S.Burst = 2.4; S.Roam = 28; S.Alt[0] = .25; S.Alt[1] = .8; S.Spacing = 2; S.Cull = 60;
		S.bRows = true;
		S.bBitesRock = true; S.BiteEvery = 9;   // grazing: seagrass blades and algae, a mouthful every few seconds
		S.FirstSight = TEXT("Green turtle (Chelonia mydas). Protected: look, don't touch.");
		TurtleHooks(S, false);
		return S;
	}
	static FSpecies MakeLogger()
	{
		const FTurtle& T = Logger();
		FSpecies S;
		S.Key = TEXT("loggerhead");
		S.Common = TEXT("Loggerhead");
		S.Scientific = TEXT("Caretta caretta");
		S.Role = ERole::Grazer;
		FBody B;
		B.HalfH = .12; B.HalfW = .18; B.Peak = .5; B.DorH = 0; B.AnH = 0; B.TailLen = .05; B.TailH = .05;
		S.Phases = {FPhase{TEXT("adult"), .8 / T.CL, 1.0 / T.CL, B}};   // shell 0.8 to 1.0 m
		S.Paint = [](int32 Phase, const FPaint& At) { return PlainPaint(Logger().Shell, Logger().Under, At); };
		FMix One;
		S.Mixes = {One};
		// hard bottom and reef
		S.Depth[0] = 2; S.Depth[1] = 4; S.Depth[2] = 18; S.Depth[3] = 30;
		S.Zones = ZHard | ZTop | ZSpur | ZFore | ZPatch;
		S.Habit = EHabit::Day;
		S.GroupsPerHa = .9; S.MaxAlive = 3; S.Weight = 3;
		S.Shy = .8;
		S.Cruise = .4; S.Burst = 2.2; S.Roam = 30; S.Alt[0] = .3; S.Alt[1] = .9; S.Spacing = 2; S.Cull = 60;
		S.bRows = true;
		S.bBitesRock = true; S.BiteEvery = 40;   // a conch or a crab found and crushed now and then
		S.BiteSound = [](int32 Phase, TArray<FKeysBurst>& Out, FString& What)
		{
			// a shell giving way in its jaws: a low knock, and two sharp cracks of high noise a moment apart
			FKeysBurst Knock;
			Knock.bTone = true; Knock.Wave = KeysDSP::EWave::Triangle; Knock.Tone = 190; Knock.ToneTo = 90; Knock.Gain = .22f; Knock.Att = .002f; Knock.Dur = .07f;
			FKeysBurst Crack;
			Crack.Filter = KeysDSP::EFilter::Bandpass; Crack.Freq = 2400; Crack.FreqTo = 1100; Crack.Q = 1.6f; Crack.Gain = .34f; Crack.Att = .001f; Crack.Dur = .05f;
			FKeysBurst Crack2 = Crack;
			Crack2.Freq = 1800; Crack2.FreqTo = 800; Crack2.Gain = .24f; Crack2.Dur = .08f; Crack2.Delay = .11f;
			Out = {Knock, Crack, Crack2};
			What = TEXT("3 one-shots through AKeysAudio::Burst at the bite, reference distance 2 m: triangle tone 190>90 Hz gain 0.22 70 ms; noise band-pass 2400>1100 Hz Q 1.6 gain 0.34 50 ms; noise band-pass 1800>800 Hz Q 1.6 gain 0.24 80 ms after 110 ms");
		};
		S.FirstSight = TEXT("Loggerhead (Caretta caretta). Protected: look, don't touch.");
		TurtleHooks(S, true);
		return S;
	}

	// registered only where the framework has the hooks: without them these three would be drawn as fish
	template <typename TS> static bool RegisterAll()
	{
		if constexpr (CHooks<TS>)
		{
			Register(&MakeRay);
			Register(&MakeGreen);
			Register(&MakeLogger);
			return true;
		}
		else return false;
	}
	static const bool GRegistered = RegisterAll<FSpecies>();
}
