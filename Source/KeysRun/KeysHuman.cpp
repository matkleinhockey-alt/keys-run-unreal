#include "KeysHuman.h"
#include "KeysMesh.h"

namespace KeysShapes
{
	void AddBlob(FKeysMesh& M, const FVector& Center, const FVector& Radii, const FQuat& Rot, const FLinearColor& Color, int32 NU, int32 NV, double VMin)
	{
		NU = FMath::Max(3, NU);
		NV = FMath::Max(2, NV);
		const double A0 = FMath::Clamp(VMin, -1.0, 1.0) * PI * .5, A1 = PI * .5;
		auto P = [&](int32 i, int32 j)
		{
			const double Lon = 2 * PI * i / NU, Lat = FMath::Lerp(A0, A1, double(j) / NV);
			const FVector L(FMath::Cos(Lat) * FMath::Cos(Lon) * Radii.X, FMath::Cos(Lat) * FMath::Sin(Lon) * Radii.Y, FMath::Sin(Lat) * Radii.Z);
			return Center + Rot.RotateVector(L);
		};
		for (int32 j = 0; j < NV; j++)
			for (int32 i = 0; i < NU; i++)
			{
				const FVector A = P(i, j), B = P(i + 1, j), C = P(i + 1, j + 1), D = P(i, j + 1);
				const FVector Out = (A + C) * .5 - Center;
				if (j == NV - 1) M.AddTri(A, B, C, Color, Out);   // the top ring closes to a point
				else M.AddQuad(A, B, C, D, Color, Out);
			}
	}

	void SolveIK(const FVector& S, const FVector& Target, double L1, double L2, const FVector& Pole, FVector& OutElbow, FVector& OutEnd)
	{
		const FVector D = Target - S;
		double Len = FMath::Min(D.Size(), L1 + L2 - .003);
		if (Len < .01) Len = .01;
		FVector Dir = D.GetSafeNormal();
		if (Dir.IsNearlyZero()) Dir = FVector(0, 0, -1);
		const double A1 = (L1 * L1 - L2 * L2 + Len * Len) / (2 * Len);
		const double HH = FMath::Sqrt(FMath::Max(0.0, L1 * L1 - A1 * A1));
		FVector PP = Pole - Dir * FVector::DotProduct(Pole, Dir);
		PP = PP.GetSafeNormal();
		OutElbow = S + Dir * A1 + PP * HH;
		OutEnd = S + Dir * Len;
	}
}

using namespace KeysShapes;

void FKeysHuman::Init(const FKeysHumanLook& InLook, bool bInSeated)
{
	Look = InLook;
	bSeated = bInSeated;
	HipY = bSeated ? .48 : (Look.bBikini ? .96 : .93);
	Ph = FMath::FRand() * 10;
	SipT = FMath::FRandRange(.6, 2.0);
	DefaultPose();
}

void FKeysHuman::DefaultPose()
{
	PoseLegs(J(-.105, .085, 0), J(.105, .085, 0));
	if (bSeated) Pose(J(-.16, HipY + .08, .34), J(.16, HipY + .08, .34));
	else Pose(J(-.24, HipY + .02, .05), J(.24, HipY + .02, .05));
}

void FKeysHuman::IdleUpdate(double T, double BoatSpeedMs)
{
	HeadYaw = FMath::Sin(T * .27 + Ph) * .35 + FMath::Sin(T * .71 + Ph * 2) * .1;
	if (!bHoldHead) HeadNod = FMath::Sin(T * .4 + Ph) * .05;
	Wind = FMath::Min(1.0, FMath::Abs(BoatSpeedMs) / 22);
}

FTransform FKeysHuman::ToBoat() const
{
	return FTransform(FRotator(FMath::RadiansToDegrees(Pitch), FMath::RadiansToDegrees(Yaw), FMath::RadiansToDegrees(Roll)), Pos);
}

void FKeysHuman::Build(FKeysMesh& M) const
{
	if (!bVisible) return;
	const FTransform X = ToBoat();
	const FQuat Q = X.GetRotation();
	auto W = [&X](const FVector& P) { return X.TransformPosition(P); };
	const bool bFem = Look.bBikini;
	const FLinearColor Skin = Look.Skin, Bottom = Look.Bottom, Hair = Look.Hair;
	const FLinearColor Top = bFem ? Skin : Look.Shirt;
	const FLinearColor Dark(FColor(0x14, 0x16, 0x1a));
	const FLinearColor Lip = FMath::Lerp(Skin, FLinearColor(FColor(0xb3, 0x50, 0x5a)), bFem ? .62f : .45f);

	// legs: hip, knee, ankle, foot
	for (int32 i = 0; i < 2; i++)
	{
		const double Sx = i == 0 ? -1 : 1;
		const FVector Hip = J(Sx * .095, HipY, 0);
		FVector Knee, Ankle;
		if (bSeated) { Knee = J(Sx * .1, HipY + .03, .42); Ankle = J(Sx * .11, .09, .47); }
		else SolveIK(Hip, i == 0 ? FootL : FootR, .43, .42, FVector(1, 0, .1), Knee, Ankle);
		M.AddTube(W(Hip), W(Knee), bFem ? .096 : .085, .06, 7, Look.bShortsLong ? Bottom : Skin);
		AddBlob(M, W(Knee), FVector(.06), Q, Skin, 6, 3);
		M.AddTube(W(Knee), W(Ankle), .058, .04, 7, Skin);
		AddBlob(M, W(Ankle + FVector(.07, 0, -.035)), FVector(.105, .0425, .031), Q, Look.bShoes ? Look.Shoes : Skin, 6, 3);
	}

	// hips and a lathed torso, colored in bands like the JS vertex colors
	AddBlob(M, W(FVector(0, 0, HipY + .05)), FVector(.17 * (bFem ? .86 : .78), .17 * (bFem ? 1.12 : 1.03), .17 * .72), Q, Bottom, 10, 5);
	{
		static const double ProfM[10][2] = {{.155, 0}, {.165, .07}, {.148, .18}, {.135, .25}, {.15, .33}, {.168, .41}, {.172, .46}, {.155, .52}, {.09, .57}, {.05, .6}};
		static const double ProfF[10][2] = {{.168, 0}, {.172, .06}, {.14, .17}, {.116, .25}, {.128, .31}, {.148, .38}, {.15, .44}, {.136, .5}, {.085, .56}, {.05, .6}};
		const double (*Prof)[2] = bFem ? ProfF : ProfM;
		const int32 NA = 10;
		const double Base = HipY + .03;
		auto P = [&](int32 k, int32 a)
		{
			const double Ang = 2 * PI * a / NA, R = Prof[k][0];
			return FVector(FMath::Cos(Ang) * R * .66, FMath::Sin(Ang) * R, Base + Prof[k][1]);
		};
		for (int32 k = 0; k < 9; k++)
		{
			const double H = (Prof[k][1] + Prof[k + 1][1]) * .5;
			FLinearColor Col = Top;
			if (H < .06) Col = Bottom;
			if (bFem && ((H > .35 && H < .45) || H < .08)) Col = Bottom;
			for (int32 a = 0; a < NA; a++)
			{
				const FVector A = P(k, a), B = P(k, a + 1), C = P(k + 1, a + 1), D = P(k + 1, a);
				const FVector Mid = (A + C) * .5;
				M.AddQuad(W(A), W(B), W(C), W(D), Col, Q.RotateVector(FVector(Mid.X, Mid.Y, 0)));
			}
		}
	}
	if (bFem)
		for (int32 i = 0; i < 2; i++)
		{
			const double Sx = i == 0 ? -1 : 1;
			AddBlob(M, W(J(Sx * .062, HipY + .43, .082)), FVector(.072, .078, .074), Q, Bottom, 7, 4);
		}
	M.AddTube(W(J(0, HipY + .58, 0)), W(J(0, HipY + .72, 0)), bFem ? .041 : .048, bFem ? .038 : .044, 7, Skin);

	// arms solved with two-bone IK, as in the JS
	FVector HandPos[2];
	FVector HandDir[2];
	const double FA = bFem ? .85 : 1;
	for (int32 i = 0; i < 2; i++)
	{
		const double Sx = i == 0 ? -1 : 1;
		const FVector S = J(Sx * (bFem ? .175 : .19), HipY + .53, 0);
		AddBlob(M, W(J(Sx * (bFem ? .168 : .185), HipY + .53, 0)), FVector(.06, .065, .062) * (bFem ? .86 : 1), Q, Top, 6, 4);
		FVector E, Hn;
		SolveIK(S, i == 0 ? HandL : HandR, .29, .27, FVector(-.55, Sx * .45, -.8), E, Hn);
		const FVector Dir = (Hn - S).GetSafeNormal();
		M.AddTube(W(S), W(E), .046 * FA, .038 * FA, 7, Top);
		M.AddTube(W(E), W(Hn), .036 * FA, .029 * FA, 7, bFem ? Skin : Top);
		AddBlob(M, W(E), FVector(.038 * FA), Q, Top, 6, 3);
		HandPos[i] = Hn + Dir * .035;
		HandDir[i] = Dir;
		AddBlob(M, W(HandPos[i]), FVector(.045 * FA, .03 * FA, .05 * FA), Q * FQuat::FindBetweenNormals(FVector::ZAxisVector, Dir.IsNearlyZero() ? FVector::ZAxisVector : Dir), Skin, 6, 3);
	}

	// beer can in the right hand: raised, and tipped back for a pull
	if (bBeer)
	{
		const double A = 2.1 * SipAmt;
		const FVector Axis(-FMath::Sin(A), 0, FMath::Cos(A));
		const FVector C = HandPos[1] + FVector(0, 0, .04);
		M.AddTube(W(C - Axis * .06), W(C + Axis * .035), .033, .033, 8, FLinearColor(FColor(0xc9, 0xcc, 0xd2)));
		M.AddTube(W(C + Axis * .035), W(C + Axis * .06), .034, .034, 8, FLinearColor(FColor(0x2f, 0x6f, 0xd0)));
	}

	// head
	const double HS = bFem ? .94 : 1;
	const FQuat HR = FQuat(FRotator(FMath::RadiansToDegrees(-HeadNod), FMath::RadiansToDegrees(HeadYaw), 0));
	const FVector HP = J(0, HipY + .77, 0);
	auto HW = [&](const FVector& P) { return W(HP + HR.RotateVector(P * HS)); };
	const FQuat HQ = Q * HR;
	AddBlob(M, HW(FVector(0, 0, .02)), FVector(.1, .093, .11) * HS, HQ, Skin, 10, 6);
	AddBlob(M, HW(FVector(.022, 0, -.045)), (bFem ? FVector(.071, .065, .058) : FVector(.075, .075, .062)) * HS, HQ, Skin, 8, 4);
	M.AddBox(HW(FVector(.1, 0, 0)), FVector(.04, .022, .03) * HS, Skin, HQ);
	M.AddBox(HW(FVector(.088, 0, -.052)), FVector(.01, .036, .009) * HS, Lip, HQ);
	for (int32 i = 0; i < 2; i++)
	{
		const double Sx = i == 0 ? -1 : 1;
		AddBlob(M, HW(FVector(-.005, Sx * .092, 0)), FVector(.018, .011, .024) * HS, HQ, Skin, 5, 3);
		if (!Look.bGlasses)
		{
			AddBlob(M, HW(FVector(.083, Sx * .034, .022)), FVector(.007, .014, .0105) * HS, HQ, FLinearColor(FColor(0xf4, 0xf1, 0xec)), 6, 3);
			AddBlob(M, HW(FVector(.09, Sx * .034, .022)), FVector(.004, .007, .007) * HS, HQ, Dark, 5, 3);
		}
		M.AddBox(HW(FVector(.09, Sx * .036, .046)), FVector(.008, .034, .006) * HS, Hair, HQ);
	}
	if (Look.bGlasses)
	{
		M.AddBox(HW(FVector(.095, 0, .022)), FVector(.014, .165, .034) * HS, FLinearColor(FColor(0x0b, 0x0d, 0x10)), HQ);
		for (int32 i = 0; i < 2; i++)
			M.AddBox(HW(FVector(.04, (i == 0 ? -1 : 1) * .085, .025)), FVector(.11, .006, .008) * HS, FLinearColor(FColor(0x0b, 0x0d, 0x10)), HQ);
	}
	if (Look.bCap)
	{
		AddBlob(M, HW(FVector(-.005, 0, .035)), FVector(.114, .112, .1) * HS, HQ, Look.CapColor, 10, 3, 0);
		M.AddBox(HW(FVector(.11, 0, .045)), FVector(.12, .16, .012) * HS, Look.CapColor, HQ * FQuat(FVector::YAxisVector, .15));
		AddBlob(M, HW(FVector(-.02, 0, 0)), FVector(.099, .099, .062) * HS, HQ, Hair, 8, 3);
	}
	else AddBlob(M, HW(FVector(-.012, 0, .022)), FVector(.116, .105, .116) * HS, HQ * FQuat(FVector::YAxisVector, -.12), Hair, 10, 4, -.12);

	if (Look.bLongHair)
	{
		// the hair swings back from a pivot at the crown as the boat picks up speed
		const FVector Piv(-.02, 0, .06);
		const FQuat Blow(FVector::YAxisVector, Wind * .45 + FMath::Sin(Ph * 3.1) * .03);
		auto HairW = [&](const FVector& P) { return HW(Piv + Blow.RotateVector(P - Piv)); };
		if (Look.HairStyle == 2)
		{
			AddBlob(M, HW(FVector(-.105, 0, .035)), FVector(.03), HQ, Hair, 5, 3);
			M.AddTube(HairW(FVector(-.12, 0, .03)), HairW(FVector(-.16, 0, -.26)), .042, .012, 7, Hair);
		}
		else if (Look.HairStyle == 3)
		{
			AddBlob(M, HW(FVector(-.075, 0, .1)), FVector(.05, .05, .043), HQ, Hair, 6, 4);
		}
		else
		{
			const double Len = Look.HairStyle == 1 ? .34 : .4;
			M.AddTube(HairW(FVector(-.045, 0, .03)), HairW(FVector(-.075, 0, .03 - Len)), .1, .075, 8, Hair);
			for (int32 i = 0; i < 2; i++)
			{
				const double Sx = i == 0 ? -1 : 1;
				M.AddTube(HairW(FVector(.035, Sx * .088, -.02)), HairW(FVector(.06, Sx * .112, Look.HairStyle == 1 ? -.26 : -.3)), .032, .012, 6, Hair);
			}
		}
	}
}
