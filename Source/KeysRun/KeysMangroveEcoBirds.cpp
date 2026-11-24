// The birds of the mangroves (part of the ecosystem, KeysMangroveEco.cpp): great egrets, great blue and little blue
// herons, parties of white ibis and the odd roseate spoonbill wading off the roots; cormorants and anhingas drying
// their wings on low limbs; a belted kingfisher or a green heron on a prop root; night herons that sleep there by day
// and hunt after dark; terns working the edge, an osprey that hovers and dives, brown pelicans; and the rookery keys.
//
// Each bird is a few dozen triangles built from its pose (body, an S of neck, head and bill, legs, two-part wings),
// written into one of two meshes: the ones that are moving, rebuilt every frame, and the ones standing still or far
// off, rebuilt only when one of them changes. They are drawn larger than life (BirdScale: the tall waders a little, the
// small ones on the roots twice and more; none stands taller than a person on deck), each with a dark edge as wide
// as a pixel or two at the distance it is seen from (a dark copy of itself, a little larger, standing just behind it
// as the camera sees it: the material is two-sided, so a hull turned inside out would hide the bird), and with legs, neck and
// bill never thinner than about a pixel, so they read from a boat against bright water.
#include "KeysMangroveEcoInt.h"
#include "KeysGeo.h"
#include "KeysGeoReal.h"
#include "KeysLife.h"
#include "KeysTown.h"
#include "Misc/CommandLine.h"

namespace KeysMangroveEco
{
	namespace
	{
		struct FBirdSpec
		{
			double Body, Leg, Neck, Bill, Head, Span, Speed, FlapHz, StandPitch, Wade, FlushR, Draw;
			uint32 BodyC, BellyC, NeckC, HeadC, BillC, LegC, WingC, TipC;
			uint8 BillShape;   // 0 straight, 1 curved down, 2 spoon, 3 pouch
			bool bNeckOut;     // flies with its neck stretched out (herons and pelicans fold theirs)
		};
		const FBirdSpec Specs[NumBirds] = {
			// body leg  neck bill head  span speed flap pitch wade flush draw   body      belly     neck      head      bill      leg       wing      tip
			{.42, .50, .48, .12, .045, 1.40, 9,   2.4, .55, .45, 30, 700, 0xf4f4ee, 0xf4f4ee, 0xf4f4ee, 0xf4f4ee, 0xe8b030, 0x1c1c1c, 0xf4f4ee, 0xf0f0e8, 0, false},   // great egret
			{.50, .58, .52, .14, .050, 1.80, 9,   2.0, .55, .60, 36, 700, 0x26364e, 0x3a4860, 0x565268, 0xe8e8e8, 0xd8a030, 0x4a4638, 0x2a3a52, 0x121a26, 0, false},   // great blue heron (set dark: full sun washes a mid slate out to pale grey)
			{.30, .32, .30, .08, .035, 1.00, 9,   3.0, .50, .30, 24, 450, 0x1f2c48, 0x1f2c48, 0x45304a, 0x45304a, 0x4a5a70, 0x3a4232, 0x1f2c48, 0x162036, 0, false},   // little blue heron (set dark, as above)
			{.30, .30, .24, .16, .035, .95,  10,  3.6, .35, .22, 22, 500, 0xf4f4ee, 0xf4f4ee, 0xf4f4ee, 0xf2d8d0, 0xe04a30, 0xdc4a32, 0xf4f4ee, 0x181818, 1, true},    // white ibis
			{.40, .40, .32, .17, .040, 1.25, 9.5, 3.0, .40, .32, 26, 600, 0xee7896, 0xf090aa, 0xf6e4e6, 0xdfe2cc, 0x8a9078, 0xc03c54, 0xe8608a, 0xd03862, 2, true},    // roseate spoonbill
			{.42, .07, .28, .07, .040, 1.20, 12,  4.0, 1.0, 0,   20, 400, 0x1c1c1e, 0x242426, 0x1c1c1e, 0x1c1c1e, 0xd09030, 0x141414, 0x26262a, 0x141414, 0, true},    // cormorant
			{.42, .06, .38, .09, .030, 1.15, 11,  3.5, .90, 0,   20, 400, 0x1a1a1c, 0x1a1a1c, 0x4a3a2a, 0x4a3a2a, 0xd8b040, 0x2a2a20, 0x2c2c30, 0xb8b8b0, 0, true},    // anhinga
			{.16, .025, .03, .06, .046, .50, 11,  7.0, .90, 0,   14, 200, 0x1f4f92, 0xf0f0ee, 0xf0f0ee, 0x1a4484, 0x181c20, 0x3a3a3a, 0x1a4484, 0x12223c, 0, true},    // belted kingfisher
			{.22, .14, .14, .07, .035, .65,  9,   4.0, .25, .12, 12, 250, 0x24402f, 0x62301f, 0x62281a, 0x16261d, 0x2a2a2a, 0xd8b838, 0x24402f, 0x16261d, 0, false},   // green heron (set dark, as above)
			{.30, .22, .14, .08, .050, 1.10, 9,   2.8, .60, .20, 16, 350, 0x27303c, 0xc4cad2, 0xc4cad2, 0x0e1216, 0x1a1a1c, 0xe0c440, 0x66707e, 0x30363e, 0, false},   // night heron
			{.20, .03, .03, .06, .035, .80,  10,  3.5, .15, 0,   0,  400, 0xf4f4f2, 0xf4f4f2, 0xf4f4f2, 0x1a1a1c, 0xf08020, 0x202020, 0xc8ccd0, 0x8a9098, 0, true},    // tern
			{.34, .10, .06, .035, .050, 1.60, 11, 2.4, .90, 0,   0,  700, 0x4a3a2c, 0xf0ece4, 0xf0ece4, 0xf0ece4, 0x2a2a2a, 0xc8c8b8, 0x4a3a2c, 0x2a2018, 0, true},    // osprey
			{.56, .11, .36, .32, .060, 2.15, 9,   1.8, .75, 0,   22, 700, 0x8a8f96, 0x2c2622, 0xf0eee6, 0xf2dc8a, 0x8a8a8c, 0x26262a, 0x8e9298, 0x3a3632, 3, false},   // brown pelican (drawn by EmitPelican)
			{.66, .13, .40, .40, .070, 2.75, 9,   1.6, .75, .35, 24, 700, 0xf6f5f0, 0xf6f2e6, 0xf6f5f0, 0xf6f5f0, 0xf0a030, 0xf08a48, 0xf6f5f0, 0x141416, 3, false},   // white pelican (drawn by EmitPelican)
		};
		bool IsWader(int32 K) { return K <= Spoonbill || K == WhitePelican; }   // (a white pelican stands and fishes in the shallows with its flock)
		// How large each is drawn, as a scale on its model. At 1 the models already stand taller than the birds do in
		// life (their necks are drawn upright), so at the 1.5 they all had a great blue heron stood 1.89 m above its
		// feet: taller than a person on deck. It is brought down to 1.67 m, the same as the great egret; the two
		// smallest on the roots, which were specks, are drawn larger (a kingfisher .57 m, a green heron .62 m); the rest
		// stay as they were. What makes them all read is the closer look's lens.
		const double Scales[NumBirds] = {1.5, 1.35, 1.5, 1.5, 1.5, 1.5, 1.5, 2.5, 1.8, 1.5, 1.5, 1.5, 1.4, 1.35};
		// -KRBirdsBefore (a test switch, for measuring against): the birds as they were before the birdview lane, 1.5
		// times life (kingfisher 1.9, green heron 1.7), no dark edge, no least thickness, and no closer look
		bool Before() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("KRBirdsBefore")); return b; }
		double ScaleOf(int32 K) { return Before() && K != WhitePelican ? (K == Kingfisher ? 1.9 : K == GreenHeron ? 1.7 : 1.5) : Scales[K]; }

		// ---- writing triangles (game metres in, Unreal out)
		struct FPen
		{
			FSoup& B;
			double Rim = 0, MinR = 0;   // the dark edge's width, and the least a limb is thick (metres; both grow with the distance it is seen from)
			FVector Back = FVector::ZeroVector;   // from the bird straight away from the camera, a body's length: where its dark copy stands
			FLinearColor RimC = FLinearColor(.012f, .016f, .022f);
			void Tri(const FVector& A, const FVector& Bb, const FVector& C, const FVector& N, const FLinearColor& Col)
			{
				const FVector UA = G2U(A), UB = G2U(Bb), UC = G2U(C), UN = D2U(N).GetSafeNormal();
				const bool bSwap = FVector::DotProduct(FVector::CrossProduct(UB - UA, UC - UA), UN) > 0;   // Unreal: front faces wind clockwise against the normal
				const int32 I = B.V.Num();
				B.V.Add(UA); B.V.Add(bSwap ? UC : UB); B.V.Add(bSwap ? UB : UC);
				B.N.Add(UN); B.N.Add(UN); B.N.Add(UN);
				B.C.Add(Col); B.C.Add(Col); B.C.Add(Col);
				B.T.Add(I); B.T.Add(I + 1); B.T.Add(I + 2);
			}
			void Tri2(const FVector& A, const FVector& Bb, const FVector& C, const FLinearColor& Top, const FLinearColor& Under)
			{
				FVector N = FVector::CrossProduct(Bb - A, C - A).GetSafeNormal();
				if (N.Y < 0) N = -N;
				Tri(A, Bb, C, N, Top);
				Tri(A, Bb, C, -N, Under);
			}
			// a three-sided limb
			void Prism(const FVector& P0, const FVector& P1, double R0In, double R1In, const FLinearColor& Col, bool bEdge = false)
			{
				// (a limb that is there at all is never thinner than MinR; one drawn shut, with no thickness, stays shut)
				const bool bIs = R0In > 0 || R1In > 0;
				const double R0 = bIs ? FMath::Max(R0In, MinR) : 0, R1 = bIs ? FMath::Max(R1In, MinR * .6) : 0;
				const FVector D = (P1 - P0).GetSafeNormal(.0001, FVector(0, 1, 0));
				const FVector U = FVector::CrossProduct(D, FMath::Abs(D.Y) < .9 ? FVector(0, 1, 0) : FVector(1, 0, 0)).GetSafeNormal();
				const FVector W = FVector::CrossProduct(D, U);
				FVector Ring[3];
				for (int32 k = 0; k < 3; k++) { const double A = 2 * PI * k / 3; Ring[k] = U * FMath::Cos(A) + W * FMath::Sin(A); }
				for (int32 k = 0; k < 3; k++)
				{
					const FVector& Ra = Ring[k];
					const FVector& Rb = Ring[(k + 1) % 3];
					const FVector N = (Ra + Rb).GetSafeNormal();
					Tri(P0 + Ra * R0, P0 + Rb * R0, P1 + Rb * R1, N, Col);
					Tri(P0 + Ra * R0, P1 + Rb * R1, P1 + Ra * R1, N, Col);
				}
				if (!bEdge || Rim <= 0) return;
				// its dark edge: the same limb a little thicker, behind it, so only its rim shows round it
				// (no wider than the limb is thick: a neck in a fat black line is a black neck)
				const double E0 = bIs ? R0 + FMath::Min(Rim, R0 * 1.2 + .004) : 0, E1 = bIs ? R1 + FMath::Min(Rim, R1 * 1.2 + .004) : 0;
				const FVector Q0 = P0 + Back, Q1 = P1 + Back;
				for (int32 k = 0; k < 3; k++)
				{
					const FVector& Ra = Ring[k];
					const FVector& Rb = Ring[(k + 1) % 3];
					const FVector N = (Ra + Rb).GetSafeNormal();
					Tri(Q0 + Ra * E0, Q0 + Rb * E0, Q1 + Rb * E1, N, RimC);
					Tri(Q0 + Ra * E0, Q1 + Rb * E1, Q1 + Ra * E1, N, RimC);
				}
			}
			// a rounded body: a spindle Len long on Axis, Rw wide (along Side) and Rh deep (along Top), smooth shaded,
			// one colour above and another below
			void Egg(const FVector& C, const FVector& Axis, const FVector& Side, const FVector& Top, double Len, double Rw, double Rh, const FLinearColor& Above, const FLinearColor& Below, int32 Seg, bool bEdge = true)
			{
				Shell(C, Axis, Side, Top, Len, Rw, Rh, Above, Below, Seg, 0);
				// its dark edge: the same body a little larger, behind it (nothing, for a body drawn with no size)
				if (bEdge && Rim > 0) Shell(C + Back, Axis, Side, Top, Len, Rw, Rh, RimC, RimC, Seg, Len > 0 ? Rim : 0);
			}
			void Shell(const FVector& C, const FVector& Axis, const FVector& Side, const FVector& Top, double Len, double Rw, double Rh, const FLinearColor& Above, const FLinearColor& Below, int32 Seg, double Grow)
			{
				const int32 I0 = B.V.Num();
				auto Put = [&](const FVector& P)
				{
					const FVector Nn = FVector((P - C).GetSafeNormal(.0001, Top));
					B.V.Add(G2U(P + Nn * Grow)); B.N.Add(D2U(Nn));
					// (its back and flanks are the upper colour; the under colour is the belly only: seen from the side a
					// kingfisher or a night heron is its dark back, not a wash of the two)
					B.C.Add(FMath::Lerp(Below, Above, float(FMath::Clamp(FVector::DotProduct(Nn, Top) * 3.0 + .8, 0.0, 1.0))));
				};
				Put(C - Axis * Len * .5);
				for (int32 r = 0; r < 2; r++)
				{
					const double T = r ? .2 : -.2, K = r ? .82 : 1.0;
					for (int32 i = 0; i < Seg; i++) { const double A = 2 * PI * i / Seg; Put(C + Axis * Len * T + (Side * FMath::Cos(A) * Rw + Top * FMath::Sin(A) * Rh) * K); }
				}
				Put(C + Axis * Len * .5);
				const int32 Nose = I0 + 1 + 2 * Seg;
				auto Face = [&](int32 A, int32 Bb, int32 Cc)
				{
					const double D = FVector::DotProduct(FVector::CrossProduct(B.V[Bb] - B.V[A], B.V[Cc] - B.V[A]), B.N[A] + B.N[Bb] + B.N[Cc]);
					B.T.Add(A); B.T.Add(D > 0 ? Cc : Bb); B.T.Add(D > 0 ? Bb : Cc);
				};
				for (int32 i = 0; i < Seg; i++)
				{
					const int32 a = I0 + 1 + i, b = I0 + 1 + (i + 1) % Seg, c = a + Seg, d = b + Seg;
					Face(I0, a, b);
					Face(a, b, d);
					Face(a, d, c);
					Face(c, d, Nose);
				}
			}
		};

		FLinearColor Hex(uint32 C) { return KeysTownPaint::Hex(C); }


		// ---- the pelicans, built on the user's two pictures: a white pelican gliding low over the water seen from the
		// front quarter (the huge yellow-orange bill and pouch with a red nail at its tip, a small shaggy crest, the neck
		// tucked back in an S so the head rests on the shoulders, long broad wings with white coverts and black flight
		// feathers whose tips spread like fingers, a short fanned tail, orange legs and webbed feet trailing behind), and
		// an adult brown pelican standing hunched on a piling (pale yellow crown, white neck, the long bill held down
		// against the breast, grey at the base and orange-pink toward the tip over a dark grey pouch, silver grey-brown
		// back and wings, dark belly, short dark legs, the folded wingtips and tail hanging below its perch).
		// The white one here is the American white pelican, the photograph's near twin and the one that winters in
		// Florida Bay: it has no pink face. A young brown pelican is brown-headed and pale-bellied.
		struct FPelSpec { double Body, Span, Bill, Leg; uint32 BackC, BellyC, NeckC, CrownC, BillC, TipC, PouchC, CovertC, FlightC, LegC, TailC; bool bWhiteInner; };
		// (the brown pelican's silver-grey and brown are set dark: in the game's full sun 0x5a5e66 washed out to white
		// and its dark flight feathers to tan)
		const FPelSpec PelBrown = {.56, 2.15, .37, .085, 0x34363b, 0x1a1614, 0xf0eee6, 0xf0d470, 0x6c6c72, 0xe07850, 0x2a2c30, 0x45484f, 0x15120f, 0x1c1c20, 0x262320, false};
		const FPelSpec PelYoung = {.54, 2.10, .36, .085, 0x4c3e30, 0xdcd4c2, 0x483a2c, 0x483a2c, 0x777165, 0x8c7c62, 0x4a4038, 0x4e4032, 0x221c16, 0x2c2828, 0x33291f, false};
		const FPelSpec PelWhite = {.66, 2.75, .47, .13, 0xf6f5f0, 0xf4eedc, 0xf6f5f0, 0xf6f5f0, 0xee9a28, 0xd8401e, 0xf2a820, 0xf6f5f0, 0x101012, 0xee7c3c, 0xf6f5f0, true};
		// "mix them up": about one in four of the followers in a flock of white pelicans on a flat is a brown pelican
		// loafing with them, as they do in life on the bay's sandbars. It keeps with the flock (it is one of its
		// birds) but is drawn as a brown pelican, and does not dip its bill with the whites.
		bool BrownAmongWhites(const FBird& B) { return B.Kind == WhitePelican && B.Leader >= 0 && B.Rook < 0 && EHash(B.Seed * 1e-6, 9.0) < .26; }
		const FPelSpec& PelOf(const FBird& B) { return B.Kind == WhitePelican && !BrownAmongWhites(B) ? PelWhite : B.Rank < .22 ? PelYoung : PelBrown; }

		void EmitPelican(FSoup& Buf, const FBird& B, bool bFar, double Rim, double MinR, const FVector& Away)
		{
			const FPelSpec& K = PelOf(B);
			FPen Pen{Buf};
			const double S = B.Sz;
			Pen.Rim = Rim; Pen.MinR = MinR; Pen.Back = Away * (K.Body * S * 1.15 + .05);
			const FVector F(FMath::Cos(B.Yaw), 0, FMath::Sin(B.Yaw)), Up(0, 1, 0), Rt(-F.Z, 0, F.X);
			const bool bAir = B.State == BFly || B.State == BHover || B.State == BDive || B.State == BPatrol, bSwim = B.State == BSwim;
			// (a plunge: the brown pelican going down on bait, its neck out and its wings half shut)
			const bool bStoop = bAir && B.Pitch < -.75;
			const double Pitch = bAir ? B.Pitch : bSwim ? .06 : .78;   // at rest it sits upright and hunched
			const FVector A = F * FMath::Cos(Pitch) + Up * FMath::Sin(Pitch), Top = Up * FMath::Cos(Pitch) - F * FMath::Sin(Pitch);
			const double BodyL = K.Body * S, BodyR = BodyL * .3, LegL = K.Leg * S;
			const FVector Hip = bAir ? B.P : bSwim ? B.P - Up * BodyR * .45 : B.P + Up * (LegL + BodyL * .3);
			const FVector Body = Hip + A * BodyL * .12;
			// (no two quite alike: a brown pelican's back and wings a little lighter or darker, a white one's bill and
			// pouch a little yellower or more orange)
			const float Tone = float(.86 + .28 * EHash(B.Seed * 1e-6, 5.0));
			const bool bWh = K.bWhiteInner;
			auto Tn = [&](FLinearColor C) { if (!bWh) { C.R *= Tone; C.G *= Tone; C.B *= Tone; } return C; };
			const FLinearColor BackC = Tn(Hex(K.BackC)), BellyC = Hex(K.BellyC), NeckC = Hex(K.NeckC), BillC = bWh ? FMath::Lerp(Hex(K.BillC), Hex(0xf4b838), (Tone - .86f) * 2.2f) : Hex(K.BillC), TipC = Hex(K.TipC),
				PouchC = bWh ? FMath::Lerp(Hex(K.PouchC), Hex(0xf08a20), (1.14f - Tone) * 2.2f) : Hex(K.PouchC), CovC = Tn(Hex(K.CovertC)), FlC = Hex(K.FlightC), LegC = Hex(K.LegC);
			Pen.Egg(Body, A, Rt, Top, BodyL * 1.3, BodyR * .9, BodyR * .85, BackC, BellyC, bFar ? 4 : 6);   // (a long, heavy body, not a ball)
			// ---- the neck and head: tucked back on the shoulders in the air, upright with the bill down the breast at
			// rest, up and forward on the water, out straight in a plunge, down in the water when it dips for a fish
			const FVector Sh = Body + A * BodyL * .40 + Top * BodyR * .2;
			const double NL = BodyL * .62, Nr = BodyL * .078, Nk = FMath::Clamp(FMath::Max(B.Neck, B.Peck), 0.0, 1.0);
			FVector Bend, Head, Hd;
			if (bStoop) { Bend = Sh + A * NL * .5; Head = Sh + A * NL; Hd = A; }
			else if (bAir) { Bend = Sh + Top * NL * .40 - A * NL * .2; Head = Sh + Top * NL * .46 + A * NL * .2; Hd = A * .97 - Top * .22; }
			else if (bSwim) { Bend = Sh + Up * NL * .7 - F * NL * .1; Head = Bend + Up * NL * .22 + F * NL * .18; Hd = F * .85 - Up * .5; }
			else
			{
				Bend = Sh + Up * NL * .72 - F * NL * .14; Head = Bend + Up * NL * .2 + F * NL * .2;
				Hd = B.Rank > .72 ? F * .9 - Up * .42 : F * .4 - Up * .92;   // (one in four has its head up, looking about)
			}
			if (Nk > 0 && !bAir)
			{
				const FVector Dir = (F * .8 - Up * .6).GetSafeNormal();
				Bend = FMath::Lerp(Bend, Sh + Dir * NL * .5, Nk); Head = FMath::Lerp(Head, Sh + Dir * NL * 1.05, Nk); Hd = FMath::Lerp(Hd, F * .45 - Up * .9, Nk);
			}
			Hd = Hd.GetSafeNormal();
			const FVector HdUp = (Up - Hd * FVector::DotProduct(Up, Hd)).GetSafeNormal(.001, A);
			if (bFar) Pen.Prism(Sh - A * BodyR * .2, Head, Nr * 1.5, Nr, NeckC, true);
			else { Pen.Prism(Sh - A * BodyR * .2, Bend, Nr * 1.5, Nr * 1.15, NeckC, true); Pen.Prism(Bend, Head, Nr * 1.15, Nr, NeckC, true); }
			const double HeadR = BodyL * .115;   // (a big head and a huge bill: they are what a pelican is)
			Pen.Egg(Head + Hd * HeadR * .3, Hd, Rt, HdUp, HeadR * 2.6, HeadR, HeadR * 1.05, Hex(K.CrownC), NeckC, 5);
			// (the white pelican's small shaggy crest, at the back of the head; shut for the others)
			const double Cr = K.bWhiteInner ? HeadR : 0;
			Pen.Tri(Head - Hd * HeadR * .7 + HdUp * HeadR * .5, Head - Hd * HeadR * .7 - HdUp * HeadR * .3, Head - Hd * (HeadR * .7 + Cr * 1.5) + HdUp * Cr * .1, Rt, NeckC);
			// ---- the bill and its pouch
			const FVector B0 = Head + Hd * HeadR * 1.2;
			const double BL = K.Bill * S;
			const FVector Mid = B0 + Hd * BL * .55, Tip = B0 + Hd * BL;
			Pen.Prism(B0, Mid, HeadR * .42, HeadR * .3, BillC, true);
			Pen.Prism(Mid, Tip, HeadR * .3, HeadR * .13, FMath::Lerp(BillC, TipC, .6f), true);
			Pen.Prism(Tip - Hd * .02 * S, Tip + Hd * .012 * S - HdUp * .022 * S, HeadR * .15, HeadR * .07, TipC);   // the nail, hooked down
			const FVector Thr = B0 - Hd * HeadR * .7 - HdUp * HeadR * .75, Deep = B0 + Hd * BL * .36 - HdUp * HeadR * 1.55, Pw = Rt * HeadR * .42;
			Pen.Tri(Thr, Deep + Pw, Tip, Rt, PouchC);
			Pen.Tri(Thr, Deep - Pw, Tip, -Rt, PouchC);
			Pen.Tri(B0 + Pw * .9, Deep + Pw, Tip, Rt, PouchC);
			Pen.Tri(B0 - Pw * .9, Deep - Pw, Tip, -Rt, PouchC);
			// ---- the legs and webbed feet: trailing under the tail in the air, under it at rest, out of sight on the water
			const double Lr = FMath::Max(.012, LegL * .13);
			for (const double Sd : {-1.0, 1.0})
			{
				FVector HipS = Hip, Foot = Hip, W0 = Hip, W1 = Hip;
				if (bAir)
				{
					HipS = Body - A * BodyL * .36 - Top * BodyR * .55 + Rt * BodyR * .32 * Sd;
					Foot = HipS - A * BodyL * .36 - Top * BodyR * .1;
					W0 = Foot - A * .17 * S + Rt * .075 * S - Top * .02 * S; W1 = Foot - A * .17 * S - Rt * .075 * S - Top * .02 * S;
				}
				else if (!bSwim)
				{
					HipS = Hip - Up * BodyL * .26 + Rt * BodyR * .4 * Sd;
					Foot = B.P + Rt * .075 * S * Sd;
					W0 = Foot + F * .15 * S + Rt * .07 * S; W1 = Foot + F * .15 * S - Rt * .07 * S;
				}
				Pen.Prism(HipS, Foot, bSwim ? 0 : Lr, bSwim ? 0 : Lr * .8, LegC);
				Pen.Tri(Foot, W0, W1, bAir ? Top : Up, LegC);
			}
			// ---- the wings. In the air: an inner and an outer panel, each with its coverts along the leading edge and
			// its flight feathers behind them, and five primaries spread like fingers at the tip. It flaps a few beats
			// and glides a long way on set wings, tips a little up.
			const double W = bAir ? FMath::Max(B.Wing, .001) : 0;
			const double Cyc = FMath::Fmod(B.Ph * .8 + B.Rank * 4.2, 4.2);
			const bool bFlap = Cyc < 1.3 || (B.State == BFly && B.FlyT < 1.5) || B.State == BHover;
			const double Th = bStoop ? .25 : bFlap ? FMath::Sin(B.Ph * 2 * PI * B.FlapHz) * .55 + .1 : .09 + .03 * FMath::Sin(B.Ph * 1.1);
			const double Sweep = bStoop ? .7 : 0, Half = K.Span * .5 * S * W, C0 = BodyL * .95 * (W > 0 ? 1 : 0), C1 = BodyL * .8 * (W > 0 ? 1 : 0);   // (broad wings: a chord near the body's own length)
			for (const double Sd : {-1.0, 1.0})
			{
				const FVector J = Body + A * BodyL * .2 + Top * BodyR * .62 + Rt * BodyR * .55 * Sd * (W > 0 ? 1 : 0);
				const FVector O1 = Rt * Sd * FMath::Cos(Th) + Top * FMath::Sin(Th);
				const FVector Wr = J + O1 * Half * .5 + A * Half * (.05 - Sweep * .35);
				const double Th2 = Th * 1.3 + (bFlap || bStoop ? 0 : .14);
				const FVector O2 = Rt * Sd * FMath::Cos(Th2) + Top * FMath::Sin(Th2);
				const FVector Hn = Wr + O2 * Half * .34 - A * Half * (.05 + Sweep * .5);
				const FVector JL = J + A * C0 * .45, JM = J - A * C0 * .05, JT = J - A * C0 * .55;
				const FVector WL = Wr + A * C1 * .42, WM = Wr - A * C1 * .05, WT = Wr - A * C1 * .6;
				const FVector HL = Hn + A * C1 * .24, HM = Hn - A * C1 * .08, HT = Hn - A * C1 * .5;
				const FVector Nn = FVector::CrossProduct(O1, A).GetSafeNormal(.001, Top) * (Sd > 0 ? 1.0 : -1.0);
				auto Quad = [&](const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, const FLinearColor& Col) { Pen.Tri(P0, P1, P2, Nn, Col); Pen.Tri(P0, P2, P3, Nn, Col); };
				Quad(JL, WL, WM, JM, CovC);
				Quad(WL, HL, HM, WM, CovC);
				// (the white pelican's inner secondaries are white, the outer ones and the primaries black)
				const FVector IM = FMath::Lerp(JM, WM, .45), IT = FMath::Lerp(JT, WT, .45);
				Quad(JM, IM, IT, JT, K.bWhiteInner ? CovC : FlC);
				Quad(IM, WM, WT, IT, FlC);
				Quad(WM, HM, HT, WT, FlC);
				for (int32 f = 0; f < 5; f++)
				{
					const double U = f / 4.0;
					// (broad feathers that touch at their roots and part toward their tips, not spikes)
					const FVector Base = FMath::Lerp(HL, HT, U), Step = (HT - HL) * .125;
					const FVector Dir = (O2 * (1 - .2 * U) - A * (.05 + .7 * U) + Top * (bFlap || bStoop ? 0 : .03 * (1 - U))).GetSafeNormal(.001, O2);
					Pen.Tri(Base - Step, Base + Step, Base + Dir * Half * .2 * (1 - .15 * U), Nn, FlC);
				}
				// shut, at rest and on the water: laid along the flank, its dark tip crossing the tail and hanging below it
				const double Fo = bAir ? 0 : 1;
				const FVector P0 = Body + (A * BodyL * .3 + Top * BodyR * .55 + Rt * Sd * BodyR * .88) * Fo, P1 = Body + (-A * BodyL * .66 - Top * BodyR * .05 + Rt * Sd * BodyR * .3) * Fo;
				const FVector P2 = Body + (-A * BodyL * .05 - Top * BodyR * .8 + Rt * Sd * BodyR * .9) * Fo, Ou = Rt * Sd * .012 * Fo;
				Pen.Tri(P0, P1, P2, Rt * Sd, CovC);
				Pen.Tri(FMath::Lerp(P0, P1, .58) + Ou, P1 + Ou, FMath::Lerp(P2, P1, .58) + Ou, Rt * Sd, FlC);
			}
			// ---- the short tail, fanned
			const FVector T0 = Body - A * BodyL * .47, Tw = Rt * BodyR * (bAir ? .75 : .4), Te = T0 - A * BodyL * .24 - Top * BodyR * (bAir ? 0 : .2);
			Pen.Tri(T0 + Tw * .5, T0 - Tw * .5, Te + Tw, Top, Hex(K.TailC));
			Pen.Tri(T0 - Tw * .5, Te - Tw, Te + Tw, Top, Hex(K.TailC));
		}

		// One bird from its pose. Every bird writes the same number of vertices whatever it is doing (a folded wing is
		// drawn shut, not left out), so a mesh's layout only changes when a bird joins or leaves it.
		void Emit(FSoup& Buf, const FBird& B, bool bFar, double Rim, double MinR, const FVector& Away)
		{
			if (B.Kind == Pelican || B.Kind == WhitePelican) { EmitPelican(Buf, B, bFar, Rim, MinR, Away); return; }
			const FBirdSpec& K = Specs[B.Kind];
			FPen Pen{Buf};
			Pen.Rim = Rim; Pen.MinR = MinR; Pen.Back = Away * (K.Body * B.Sz * 1.15 + .05);
			const double S = B.Sz;
			const FVector F(FMath::Cos(B.Yaw), 0, FMath::Sin(B.Yaw)), Up(0, 1, 0), Rt(-F.Z, 0, F.X);
			const bool bAir = B.State == BFly || B.State == BHover || B.State == BDive || B.State == BPatrol, bSwim = B.State == BSwim;
			const double Pitch = bAir ? B.Pitch : bSwim ? .1 : B.State == BWalk ? K.StandPitch * .55 : B.State == BStrike ? K.StandPitch * .2 : K.StandPitch;
			const FVector A = F * FMath::Cos(Pitch) + Up * FMath::Sin(Pitch), Dn = Up * FMath::Cos(Pitch) - F * FMath::Sin(Pitch);
			const double LegL = K.Leg * S, BodyL = K.Body * S, BodyR = K.Body * .3 * S;
			const FVector Hip = bAir ? B.P : bSwim ? B.P - Up * BodyR * .45 : B.P + Up * LegL;
			const FVector Body = Hip + A * BodyL * .1;
			const FLinearColor BodyC = Hex(K.BodyC), BellyC = Hex(K.BellyC), NeckC = Hex(K.NeckC), LegC = Hex(K.LegC), WingC = Hex(K.WingC), TipC = Hex(K.TipC), BillC = Hex(K.BillC);
			Pen.Egg(Body, A, Rt, Dn, BodyL, BodyR * .95, BodyR, BodyC, BellyC, bFar ? 4 : 6);
			// the neck: an S at rest, straight down the line of a strike, folded back on the shoulders asleep or (herons) in flight
			const FVector Sh = Body + A * BodyL * .36 + Dn * BodyR * .25;
			const double NL = K.Neck * S, Nk = FMath::Max(B.Neck, B.Peck);
			FVector Bend = Sh + Up * NL * .5 - F * NL * .14, Head = Bend + Up * NL * .42 + F * NL * .24;
			double HeadPitch = -.1;
			if (bAir && K.bNeckOut) { Bend = Sh + A * NL * .5; Head = Sh + A * NL; HeadPitch = Pitch * .6; }
			else if (bAir || Nk < 0)
			{
				const double U = bAir ? 1 : -Nk;
				Bend = FMath::Lerp(Bend, Sh + Up * NL * .2 - F * NL * .12, U); Head = FMath::Lerp(Head, Sh + Up * NL * .3 + F * NL * .1, U);
				HeadPitch = bAir ? Pitch * .5 : -.1 - .5 * U;
			}
			else if (Nk > 0)
			{
				const FVector Dir = (F * .8 - Up * .52).GetSafeNormal();
				Bend = FMath::Lerp(Bend, Sh + Dir * NL * .55, Nk); Head = FMath::Lerp(Head, Sh + Dir * NL * 1.1, Nk);
				HeadPitch = -.1 - .65 * Nk;
			}
			const double Nr = FMath::Max(.014, K.Body * (B.Kind == Anhinga ? .05 : .075)) * S;
			if (bFar) Pen.Prism(Sh - A * BodyR * .3, Head, Nr * 1.25, Nr * .8, NeckC, true);
			else { Pen.Prism(Sh - A * BodyR * .3, Bend, Nr * 1.25, Nr, NeckC, true); Pen.Prism(Bend, Head, Nr, Nr * .8, NeckC, true); }
			const FVector Hd = (F * FMath::Cos(HeadPitch) + Up * FMath::Sin(HeadPitch)).GetSafeNormal(), HdUp = Up * FMath::Cos(HeadPitch) - F * FMath::Sin(HeadPitch);
			const double HeadR = K.Head * S;
			Pen.Egg(Head + Hd * HeadR * .5, Hd, Rt, HdUp, HeadR * 2.8, HeadR, HeadR, Hex(K.HeadC), Hex(K.BillShape == 3 ? K.NeckC : K.HeadC), 5);
			// the bill
			const FVector B0 = Head + Hd * HeadR * 1.5;
			const double BL = K.Bill * S, Br = FMath::Max(.008, K.Head * .32) * S;
			const FVector Mid = B0 + Hd * BL * .5;
			const FVector Tip = K.BillShape == 1 ? Mid + (Hd * .45 - HdUp * .26) * BL : B0 + Hd * BL;
			if (bFar) Pen.Prism(B0, Tip, Br, Br * .3, BillC, true);
			else { Pen.Prism(B0, Mid, Br, Br * .65, BillC, true); Pen.Prism(Mid, Tip, Br * .65, Br * .2, BillC, true); }
			if (bFar) {}
			else if (K.BillShape == 2) Pen.Prism(Tip - Hd * .035 * S, Tip + Hd * .03 * S, .03 * S, .026 * S, BillC);
			else if (K.BillShape == 3) Pen.Prism(B0 - HdUp * .035 * S, Tip - HdUp * .012 * S, .034 * S, .01 * S, Hex(0x7a5c48));
			else Pen.Prism(Tip, Tip, 0, 0, BillC);
			// the legs: wading or perched they stand, walking they swing, in flight the long ones trail
			const double Lr = FMath::Max(.008, K.Leg * .024) * S;
			const FVector Tail = Body - A * BodyL * .48;
			for (const double Sd : {-1.0, 1.0})
			{
				const FVector HipS = Hip + Rt * BodyR * .45 * Sd - A * BodyL * .05;
				FVector Knee = HipS, Foot = HipS;
				if (bAir) { if (K.Leg > .2) { Knee = Tail - A * LegL * .45 + Rt * .02 * Sd; Foot = Tail - A * LegL * .95 + Rt * .02 * Sd; } }
				else if (!bSwim)
				{
					const double Swing = B.State == BWalk ? FMath::Sin(B.Ph * 5 + (Sd > 0 ? PI : 0)) * .13 * S : 0;
					Foot = B.P + Rt * .045 * S * Sd + F * (.02 * Sd * S + Swing) - Up * (K.Wade > 0 && B.State != BPerch && B.State != BRoost && B.State != BDry ? .3 : 0.0);
					Knee = FMath::Lerp(HipS, Foot, .5) - F * .035 * S + Up * FMath::Max(0.0, Swing) * .4;
				}
				if (bFar) Pen.Prism(HipS, Foot, Lr * 1.3, Lr, LegC);
				else { Pen.Prism(HipS, Knee, Lr, Lr, LegC); Pen.Prism(Knee, Foot, Lr, Lr * .8, LegC); }
			}
			// the wings: shoulder to wrist, wrist to tip; folded they lie shut along the body
			const double W = B.Wing, Th = bAir ? FMath::Sin(B.Ph * 2 * PI * B.FlapHz) * (B.State == BDive ? .1 : .7) + .08 : .35;   // (held out to dry: elbows up, hands hanging)
			const double Half = K.Span * .5 * S * W, Chord = FMath::Max(K.Body * .62, K.Span * .17) * S * (bAir ? 1 : .6);
			for (const double Sd : {-1.0, 1.0})
			{
				const FVector J = Body + A * BodyL * .16 + Dn * BodyR * .55 + Rt * BodyR * .5 * Sd;
				const double Sweep = B.State == BDive ? .55 : bAir ? 0 : .3;
				const FVector El = J + (Rt * Sd * FMath::Cos(Th) + Dn * FMath::Sin(Th)) * Half * .45 - A * Half * .45 * Sweep;
				const double Th2 = bAir ? Th * 1.25 - .12 : -.95;
				const FVector Wt = El + (Rt * Sd * FMath::Cos(Th2) + Dn * FMath::Sin(Th2)) * Half * .55 - A * Half * (.14 + Sweep * .5);
				const FVector J0 = J + A * Chord * .42 * W, J1 = J - A * Chord * .5 * W, E0 = El + A * Chord * .34 * W, E1 = El - A * Chord * .5 * W;
				Pen.Tri2(J0, J1, E1, WingC, WingC * .8f);
				Pen.Tri2(J0, E1, E0, WingC, WingC * .8f);
				Pen.Tri2(E0, E1, Wt, TipC, TipC * .8f);
			}
			const double TailL = K.Body * (B.Kind == Anhinga ? .62 : .3) * S, TailW = BodyR * (bAir ? .8 : .45);
			Pen.Tri2(Tail + Rt * TailW, Tail - Rt * TailW, Tail - A * TailL - Dn * TailL * (bAir ? 0 : .25), B.Kind == Anhinga ? TipC : WingC, WingC * .7f);
			// what an osprey or a kingfisher carries off
			const double Fl = B.bCarry ? (B.Kind == Osprey ? .3 : .09) * S : 0;
			if (!bFar) Pen.Egg(Body - Dn * (BodyR + Fl * .25) - A * Fl * .1, A, Rt, Dn, Fl, Fl * .14, Fl * .2, Hex(0x8a98a0), Hex(0xe8ecec), 4, false);
		}

		// The dead snag a cormorant or an anhinga dries its wings on: a weathered post standing in the water under the
		// end of its limb, with a bar across the top that the bird sits on. The limb alone (KeysMangrove.cpp) is thin
		// and, from the open water, end on: the bird seemed to sit in mid-air. The bar lies along the shore, so it is
		// seen side on from the water, and the post is seen from everywhere.
		void EmitSnag(FSoup& Buf, const FEco& E, const FBird& B, double Rim)
		{
			FPen Pen{Buf};
			Pen.MinR = Rim * .3;
			const FLinearColor Wood = Hex(0x6e6052), Pale = Hex(0x93877a);
			const FVector Top = B.Perch - FVector(0, .05, 0);
			const double Foot = FMath::Min(FloorAt(Top.X, Top.Z), E.Sea - .4);
			const double Lean = (EHash(B.Seed * 1e-6, 7.0) - .5) * .5;
			const FVector2D Tn(-B.Out.Y, B.Out.X);
			const FVector Along(Tn.X, 0, Tn.Y);
			Pen.Prism(FVector(Top.X + Tn.X * Lean, Foot, Top.Z + Tn.Y * Lean), Top, .085, .06, Wood);
			Pen.Prism(Top - Along * .75 - FVector(0, .06, 0), Top + Along * .8 + FVector(0, .05, 0), .05, .035, Pale);
			Pen.Prism(Top + FVector(0, -.5, 0), Top + FVector(0, -.2, 0) - Along * .45 + FVector(B.Out.X, 0, B.Out.Y) * .25, .04, .02, Wood);
		}

		// The piling a brown pelican sits on (the user's second picture: an adult hunched on a weathered wooden piling):
		// an old post standing in the water off the roots, grey with weather, darker where the tide wets it.
		void EmitPiling(FSoup& Buf, const FEco& E, const FBird& B, double Rim)
		{
			FPen Pen{Buf};
			Pen.MinR = Rim * .3;
			const FVector Top = B.Perch - FVector(0, .02, 0);
			const double Foot = FMath::Min(FloorAt(Top.X, Top.Z), E.Sea - .4);
			const double Lean = (EHash(B.Seed * 1e-6, 7.0) - .5) * .22;
			const FVector Wet(Top.X + B.Out.Y * Lean * .7, E.Sea + .45, Top.Z - B.Out.X * Lean * .7);
			Pen.Prism(FVector(Top.X + B.Out.Y * Lean, Foot, Top.Z - B.Out.X * Lean), Wet, .17, .165, Hex(0x3e3830));
			Pen.Prism(Wet, Top, .165, .15, Hex(0x8c8478));
		}

		// where its feet are: on the bottom, but never so deep that the water is over its knees (it keeps to the shallow side)
		double FeetY(const FEco& E, const FBird& B, double X, double Z) { return FMath::Max(FloorAt(X, Z), E.Sea - Specs[B.Kind].Leg * B.Sz * .7); }
		// can a wader of that leg stand there now?
		bool Wadeable(const FEco& E, double X, double Z, double Limit)
		{
			if (!Wet(X, Z)) return false;
			const double D = E.Sea - FloorAt(X, Z);
			return D > -.2 && D <= Limit;
		}

		void StartFly(FBird& B, const FVector& To, uint8 Next, double Arc, double Speed = 0)
		{
			const FBirdSpec& K = Specs[B.Kind];
			const bool bAir = B.State == BFly || B.State == BHover || B.State == BDive || B.State == BPatrol;
			B.From = bAir ? B.P : B.P + FVector(0, K.Leg * B.Sz, 0);
			B.To = To;
			B.P = B.From;
			B.FlyT = 0;
			B.FlyDur = FMath::Max(.5, FVector::Distance(B.From, To) / (Speed > 0 ? Speed : K.Speed));
			B.Arc = Arc;
			B.Next = Next;
			B.State = BFly;
			B.Wing = 1;
			B.Neck = 0; B.Peck = 0;
		}
		// where a bird rests when it is not doing anything else
		uint8 RestState(const FEco& E, const FBird& B)
		{
			const bool bAsleep = B.Kind == NightHeron ? E.Phase != 0 : E.Phase == 0;
			if (bAsleep) return BRoost;
			if (B.Kind == Cormorant || B.Kind == Anhinga) return BDry;
			return IsWader(B.Kind) && B.Rook < 0 ? BStand : BPerch;
		}
		void Land(const FEco& E, FBird& B)
		{
			B.P = B.To;
			B.State = B.Next;
			B.Pitch = 0;
			B.Wing = B.State == BDry ? .8 : 0;
			B.Neck = B.State == BRoost ? -1 : 0;
			if (B.State == BStand || B.State == BWalk) B.P.Y = FeetY(E, B, B.P.X, B.P.Z);
			B.Think = FMath::FRandRange(1.0, 3.0);
		}
	}

	double BirdScale(int32 Kind) { return Kind >= 0 && Kind < NumBirds ? ScaleOf(Kind) : 1; }
	bool BirdsBefore() { return Before(); }
	void BirdSoup(FSoup& Buf, const FBird& B, bool bFar, double Rim) { Emit(Buf, B, bFar, Rim, Rim * .25, FVector::ZeroVector); }
	void PoseBird(FBird& B, int32 Kind)
	{
		B = FBird();
		B.Kind = uint8(Kind);
		B.Sz = ScaleOf(Kind);
		B.State = IsWader(Kind) ? BStand : Kind == Cormorant || Kind == Anhinga ? BDry : Kind == NightHeron ? BRoost : Kind == Tern || Kind == Osprey ? BPatrol : BPerch;
		B.Wing = B.State == BDry ? .8 : B.State == BPatrol ? 1 : 0;
		B.Neck = B.State == BRoost ? -1 : 0;
		B.FlapHz = Specs[Kind].FlapHz;
	}
	// test (kr.eco fly): the bird takes wing now and glides low to To, then is away for a while as a flushed bird is
	void FlyPast(FBird& B, const FVector& To, double Delay)
	{
		StartFly(B, To, BAway, 1.2);
		B.FlyT = -Delay;
		B.Cool = 12;
	}
	// how much of a rookery's day birds are at home at this hour, and a bird's place in the queue: by day its turn to
	// be out on the flats and to be at home comes round every seven minutes, so there is always someone coming or going
	static double RookHome(const FEco& E)
	{
		const double H = E.Hour;
		return E.Phase == 0 ? 1 : E.Phase == 1 ? FMath::Lerp(1.0, .45, FMath::Clamp((H - 5.25) / 2.5, 0.0, 1.0)) : E.Phase == 2 ? .62 : FMath::Lerp(.62, 1.0, FMath::Clamp((H - 17.5) / 2.25, 0.0, 1.0));
	}
	static double RookRank(const FEco& E, const FBird& B) { return FMath::Frac(B.Rank + E.Now / 420.0); }

	// ---------------------------------------------------------------- making a tile's birds
	void SpawnBirds(FEco& E, FTileE& T, const FPlan& P)
	{
		// how much of a rookery is at home at this hour (the rest are out on the flats)
		E.HomeFrac = RookHome(E);
		for (const FSpot& Sp : P.Spots)
		{
			if (Sp.Kind >= NumBirds) continue;
			const int32 First = T.Birds.Num();
			for (int32 i = 0; i < Sp.N; i++)
			{
				FBird& B = T.Birds.AddDefaulted_GetRef();
				B.Kind = Sp.Kind; B.Rook = Sp.Rook; B.Out = Sp.Out; B.Site = Sp.Site; B.bHasSite = Sp.Rook < 0; B.bPost = Sp.bPost;
				B.Seed = Sp.Seed + i * 7919;
				const double R1 = EHash(B.Seed * 1e-6, 1.0 + i), R2 = EHash(B.Seed * 1e-6, 2.0 + i);
				B.Sz = ScaleOf(B.Kind) * (B.Kind >= Pelican ? .9 + .2 * R1 : .96 + .08 * R1);   // (pelicans differ more, one from the next)
				B.Yaw = 2 * PI * R2;
				// (a bird drying its wings on a limb sits across it, its back or its breast to the water)
				if ((B.Kind == Cormorant || B.Kind == Anhinga) && B.Rook < 0) B.Yaw = FMath::Atan2(Sp.Out.Y, Sp.Out.X) + (R2 < .5 ? PI : 0) + (R1 - .5) * .5;
				B.Ph = 10 * R1;
				B.FlapHz = Specs[B.Kind].FlapHz * (.9 + .2 * R2);
				B.Rank = EHash(B.Seed * 1e-6, 3.0);
				B.Think = 1 + 3 * R1;
				B.Move = 40 + 80 * R2;
				B.Home = Sp.P;
				if (i > 0)
				{
					// a party: the others keep near the first
					B.Leader = First;
					B.Off = FVector2D(FMath::Cos(i * 2.4 + R1 * 6), FMath::Sin(i * 2.4 + R1 * 6)) * (1.4 + .7 * i * R2);
					B.Home += FVector(B.Off.X, 0, B.Off.Y);
				}
				B.Perch = Sp.P;
				if (IsWader(B.Kind) && B.Rook < 0) B.Home.Y = FeetY(E, B, B.Home.X, B.Home.Z);
				B.P = B.Home;
				B.State = RestState(E, B);
				// (more than one brown pelican at a spot is a line of them on the wing, gliding low along the edge)
				B.bLine = B.Kind == Pelican && Sp.N > 1 && B.Rook < 0;
				if (B.bLine) { B.Off = FVector2D(i, 0); B.Home = Sp.P; B.P = Sp.P - FVector(Sp.Out.Y, 0, -Sp.Out.X) * 5.0 * i; }
				if (B.Kind == Tern || B.Kind == Osprey || B.bLine) { B.State = E.Phase == 0 ? BAway : BPatrol; B.Wing = 1; B.To = B.Home; }
				else if (B.Kind == NightHeron && B.Rook < 0 && E.Phase == 0) B.State = BRoost;   // (it leaves its root for the water at its first thought)
				else if (B.Rook >= 0 && B.Kind != NightHeron && RookRank(E, B) > E.HomeFrac) B.State = BAway;
				B.Wing = B.State == BDry ? .8 : B.State == BPatrol ? 1 : 0;
				B.Neck = B.State == BRoost ? -1 : 0;
				B.bStill = B.bWasStill = false;
			}
		}
	}

	// ---------------------------------------------------------------- the rookery keys: out at dawn, back at dusk
	void TickRookeries(FEco& E)
	{
		const TArray<FRookery>& Rk = Rookeries();
		const double Target = E.Phase == 3 ? 1.0 : RookHome(E);
		for (int32 r = 0; r < Rk.Num(); r++)
		{
			if (FVector2D::Distance(Rk[r].C, FVector2D(E.Cam.X, E.Cam.Z)) > 600 + Rk[r].R) continue;
			E.RookCD[r] -= E.Dt;
			if (E.RookCD[r] > 0) continue;
			E.RookCD[r] = FMath::FRandRange(4.0, 8.0);
			// the way they go: out over the open water, the same way every day
			const FVector2D Dir = RookSeaDir(r);
			int32 Home = 0, All = 0;
			TArray<FBird*> Go, Come;
			for (TPair<int64, FTileE*>& T : E.Tiles)
				for (FBird& B : T.Value->Birds)
				{
					if (B.Rook != r || B.Kind == NightHeron) continue;
					All++;
					const bool bHome = B.State != BAway && !(B.State == BFly && B.Next == BAway);
					Home += bHome ? 1 : 0;
					const double Rank = RookRank(E, B);
					if (bHome && Rank > Target && B.State != BFly && B.Delay <= 0 && Go.Num() < 7) Go.Add(&B);
					if (B.State == BAway && Rank <= Target && B.Delay <= 0 && B.Cool <= 0 && Come.Num() < 7) Come.Add(&B);
				}
			if (Go.Num())
			{
				const int32 N = FMath::Min(Go.Num(), FMath::RandRange(4, 7));
				for (int32 i = 0; i < N; i++)
				{
					FBird& B = *Go[i];
					const FVector Far = B.Perch + FVector(Dir.X * 560 + (i - N * .5) * 6, 22 + i, Dir.Y * 560 + (i - N * .5) * 4);
					StartFly(B, Far, BAway, 4);
					B.FlyT = -i * .45;   // (each a moment after the one before)
				}
				E.bStillSoon = true;
				E.Sound(TEXT("flight"), Go[0]->Kind, Go[0]->P, FMath::Min(1.f, N / 6.f));
				UE_LOG(LogTemp, Log, TEXT("KREco rookery %d (%s) at %.0f, %.0f, %s: a flight of %d leaves for the flats (%d of its %d day birds were at home)"), r, RegionName(Rk[r].Region), Rk[r].C.X, Rk[r].C.Y, PhaseWord(E), N, Home, All);
			}
			if (Come.Num() && (!Go.Num() || E.Phase == 2))
			{
				const int32 N = FMath::Min(Come.Num(), FMath::RandRange(4, 7));
				for (int32 i = 0; i < N; i++)
				{
					FBird& B = *Come[i];
					B.P = B.Perch + FVector(Dir.X * 540 + (i - N * .5) * 6, 24 + i, Dir.Y * 540 + (i - N * .5) * 4);
					B.State = BPatrol;   // (in the air already: StartFly takes its place as it is)
					StartFly(B, B.Perch, RestState(E, B), 3);
					B.FlyT = -i * .45;
				}
				E.bStillSoon = true;
				UE_LOG(LogTemp, Log, TEXT("KREco rookery %d (%s) at %.0f, %.0f, %s: a flight of %d comes in to roost (%d of its %d day birds were at home)"), r, RegionName(Rk[r].Region), Rk[r].C.X, Rk[r].C.Y, PhaseWord(E), N, Home, All);
			}
		}
	}

	// ---------------------------------------------------------------- every bird's turn
	void UpdateBirds(FEco& E, FTileE& T)
	{
		const double Dt = E.Dt;
		const FVector2D Cam2(E.Cam.X, E.Cam.Z);
		for (int32 bi = 0; bi < T.Birds.Num(); bi++)
		{
			FBird& B = T.Birds[bi];
			const FBirdSpec& K = Specs[B.Kind];
			// (a bird that changes what it is doing wants the still mesh laid out again: at once if it is near enough
			// to be watched, within a second if it is far off - a rookery's comings and goings would lay it out every frame)
			struct FDirty
			{
				FEco& Ec; const FBird& Bd; bool bWas;
				FDirty(FEco& InE, const FBird& InB) : Ec(InE), Bd(InB), bWas(InE.bStillDirty) { Ec.bStillDirty = false; }
				~FDirty()
				{
					const bool bMine = Ec.bStillDirty;
					Ec.bStillDirty = bWas;
					if (bMine) { if (FVector::DistSquared(Bd.P, Ec.Cam) < 90.0 * 90.0) Ec.bStillDirty = true; else Ec.bStillSoon = true; }
				}
			} DirtyScope(E, B);
			FVector2D P2(B.P.X, B.P.Z);
			if (FVector2D::DistSquared(P2, Cam2) > 600.0 * 600.0)
			{
				// too far to matter: it holds as it is (a flight that went out of sight has gone where it was going)
				if (B.State == BFly && B.Next == BAway) B.State = BAway;
				continue;
			}
			B.Ph += Dt;
			B.FlushCD -= Dt; B.Cool -= Dt; B.Think -= Dt; B.Move -= Dt;
			if (B.Delay > 0)
			{
				B.Delay -= Dt;
				if (B.Delay <= 0 && B.Next == BAway && B.State == BPerch) { B.State = BFly; B.Wing = 1; E.bStillDirty = true; }
				continue;
			}
			const bool bRest = B.State == BStand || B.State == BWalk || B.State == BPerch || B.State == BDry || B.State == BRoost || B.State == BStrike;
			// ---- the boat: close and fast puts them up; idling past does not
			if (bRest && K.FlushR > 0 && B.FlushCD <= 0)
			{
				const double D = FVector2D::Distance(P2, E.Boat), R = K.FlushR * (B.Rook >= 0 ? .5 : 1);
				if ((E.BoatSpd > 3.5 && D < R) || (E.BoatSpd > 1.2 && D < 6))
				{
					const FVector2D Away = (P2 - E.Boat).GetSafeNormal();
					const FVector2D Side(-Away.Y, Away.X);
					const double Sw = FMath::FRandRange(-.5, .5);
					StartFly(B, FVector(B.P.X + (Away.X + Side.X * Sw) * 120, 14 + 8 * FMath::FRand(), B.P.Z + (Away.Y + Side.Y * Sw) * 120), BAway, 3, K.Speed * 1.15);
					B.Cool = FMath::FRandRange(14.0, 24.0);
					B.FlushCD = 6;
					if (B.RoostIx >= 0 && T.RoostUsed.IsValidIndex(B.RoostIx)) { T.RoostUsed[B.RoostIx] = false; B.RoostIx = -1; }
					E.FlushN[B.Kind]++;
					E.Sound(TEXT("flush"), B.Kind, B.P, float(K.Body * 2));
					if (E.FlushT <= 0) { E.FlushT = E.Now; E.FlushD = D; E.FlushSpd = E.BoatSpd; }
					E.FlushD = FMath::Min(E.FlushD, D);
				}
			}
			switch (B.State)
			{
			case BFly:
			{
				B.FlyT += Dt;
				if (B.FlyT < 0) { B.Wing = 0; break; }
				B.Wing = 1;
				const double U = FMath::Clamp(B.FlyT / B.FlyDur, 0.0, 1.0), Us = U * U * (3 - 2 * U);
				const bool bLandFeet = B.Next == BStand || B.Next == BPerch || B.Next == BDry || B.Next == BRoost;
				const FVector To = bLandFeet ? B.To + FVector(0, K.Leg * B.Sz * (1 - FMath::Clamp((U - .9) * 10, 0.0, 1.0)), 0) : B.To;
				FVector Np = FMath::Lerp(B.From, To, Us) + FVector(0, B.Arc * FMath::Sin(PI * U), 0);
				// (a pelican up off a flat or the water clears it at once: it does not plough along through the water)
				if (B.Kind >= Pelican && B.Next != BDive && B.Next != BSwim)
					Np.Y = FMath::Max(Np.Y, E.Sea + .55 * B.Sz * FMath::Min(1.0, U * 10) * (bLandFeet ? FMath::Clamp((1 - U) * 10, 0.0, 1.0) : 1.0));
				const FVector V = Np - B.P;
				if (V.SizeSquared2D() > 1e-8)
				{
					B.Yaw += WrapPi(FMath::Atan2(V.Z, V.X) - B.Yaw) * FMath::Min(1.0, Dt * 8);
					B.Pitch = FMath::Clamp(FMath::Atan2(V.Y, FMath::Max(.001, Hyp(V.X, V.Z))), -1.3, .9);
				}
				B.P = Np;
				if (U >= 1)
				{
					if (B.Next == BDive)
					{
						// a plunge: the splash, the small fish scatter, and up again
						if (E.Life) E.Life->Splash(B.P.X, B.P.Z, B.Kind == Pelican || B.Kind == Osprey ? 16 : 6, B.Kind == Pelican ? 1.0 : B.Kind == Osprey ? .8 : .35);
						if (FSchoolE* Sc = NearestSchool(E, FVector2D(B.P.X, B.P.Z), 5)) { Sc->Scatter = FMath::Max(Sc->Scatter, .7); Sc->ThreatP = FVector2D(B.P.X, B.P.Z); }
						B.bCarry = B.Kind == Osprey || B.Kind == Kingfisher;
						E.Sound(TEXT("plunge"), B.Kind, B.P, B.Kind == Pelican ? 1.f : B.Kind == Osprey ? .8f : .35f);
						if (B.Kind == Osprey && LinkDue(E, LinkOsprey, 1))
							UE_LOG(LogTemp, Log, TEXT("KREco osprey: fish near the surface at %.0f, %.0f: it hovered, dived feet first and carries one off to %.0f, %.0f"), B.P.X, B.P.Z, B.Perch.X, B.Perch.Z);
						if (B.Kind == Pelican || B.Kind == Cormorant) { B.State = BSwim; B.T = B.Kind == Pelican ? 4 : 9; B.Wing = 0; B.Pitch = 0; }
						else if (B.Kind == Tern) { B.State = BPatrol; B.To = B.Home; B.Cool = 6; }
						else StartFly(B, B.Perch, BPerch, 2.5);
					}
					else if (B.Next == BHover) { B.State = BHover; B.T = 2.6; B.Pitch = .5; }
					else if (B.Next == BAway) B.State = BAway;
					else Land(E, B);
					E.bStillDirty = true;
				}
				break;
			}
			case BAway:
				// flushed birds come back when the boat has gone; birds asleep elsewhere wait for their hour
				if (B.Rook < 0 && B.Cool <= 0 && B.Kind != Tern && B.Kind != Osprey && !B.bLine)
				{
					const FVector Hm = IsWader(B.Kind) ? B.Home : B.Perch;
					if (FVector2D::Distance(FVector2D(Hm.X, Hm.Z), E.Boat) > K.FlushR * 1.8 || E.BoatSpd < 2)
					{
						const FVector2D In = (FVector2D(Hm.X, Hm.Z) - E.Boat).GetSafeNormal();
						B.P = Hm + FVector(In.X * 110, 16, In.Y * 110);
						B.State = BPatrol;
						StartFly(B, Hm, RestState(E, B), 2);
					}
					else B.Cool = 5;
				}
				else if ((B.Kind == Tern || B.Kind == Osprey || B.bLine) && E.Phase != 0 && B.Cool <= 0) { B.State = BPatrol; B.P = B.Home + FVector(0, 6, 0); B.To = B.Home; B.Wing = 1; }
				break;
			case BSwim:
				B.T -= Dt;
				// a cormorant goes under for a while in the middle of its swim
				B.P.Y = E.Sea - (B.Kind == Cormorant && B.T > 2.5 && B.T < 6.5 ? .8 : 0);
				B.Neck = 0;
				if (B.T <= 0)
				{
					if (B.bLine) { B.State = BPatrol; B.Wing = 1; B.To = B.Home; B.P.Y = E.Sea + .4; E.bStillDirty = true; }   // (up off the water and after the others)
					else StartFly(B, B.Perch, RestState(E, B), 2);
				}
				break;
			case BHover:
				B.T -= Dt;
				B.Pitch = .55;
				if (B.T <= 0)
				{
					const FVector2D Aim(B.To.X, B.To.Z);
					StartFly(B, FVector(Aim.X, E.Sea + .1, Aim.Y), BDive, 0, 16);
				}
				break;
			case BPatrol:
			{
				// on the wing all day: a tern working along the edge over the small fish, an osprey wheeling high
				if (E.Phase == 0) { B.State = BAway; break; }
				FVector Want = B.To;
				double Speed = K.Speed * .8;
				// one of a line of pelicans keeps its place astern of the first, five metres apart, as long as that one is flying
				const FBird* Ld = B.bLine && B.Leader >= 0 && T.Birds.IsValidIndex(B.Leader) && T.Birds[B.Leader].State == BPatrol ? &T.Birds[B.Leader] : nullptr;
				if (Ld)
				{
					const FVector Slot = Ld->P - FVector(FMath::Cos(Ld->Yaw), 0, FMath::Sin(Ld->Yaw)) * 5.0 * B.Off.X;
					const FVector2D To2(Slot.X - B.P.X, Slot.Z - B.P.Z);
					const double L2 = To2.Size();
					if (L2 > .5) B.Yaw += FMath::Clamp(WrapPi(FMath::Atan2(To2.Y, To2.X) - B.Yaw), -2.2 * Dt, 2.2 * Dt); else B.Yaw += WrapPi(Ld->Yaw - B.Yaw) * FMath::Min(1.0, Dt * 2);
					Speed *= FMath::Clamp(.7 + L2 * .12, .7, 1.5);
					const double Vy = FMath::Clamp(Slot.Y - B.P.Y, -2.0, 2.0);
					B.P += FVector(FMath::Cos(B.Yaw) * Speed, Vy, FMath::Sin(B.Yaw) * Speed) * Dt;
					B.Pitch = FMath::Clamp(Vy * .08, -.3, .3);
					B.Wing = 1;
					// (and goes down on a shower of bait with the rest)
					for (FShower& Sh : E.Showers)
						if (Sh.Id > B.Shower && E.Now - Sh.T < 2 && FVector2D::Distance(Sh.P, P2) < 200)
						{
							B.Shower = Sh.Id;
							if (!Sh.bTold) Sh.Pelicans++;
							StartFly(B, FVector(Sh.P.X + FMath::FRandRange(-2.0, 2.0), E.Sea + .05, Sh.P.Y + FMath::FRandRange(-2.0, 2.0)), BDive, 5, 13);
							break;
						}
					break;
				}
				if (B.Kind == Osprey)
				{
					const double Ang = B.Ph * .18 + B.Rank * 6;
					Want = B.Home + FVector(FMath::Cos(Ang) * 38, 0, FMath::Sin(Ang) * 38);
					if (B.Think <= 0)
					{
						B.Think = 3;
						// a fish worth stooping on: mullet first, else a school of small fish up near the surface
						FVector2D Fish;
						bool bFish = false;
						if (FGroup* G = NearestGroup(E, FVector2D(B.Home.X, B.Home.Z), 110, Mullet)) { Fish = G->C; bFish = true; }
						else if (FSchoolE* Sc = NearestSchool(E, FVector2D(B.Home.X, B.Home.Z), 90)) { Fish = Sc->C; bFish = Sc->bAwake; }
						if (bFish && B.Cool <= 0)
						{
							B.Cool = FMath::FRandRange(45.0, 80.0);
							B.To = FVector(Fish.X, 0, Fish.Y);
							E.Sound(TEXT("whistle"), B.Kind, B.P, .8f);
							// the nearest crown to carry it to
							double BestD = 1e18;
							for (const FVector& R : T.Roosts) { const double D = FVector::DistSquared(R, B.P); if (D < BestD) { BestD = D; B.Perch = R; } }
							const FVector Over(Fish.X, E.Sea + 15, Fish.Y);
							StartFly(B, Over, BHover, 0);
							B.To = Over;
							break;
						}
					}
				}
				else
				{
					// a shower of bait within reach comes first
					for (FShower& Sh : E.Showers)
						if (Sh.Id > B.Shower && E.Now - Sh.T < 2 && FVector2D::Distance(Sh.P, P2) < 230)
						{
							B.Shower = Sh.Id;
							if (!Sh.bTold) (B.Kind == Pelican ? Sh.Pelicans : Sh.Terns)++;
							if (B.Kind == Tern) E.Sound(TEXT("kree"), B.Kind, B.P, .7f);
							StartFly(B, FVector(Sh.P.X + FMath::FRandRange(-1.5, 1.5), E.Sea + .05, Sh.P.Y + FMath::FRandRange(-1.5, 1.5)), BDive, B.Kind == Pelican ? 5 : 2.5, 14);
							break;
						}
					if (B.State != BPatrol) break;
					if (FVector2D::Distance(FVector2D(Want.X, Want.Z), P2) < 5 || B.Think <= 0)
					{
						B.Think = 9;
						// over a school it has reached, it may drop on it
						FSchoolE* Under = NearestSchool(E, P2, 6);
						if (Under && Under->bAwake && Under->Scatter < .2 && B.Cool <= 0 && FMath::FRand() < (B.Kind == Pelican ? .15 : .4))
						{
							B.Cool = B.Kind == Pelican ? 30 : 8;
							StartFly(B, FVector(Under->C.X, E.Sea + .05, Under->C.Y), BDive, B.Kind == Pelican ? 6 : 0, 13);   // (a pelican climbs first and plunges)
							break;
						}
						// on to the next: a school of small fish somewhere along this shore, else a turn along the edge
						const FVector2D Tn(-B.Out.Y, B.Out.X);
						FVector2D Next = FVector2D(B.Home.X, B.Home.Z) + Tn * FMath::FRandRange(-70.0, 70.0) + B.Out * FMath::FRandRange(-2.0, 22.0);
						if (FSchoolE* Sc = NearestSchool(E, Next, 60)) { Next = Sc->C; B.Prey = Sc->Id; } else B.Prey = -1;
						B.To = FVector(Next.X, B.Kind == Pelican ? 1.3 + 1.5 * FMath::FRand() : 6 + 3 * FMath::FRand(), Next.Y);   // (pelicans glide just over the water)
						Want = B.To;
					}
				}
				const FVector2D To2(Want.X - B.P.X, Want.Z - B.P.Z);
				B.Yaw += FMath::Clamp(WrapPi(FMath::Atan2(To2.Y, To2.X) - B.Yaw), -1.6 * Dt, 1.6 * Dt);
				const double WantY = B.Kind == Osprey ? E.Sea + 27 + 3 * FMath::Sin(B.Ph * .3) : E.Sea + Want.Y + (B.Kind == Pelican ? .2 : .6) * FMath::Sin(B.Ph * (B.Kind == Pelican ? .5 : 1.7));
				const double Vy = FMath::Clamp(WantY - B.P.Y, -3.0, 3.0);
				B.P += FVector(FMath::Cos(B.Yaw) * Speed, Vy, FMath::Sin(B.Yaw) * Speed) * Dt;
				B.Pitch = FMath::Clamp(Vy * .08, -.4, .4);
				B.Wing = 1;
				break;
			}
			case BStrike:
				B.T -= Dt;
				B.Neck = FMath::Clamp(B.T > .45 ? (.9 - B.T) / .2 : B.T / .45, 0.0, 1.0);
				if (B.T <= 0) { B.State = B.Next; B.Neck = 0; E.bStillDirty = true; }
				break;
			case BWalk:
			{
				// (a party's followers are led from their own thoughts below; everyone walks the same way)
				const FVector2D To2(B.To.X - B.P.X, B.To.Z - B.P.Z);
				const double L = To2.Size(), Sp = (B.Kind == Ibis || B.Kind == Spoonbill ? .6 : .42) * B.Sz;
				if (IsWader(B.Kind) && (B.Kind == Ibis || B.Kind == Spoonbill)) B.Peck = FMath::Max(0.0, FMath::Sin(B.Ph * (B.Kind == Ibis ? 4.2 : 2.6) + B.Rank * 6)) * .9;
				if (L < .25) { B.State = BStand; B.Peck = 0; E.bStillDirty = true; break; }
				B.Yaw += WrapPi(FMath::Atan2(To2.Y, To2.X) - B.Yaw) * FMath::Min(1.0, Dt * 3);
				const FVector2D Np = P2 + To2 / L * FMath::Min(L, Sp * Dt);
				if (!Wadeable(E, Np.X, Np.Y, K.Wade + .12)) { B.State = BStand; B.Peck = 0; break; }
				B.P = FVector(Np.X, FeetY(E, B, Np.X, Np.Y), Np.Y);
				break;
			}
			default:
				break;
			}
			// ---- thoughts, for a bird that is standing or sitting
			const bool bThinks = B.State == BStand || B.State == BPerch || B.State == BDry || B.State == BRoost;
			if (bThinks && B.Think <= 0)
			{
				B.Think = FMath::FRandRange(1.5, 4.0);
				P2 = FVector2D(B.P.X, B.P.Z);
				const uint8 Rest = RestState(E, B);
				const bool bWadesNow = (IsWader(B.Kind) && B.Rook < 0) || (B.Kind == NightHeron && B.Rook < 0 && E.Phase == 0);
				// --- night and day: to the roost, and back
				if (Rest == BRoost && B.State != BRoost)
				{
					FVector Rp = B.Perch;
					bool bHave = !bWadesNow || B.Kind == NightHeron;
					if (!bHave)
					{
						double BestD = FMath::Square(160.0);
						int32 Best = -1;
						for (int32 i = 0; i < T.Roosts.Num(); i++)
						{
							const double D = FVector::DistSquared(T.Roosts[i], B.P);
							if (!T.RoostUsed[i] && D < BestD) { BestD = D; Best = i; }
						}
						if (Best >= 0) { T.RoostUsed[Best] = true; B.RoostIx = Best; Rp = T.Roosts[Best]; bHave = true; }
					}
					if (bHave && FVector::Distance(Rp, B.P) > .5) StartFly(B, Rp, BRoost, 2.5);
					else { B.State = BRoost; B.Neck = -1; B.Wing = 0; }
					E.bStillDirty = true;
					continue;
				}
				if (Rest != BRoost && B.State == BRoost)
				{
					if (B.RoostIx >= 0 && T.RoostUsed.IsValidIndex(B.RoostIx)) T.RoostUsed[B.RoostIx] = false;
					B.RoostIx = -1;
					B.Neck = 0;
					if (bWadesNow && B.Kind == NightHeron)
					{
						// down off its root to the nearest shallows, to hunt them till first light
						double BestD = FMath::Square(60.0);
						FVector2D Q = FVector2D::ZeroVector;
						for (const FVector2D& W : T.Wade) { const double D = FVector2D::DistSquared(W, P2); if (D < BestD && Wadeable(E, W.X, W.Y, .75)) { BestD = D; Q = W; } }
						if (!Q.IsZero()) StartFly(B, FVector(Q.X, FeetY(E, B, Q.X, Q.Y), Q.Y), BStand, 2);
						else { B.State = BPerch; B.Wing = 0; }
					}
					else if (bWadesNow) StartFly(B, B.Home, BStand, 2.5);
					else B.State = Rest == BDry ? BDry : Rest, B.Wing = Rest == BDry ? .8 : 0;
					E.bStillDirty = true;
					continue;
				}
				if (B.State == BRoost) continue;
				// --- a pelican, a cormorant or an anhinga near a shower of bait goes to it
				if ((B.Kind == Pelican || B.Kind == Cormorant) && E.Phase != 0)
					for (FShower& Sh : E.Showers)
						if (Sh.Id > B.Shower && E.Now - Sh.T < 3 && FVector2D::Distance(Sh.P, P2) < (B.Kind == Pelican ? 260 : 70))
						{
							B.Shower = Sh.Id;
							if (!Sh.bTold) (B.Kind == Pelican ? Sh.Pelicans : Sh.Others)++;
							StartFly(B, FVector(Sh.P.X + FMath::FRandRange(-2.0, 2.0), E.Sea + .05, Sh.P.Y + FMath::FRandRange(-2.0, 2.0)), BDive, B.Kind == Pelican ? 6 : 2);
							E.bStillDirty = true;
							break;
						}
				if (B.State == BFly) continue;
				// --- the waders: to where the small fish are thick and the water is shallow enough to stand in
				if (bWadesNow)
				{
					const double Wade = B.Kind == NightHeron ? .75 : K.Wade;
					if (B.Leader >= 0 && T.Birds.IsValidIndex(B.Leader))
					{
						// one of a party: keep by the leader, flying when it flies
						const FBird& Ld = T.Birds[B.Leader];
						const FVector Ldp = Ld.State == BFly ? Ld.To : Ld.P;
						const FVector Want(Ldp.X + B.Off.X, 0, Ldp.Z + B.Off.Y);
						const double D = FVector2D::Distance(FVector2D(Want.X, Want.Z), P2);
						if (Ld.State == BAway) continue;
						// (white pelicans fish together: when the first dips its bill the rest do, a moment after)
						if (B.Kind == WhitePelican && !BrownAmongWhites(B) && Ld.State == BStrike && D < 14) { B.State = BStrike; B.Next = BStand; B.T = .9 + .3 * B.Rank; E.bStillDirty = true; continue; }
						if (B.Kind == WhitePelican && D <= 14 && D < 2.5 && FMath::FRand() > .12) continue;
						if (D > 14) { StartFly(B, FVector(Want.X, FeetY(E, B, Want.X, Want.Z), Want.Z), BStand, 2.2); B.FlyT = -FMath::FRandRange(.2, 1.2); E.bStillDirty = true; }
						else if (D > 1.2 || FMath::FRand() < .5) { B.To = Want + FVector(FMath::FRandRange(-1.0, 1.0), 0, FMath::FRandRange(-1.0, 1.0)); B.State = BWalk; E.bStillDirty = true; }
						continue;
					}
					double Ds = 0;
					FSchoolE* Sc = NearestSchool(E, P2, 48, &Ds);
					if (Sc && Sc->N >= 12)
					{
						B.Prey = Sc->Id;
						if (Ds < 4.6 && E.Sea - Sc->Floor > .06 && E.Sea - FloorAt(B.P.X, B.P.Z) <= Wade + .12)
						{
							// in reach: it strikes the more often the thicker they are
							if (Sc->Scatter < .25 && FMath::FRand() < FMath::Clamp(Sc->N / 60.0, .25, 1.0) * .5)
							{
								B.Yaw = FMath::Atan2(Sc->C.Y - B.P.Z, Sc->C.X - B.P.X);
								B.State = BStrike; B.Next = BStand; B.T = .9;
								B.Strikes++;
								E.StrikeId++; E.StrikeAt = B.P + FVector(0, .5, 0); E.StrikeDir = FVector2D(-FMath::Sin(B.Yaw), FMath::Cos(B.Yaw));
								E.Sound(TEXT("strike"), B.Kind, B.P, .3f);
								if (FMath::FRand() < .2) E.Sound(B.Kind == Ibis ? TEXT("honk") : TEXT("croak"), B.Kind, B.P, .6f);
								Sc->Scatter = FMath::Max(Sc->Scatter, .55);
								Sc->ThreatP = P2 + (Sc->C - P2).GetSafeNormal() * .8;
								if (E.Life && FVector2D::Distance(P2, Cam2) < 120) E.Life->Splash(Sc->ThreatP.X, Sc->ThreatP.Y, 3, .25);
								E.bStillDirty = true;
								if (LinkDue(E, LinkWaders, 4))
									UE_LOG(LogTemp, Log, TEXT("KREco link waders: %d %s thick in %.2f m of water at %.0f, %.0f: a %s standing %.1f m off strikes (its strike %d here; the school scatters)"),
										Sc->N, KindName(Sc->Kind), E.Sea - Sc->Floor, Sc->C.X, Sc->C.Y, KindName(B.Kind), Ds, B.Strikes);
							}
							continue;
						}
						// not in reach: the nearest place to it shallow enough to stand, walking if it is near, flying if not
						const FVector2D Dir = (P2 - Sc->C).GetSafeNormal();
						for (const FVector2D& Try : {Dir, Sc->Out, FVector2D(-Dir.Y, Dir.X), FVector2D(Dir.Y, -Dir.X)})
							for (double D = 2.4; D <= 9; D += 1.6)
							{
								const FVector2D Q = Sc->C + Try * D;
								if (!Wadeable(E, Q.X, Q.Y, Wade)) continue;
								const double Dq = FVector2D::Distance(Q, P2);
								if (Dq < 1) break;
								if (Dq > 20) { StartFly(B, FVector(Q.X, FeetY(E, B, Q.X, Q.Y), Q.Y), BStand, 2.5); }
								else { B.To = FVector(Q.X, 0, Q.Y); B.State = BWalk; }
								E.bStillDirty = true;
								goto Thought;
							}
					}
					// nothing to stalk: now and then it lifts off and lands further along; else a few steps
					if (B.Move <= 0 && T.Wade.Num())
					{
						B.Move = FMath::FRandRange(50.0, 110.0);
						for (int32 Try = 0; Try < 6; Try++)
						{
							const FVector2D Q = T.Wade[FMath::RandRange(0, T.Wade.Num() - 1)];
							const double Dq = FVector2D::Distance(Q, P2);
							if (Dq < 22 || Dq > 95 || !Wadeable(E, Q.X, Q.Y, Wade) || FVector2D::Distance(Q, E.Boat) < 25) continue;
							StartFly(B, FVector(Q.X, FeetY(E, B, Q.X, Q.Y), Q.Y), BStand, 3);
							E.bStillDirty = true;
							goto Thought;
						}
					}
					if (FMath::FRand() < (B.Kind == Ibis || B.Kind == Spoonbill ? .8 : .35) || !Wadeable(E, B.P.X, B.P.Z, Wade + .1))
					{
						const double A = FMath::FRand() * 2 * PI, R = FMath::FRandRange(1.5, 4.5);
						const FVector2D Q = P2 + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R;
						if (Wadeable(E, Q.X, Q.Y, Wade)) { B.To = FVector(Q.X, 0, Q.Y); B.State = BWalk; E.bStillDirty = true; }
						else if (!Wadeable(E, B.P.X, B.P.Z, Wade + .1) && FVector2D::Distance(P2, FVector2D(B.Home.X, B.Home.Z)) > 2 && Wadeable(E, B.Home.X, B.Home.Z, Wade + .1))
						{ StartFly(B, B.Home, BStand, 2); E.bStillDirty = true; }
					}
					continue;
				}
				// --- by day a night heron sits on its root; whoever sat down somewhere else goes home
				if (B.Kind == NightHeron && B.Rook < 0 && E.Phase != 0 && FVector::Distance(B.P, B.Perch) > .5) { StartFly(B, B.Perch, BRoost, 2); E.bStillDirty = true; continue; }
				// --- on a root or a limb over the water
				if (B.Rook < 0 && B.Cool <= 0 && E.Phase != 0 && (B.Kind == Kingfisher || B.Kind == GreenHeron || B.Kind == Cormorant || B.Kind == Anhinga))
				{
					double Ds = 0;
					FSchoolE* Sc = NearestSchool(E, P2, B.Kind == GreenHeron ? 3.2 : B.Kind == Kingfisher ? 16 : 40, &Ds);
					if (Sc && Sc->bAwake && Sc->Scatter < .2)
					{
						if (B.Kind == GreenHeron)
						{
							B.Yaw = FMath::Atan2(Sc->C.Y - B.P.Z, Sc->C.X - B.P.X);
							B.State = BStrike; B.Next = BPerch; B.T = .9; B.Cool = FMath::FRandRange(6.0, 14.0);
							Sc->Scatter = FMath::Max(Sc->Scatter, .4); Sc->ThreatP = P2;
						}
						else if (B.Kind == Kingfisher) { E.Sound(TEXT("rattle"), B.Kind, B.P, .7f); B.Cool = FMath::FRandRange(20.0, 45.0); B.bCarry = false; StartFly(B, FVector(Sc->C.X, E.Sea + .03, Sc->C.Y), BDive, 1.2, 12); }
						else { B.Cool = FMath::FRandRange(90.0, 160.0); StartFly(B, FVector(Sc->C.X + Sc->Out.X * 3, E.Sea + .03, Sc->C.Y + Sc->Out.Y * 3), BDive, 1.5); }
						E.bStillDirty = true;
						continue;
					}
					if (B.Kind == Kingfisher) B.bCarry = false;
				}
				// a cormorant shuts its wings once it is dry and opens them again after the next swim
				if ((B.Kind == Cormorant || B.Kind == Anhinga) && B.State == BDry && B.Cool < 40 && B.Cool > 0 && B.Rank < .5) { B.State = BPerch; B.Wing = 0; E.bStillDirty = true; }
			}
			Thought:
			const bool bStill = (B.State == BStand || B.State == BPerch || B.State == BRoost || B.State == BDry) && B.Peck <= 0;
			if (bStill != B.bWasStill) E.bStillDirty = true;
			B.bStill = B.bWasStill = bStill;
		}
	}

	void EmitBirds(FEco& E, FTileE& T, bool bStillPass)
	{
		const FVector2D Cam2(E.Cam.X, E.Cam.Z);
		FSoup& Buf = bStillPass ? E.Still : E.Live;
		for (FBird& B : T.Birds)
		{
			// (its snag stands whether the bird is on it or not)
			if (bStillPass && !Before() && (B.Kind == Cormorant || B.Kind == Anhinga) && B.Rook < 0)
			{
				const double Ds = FVector2D::Distance(FVector2D(B.Perch.X, B.Perch.Z), Cam2);
				if (Ds < 320) EmitSnag(Buf, E, B, FMath::Clamp(Ds * .0016, .015, .22));
			}
			if (bStillPass && B.bPost)
			{
				const double Ds = FVector2D::Distance(FVector2D(B.Perch.X, B.Perch.Z), Cam2);
				if (Ds < 500) EmitPiling(Buf, E, B, FMath::Clamp(Ds * .0016, .015, .22));
			}
			if (bStillPass) B.bInStill = false;
			if (B.State == BAway || (B.State == BFly && B.FlyT < 0 && B.Next != BAway && B.From.Y > 15)) continue;
			// the still mesh holds whoever stood still when it was last laid out; the moving mesh holds everyone else,
			// and with them a bird that has come to rest since (till the still mesh is laid out again)
			if (bStillPass ? !B.bStill : (B.bStill && B.bInStill)) continue;
			const double D2 = FVector2D::DistSquared(FVector2D(B.P.X, B.P.Z), Cam2);
			if (D2 > FMath::Square(Specs[B.Kind].Draw)) continue;
			if (B.State == BSwim && B.P.Y < E.Sea - .4) continue;
			// (past 70 m a bird is drawn in half the pieces. Its dark edge is two pixels or so wide through the closer
			// look's lens at whatever distance it stands, less on a rookery's green crowns; a bird on the wing has none:
			// it is seen against the sky or the trees, and a rookery's flights are many.)
			const double D = FMath::Sqrt(D2);
			const bool bAir = B.State == BFly || B.State == BHover || B.State == BDive || B.State == BPatrol;
			const double Rim = Before() || bAir ? 0 : FMath::Clamp(D * (B.Rook >= 0 ? .0007 : .0013), .015, .2);
			Emit(Buf, B, D2 > 70.0 * 70.0, Rim, Before() ? 0 : FMath::Clamp(D * .00035, .004, .06), (B.P - E.Cam).GetSafeNormal());
			if (bStillPass) { B.bInStill = true; E.StillN++; } else E.BirdsMoved++;
		}
	}
}
