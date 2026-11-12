#include "KeysDog.h"
#include "KeysKit.h"
#include "KeysMesh.h"

namespace KeysDog
{
	const FLook& Look(const FString& Breed)
	{
		static const TMap<FString, FLook> Looks = {
			{TEXT("terrier"), {0xf2ece0, 0xb5803f, 0xb5803f, 0x26221f, 1, 1, 1, false, true, 0}},
			{TEXT("lab"), {0xd8b068, 0xd8b068, 0xc4994f, 0x1d1a17, 1.75, 1.05, 1.05, false, false, 0}},
			{TEXT("beagle"), {0xf4efe4, 0x8a5a2e, 0x5f3d20, 0x1d1a17, 1.25, 1, .88, false, true, .35}},
			{TEXT("dachshund"), {0x7a4a2a, 0x7a4a2a, 0x55301a, 0x1d1a17, 1.15, 1.6, .5, false, false, 0}},
			{TEXT("husky"), {0xe6e8ea, 0x5f656b, 0x5f656b, 0x1d1a17, 1.8, 1.05, 1.1, true, true, 1.1}},
			{TEXT("blacklab"), {0x2f2b29, 0x2f2b29, 0x2f2b29, 0x0d0c0b, 1.8, 1.08, 1.08, false, false, 0}},
			{TEXT("mutt"), {0xb0875a, 0x6e5238, 0x6e5238, 0x26221f, 1.4, 1.1, 1, false, true, .2}}};
		const FLook* F = Looks.Find(Breed);
		return F ? *F : Looks[TEXT("terrier")];
	}

	void Build(FKeysMesh& M, const FLook& L, const FPose& P, const FTransform& Xf, double Lift)
	{
		using namespace KeysKit;
		const double H = ShoulderH, B = L.Body, Dy = -H * .5 * (1 - L.Leg), LegK = L.Leg;
		auto Col = [Lift](uint32 C) { FLinearColor X = Hex(C) * float(Lift); X.A = 1; return X; };
		const bool bSit = P.Act == EAct::Sit && !P.bTrotting, bLie = P.Act == EAct::Lie, bSniff = P.Act == EAct::Sniff && !P.bTrotting, bBrace = P.Act == EAct::Brace;
		const double Crouch = bBrace ? H * .22 : bSit ? H * .16 * LegK : bLie ? H * .36 * LegK : 0;
		const double BodyPitch = bSit ? -.3 : bBrace ? .16 : bSniff ? .1 : 0;   // web rotation.x: + tips the nose down
		// the dog's own frame from the web one (x side, y up, z nose) to Unreal (X nose, Y side, Z up), then scaled and placed
		const double S = L.Scale;
		const FQuat Q0 = Xf.GetRotation();
		auto Place = [&](const FVector& Web) { return Xf.TransformPosition(FVector(Web.Z, Web.X, Web.Y) * S); };
		auto Box = [&](const FVector& WebC, double W, double Hh, double D, uint32 C, const FQuat& Rot)
		{
			M.AddBox(Place(WebC), FVector(D, W, Hh) * S, Col(C), Q0 * Rot);
		};
		// body group: pivots at the origin, dropped by the crouch and tipped by BodyPitch
		const FQuat QBody(FVector::YAxisVector, BodyPitch);
		auto BodyPt = [&](double X, double Y, double Z) { const FVector U = QBody.RotateVector(FVector(Z, X, Y)); return FVector(U.Y, U.Z - Crouch, U.X); };
		Box(BodyPt(0, H * .72 + Dy, 0), H * .52, H * .48, H * .95 * B, L.Coat, QBody);
		if (L.bSaddle) Box(BodyPt(0, H * .84 + Dy, -H * .12 * B), H * .54, H * .26, H * .42 * B, L.Mark, QBody);
		Box(BodyPt(0, H * .66 + Dy, H * .5 * B), H * .46, H * .36, H * .3, L.Coat, QBody);
		// head
		double HeadNod, HeadYaw;
		if (bBrace) { HeadNod = -.25 + P.Lean; HeadYaw = 0; }
		else if (bSniff) { HeadNod = .85 + FMath::Sin(P.T * 5 + P.Ph) * .07; HeadYaw = FMath::Sin(P.T * 1.3 + P.Ph) * .45; }
		else if (bLie) { HeadNod = .28 + FMath::Sin(P.T * .5 + P.Ph) * .05; HeadYaw = FMath::Sin(P.T * .31 + P.Ph) * .4; }
		else { HeadNod = FMath::Sin(P.T * .8 + P.Ph) * .08 + P.Lean; HeadYaw = FMath::Sin(P.T * .47 + P.Ph) * .5; }
		const FVector HeadAt = BodyPt(0, H * .95 + Dy, H * .56 * B);
		const FQuat QHead = QBody * FQuat(FVector::ZAxisVector, HeadYaw) * FQuat(FVector::YAxisVector, HeadNod);
		auto HeadPt = [&](double X, double Y, double Z) { const FVector U = QHead.RotateVector(FVector(Z, X, Y)); return HeadAt + FVector(U.Y, U.Z, U.X); };
		Box(HeadPt(0, 0, 0), H * .38, H * .34, H * .36, L.Coat, QHead);
		Box(HeadPt(0, -H * .06, H * .26), H * .2, H * .17, H * .22, L.Coat, QHead);
		Box(HeadPt(0, -H * .04, H * .39), H * .1, H * .09, H * .06, L.Nose, QHead);
		for (int32 sx = -1; sx <= 1; sx += 2) Box(HeadPt(sx * H * .1, H * .03, H * .17), H * .06, H * .06, H * .03, 0x1a1714, QHead);
		const double Back = bBrace ? 1 : P.bExcited ? -.5 : 0;
		for (int32 sx = -1; sx <= 1; sx += 2)
		{
			const double EarRot = .1 + Back * .9 + FMath::Sin(P.T * 2.3 + sx) * .05;
			const FQuat QEar = QHead * FQuat(FVector::YAxisVector, -EarRot * .5);
			if (L.bPrick) Box(HeadPt(sx * H * .15, H * .16 + H * .1, -H * .02), H * .08, H * .22, H * .05, L.Ear, QEar);
			else Box(HeadPt(sx * H * .2, H * .1, -H * .02), H * .05, H * .16, H * .1, L.Ear, QEar);
		}
		// tail: wags, tucks when braced, curls over the back if the breed does
		const FVector TailAt = BodyPt(0, H * .86 + Dy, -H * .46 * B);
		const double TailX = bBrace ? .9 : -.5 - L.TailUp, TailY = bBrace ? 0 : FMath::Sin(P.T * (8 + 14 * P.Wag) + P.Ph) * .7 * P.Wag;
		const FQuat QTail = QBody * FQuat(FVector::ZAxisVector, TailY) * FQuat(FVector::YAxisVector, TailX);
		{
			const FVector U = QTail.RotateVector(FVector(0, 0, H * .14));
			Box(TailAt + FVector(U.Y, U.Z, U.X), H * .09, H * .3, H * .09, L.Coat, QTail);
		}
		// legs
		int32 i = 0;
		for (int32 sx = -1; sx <= 1; sx += 2)
			for (int32 sz = -1; sz <= 1; sz += 2, i++)
			{
				const double Swing = P.bTrotting ? FMath::Sin(P.Step + (i < 2 ? 0 : PI) + (sz > 0 ? PI : 0)) * .22 : 0;
				const double Z = sz * H * .3 * B + Swing * H + (bLie && sz > 0 ? H * .22 : 0);
				const double Y = H * .25 * LegK - Crouch + (P.bTrotting ? FMath::Max(0.0, FMath::Cos(P.Step + i)) * H * .04 : 0);
				const double RX = bSit && sz < 0 ? .9 : 0, RZ = bBrace ? (i < 2 ? -.18 : .18) : bLie ? (i < 2 ? -.7 : .7) : 0;
				const FQuat QLeg = FQuat(FVector::YAxisVector, RX) * FQuat(FVector::XAxisVector, RZ);
				Box(FVector(sx * H * .17, FMath::Max(Y, H * .25 * LegK * .4), Z), H * .12, H * .5 * LegK, H * .13, L.Coat, QLeg);
			}
	}
}
