// The tuna rod and lever-drag reel (KeysMesh.h BuildTunaRod), built from lathes, tubes and rounded boxes.
#include "KeysMesh.h"
#include "KeysHuman.h"
#include "KeysData.h"

namespace
{
	const FLinearColor Alu(.82f, .83f, .85f), AluDark(.62f, .63f, .66f), Black(.018f, .018f, .02f), Foam(.03f, .03f, .032f),
		Blue(.03f, .16f, .75f), Mono(.12f, .38f, 1.f), Silver(.9f, .9f, .92f);

	// a solid of revolution about Axis through C: Prof is (radius, distance along the axis); smooth, capped by the profile
	void Lathe(FKeysMesh& M, const FVector& C, const FVector& Axis, const FVector& Ref, const TArray<FVector2D>& Prof, int32 Sides, const FLinearColor& Col)
	{
		const FVector A = Axis.GetSafeNormal();
		FVector U = (Ref - A * FVector::DotProduct(Ref, A)).GetSafeNormal();
		if (U.IsNearlyZero()) U = FVector::CrossProduct(A, FMath::Abs(A.Z) < .9 ? FVector::ZAxisVector : FVector::XAxisVector).GetSafeNormal();
		const FVector W = FVector::CrossProduct(A, U);
		const int32 Base = M.V.Num();
		const int32 NP = Prof.Num();
		for (int32 j = 0; j < NP; j++)
		{
			// the profile's own slope gives the normal's lean along the axis
			const FVector2D D = Prof[FMath::Min(j + 1, NP - 1)] - Prof[FMath::Max(j - 1, 0)];
			for (int32 i = 0; i < Sides; i++)
			{
				const double Ang = 2 * PI * i / Sides;
				const FVector O = U * FMath::Cos(Ang) + W * FMath::Sin(Ang);
				FVector Nn = (O * D.Y - A * D.X).GetSafeNormal();
				if (Nn.IsNearlyZero()) Nn = j == 0 ? -A : A;
				M.AddVert(C + A * Prof[j].Y + O * Prof[j].X, Nn, Col);
			}
		}
		for (int32 j = 0; j + 1 < NP; j++)
			for (int32 i = 0; i < Sides; i++)
			{
				const int32 a = Base + j * Sides + i, b = Base + j * Sides + (i + 1) % Sides, c = Base + (j + 1) * Sides + (i + 1) % Sides, d = Base + (j + 1) * Sides + i;
				const FVector Nn = M.N[a] + M.N[c];
				if (FVector::DotProduct(FVector::CrossProduct(M.V[b] - M.V[a], M.V[c] - M.V[a]), Nn) > 0) M.T.Append({a, c, b, a, d, c});
				else M.T.Append({a, b, c, a, c, d});
			}
	}
	void Tube(FKeysMesh& M, const FVector& A, const FVector& B, double RA, double RB, int32 Sides, const FLinearColor& Col, uint8 Kind)
	{
		const uint8 W = M.Surf; M.Surf = Kind; M.AddTube(A, B, RA, RB, Sides, Col); M.Surf = W;
	}
	// a closed cylinder: a lathe with rounded rims
	void Puck(FKeysMesh& M, const FVector& C, const FVector& Axis, const FVector& Ref, double R, double H, double Bev, int32 Sides, const FLinearColor& Col, uint8 Kind)
	{
		const uint8 W = M.Surf; M.Surf = Kind;
		Lathe(M, C - Axis * H * .5, Axis, Ref, {{0, 0}, {R - Bev, 0}, {R - Bev * .3, Bev * .3}, {R, Bev}, {R, H - Bev}, {R - Bev * .3, H - Bev * .3}, {R - Bev, H}, {0, H}}, Sides, Col);
		M.Surf = W;
	}
}

namespace KeysMesh
{
	int32 GRodLod = 1;

	void BuildRodHolder(FKeysMesh& M, const FVector& Mouth, const FVector& Axis, const FVector& SurfaceN)
	{
		const uint8 W = M.Surf;
		M.Surf = SurfSteel;
		// the flange lies on the surface; the tube runs down the rod's own angle
		Lathe(M, Mouth - SurfaceN * .004, SurfaceN, Axis, {{.03, 0}, {.058, 0}, {.062, .004}, {.058, .009}, {.034, .01}, {.03, .006}}, 20, Silver);
		M.AddTube(Mouth, Mouth - Axis * .26, .031, .029, 16, AluDark);
		M.Surf = W;
	}

	FVector BuildTunaRod(FKeysMesh& M, const FRodPose& P)
	{
		const uint8 Keep = M.Surf;
		const FVector D = P.Dir.GetSafeNormal();
		const FVector Up = (P.Up - D * FVector::DotProduct(P.Up, D)).GetSafeNormal(UE_SMALL_NUMBER, FVector::CrossProduct(D, FVector::YAxisVector));
		const FVector Side = FVector::CrossProduct(D, Up);
		const bool bNear = P.Lod == 0;
		const int32 RS = bNear ? 16 : 8;
		const double L = P.Len;
		auto At = [&](double S) { return P.Butt + D * S; };

		// ---- the aluminium gimbal butt, its cross-pin, and the reel seat with its clamp rings
		M.Surf = SurfSteel;
		Lathe(M, P.Butt, D, Up, {{0, 0}, {.016, 0}, {.021, .006}, {.022, .03}, {.0215, .3}, {.0235, .31}, {.0235, .33}, {.0215, .34}, {.0215, .5}, {.025, .51}, {.025, .53}, {.017, .54}}, RS, Alu);
		if (bNear)
		{
			Tube(M, At(.012) - Side * .024, At(.012) + Side * .024, .004, .004, 8, AluDark, SurfSteel);
			for (const double S : {.36, .47})
				Puck(M, At(S), D, Up, .028, .018, .004, RS, Alu, SurfSteel);   // the clamp rings, knurled by the steel's brushing
		}

		// ---- the long black foam fore grip
		M.Surf = SurfRubber;
		Lathe(M, At(.54), D, Up, {{.017, 0}, {.024, .004}, {.026, .02}, {.025, .25}, {.024, .46}, {.021, .48}, {.012, .485}}, RS, Foam);
		const double BlankS = 1.02;
		// winding check
		Puck(M, At(BlankS + .005), D, Up, .015, .012, .003, RS, Alu, SurfSteel);

		// ---- the blank: a tapered curve bowing toward the load; the first stretch cross-wrapped in blue and silver
		const FVector B0 = At(BlankS), B2 = P.Butt + D * L + P.Bend, B1 = (B0 + B2) * .5 + P.Bend * .25 - D * 0;
		const FVector Ctrl = B0 + D * ((B2 - B0).Size() * .45);
		auto Curve = [&](double T) { return B0 * FMath::Square(1 - T) + Ctrl * 2 * T * (1 - T) + B2 * T * T; };
		(void)B1;
		const int32 NSeg = bNear ? 22 : 10;
		const double WrapEnd = .2;   // of the blank
		for (int32 k = 0; k < NSeg; k++)
		{
			const double T0 = double(k) / NSeg, T1 = double(k + 1) / NSeg;
			const double R0 = FMath::Lerp(.0125, .0042, T0), R1 = FMath::Lerp(.0125, .0042, T1);
			const bool bWrap = T1 <= WrapEnd + 1e-6;
			Tube(M, Curve(T0), Curve(T1), bWrap ? R0 + .0012 : R0, bWrap ? R1 + .0012 : R1, bNear ? 12 : 6, bWrap ? Blue : Black, SurfGelcoat);
		}
		if (bNear)
		{
			// the diamond cross-wrap: two silver helices winding opposite ways over the blue underwrap, and trim bands
			const int32 NH = 28;
			for (int32 h = -1; h <= 1; h += 2)
				for (int32 Strand = 0; Strand < 3; Strand++)
				{
					FVector Prev;
					for (int32 k = 0; k <= NH; k++)
					{
						const double T = WrapEnd * k / NH;
						const FVector C = Curve(T);
						const double Ang = h * T / WrapEnd * PI * 6 + Strand * 2 * PI / 3;
						const double R = FMath::Lerp(.0125, .0042, T) + .0016;
						const FVector Q = C + (Up * FMath::Cos(Ang) + Side * FMath::Sin(Ang)) * R;
						if (k) Tube(M, Prev, Q, .0011, .0011, 4, Silver, SurfSteel);
						Prev = Q;
					}
				}
			for (const double T : {0.0, WrapEnd})
				Tube(M, Curve(FMath::Max(0.0, T - .006)), Curve(T + .006), FMath::Lerp(.0125, .0042, T) + .0022, FMath::Lerp(.0125, .0042, T) + .0022, 12, Silver, SurfSteel);
		}

		// ---- the guides: rings on wrapped feet along the reel side, and the tip-top
		const int32 NG = bNear ? 6 : 3;
		for (int32 g = 0; g < NG; g++)
		{
			const double T = .26 + .7 * double(g) / NG;
			const FVector C = Curve(T);
			const double Rb = FMath::Lerp(.0125, .0042, T);
			const double Ring = FMath::Lerp(.016, .007, T), Stand = Rb + Ring + .01;
			const FVector Tn = (Curve(FMath::Min(1.0, T + .01)) - Curve(FMath::Max(0.0, T - .01))).GetSafeNormal();
			Tube(M, C - Tn * .018, C + Tn * .018, Rb + .0015, Rb + .0015, bNear ? 10 : 6, Blue, SurfGelcoat);   // the thread wrap
			Tube(M, C + Up * Rb, C + Up * (Stand - Ring), .0016, .0016, 4, Silver, SurfSteel);
			if (bNear)
			{
				// the ring: a small torus around the line's path
				const FVector RC = C + Up * Stand;
				FVector Prev;
				for (int32 k = 0; k <= 10; k++)
				{
					const double A = 2 * PI * k / 10;
					const FVector Q = RC + (Up * FMath::Cos(A) + Side * FMath::Sin(A)) * Ring;
					if (k) Tube(M, Prev, Q, .0017, .0017, 4, Silver, SurfSteel);
					Prev = Q;
				}
			}
		}
		const FVector Tip = Curve(1.0);
		Puck(M, Tip, (Tip - Curve(.97)).GetSafeNormal(), Up, .006, .012, .002, 8, Silver, SurfSteel);

		// ---- the reel: on top of the seat, its spool across the rod
		if (P.bReel)
		{
			const double Sc = 1.0;
			const FVector RC = At(.415) + Up * .085 * Sc;   // the spool's axis centre
			const double HalfW = .052 * Sc, PlateR = .062 * Sc, PlateT = .016 * Sc;
			// the foot and the seat clamp it bolts to
			M.Surf = SurfSteel;
			M.AddRoundBox(At(.415) + Up * .03, FVector(.13, .03, .036) * Sc, .006, Alu, FRotationMatrix::MakeFromXZ(D, Up).ToQuat(), bNear ? 2 : 1);
			// side plates, machined: a thick disc with a raised hub
			for (int32 s = -1; s <= 1; s += 2)
			{
				const FVector PC = RC + Side * (s * (HalfW + PlateT * .5));
				Puck(M, PC, Side * s, D, PlateR, PlateT, .005, bNear ? 28 : 12, Alu, SurfSteel);
				Puck(M, PC + Side * (s * PlateT * .7), Side * s, D, PlateR * .55, PlateT * .6, .003, bNear ? 20 : 10, AluDark, SurfSteel);
				if (bNear)
					for (int32 b = 0; b < 8; b++)
					{
						const double A = 2 * PI * b / 8 + .2;
						const FVector BC = PC + Side * (s * PlateT * .5) + (D * FMath::Cos(A) + Up * FMath::Sin(A)) * PlateR * .82;
						Puck(M, BC, Side * s, D, .0045, .004, .0012, 8, Silver, SurfSteel);
					}
			}
			// pillars between the plates, and the open frame's bottom bar
			for (const double A : {PI * .15, PI * .85, PI * 1.5})
			{
				const FVector O = (D * FMath::Cos(A) + Up * FMath::Sin(A)) * PlateR * .86;
				Tube(M, RC + O - Side * HalfW, RC + O + Side * HalfW, .0065, .0065, bNear ? 10 : 6, Alu, SurfSteel);
			}
			// the spool, packed with bright blue mono
			{
				const uint8 W = M.Surf; M.Surf = SurfGlass;
				Lathe(M, RC - Side * HalfW, Side, D, {{0, 0}, {PlateR * .8, 0}, {PlateR * .82, .004}, {PlateR * .82, HalfW * 2 - .004}, {PlateR * .8, HalfW * 2}, {0, HalfW * 2}}, bNear ? 24 : 10, Mono);
				M.Surf = W;
			}
			// the lever drag: an arched arm over the right plate with a knurled knob
			const FVector RP = RC + Side * (HalfW + PlateT);
			M.Surf = SurfSteel;
			M.AddRoundBox(RP + Up * PlateR * .55 + D * .01 + Side * .006, FVector(.075, .012, .014), .005, Alu, FRotationMatrix::MakeFromXZ(D, Side).ToQuat() * FQuat(FVector::YAxisVector, -.5), 1);
			Puck(M, RP + Up * PlateR * .95 + D * .03 + Side * .006, Up, D, .011, .022, .003, bNear ? 16 : 8, Silver, SurfSteel);
			// the crank: an arm from the hub out and up, and its big black oval knob
			const FVector Hub = RP + Side * .012;
			Puck(M, Hub, Side, D, .016, .018, .004, bNear ? 16 : 8, Alu, SurfSteel);
			const FVector ArmEnd = Hub + Side * .012 + (Up * .7 - D * .7).GetSafeNormal() * .085;
			M.AddRoundBox((Hub + ArmEnd) * .5 + Side * .006, FVector((ArmEnd - Hub).Size() + .02, .012, .02), .005, Alu, FRotationMatrix::MakeFromXZ((ArmEnd - Hub).GetSafeNormal(), Side).ToQuat(), 1);
			M.Surf = SurfRubber;
			KeysShapes::AddBlob(M, ArmEnd + Side * .045, FVector(.022, .05, .021), FRotationMatrix::MakeFromXZ(D, Up).ToQuat(), Black, bNear ? 12 : 6, bNear ? 6 : 3);
			Tube(M, ArmEnd + Side * .005, ArmEnd + Side * .02, .008, .008, 8, Alu, SurfSteel);
			// the gear shift knob on the left plate and the bail of line feeding out the top
			Puck(M, RC - Side * (HalfW + PlateT * 1.6), Side, D, .012, .014, .004, 12, Silver, SurfSteel);
		}
		M.Surf = Keep;
		return Tip;
	}
}

// ---------------------------------------------------------------- the fighting chair
namespace KeysMesh
{
	bool HasFightChair(const KeysData::FBoatSpec& S) { return S.Id == TEXT("freeman"); }

	FVector FightChairBase(const KeysData::FBoatSpec& S)
	{
		// 2.6 m in from the transom, on the centreline: clear of the leaning post, the stern rod holders and the guests'
		// lap of the cockpit (which crosses the stern behind it)
		const double X = SternX(S) + 2.75;
		return FVector(X, 0, SoleAt(S, X) + .38);
	}

	void BuildFightChairPedestal(FKeysMesh& M, const FVector& Base, double Sole)
	{
		const uint8 W = M.Surf;
		M.Surf = SurfSteel;
		const double H = Base.Z - Sole;
		Lathe(M, FVector(Base.X, Base.Y, Sole), FVector::UpVector, FVector::ForwardVector,
			{{0, 0}, {.26, 0}, {.27, .012}, {.25, .03}, {.1, .05}, {.075, .09}, {.07, H - .07}, {.09, H - .04}, {.15, H - .02}, {.15, H}, {0, H}}, 32, Silver);
		M.Surf = W;
	}

	FVector ChairGimbal(const FVector& Base, double Yaw)
	{
		return Base + FQuat(FVector::UpVector, Yaw).RotateVector(FVector(.36, 0, .14));
	}

	void BuildFightChair(FKeysMesh& M, const FVector& Base, double Yaw)
	{
		const uint8 Keep = M.Surf;
		const FQuat Q(FVector::UpVector, Yaw);
		auto P = [&](double X, double Y, double Z) { return Base + Q.RotateVector(FVector(X, Y, Z)); };
		const FLinearColor Teak(.36f, .17f, .065f), TeakL(.45f, .22f, .09f);
		auto Wood = [&](const FVector& C, const FVector& Sz, double R, const FQuat& Rot = FQuat::Identity)
		{
			M.Surf = SurfWood;
			M.AddRoundBox(C, Sz, R, Teak, Q * Rot, 2);
		};
		// the swivel head and the frame under the seat
		Puck(M, Base + FVector(0, 0, .02), FVector::UpVector, FVector::ForwardVector, .16, .04, .008, 24, Silver, SurfSteel);
		Tube(M, P(0, 0, .04), P(0, 0, .1), .05, .05, 16, Silver, SurfSteel);
		// the seat: a thick varnished teak pan, a little dished
		Wood(P(.02, 0, .14), FVector(.56, .58, .08), .035);
		Wood(P(.02, 0, .19), FVector(.5, .52, .03), .012);
		// the back: two posts and a broad curved back of three staves
		for (int32 s = -1; s <= 1; s += 2) Wood(P(-.27, s * .22, .42), FVector(.06, .06, .58), .02, FQuat(FVector::YAxisVector, -.12));
		for (int32 k = -1; k <= 1; k++)
		{
			const double A = k * .32;
			Wood(P(-.28 + FMath::Abs(k) * .04, k * .155, .5), FVector(.05, .17, .5), .02, FQuat(FVector::ZAxisVector, A) * FQuat(FVector::YAxisVector, -.12));
		}
		Wood(P(-.31, 0, .78), FVector(.07, .5, .07), .03, FQuat(FVector::YAxisVector, -.12));
		// the arms: broad flat teak rests on turned posts, a rod holder at the front of each
		for (int32 s = -1; s <= 1; s += 2)
		{
			Wood(P(.02, s * .32, .44), FVector(.62, .1, .045), .02);
			Tube(M, P(.22, s * .32, .19), P(.22, s * .32, .42), .02, .02, 12, Silver, SurfSteel);
			Tube(M, P(-.15, s * .32, .19), P(-.15, s * .32, .42), .02, .02, 12, Silver, SurfSteel);
			Tube(M, P(.3, s * .34, .4), P(.36, s * .4, .62), .03, .03, 14, Silver, SurfSteel);
		}
		// the gimbal socket between the knees, with its cross-pin
		Tube(M, P(.3, 0, .17), P(.36, 0, .14), .04, .04, 16, Silver, SurfSteel);
		Puck(M, P(.36, 0, .14), FVector::UpVector, FVector::ForwardVector, .04, .05, .008, 16, Silver, SurfSteel);
		Tube(M, P(.36, -.05, .14), P(.36, .05, .14), .007, .007, 8, AluDark, SurfSteel);
		// the footrest: a stainless arm from the pedestal and a teak bar across, set for the angler's legs
		Tube(M, P(0, 0, -.15), P(.62, 0, -.22), .025, .025, 12, Silver, SurfSteel);
		Wood(P(.66, 0, -.22), FVector(.1, .62, .05), .02);
		for (int32 s = -1; s <= 1; s += 2) Tube(M, P(.66, s * .31, -.22), P(.66, s * .36, -.22), .022, .022, 10, Silver, SurfSteel);
		(void)TeakL;
		M.Surf = Keep;
	}
}
