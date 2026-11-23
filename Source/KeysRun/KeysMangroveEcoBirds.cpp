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
			{.50, .58, .52, .14, .050, 1.80, 9,   2.0, .55, .60, 36, 700, 0x5a6a80, 0x6f7a8c, 0x8a8494, 0xe8e8e8, 0xd8a838, 0x5a5648, 0x5f6f84, 0x2c3440, 0, false},   // great blue heron
			{.30, .32, .30, .08, .035, 1.00, 9,   3.0, .50, .30, 24, 450, 0x3c4a66, 0x3c4a66, 0x5a4660, 0x5a4660, 0x4a5a70, 0x4a5240, 0x3c4a66, 0x2c3850, 0, false},   // little blue heron
			{.30, .30, .24, .16, .035, .95,  10,  3.6, .35, .22, 22, 500, 0xf4f4ee, 0xf4f4ee, 0xf4f4ee, 0xf2d8d0, 0xe04a30, 0xdc4a32, 0xf4f4ee, 0x181818, 1, true},    // white ibis
			{.40, .40, .32, .17, .040, 1.25, 9.5, 3.0, .40, .32, 26, 600, 0xf2a0b4, 0xf4b4c4, 0xf6ecec, 0xe4e6d4, 0x9aa088, 0xc05060, 0xf08aa6, 0xe0607a, 2, true},    // roseate spoonbill
			{.42, .07, .28, .07, .040, 1.20, 12,  4.0, 1.0, 0,   20, 400, 0x1c1c1e, 0x242426, 0x1c1c1e, 0x1c1c1e, 0xd09030, 0x141414, 0x26262a, 0x141414, 0, true},    // cormorant
			{.42, .06, .38, .09, .030, 1.15, 11,  3.5, .90, 0,   20, 400, 0x1a1a1c, 0x1a1a1c, 0x4a3a2a, 0x4a3a2a, 0xd8b040, 0x2a2a20, 0x2c2c30, 0xb8b8b0, 0, true},    // anhinga
			{.16, .025, .03, .06, .046, .50, 11,  7.0, .90, 0,   14, 200, 0x35639a, 0xf0f0ee, 0xf0f0ee, 0x2c5488, 0x181c20, 0x3a3a3a, 0x2c5488, 0x1c2c44, 0, true},    // belted kingfisher
			{.22, .14, .14, .07, .035, .65,  9,   4.0, .25, .12, 12, 250, 0x3a5a4a, 0x7a4a3a, 0x7a3a2a, 0x24382e, 0x2a2a2a, 0xd8b838, 0x3a5a4a, 0x24382e, 0, false},   // green heron
			{.30, .22, .14, .08, .050, 1.10, 9,   2.8, .60, .20, 16, 350, 0x27303c, 0xc4cad2, 0xc4cad2, 0x0e1216, 0x1a1a1c, 0xe0c440, 0x66707e, 0x30363e, 0, false},   // night heron
			{.20, .03, .03, .06, .035, .80,  10,  3.5, .15, 0,   0,  400, 0xf4f4f2, 0xf4f4f2, 0xf4f4f2, 0x1a1a1c, 0xf08020, 0x202020, 0xc8ccd0, 0x8a9098, 0, true},    // tern
			{.34, .10, .06, .035, .050, 1.60, 11, 2.4, .90, 0,   0,  700, 0x4a3a2c, 0xf0ece4, 0xf0ece4, 0xf0ece4, 0x2a2a2a, 0xc8c8b8, 0x4a3a2c, 0x2a2018, 0, true},    // osprey
			{.55, .10, .38, .30, .060, 2.10, 10,  1.8, .70, 0,   22, 700, 0x9a9288, 0x6b645c, 0xf2efe4, 0xf6e9a8, 0xd9b45e, 0x2a2a2c, 0x6b645c, 0x3a3632, 3, false},   // brown pelican
		};
		bool IsWader(int32 K) { return K <= Spoonbill; }
		// How large each is drawn, as a scale on its model. At 1 the models already stand taller than the birds do in
		// life (their necks are drawn upright), so at the 1.5 they all had a great blue heron stood 1.89 m above its
		// feet: taller than a person on deck. It is brought down to 1.67 m, the same as the great egret; the two
		// smallest on the roots, which were specks, are drawn larger (a kingfisher .57 m, a green heron .62 m); the rest
		// stay as they were. What makes them all read is the closer look's lens.
		const double Scales[NumBirds] = {1.5, 1.35, 1.5, 1.5, 1.5, 1.5, 1.5, 2.5, 1.8, 1.5, 1.5, 1.5, 1.5};
		// -KRBirdsBefore (a test switch, for measuring against): the birds as they were before the birdview lane, 1.5
		// times life (kingfisher 1.9, green heron 1.7), no dark edge, no least thickness, and no closer look
		bool Before() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("KRBirdsBefore")); return b; }
		double ScaleOf(int32 K) { return Before() ? (K == Kingfisher ? 1.9 : K == GreenHeron ? 1.7 : 1.5) : Scales[K]; }

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
				const double E0 = bIs ? R0 + Rim : 0, E1 = bIs ? R1 + Rim : 0;
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

		// One bird from its pose. Every bird writes the same number of vertices whatever it is doing (a folded wing is
		// drawn shut, not left out), so a mesh's layout only changes when a bird joins or leaves it.
		void Emit(FSoup& Buf, const FBird& B, bool bFar, double Rim, const FVector& Away)
		{
			const FBirdSpec& K = Specs[B.Kind];
			FPen Pen{Buf};
			Pen.Rim = Rim; Pen.MinR = Rim * .5; Pen.Back = Away * (K.Body * B.Sz * 1.15 + .05);
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
			Pen.MinR = Rim * .6;
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
	void BirdSoup(FSoup& Buf, const FBird& B, bool bFar, double Rim) { Emit(Buf, B, bFar, Rim, FVector::ZeroVector); }
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
				B.Kind = Sp.Kind; B.Rook = Sp.Rook; B.Out = Sp.Out; B.Site = Sp.Site; B.bHasSite = Sp.Rook < 0;
				B.Seed = Sp.Seed + i * 7919;
				const double R1 = EHash(B.Seed * 1e-6, 1.0 + i), R2 = EHash(B.Seed * 1e-6, 2.0 + i);
				B.Sz = ScaleOf(B.Kind) * (.96 + .08 * R1);
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
				if (B.Kind == Tern || B.Kind == Osprey) { B.State = E.Phase == 0 ? BAway : BPatrol; B.Wing = 1; B.To = B.Home; }
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
				E.bStillDirty = true;
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
				E.bStillDirty = true;
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
				const FVector Np = FMath::Lerp(B.From, To, Us) + FVector(0, B.Arc * FMath::Sin(PI * U), 0);
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
				if (B.Rook < 0 && B.Cool <= 0 && B.Kind != Tern && B.Kind != Osprey)
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
				else if ((B.Kind == Tern || B.Kind == Osprey) && E.Phase != 0 && B.Cool <= 0) { B.State = BPatrol; B.P = B.Home + FVector(0, 6, 0); B.To = B.Home; B.Wing = 1; }
				break;
			case BSwim:
				B.T -= Dt;
				// a cormorant goes under for a while in the middle of its swim
				B.P.Y = E.Sea - (B.Kind == Cormorant && B.T > 2.5 && B.T < 6.5 ? .8 : 0);
				B.Neck = 0;
				if (B.T <= 0) StartFly(B, B.Perch, RestState(E, B), 2);
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
							if (!Sh.bTold) Sh.Terns++;
							E.Sound(TEXT("kree"), B.Kind, B.P, .7f);
							StartFly(B, FVector(Sh.P.X + FMath::FRandRange(-1.5, 1.5), E.Sea + .05, Sh.P.Y + FMath::FRandRange(-1.5, 1.5)), BDive, 2.5, 14);
							break;
						}
					if (B.State != BPatrol) break;
					if (FVector2D::Distance(FVector2D(Want.X, Want.Z), P2) < 5 || B.Think <= 0)
					{
						B.Think = 9;
						// over a school it has reached, it may drop on it
						FSchoolE* Under = NearestSchool(E, P2, 6);
						if (Under && Under->bAwake && Under->Scatter < .2 && B.Cool <= 0 && FMath::FRand() < .4)
						{
							B.Cool = 8;
							StartFly(B, FVector(Under->C.X, E.Sea + .05, Under->C.Y), BDive, 0, 13);
							break;
						}
						// on to the next: a school of small fish somewhere along this shore, else a turn along the edge
						const FVector2D Tn(-B.Out.Y, B.Out.X);
						FVector2D Next = FVector2D(B.Home.X, B.Home.Z) + Tn * FMath::FRandRange(-70.0, 70.0) + B.Out * FMath::FRandRange(-2.0, 22.0);
						if (FSchoolE* Sc = NearestSchool(E, Next, 60)) { Next = Sc->C; B.Prey = Sc->Id; } else B.Prey = -1;
						B.To = FVector(Next.X, 6 + 3 * FMath::FRand(), Next.Y);
						Want = B.To;
					}
				}
				const FVector2D To2(Want.X - B.P.X, Want.Z - B.P.Z);
				B.Yaw += FMath::Clamp(WrapPi(FMath::Atan2(To2.Y, To2.X) - B.Yaw), -1.6 * Dt, 1.6 * Dt);
				const double WantY = B.Kind == Osprey ? E.Sea + 27 + 3 * FMath::Sin(B.Ph * .3) : E.Sea + Want.Y + .6 * FMath::Sin(B.Ph * 1.7);
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
		for (const FBird& B : T.Birds)
		{
			// (its snag stands whether the bird is on it or not)
			if (bStillPass && !Before() && (B.Kind == Cormorant || B.Kind == Anhinga) && B.Rook < 0)
			{
				const double Ds = FVector2D::Distance(FVector2D(B.Perch.X, B.Perch.Z), Cam2);
				if (Ds < 320) EmitSnag(Buf, E, B, FMath::Clamp(Ds * .0008, .01, .15));
			}
			if (B.State == BAway || B.bStill != bStillPass || (B.State == BFly && B.FlyT < 0 && B.Next != BAway && B.From.Y > 15)) continue;
			const double D2 = FVector2D::DistSquared(FVector2D(B.P.X, B.P.Z), Cam2);
			if (D2 > FMath::Square(Specs[B.Kind].Draw)) continue;
			if (B.State == BSwim && B.P.Y < E.Sea - .4) continue;
			// (past 70 m a bird is drawn in half the pieces; its dark edge is about a pixel and a half wide through the
			// closer look's lens at whatever distance it stands)
			Emit(Buf, B, D2 > 70.0 * 70.0, Before() ? 0 : FMath::Clamp(FMath::Sqrt(D2) * (B.Rook >= 0 ? .0005 : .0008), .01, .15), (B.P - E.Cam).GetSafeNormal());   // (on a rookery's green crowns they need less of it)
			if (bStillPass) E.StillN++; else E.BirdsMoved++;
		}
	}
}
