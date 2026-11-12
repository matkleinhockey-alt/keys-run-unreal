// The helm's instrument panel. Every boat's dash carries two big flush-mounted screens (the chartplotter and the fish
// finder, the same pages G puts over the view), a big round tachometer per engine with the speedometer and a compass
// card beside them, a small trim gauge per engine and fuel, water temperature, oil pressure and volts gauges, all in
// black bezels with chrome rings; and a throttle binnacle whose levers follow the throttle.
//
// The fixed parts (the pod, the panel, the bezels, the rings, the binnacle) are built with the hull (KeysMesh). The faces
// are flat discs and screens in their own mesh, UV-mapped onto two canvas render targets the pawn draws into: the gauges
// every frame while you are close (the needles move smoothly), the screens a few times a second. The render targets
// get mips, so the dials stay crisp and unaliased at any distance; M_KR_Dash shows them as backlit glass.
#include "KeysBoatPawn.h"
#include "KeysMesh.h"
#include "KeysData.h"
#include "KeysAudio.h"
#include "KeysHUD.h"
#include "KeysWorld.h"
#include "ProceduralMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/TextureRenderTarget2D.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	FLinearColor DSrgb(double R, double G, double B) { return FLinearColor(FColor(uint8(R * 255), uint8(G * 255), uint8(B * 255))); }
	double DStep(double A, double B, double X) { const double T = FMath::Clamp((X - A) / (B - A), 0.0, 1.0); return T * T * (3 - 2 * T); }

	// the gauge render target: a row of 256 px cells for the big dials (speed, six tachs, compass) over a row of 192 px
	// cells for the small ones (six trims, fuel, temp, oil, volts)
	constexpr int32 GaugeW = 2048, GaugeH = 512, BigCell = 256, SmallCell = 192;
	// the screen render target: the chart on the left half, the sonar on the right, 16:9 each
	constexpr int32 ScreenW = 2048, ScreenH = 1024, ScrCellW = 1024, ScrCellH = 576;

	void GaugeCell(uint8 Kind, int32 Index, double& X, double& Y, double& Size)
	{
		switch (Kind)
		{
		case KeysMesh::DashSpeed: X = 0; Y = 0; Size = BigCell; return;
		case KeysMesh::DashTach: X = (1 + Index) * BigCell; Y = 0; Size = BigCell; return;
		case KeysMesh::DashCompass: X = 7 * BigCell; Y = 0; Size = BigCell; return;
		case KeysMesh::DashTrim: X = Index * SmallCell; break;
		case KeysMesh::DashFuel: X = 6 * SmallCell; break;
		case KeysMesh::DashTemp: X = 7 * SmallCell; break;
		case KeysMesh::DashOil: X = 8 * SmallCell; break;
		default: X = 9 * SmallCell; break;
		}
		Y = BigCell;
		Size = SmallCell;
	}
}

namespace KeysMesh
{
	FDashLayout DashLayout(const KeysData::FBoatSpec& S)
	{
		FDashLayout D;
		const FBoatLayout B = BoatLayout(S);
		const bool bMid = S.Id == TEXT("midnight"), bFree = S.Id == TEXT("freeman"), bBig = bMid || bFree;
		const double CA = B.MidX - B.ConL * .5;
		const int32 E = FMath::Clamp(S.Engines, 1, 6);
		// the panel leans back about 52 degrees, so it faces the driver's eye nearly square on
		const double Th = .9, Sn = FMath::Sin(Th), Cs = FMath::Cos(Th);
		const double Mg = .02, Gap = .014, Bz = D.Bezel;
		// screens: 16" on the centre consoles, 19" on the Freeman, 21" on the Midnight (16:9 glass)
		const double SW = bMid ? .47 : bFree ? .43 : .355, SH = SW * 9 / 16;
		const double R1 = bBig ? .058 : .052, R2 = bBig ? .036 : .033;
		D.Levers = E == 1 ? 1 : 2;
		D.Ux = FVector(0, 1, 0);
		D.Uy = FVector(Sn, 0, Cs);
		D.N = FVector(-Cs, 0, Sn);
		const double ScreensW = 2 * (SW + 2 * Bz) + Gap;
		D.W = bMid ? ScreensW + 2 * Mg + .03 : FMath::Max(ScreensW + 2 * Mg, B.ConW * .82);
		D.H = Mg + 2.4 * R2 + Gap + 2.4 * R1 + Gap + SH + 2 * Bz + Mg;
		// the binnacle stands to starboard of the wheel
		const double Yb = bMid ? .66 : B.ConW * .3;
		if (bMid)
		{
			D.bPod = false;
			D.TopZ = SoleAt(S, CA) - .02 + 1.28;
			D.SlopeRun = D.H * Sn;
			D.SlopeDrop = D.H * Cs;
			D.O = FVector(CA, 0, D.TopZ - D.SlopeDrop) + D.N * .012;
			const double Xb = CA + .17, Zs = D.TopZ - D.SlopeDrop + .17 / D.SlopeRun * D.SlopeDrop;
			D.Pivot = FVector(Xb, Yb, Zs + .06 + .075);
		}
		else
		{
			D.TopZ = SoleAt(S, CA + .1) - .02 + (bFree ? 1.2 : 1.18);
			D.O = FVector(CA + .1, 0, D.TopZ + .015);
			D.Pivot = FVector(CA + .05, Yb, D.TopZ + .075);
		}
		// the screens across the top: the chart to port, the sonar to starboard
		const double VS = Mg + 2.4 * R2 + Gap + 2.4 * R1 + Gap + Bz + SH * .5;
		for (int32 k = 0; k < 2; k++)
		{
			FDashScreen Sc;
			Sc.Kind = k;
			Sc.U = (k == 0 ? -1 : 1) * (SW * .5 + Bz + Gap * .5);
			Sc.V = VS;
			Sc.W = SW;
			Sc.H = SH;
			D.Screens.Add(Sc);
		}
		// the gauges in two rows under them, kept clear of the binnacle (on the centre consoles it stands in front of
		// the panel's starboard end)
		const double ZoneL = -D.W * .5 + Mg, ZoneR = bMid ? D.W * .5 - Mg : FMath::Min(D.W * .5 - Mg, Yb - .1);
		auto Row = [&](const TArray<TPair<uint8, int32>>& Items, double RMax, double V)
		{
			const int32 Nn = Items.Num();
			const double R = FMath::Min(RMax, (ZoneR - ZoneL) / (Nn * 2.5)), Step = R * 2.5, Uc = (ZoneL + ZoneR) * .5;
			for (int32 i = 0; i < Nn; i++)
			{
				FDashGauge G;
				G.Kind = Items[i].Key;
				G.Index = Items[i].Value;
				G.R = R;
				G.U = Uc + (i - (Nn - 1) * .5) * Step;
				G.V = V;
				D.Gauges.Add(G);
			}
		};
		TArray<TPair<uint8, int32>> Big, Small;
		Big.Add({DashSpeed, 0});
		for (int32 e = 0; e < E; e++) Big.Add({DashTach, e});
		Big.Add({DashCompass, 0});
		for (int32 e = 0; e < E; e++) Small.Add({DashTrim, e});
		for (const uint8 K : {DashFuel, DashTemp, DashOil, DashVolt}) Small.Add({K, 0});
		Row(Big, R1, Mg + 2.4 * R2 + Gap + 1.2 * R1);
		Row(Small, R2, Mg + 1.2 * R2);
		// the driver stands at the wheel, his back to the leaning post or the helm seats, and looks down at the panel
		const double EX = CA - .5;
		D.Eye = FVector(EX, 0, SoleAt(S, EX) + 1.66);
		const FVector Pc = D.P(0, D.H * .45);
		D.Pitch = -FMath::Atan2(D.Eye.Z - Pc.Z, Pc.X - D.Eye.X) * .78;
		return D;
	}

	double ThrottleAngle(double Thr, int32 Gear)
	{
		// upright in neutral; out of the detent into gear, then on up to about 70 degrees ahead or 40 astern
		const double Det = .2;
		if (Thr >= 0) return (Gear == 1 ? 0.0 : Det * DStep(0, .03, Thr)) + 1.0 * FMath::Min(Thr, 1.0);
		return -Det * DStep(0, .03, -Thr) - .5 * FMath::Min(1.0, -Thr / .35);
	}

	void BuildDash(FKeysMesh& M, const FDashLayout& D, const FLinearColor& Moulding, uint8 MouldingKind)
	{
		const uint8 Keep = M.Surf;
		const FLinearColor PanelC = DSrgb(.035, .037, .04), BezelC = DSrgb(.018, .018, .02), Chrome = DSrgb(.9, .91, .93);
		const FQuat PanelRot = FRotationMatrix::MakeFromXY(-D.N, D.Ux).ToQuat();
		// the pod: a gelcoat wedge on the console top whose sloping face is the panel, a short cap over its top edge
		if (D.bPod)
		{
			M.Surf = MouldingKind;
			const double Hw = D.W * .5 + .018;
			const FVector A = D.P(0, -.012, -.003), Bt = D.P(0, D.H + .012, -.003), Cc = Bt + FVector(.07, 0, -.006);
			const FVector Dd(Cc.X + .025, 0, D.TopZ - .12), Ee(A.X - .01, 0, D.TopZ - .12);
			const FVector Prof[5] = {A, Bt, Cc, Dd, Ee};
			auto At = [&](int32 i, double Y) { return FVector(Prof[i].X, Y, Prof[i].Z); };
			for (int32 i = 0; i < 5; i++)
			{
				const int32 j = (i + 1) % 5;
				if (i == 3) continue;   // the foot is inside the console
				const FVector Mid = (Prof[i] + Prof[j]) * .5, Cn = (A + Bt + Cc + Dd + Ee) / 5;
				FVector Out = Mid - Cn;
				Out.Y = 0;
				M.AddQuad(At(i, -Hw), At(j, -Hw), At(j, Hw), At(i, Hw), Moulding, Out);
			}
			for (int32 s = -1; s <= 1; s += 2)
				for (int32 i = 1; i < 4; i++) M.AddTri(At(0, s * Hw), At(i, s * Hw), At(i + 1, s * Hw), Moulding, FVector(0, s, 0));
			// a rounded lip along the top edge
			M.AddTube(FVector(Bt.X + .03, -Hw, Bt.Z - .003), FVector(Bt.X + .03, Hw, Bt.Z - .003), .016, .016, 10, Moulding);
		}
		// the black panel; in the Midnight's slope it is a slab with its edges showing
		M.Surf = SurfRubber;
		{
			const double Hw = D.W * .5;
			const FVector P0 = D.P(-Hw, 0, .002), P1 = D.P(Hw, 0, .002), P2 = D.P(Hw, D.H, .002), P3 = D.P(-Hw, D.H, .002);
			M.AddQuad(P0, P1, P2, P3, PanelC, D.N);
			if (!D.bPod)
			{
				const FVector Back = -D.N * .06;
				M.AddQuad(P0, P1, P1 + Back, P0 + Back, PanelC, -D.Uy);
				M.AddQuad(P3, P2, P2 + Back, P3 + Back, PanelC, D.Uy);
				M.AddQuad(P0, P3, P3 + Back, P0 + Back, PanelC, -D.Ux);
				M.AddQuad(P1, P2, P2 + Back, P1 + Back, PanelC, D.Ux);
			}
		}
		// the screens' bezels: glossy black frames standing a centimetre proud, the glass flush with their face
		M.Surf = SurfGelcoat;
		for (const FDashScreen& Sc : D.Screens)
		{
			M.AddRoundBox(D.P(Sc.U, Sc.V, .005), FVector(.012, Sc.W + 2 * D.Bezel, Sc.H + 2 * D.Bezel), .005, BezelC, PanelRot, 2);
			// a row of small keys along the bottom of the frame
			for (int32 k = 0; k < 5; k++)
				M.AddRoundBox(D.P(Sc.U + Sc.W * (.18 + .08 * k), Sc.V - Sc.H * .5 - D.Bezel * .5, .0115), FVector(.003, .014, .006), .0015, DSrgb(.12, .12, .13), PanelRot, 1);
		}
		// each gauge: a black bezel standing off the panel and a chrome ring round the glass
		for (const FDashGauge& G : D.Gauges)
		{
			const int32 Seg = 32;
			const double R = G.R, Rb = R * 1.17, Rc = R * 1.09;
			M.Surf = SurfGelcoat;
			M.AddTube(D.P(G.U, G.V, -.002), D.P(G.U, G.V, .006), Rb, Rb, Seg, BezelC);
			for (int32 k = 0; k < Seg; k++)
			{
				const double A0 = 2 * PI * k / Seg, A1 = 2 * PI * (k + 1) / Seg;
				auto Pt = [&](double A, double Rr) { return D.P(G.U + FMath::Cos(A) * Rr, G.V + FMath::Sin(A) * Rr, .006); };
				M.AddQuad(Pt(A0, R * .98), Pt(A1, R * .98), Pt(A1, Rb), Pt(A0, Rb), BezelC, D.N);
			}
			M.Surf = SurfSteel;
			const double Tr = FMath::Max(.0025, R * .07);
			for (int32 k = 0; k < Seg; k++)
			{
				const double A0 = 2 * PI * k / Seg, A1 = 2 * PI * (k + 1) / Seg;
				M.AddTube(D.P(G.U + FMath::Cos(A0) * Rc, G.V + FMath::Sin(A0) * Rc, .0065), D.P(G.U + FMath::Cos(A1) * Rc, G.V + FMath::Sin(A1) * Rc, .0065), Tr, Tr, 6, Chrome);
			}
		}
		// the throttle binnacle: a glossy black housing with a rounded top the levers come out of, on the console top
		// (the Midnight's on a pad raised out of its slope); a detent plate on its outboard side marks ahead, neutral
		// and astern
		{
			const FVector Pv = D.Pivot;
			const double Wl = D.Levers * D.LeverGap + .03, Zb = Pv.Z - .075;
			if (!D.bPod)
			{
				M.Surf = MouldingKind;
				M.AddRoundBox(FVector(Pv.X, Pv.Y, Zb - .07), FVector(.2, Wl + .07, .16), .03, Moulding, FQuat::Identity, 2);
			}
			M.Surf = SurfGelcoat;
			M.AddRoundBox(FVector(Pv.X, Pv.Y, Zb + .04), FVector(.13, Wl, .085), .02, BezelC, FQuat::Identity, 2);
			M.AddTube(Pv - FVector(0, Wl * .5, 0), Pv + FVector(0, Wl * .5, 0), .052, .052, 20, BezelC);
			const FVector Ax(0, 1, 0);
			for (int32 s = -1; s <= 1; s += 2)
			{
				const FVector Ctr = Pv + FVector(0, s * Wl * .5, 0);
				for (int32 k = 0; k < 20; k++)
				{
					const double A0 = 2 * PI * k / 20, A1 = 2 * PI * (k + 1) / 20;
					M.AddTri(Ctr, Ctr + FVector(FMath::Sin(A0), 0, FMath::Cos(A0)) * .052, Ctr + FVector(FMath::Sin(A1), 0, FMath::Cos(A1)) * .052, BezelC, Ax * s);
				}
			}
			// the detent plate: a chrome arc, a white neutral mark between a green ahead mark and a red astern mark
			const double Yp = Pv.Y + Wl * .5 + .002;
			M.Surf = SurfSteel;
			for (int32 k = 0; k < 10; k++)
			{
				const double A0 = -.75 + 1.75 * k / 10, A1 = -.75 + 1.75 * (k + 1) / 10;
				M.AddTube(Pv + FVector(FMath::Sin(A0) * .043, Wl * .5 + .002, FMath::Cos(A0) * .043), Pv + FVector(FMath::Sin(A1) * .043, Wl * .5 + .002, FMath::Cos(A1) * .043), .0025, .0025, 5, Chrome);
			}
			M.Surf = SurfDefault;
			auto Mark = [&](double A, const FLinearColor& Col)
			{
				const FVector Rd(FMath::Sin(A), 0, FMath::Cos(A)), Tg(FMath::Cos(A), 0, -FMath::Sin(A));
				const FVector C0 = FVector(Pv.X, Yp + .001, Pv.Z) + Rd * .03, C1 = FVector(Pv.X, Yp + .001, Pv.Z) + Rd * .039;
				M.AddQuad(C0 - Tg * .003, C0 + Tg * .003, C1 + Tg * .003, C1 - Tg * .003, Col, FVector(0, 1, 0));
			};
			Mark(0, DSrgb(.95, .95, .95));
			Mark(.55, DSrgb(.2, .75, .3));
			Mark(-.45, DSrgb(.85, .15, .1));
		}
		M.Surf = Keep;
	}

	void BuildThrottleLevers(FKeysMesh& M, const FDashLayout& D)
	{
		const uint8 Keep = M.Surf;
		const FLinearColor Chrome = DSrgb(.9, .91, .93), Grip = DSrgb(.03, .03, .035), Rocker = DSrgb(.32, .33, .35);
		const double Gap = D.LeverGap, Len = D.LeverLen;
		for (int32 i = 0; i < D.Levers; i++)
		{
			const double Y = (i - (D.Levers - 1) * .5) * Gap;
			// a chrome arm out of a collar in the housing's top, curving back toward the driver
			M.Surf = SurfSteel;
			M.AddTube(FVector(0, Y, .03), FVector(0, Y, .062), .015, .013, 12, Chrome);
			TArray<FVector> Arm;
			for (int32 k = 0; k <= 6; k++)
			{
				const double T = double(k) / 6;
				Arm.Add(FVector(-.022 * T * T, Y, .04 + (Len - .04) * T));
			}
			M.AddSweep(Arm, .0085, .0085, FVector(0, 1, 0), Chrome, 10);
			// the handle: a black grip across the top, the twin grips side by side make one bar
			M.Surf = SurfRubber;
			const double GW = D.Levers == 1 ? .1 : Gap * .94;
			M.AddRoundBox(FVector(-.022, Y, Len + .008), FVector(.036, GW, .032), .015, Grip, FQuat::Identity, 3);
			// the trim rocker on the port grip, under the driver's thumb
			if (i == 0)
			{
				M.AddRoundBox(FVector(-.032, Y + GW * .12, Len + .026), FVector(.02, .026, .008), .003, Rocker, FQuat::Identity, 1);
				M.AddRoundBox(FVector(-.027, Y + GW * .12, Len + .031), FVector(.003, .02, .003), .001, DSrgb(.8, .8, .8), FQuat::Identity, 1);
			}
		}
		M.Surf = Keep;
	}
}

// ================================================================ the pawn's side: faces, values, drawing
namespace
{
	// solid triangles and text straight onto the render target's canvas, no per-frame objects
	struct FDashPaint
	{
		FCanvas* C = nullptr;
		FHitProxyId Hit;
		UFont* Font = nullptr;

		void Tri(const FVector2D& A, const FVector2D& B, const FVector2D& Cc, const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC, bool bBlend = false) const
		{
			const ESimpleElementBlendMode Bm = bBlend ? SE_BLEND_Translucent : SE_BLEND_Opaque;
			FBatchedElements* BE = C->GetBatchedElements(FCanvas::ET_Triangle, nullptr, GWhiteTexture, Bm);
			const int32 I0 = BE->AddVertexf(FVector4f(float(A.X), float(A.Y), 0, 1), FVector2f(0, 0), CA, Hit);
			const int32 I1 = BE->AddVertexf(FVector4f(float(B.X), float(B.Y), 0, 1), FVector2f(0, 0), CB, Hit);
			const int32 I2 = BE->AddVertexf(FVector4f(float(Cc.X), float(Cc.Y), 0, 1), FVector2f(0, 0), CC, Hit);
			BE->AddTriangle(I0, I1, I2, GWhiteTexture, Bm);
		}
		void Tri(const FVector2D& A, const FVector2D& B, const FVector2D& Cc, const FLinearColor& Col, bool bBlend = false) const { Tri(A, B, Cc, Col, Col, Col, bBlend); }
		void Quad(const FVector2D& A, const FVector2D& B, const FVector2D& Cc, const FVector2D& D, const FLinearColor& Col, bool bBlend = false) const
		{
			Tri(A, B, Cc, Col, bBlend);
			Tri(A, Cc, D, Col, bBlend);
		}
		void Rect(double X, double Y, double W, double H, const FLinearColor& Col, bool bBlend = false) const
		{
			Quad(FVector2D(X, Y), FVector2D(X + W, Y), FVector2D(X + W, Y + H), FVector2D(X, Y + H), Col, bBlend);
		}
		// angles: 0 straight up, clockwise
		static FVector2D Dir(double A) { return FVector2D(FMath::Sin(A), -FMath::Cos(A)); }
		void Disc(const FVector2D& Ct, double R, const FLinearColor& In, const FLinearColor& Out, int32 Seg = 48, bool bBlend = false) const
		{
			for (int32 k = 0; k < Seg; k++) Tri(Ct, Ct + Dir(2 * PI * k / Seg) * R, Ct + Dir(2 * PI * (k + 1) / Seg) * R, In, Out, Out, bBlend);
		}
		void Arc(const FVector2D& Ct, double R0, double R1, double A0, double A1, const FLinearColor& Col, int32 Seg = 24, bool bBlend = false) const
		{
			for (int32 k = 0; k < Seg; k++)
			{
				const double Aa = FMath::Lerp(A0, A1, double(k) / Seg), Ab = FMath::Lerp(A0, A1, double(k + 1) / Seg);
				Quad(Ct + Dir(Aa) * R0, Ct + Dir(Ab) * R0, Ct + Dir(Ab) * R1, Ct + Dir(Aa) * R1, Col, bBlend);
			}
		}
		// a radial tick from R0 out to R1, W wide
		void Tick(const FVector2D& Ct, double A, double R0, double R1, double W, const FLinearColor& Col) const
		{
			const FVector2D D = Dir(A), Sd(-D.Y, D.X);
			Quad(Ct + D * R0 - Sd * W * .5, Ct + D * R0 + Sd * W * .5, Ct + D * R1 + Sd * W * .5, Ct + D * R1 - Sd * W * .5, Col);
		}
		void Text(const FText& T, const FVector2D& P, double Px, const FLinearColor& Col) const
		{
			FCanvasTextItem TI(P, T, Font, Col);
			// GetLargeFont is Roboto at 24 pt; the glyphs are rasterised at the scaled size, so they stay sharp
			TI.Scale = FVector2D(Px / 24.0, Px / 24.0);
			TI.bCentreX = TI.bCentreY = true;
			TI.BlendMode = SE_BLEND_Translucent;
			C->DrawItem(TI);
		}
	};

	// labels made once
	const FText& Label(const TCHAR* S)
	{
		static TMap<FString, FText> Cache;
		if (const FText* F = Cache.Find(S)) return *F;
		return Cache.Add(S, FText::AsCultureInvariant(S));
	}
	const FText& Num(int32 N)
	{
		static TMap<int32, FText> Cache;
		if (const FText* F = Cache.Find(N)) return *F;
		return Cache.Add(N, FText::AsCultureInvariant(FString::FromInt(N)));
	}

	// one dial: the values for this frame
	struct FDialIn
	{
		uint8 Kind = 0;
		int32 Index = 0, Engines = 1;
		double Value = 0;    // 0..1 along the scale (the compass: heading in degrees)
		double SpeedMax = 80;
		double Dusk = 0;
	};

	// Phase 0 the face and its markings, 1 the numerals and words, 2 the needle, hub and the glass's sheen
	void DrawDial(const FDashPaint& P, const FVector2D& Ct, double R, const FDialIn& In, int32 Phase)
	{
		using namespace KeysMesh;
		const double K = DStep(0, .6, In.Dusk);
		// backlit: white markings by day, a warm glow at dusk
		const FLinearColor Mark = FMath::Lerp(FLinearColor(.92f, .93f, .95f), FLinearColor(1.f, .86f, .62f), float(K));
		const FLinearColor Dim = Mark * .62f, Red = DSrgb(.92, .12, .08), Needle = DSrgb(1., .38, .06);
		const bool bBig = In.Kind == DashTach || In.Kind == DashSpeed || In.Kind == DashCompass;
		// the sweep: 240 degrees on the big dials, 100 on the small ones
		const double A0 = bBig ? -PI * 2 / 3 : -PI * .28, A1 = -A0;
		auto AngleOf = [&](double T) { return FMath::Lerp(A0, A1, FMath::Clamp(T, -.02, 1.02)); };
		if (In.Kind == DashCompass)
		{
			// a card that turns under a fixed lubber line: the heading reads at the top
			const double Hd = FMath::DegreesToRadians(In.Value);
			if (Phase == 0)
			{
				P.Disc(Ct, R, DSrgb(.06, .065, .075) + FLinearColor(.02f, .035f, .06f) * float(K), DSrgb(.012, .012, .016));
				if (K > .01) P.Arc(Ct, R * .45, R * .97, -PI, PI, FLinearColor(1.f, .5f, .15f, .14f * float(K)), 48, true);
				for (int32 d = 0; d < 360; d += 5)
				{
					const double A = FMath::DegreesToRadians(double(d)) - Hd;
					const bool bMajor = d % 30 == 0;
					P.Tick(Ct, A, R * (bMajor ? .74 : d % 10 == 0 ? .8 : .85), R * .93, R * (bMajor ? .03 : .014), bMajor ? Mark : Dim);
				}
				P.Tick(Ct, 0, R * .93, R * 1.0, R * .05, Needle);
			}
			else if (Phase == 1)
			{
				static const TCHAR* Card[12] = {TEXT("N"), TEXT("3"), TEXT("6"), TEXT("E"), TEXT("12"), TEXT("15"), TEXT("S"), TEXT("21"), TEXT("24"), TEXT("W"), TEXT("30"), TEXT("33")};
				for (int32 k = 0; k < 12; k++)
				{
					const double A = FMath::DegreesToRadians(30.0 * k) - Hd;
					const bool bCard = k % 3 == 0;
					P.Text(Label(Card[k]), Ct + FDashPaint::Dir(A) * R * .56, R * (bCard ? .3 : .2), k == 0 ? Needle : bCard ? Mark : Dim);
				}
				P.Text(Label(TEXT("COMPASS")), Ct + FVector2D(0, R * .2), R * .09, Dim);
			}
			else
			{
				// the lubber line and a little aircraft at the centre
				P.Tri(Ct + FVector2D(0, -R * .66), Ct + FVector2D(-R * .045, -R * .2), Ct + FVector2D(R * .045, -R * .2), Needle);
				P.Rect(Ct.X - R * .2, Ct.Y - R * .1, R * .4, R * .035, Mark * .9f);
				P.Disc(Ct, R * .05, DSrgb(.5, .5, .52), DSrgb(.2, .2, .22), 16);
			}
		}
		else if (Phase == 0)
		{
			// the face: black, a little lighter at the centre, a faint backlight ring at dusk
			P.Disc(Ct, R, DSrgb(.07, .075, .085) + FLinearColor(.015f, .03f, .05f) * float(K), DSrgb(.012, .012, .016));
			// backlit at dusk: a soft amber glow through the scale
			if (K > .01) P.Arc(Ct, R * .66, R * .97, A0 - .08, A1 + .08, FLinearColor(1.f, .5f, .15f, .16f * float(K)), 40, true);
			int32 NMaj = 4, NMin = 1;
			double Red0 = 2, Red1 = 2;
			switch (In.Kind)
			{
			case DashTach: NMaj = 7; NMin = 5; Red0 = 6.0 / 7; Red1 = 1; break;
			case DashSpeed: NMaj = FMath::RoundToInt(In.SpeedMax / 10); NMin = 2; break;
			case DashTrim: NMaj = 4; NMin = 2; break;
			case DashFuel: NMaj = 4; NMin = 2; Red0 = 0; Red1 = .12; break;
			case DashTemp: NMaj = 4; NMin = 2; Red0 = .85; Red1 = 1; break;
			case DashOil: NMaj = 4; NMin = 2; Red0 = 0; Red1 = .12; break;
			default: NMaj = 3; NMin = 2; Red0 = 0; Red1 = .1; break;
			}
			if (Red1 > Red0) P.Arc(Ct, R * (bBig ? .86 : .8), R * .93, AngleOf(Red0), AngleOf(Red1), Red, 16);
			for (int32 k = 0; k <= NMaj * NMin; k++)
			{
				const bool bMaj = k % NMin == 0;
				const double A = AngleOf(double(k) / (NMaj * NMin));
				P.Tick(Ct, A, R * (bMaj ? .74 : .82), R * .93, R * (bMaj ? .035 : .016), bMaj ? Mark : Dim);
			}
		}
		else if (Phase == 1)
		{
			switch (In.Kind)
			{
			case DashTach:
			{
				for (int32 k = 0; k <= 7; k++) P.Text(Num(k), Ct + FDashPaint::Dir(AngleOf(k / 7.0)) * R * .55, R * .28, Mark);
				P.Text(Label(TEXT("RPM x1000")), Ct + FVector2D(0, R * .5), R * .12, Dim);
				if (In.Engines > 1)
				{
					static const TCHAR* Two[2] = {TEXT("PORT"), TEXT("STBD")};
					static const TCHAR* Three[3] = {TEXT("PORT"), TEXT("CTR"), TEXT("STBD")};
					static const TCHAR* Many[6] = {TEXT("ENG 1"), TEXT("ENG 2"), TEXT("ENG 3"), TEXT("ENG 4"), TEXT("ENG 5"), TEXT("ENG 6")};
					const TCHAR* L = In.Engines == 2 ? Two[In.Index] : In.Engines == 3 ? Three[In.Index] : Many[FMath::Clamp(In.Index, 0, 5)];
					P.Text(Label(L), Ct + FVector2D(0, -R * .3), R * .13, Dim);
				}
				break;
			}
			case DashSpeed:
			{
				const int32 NMaj = FMath::RoundToInt(In.SpeedMax / 10);
				for (int32 k = 0; k <= NMaj; k++) P.Text(Num(k * 10), Ct + FDashPaint::Dir(AngleOf(double(k) / NMaj)) * R * .56, R * (NMaj > 8 ? .19 : .23), Mark);
				// miles an hour on an inner scale
				const double MphMax = In.SpeedMax * 1.15078;
				for (int32 m = 0; m <= MphMax; m += 20)
					if (m > 0) P.Text(Num(m), Ct + FDashPaint::Dir(AngleOf(m / MphMax)) * R * .34, R * .095, FLinearColor(Dim.R, Dim.G * .85f, Dim.B * .6f));
				P.Text(Label(TEXT("KNOTS")), Ct + FVector2D(0, R * .5), R * .13, Dim);
				P.Text(Label(TEXT("MPH")), Ct + FVector2D(0, R * .66), R * .08, FLinearColor(Dim.R, Dim.G * .85f, Dim.B * .6f));
				break;
			}
			default:
			{
				const TCHAR* Lo = TEXT(""), *Hi = TEXT(""), *Title = TEXT("");
				switch (In.Kind)
				{
				case DashTrim: Lo = TEXT("DN"); Hi = TEXT("UP"); Title = TEXT("TRIM"); break;
				case DashFuel: Lo = TEXT("E"); Hi = TEXT("F"); Title = TEXT("FUEL"); break;
				case DashTemp: Lo = TEXT("100"); Hi = TEXT("220"); Title = TEXT("TEMP F"); break;
				case DashOil: Lo = TEXT("0"); Hi = TEXT("80"); Title = TEXT("OIL PSI"); break;
				default: Lo = TEXT("10"); Hi = TEXT("16"); Title = TEXT("VOLTS"); break;
				}
				P.Text(Label(Lo), Ct + FDashPaint::Dir(A0) * R * .52 + FVector2D(0, R * .1), R * .25, Mark);
				P.Text(Label(Hi), Ct + FDashPaint::Dir(A1) * R * .52 + FVector2D(0, R * .1), R * .25, Mark);
				if (In.Kind == DashFuel) P.Text(Label(TEXT("1/2")), Ct + FVector2D(0, -R * .5), R * .15, Dim);
				P.Text(Label(Title), Ct + FVector2D(0, R * .44), R * .21, Mark);
				if (In.Kind == DashTrim && In.Engines > 1)
				{
					static const TCHAR* Two[2] = {TEXT("P"), TEXT("S")};
					static const TCHAR* Three[3] = {TEXT("P"), TEXT("C"), TEXT("S")};
					static const TCHAR* Many[6] = {TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"), TEXT("6")};
					const TCHAR* L = In.Engines == 2 ? Two[In.Index] : In.Engines == 3 ? Three[In.Index] : Many[FMath::Clamp(In.Index, 0, 5)];
					P.Text(Label(L), Ct + FVector2D(0, R * .66), R * .15, Dim);
				}
				break;
			}
			}
		}
		else
		{
			// the needle: tapered, with a counterweight tail, over a soft shadow; a chrome-capped hub
			const double A = AngleOf(In.Value);
			const FVector2D D = FDashPaint::Dir(A), Sd(-D.Y, D.X), Sh(R * .025, R * .04);
			const double Tip = R * (bBig ? .9 : .86), Tail = R * .2, W0 = R * .045, W1 = R * .008;
			for (int32 Pass = 0; Pass < 2; Pass++)
			{
				const FVector2D O = Pass == 0 ? Sh : FVector2D::ZeroVector;
				const FLinearColor Col = Pass == 0 ? FLinearColor(0, 0, 0, .45f) : Needle;
				const bool bBlend = Pass == 0;
				P.Quad(Ct + O - Sd * W0, Ct + O + Sd * W0, Ct + O + D * Tip + Sd * W1, Ct + O + D * Tip - Sd * W1, Col, bBlend);
				P.Quad(Ct + O - Sd * W0, Ct + O - D * Tail - Sd * W0 * .8, Ct + O - D * Tail + Sd * W0 * .8, Ct + O + Sd * W0, Col, bBlend);
			}
			P.Disc(Ct, R * .11, DSrgb(.62, .63, .66), DSrgb(.22, .22, .24), 20);
			P.Disc(Ct, R * .07, DSrgb(.08, .08, .09), DSrgb(.03, .03, .035), 16);
		}
		if (Phase == 2)
		{
			// the glass: a faint reflection across its upper left
			const FLinearColor S0(1, 1, 1, .1f), S1(1, 1, 1, 0);
			for (int32 k = 0; k < 16; k++)
			{
				const double Aa = FMath::Lerp(-2.3, .3, k / 16.0), Ab = FMath::Lerp(-2.3, .3, (k + 1) / 16.0);
				const double Fa = FMath::Sin(PI * k / 16.0), Fb = FMath::Sin(PI * (k + 1) / 16.0);
				P.Tri(Ct + FDashPaint::Dir(Aa) * R * .97, Ct + FDashPaint::Dir(Ab) * R * .97, Ct + FDashPaint::Dir(Ab) * R * (.97 - .28 * Fb), S0 * float(Fa + .1), S0 * float(Fb + .1), S1, true);
				P.Tri(Ct + FDashPaint::Dir(Aa) * R * .97, Ct + FDashPaint::Dir(Ab) * R * (.97 - .28 * Fb), Ct + FDashPaint::Dir(Aa) * R * (.97 - .28 * Fa), S0 * float(Fa + .1), S1, S1, true);
			}
		}
	}
}

bool AKeysBoatPawn::ThrottleGrip(FVector& Out) const
{
	if (DashFor.IsEmpty()) return false;
	// the grip at the top of the levers, turned with them
	const FQuat Q(FVector::YAxisVector, LeverA);
	Out = DashL.Pivot + Q.RotateVector(FVector(-.022, 0, DashL.LeverLen + .01));
	return true;
}

void AKeysBoatPawn::UpdateDash(double Dt, double T)
{
	const KeysData::FBoatSpec& S = Spec();
	if (!DashFaces)
	{
		auto Make = [this](const TCHAR* Name)
		{
			UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(this, FName(Name));
			C->SetupAttachment(GetRootComponent());
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(false);
			C->RegisterComponent();
			return C;
		};
		DashFaces = Make(TEXT("DashFaces"));
		ThrottleMesh = Make(TEXT("ThrottleLevers"));
		ThrottleMesh->SetCastShadow(true);
		auto MakeRT = [this](int32 RW, int32 RH)
		{
			UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this);
			RT->RenderTargetFormat = RTF_RGBA8_SRGB;
			RT->bForceLinearGamma = true;   // the canvas writes linear colour; the sRGB target stores it
			RT->bAutoGenerateMips = true;
			RT->MipsSamplerFilter = TF_Trilinear;
			RT->ClearColor = FLinearColor::Black;
			RT->InitAutoFormat(RW, RH);
			RT->UpdateResourceImmediate(true);
			return RT;
		};
		GaugeRT = MakeRT(GaugeW, GaugeH);
		ScreenRT = MakeRT(ScreenW, ScreenH);
		if (UMaterialInterface* DM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KeysRun/Materials/M_KR_Dash.M_KR_Dash")))
		{
			GaugeMID = UMaterialInstanceDynamic::Create(DM, this);
			GaugeMID->SetTextureParameterValue(TEXT("Tex"), GaugeRT);
			DashScreenMID = UMaterialInstanceDynamic::Create(DM, this);
			DashScreenMID->SetTextureParameterValue(TEXT("Tex"), ScreenRT);
		}
		else UE_LOG(LogTemp, Warning, TEXT("Keys Run: M_KR_Dash is missing - restart the editor once so Content/Python/init_unreal.py can create it."));
	}
	if (DashFor != S.Id)
	{
		// a new boat: lay its panel out and build the faces and the levers
		DashFor = S.Id;
		DashL = KeysMesh::DashLayout(S);
		FuelK = .9;
		FKeysMesh G, Sc;
		for (const KeysMesh::FDashGauge& Gg : DashL.Gauges)
		{
			double CX, CY, Sz;
			GaugeCell(Gg.Kind, Gg.Index, CX, CY, Sz);
			const int32 Seg = 40;
			const FVector Ctr = DashL.P(Gg.U, Gg.V, .0045);
			const FVector2D UC((CX + Sz * .5) / GaugeW, (CY + Sz * .5) / GaugeH);
			for (int32 k = 0; k < Seg; k++)
			{
				const double A0 = 2 * PI * k / Seg, A1 = 2 * PI * (k + 1) / Seg;
				const int32 From = G.V.Num();
				G.AddTri(Ctr, DashL.P(Gg.U + FMath::Cos(A0) * Gg.R, Gg.V + FMath::Sin(A0) * Gg.R, .0045), DashL.P(Gg.U + FMath::Cos(A1) * Gg.R, Gg.V + FMath::Sin(A1) * Gg.R, .0045), FLinearColor::White, DashL.N);
				// AddTri keeps the corner order (it flips only the indices), so the UVs follow the corners
				G.UV[From] = UC;
				G.UV[From + 1] = UC + FVector2D(FMath::Cos(A0) * Sz * .5 / GaugeW, -FMath::Sin(A0) * Sz * .5 / GaugeH);
				G.UV[From + 2] = UC + FVector2D(FMath::Cos(A1) * Sz * .5 / GaugeW, -FMath::Sin(A1) * Sz * .5 / GaugeH);
			}
		}
		for (const KeysMesh::FDashScreen& Sn : DashL.Screens)
		{
			const double U0 = Sn.U - Sn.W * .5, U1 = Sn.U + Sn.W * .5, V0 = Sn.V - Sn.H * .5, V1 = Sn.V + Sn.H * .5;
			const int32 From = Sc.V.Num();
			Sc.AddQuad(DashL.P(U0, V0, .0112), DashL.P(U1, V0, .0112), DashL.P(U1, V1, .0112), DashL.P(U0, V1, .0112), FLinearColor::White, DashL.N);
			const double X0 = double(Sn.Kind * ScrCellW) / ScreenW, X1 = double((Sn.Kind + 1) * ScrCellW) / ScreenW, Y1 = double(ScrCellH) / ScreenH;
			Sc.UV[From] = FVector2D(X0, Y1);
			Sc.UV[From + 1] = FVector2D(X1, Y1);
			Sc.UV[From + 2] = FVector2D(X1, 0);
			Sc.UV[From + 3] = FVector2D(X0, 0);
		}
		G.Commit(DashFaces, 0, GaugeMID ? (UMaterialInterface*)GaugeMID : AKeysWorld::GlowMaterial());
		Sc.Commit(DashFaces, 1, DashScreenMID ? (UMaterialInterface*)DashScreenMID : AKeysWorld::GlowMaterial());
		FKeysMesh Lv;
		KeysMesh::BuildThrottleLevers(Lv, DashL);
		Lv.Commit(ThrottleMesh, 0, AKeysWorld::VertexColorMaterial());
		LeverA = 0;
		GaugeT = DashScrT = 0;
		if (bScriptLog)
		{
			const FVector Pc = DashL.P(0, DashL.H * .5);
			UE_LOG(LogTemp, Log, TEXT("KRScript dash %s: panel %.2f x %.2f centre %.2f %.2f %.2f, eye %.2f %.2f %.2f pitch %.2f, %d gauges"), *S.Id, DashL.W, DashL.H, Pc.X, Pc.Y, Pc.Z, DashL.Eye.X, DashL.Eye.Y, DashL.Eye.Z, DashL.Pitch, DashL.Gauges.Num());
		}
	}

	// ---- the throttle levers follow the throttle, through the detent
	LeverA += (KeysMesh::ThrottleAngle(Thr, Gear) - LeverA) * FMath::Min(1.0, Dt * 10);
	ThrottleMesh->SetRelativeLocationAndRotation(DashL.Pivot * 100.0, FQuat(FVector::YAxisVector, LeverA));

	// ---- what the engines are doing
	const AKeysAudio* Aud = AKeysAudio::Get(this);
	const bool bOn = bRunning && (!Aud || Aud->EnginesOn());
	const double F = FMath::Min(1.0, KnotsNow() / FMath::Max(1.0, S.Top));
	double Rpm = Aud ? Aud->EngineRpm() : 700 + 5700 * FMath::Clamp(FMath::Abs(Thr) * .82 + F * .25, 0.0, 1.1);
	if (!bOn) Rpm = 0;
	TachRpm += (Rpm - TachRpm) * FMath::Min(1.0, Dt * (bOn ? 6 : 2.5));
	const double Load = FMath::Clamp(FMath::Abs(Thr) * .7 + F * .3, 0.0, 1.0);
	// no fuel dock in the game, so the tank drains slowly with the throttle and never runs dry
	if (bOn) FuelK = FMath::Max(.22, FuelK - Dt * (.00002 + .00012 * Thr * Thr));
	WaterK += ((bOn ? .5 + .07 * Load : .08) - WaterK) * FMath::Min(1.0, Dt * .05);

	// ---- redraw: every frame while you're at the helm or close by, slowly from further off, not at all from afar
	const double Dusk = AKeysWorld::Dusk(this);
	const FVector DashW = GetActorTransform().TransformPosition(DashL.P(0, DashL.H * .5) * 100.0);
	const double Dist = Camera ? FVector::Dist(Camera->GetComponentLocation(), DashW) / 100.0 : 0;
	const bool bNear = bDriveOn || Dist < 6, bSeen = bNear || Dist < 30;
	if (GaugeMID) GaugeMID->SetScalarParameterValue(TEXT("Bright"), float(.95 + .35 * Dusk));
	if (DashScreenMID) DashScreenMID->SetScalarParameterValue(TEXT("Bright"), float(.85 + .25 * Dusk));
	bDashChart = bSeen && bRunning;
	GaugeT -= Dt;
	DashScrT -= Dt;
	if (!bSeen) return;
	if (GaugeRT && GaugeT <= 0)
	{
		GaugeT = bNear ? 0 : .2;
		UCanvas* Cv = nullptr;
		FVector2D Size;
		FDrawToRenderTargetContext Ctx;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, GaugeRT, Cv, Size, Ctx);
		if (Cv && Cv->Canvas)
		{
			FDashPaint P;
			P.C = Cv->Canvas;
			P.Hit = P.C->GetHitProxyId();
			P.Font = GEngine->GetLargeFont();
			const int32 E = FMath::Clamp(S.Engines, 1, 6);
			const double SpeedMax = FMath::Clamp(FMath::CeilToDouble(S.Top * 1.12 / 10) * 10, 50.0, 100.0);
			for (int32 Phase = 0; Phase < 3; Phase++)
				for (const KeysMesh::FDashGauge& Gg : DashL.Gauges)
				{
					double CX, CY, Sz;
					GaugeCell(Gg.Kind, Gg.Index, CX, CY, Sz);
					FDialIn In;
					In.Kind = Gg.Kind;
					In.Index = Gg.Index;
					In.Engines = E;
					In.SpeedMax = SpeedMax;
					In.Dusk = Dusk;
					const double Wob = FMath::Sin(T * 1.3 + Gg.Index * 1.9) + .5 * FMath::Sin(T * 3.1 + Gg.Index);
					switch (Gg.Kind)
					{
					case KeysMesh::DashTach: In.Value = TachRpm > 1 ? (TachRpm + (Gg.Index - (E - 1) * .5) * 15 + Wob * (10 + TachRpm * .004)) / 7000 : 0; break;
					case KeysMesh::DashSpeed: In.Value = KnotsNow() / SpeedMax; break;
					case KeysMesh::DashCompass: In.Value = -H * 180 / PI; break;
					case KeysMesh::DashTrim: In.Value = TrimV + Gg.Index * .006 - (E - 1) * .003; break;
					case KeysMesh::DashFuel: In.Value = FuelK; break;
					case KeysMesh::DashTemp: In.Value = WaterK + Wob * .003; break;
					case KeysMesh::DashOil: In.Value = bOn ? .42 + .3 * FMath::Min(1.0, TachRpm / 6000) + Wob * .004 : 0; break;
					default: In.Value = bOn ? (14.3 - 10) / 6 + Wob * .003 : (12.6 - 10) / 6; break;
					}
					DrawDial(P, FVector2D(CX + Sz * .5, CY + Sz * .5), Sz * .5, In, Phase);
				}
		}
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Ctx);
		GaugeRT->UpdateResourceImmediate(false);   // the mips
	}
	if (ScreenRT && DashScrT <= 0)
	{
		DashScrT = bNear ? .1 : .5;
		UCanvas* Cv = nullptr;
		FVector2D Size;
		FDrawToRenderTargetContext Ctx;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, ScreenRT, Cv, Size, Ctx);
		if (Cv && Cv->Canvas)
		{
			APlayerController* PC = Cast<APlayerController>(GetController());
			AKeysHUD* Hud = PC ? Cast<AKeysHUD>(PC->GetHUD()) : nullptr;
			FDashPaint P;
			P.C = Cv->Canvas;
			P.Hit = P.C->GetHitProxyId();
			P.Font = GEngine->GetLargeFont();
			if (Hud && bRunning) Hud->DrawHelmScreens(Cv, this, ScrCellW, ScrCellH);
			else P.Rect(0, 0, ScreenW, ScrCellH, DSrgb(.02, .03, .05));
			// the glass: a soft diagonal reflection across each screen
			for (int32 k = 0; k < 2; k++)
			{
				const double X0 = k * ScrCellW;
				const FLinearColor S0(1, 1, 1, .07f), S1(1, 1, 1, 0);
				P.Tri(FVector2D(X0, 0), FVector2D(X0 + ScrCellW * .55, 0), FVector2D(X0, ScrCellH * .9), S0, S1, S1, true);
			}
		}
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Ctx);
		ScreenRT->UpdateResourceImmediate(false);
	}
}
