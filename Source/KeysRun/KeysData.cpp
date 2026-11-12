#include "KeysData.h"

namespace KeysData
{
	static FLinearColor Hex(uint32 C) { return FLinearColor(FColor((C >> 16) & 255, (C >> 8) & 255, C & 255)); }

	static const TMap<FString, FSpecies>& AllSpecies()
	{
		static TMap<FString, FSpecies> M;
		if (M.Num()) return M;
		auto S = [](const TCHAR* Key, const TCHAR* Name, double Min, double Max, double Str, double Mult, uint32 Color, bool bJump, const TCHAR* Fact)
		{
			M.Add(FString(Key), FSpecies{Key, Name, Min, Max, Str, Mult, Hex(Color), bJump, Fact});
		};
		S(TEXT("bonefish"), TEXT("Bonefish"), 3, 12, .65, 30, 0xd2dade, false, TEXT("The gray ghost of the flats. In Florida, bonefish are catch-and-release only."));
		S(TEXT("permit"), TEXT("Permit"), 8, 40, .85, 28, 0xb6c3ca, false, TEXT("Famously picky crab-eaters. Landing one is a career moment for flats anglers."));
		S(TEXT("tarpon"), TEXT("Tarpon"), 30, 160, 1.05, 10, 0xdfe8ee, true, TEXT("The silver king. Florida tarpon are catch-and-release unless you hold a harvest tag."));
		S(TEXT("barracuda"), TEXT("Great Barracuda"), 5, 45, .7, 6, 0x9db2bf, true, TEXT("An ambush predator that can't resist a fast, flashy tube lure."));
		S(TEXT("snook"), TEXT("Snook"), 4, 30, .6, 12, 0xcfc6a0, false, TEXT("Snook hug mangrove edges and bridge shadows, waiting for the tide to bring bait."));
		S(TEXT("redfish"), TEXT("Redfish"), 4, 28, .55, 10, 0xcf8c4e, false, TEXT("Look for copper tails waving above the surface in skinny water."));
		S(TEXT("trout"), TEXT("Spotted Seatrout"), 1, 8, .3, 12, 0xb1ae8f, false, TEXT("Big trout cruise the grass flats at first light."));
		S(TEXT("mangrove"), TEXT("Mangrove Snapper"), 1, 12, .35, 14, 0xa85a4b, false, TEXT("Smart and leader-shy. Go lighter and you'll get more bites."));
		S(TEXT("yellowtail"), TEXT("Yellowtail Snapper"), 1, 7, .3, 16, 0xe8c547, false, TEXT("Chum them up behind the boat along the reef edge."));
		S(TEXT("mutton"), TEXT("Mutton Snapper"), 4, 22, .5, 12, 0xd97b69, false, TEXT("A prized reef snapper that eats best on a bottom-fished bait."));
		S(TEXT("hogfish"), TEXT("Hogfish"), 2, 20, .4, 15, 0xeaa07c, false, TEXT("Usually speared, but a live shrimp on the bottom will tempt one."));
		S(TEXT("grouper"), TEXT("Black Grouper"), 10, 80, .95, 8, 0x5f5c53, false, TEXT("Lock the drag and pull hard - they bolt for the rocks."));
		S(TEXT("mahi"), TEXT("Mahi-Mahi"), 6, 55, .8, 9, 0x7cd34c, true, TEXT("Find floating weedlines with birds working, and the mahi are underneath."));
		S(TEXT("blackfin"), TEXT("Blackfin Tuna"), 6, 40, .85, 9, 0x2c4268, false, TEXT("Blackfin school around the offshore humps beyond the reef."));
		S(TEXT("wahoo"), TEXT("Wahoo"), 15, 90, 1, 8, 0x4d80a9, false, TEXT("Razor teeth and blistering first runs - wire leader recommended."));
		S(TEXT("sailfish"), TEXT("Sailfish"), 30, 100, 1.1, 12, 0x3155a6, true, TEXT("A Keys winter favorite, usually caught with kites and live bait."));
		S(TEXT("yellowfin"), TEXT("Yellowfin Tuna"), 20, 220, 1.15, 8, 0xe9c43a, false, TEXT("Named for the long golden second dorsal and anal fins of big adults. Yellowfin often travel with pods of dolphins."));
		S(TEXT("bluefin"), TEXT("Atlantic Bluefin Tuna"), 150, 900, 1.55, 5, 0x14224a, false, TEXT("Warm-blooded giants. In spring some pass through the Florida Straits on their way to spawn in the Gulf of Mexico."));
		S(TEXT("albacore"), TEXT("Albacore"), 15, 60, .85, 10, 0x7f93b8, false, TEXT("The longfin tuna - those huge pectoral fins give it away. A rare visitor this far south."));
		S(TEXT("cobia"), TEXT("Cobia"), 10, 90, 1, 9, 0x3b3328, false, TEXT("Cobia shadow rays, turtles and sharks - sight-cast when one cruises by."));
		S(TEXT("amberjack"), TEXT("Greater Amberjack"), 15, 120, 1.15, 7, 0x8a7b45, false, TEXT("Amberjack crowd the Marathon Hump and wrecks, then bulldog straight for the bottom."));
		S(TEXT("kingfish"), TEXT("King Mackerel"), 8, 60, .9, 9, 0x3d5a7a, false, TEXT("Kingfish run the reef edge in winter; slow-trolled live baits draw the big \"smokers\"."));
		S(TEXT("cero"), TEXT("Cero Mackerel"), 2, 12, .5, 14, 0x3f6286, false, TEXT("Cero hang over the reef - the Keys' own gold-spotted mackerel."));
		S(TEXT("jackcrevalle"), TEXT("Jack Crevalle"), 4, 35, .85, 8, 0x4f6a6e, false, TEXT("Jacks hunt in packs and never quit pulling."));
		S(TEXT("ladyfish"), TEXT("Ladyfish"), 1, 4, .35, 20, 0x7f9aa0, true, TEXT("Ladyfish jump like mini tarpon - the poor man's tarpon."));
		S(TEXT("sheepshead"), TEXT("Sheepshead"), 2, 12, .45, 14, 0xc9c9c0, false, TEXT("Sheepshead have human-like teeth for crunching crabs and barnacles off bridge pilings."));
		S(TEXT("pompano"), TEXT("Florida Pompano"), 1, 6, .4, 22, 0xf1e9b0, false, TEXT("Pompano skip along the surface behind a boat wake - and they are prized table fare."));
		S(TEXT("tripletail"), TEXT("Tripletail"), 4, 30, .6, 14, 0x4e4434, false, TEXT("Tripletail float on their sides under buoys and debris, pretending to be a leaf."));
		S(TEXT("lionfish"), TEXT("Lionfish"), .5, 3, .2, 60, 0xa5432e, false, TEXT("Invasive in Florida. There is no bag limit - every one removed helps the reef."));
		S(TEXT("goliath"), TEXT("Goliath Grouper"), 100, 500, 1.4, 4, 0x6b5c3e, false, TEXT("Protected in Florida apart from a tiny permit lottery - photograph it and let it go."));
		S(TEXT("graytrigger"), TEXT("Gray Triggerfish"), 2, 10, .45, 15, 0x6f6f68, false, TEXT("Triggerfish lock their first dorsal spine upright to wedge into reef holes."));
		S(TEXT("blacktip"), TEXT("Blacktip Shark"), 30, 150, 1.05, 5, 0x7a8187, true, TEXT("Hooked blacktips often spin out of the water."));
		S(TEXT("bullshark"), TEXT("Bull Shark"), 100, 350, 1.35, 4, 0x6b6f68, false, TEXT("Bull sharks roam from the open ocean right up into rivers and canals."));
		S(TEXT("lemonshark"), TEXT("Lemon Shark"), 60, 250, 1.15, 4, 0xa6976b, false, TEXT("Lemon sharks are protected in Florida state waters - release them."));
		S(TEXT("hammerhead"), TEXT("Great Hammerhead"), 150, 700, 1.5, 3, 0x6e7a80, false, TEXT("Great hammerheads must be released in Florida waters."));
		S(TEXT("marlin"), TEXT("Blue Marlin"), 150, 800, 1.45, 5, 0x1f3f8f, true, TEXT("The bucket-list billfish of the Gulf Stream. Blue marlin over 300 lb are almost always females."));
		S(TEXT("swordfish"), TEXT("Swordfish"), 80, 400, 1.3, 6, 0x40465a, false, TEXT("Daytime swordfish live near the bottom in well over a thousand feet of water."));
		S(TEXT("blackmarlin"), TEXT("Black Marlin"), 200, 900, 1.55, 5, 0x2b3340, true, TEXT("The only marlin whose pectoral fins lock rigidly out to the sides - a true giant, rarely seen this side of the Pacific."));
		S(TEXT("gag"), TEXT("Gag Grouper"), 6, 45, .9, 7, 0x6f6a60, false, TEXT("Gags hang over ledges and wrecks; the big ones are almost always males."));
		S(TEXT("redgrouper"), TEXT("Red Grouper"), 5, 30, .85, 7, 0x9a5a46, false, TEXT("Red groupers dig out holes in the bottom that other reef fish move into."));
		// the web game's later species (content/species.ts): flatfish, the reef's schooling snappers and jacks, the wreck giants, more pelagics
		S(TEXT("flounder"), TEXT("Southern Flounder"), 1, 12, 0.4, 18, 0x6b5f4a, false, TEXT("Both eyes end up on one side: a flounder starts life upright and one eye migrates across its skull as it settles onto the bottom."));
		S(TEXT("sole"), TEXT("Hogchoker Sole"), 0.4, 4, 0.3, 22, 0x5e5444, false, TEXT("A true sole, and small enough that it earned its name from hogs choking on the ones thrown to them: the rough scales catch in the throat."));
		S(TEXT("halibut"), TEXT("Atlantic Halibut"), 3, 400, 1.5, 9, 0x4a4a44, false, TEXT("The largest flatfish in the world, and it hunts like one: it will leave the bottom entirely and run down a fish in open water."));
		S(TEXT("mako"), TEXT("Shortfin Mako"), 80, 600, 1.7, 5, 0x1f4f86, false, TEXT("Makos have been clocked at over 40 mph and will jump clear of the water when hooked."));
		S(TEXT("warsaw"), TEXT("Warsaw Grouper"), 60, 440, 1.55, 3.5, 0x3b332e, false, TEXT("Warsaw grouper live past fifty years and almost never leave the deep ledges they settle on."));
		S(TEXT("cubera"), TEXT("Cubera Snapper"), 25, 120, 1.3, 6, 0x5a4a46, false, TEXT("Cubera snapper gather on Keys wrecks in summer to spawn, and have teeth that can take a finger."));
		S(TEXT("tigershark"), TEXT("Tiger Shark"), 200, 900, 1.6, 3, 0x5f6a63, false, TEXT("Tiger sharks eat almost anything, and have been found with licence plates in their stomachs."));
		S(TEXT("bluerunner"), TEXT("Blue Runner"), 1, 8, 0.5, 16, 0x5e8392, false, TEXT("Blue runners travel in big fast schools and make outstanding live bait for everything bigger."));
		S(TEXT("schoolmaster"), TEXT("Schoolmaster Snapper"), 1, 8, 0.4, 15, 0xc99a3c, false, TEXT("Named for the way it hangs over patch reef in tight schools. Yellow fins give it away."));
		S(TEXT("lanesnapper"), TEXT("Lane Snapper"), 0.5, 6, 0.3, 17, 0xd08a74, false, TEXT("Pink with yellow stripes and a smudge under the back  -  a small, willing reef snapper."));
		S(TEXT("vermilion"), TEXT("Vermilion Snapper"), 1, 7, 0.35, 16, 0xb8402e, false, TEXT("Called the beeliner. They stack up in big schools just off the reef drop."));
		S(TEXT("lookdown"), TEXT("Lookdown"), 0.5, 4, 0.35, 20, 0xc3d2dc, false, TEXT("A living mirror  -  flat as a plate, with a face that looks permanently unimpressed."));
		S(TEXT("palometa"), TEXT("Palometa"), 0.5, 3, 0.4, 22, 0xd7d2bd, false, TEXT("The long black fin streamers trail behind it like ribbons in the surf."));
		S(TEXT("spanishmack"), TEXT("Spanish Mackerel"), 1, 9, 0.55, 14, 0x44739a, false, TEXT("Golden spots and no bars is how you tell a Spanish from a cero. They slash through bait schools at speed."));
		S(TEXT("bigeye"), TEXT("Bigeye Tuna"), 30, 300, 1.2, 7, 0x24405f, false, TEXT("That enormous eye is for hunting in deep, dark water  -  bigeye feed far below the other tunas."));
		S(TEXT("skipjack"), TEXT("Skipjack Tuna"), 4, 30, 0.8, 11, 0x2b4160, false, TEXT("The striped belly is unique among tunas. Skipjack school in the thousands and churn the surface when they feed."));
		return M;
	}

	const FSpecies* Species(const FString& Key) { return AllSpecies().Find(Key); }

	const TArray<FString>& SpeciesKeys()
	{
		static TArray<FString> K;
		if (!K.Num()) AllSpecies().GetKeys(K);
		return K;
	}

	const TArray<FZoneRow>& ZoneTable(const FString& Zone)
	{
		using FRow = TArray<FZoneRow>;
		static TMap<FString, FRow> T;
		if (!T.Num())
		{
			auto Z = [](const TCHAR* Name, std::initializer_list<FZoneRow> Rows) { T.Add(Name, FRow(Rows)); };
			Z(TEXT("Creek"), {{TEXT("flounder"), 2}, {TEXT("sole"), 1.5}, {TEXT("snook"), 5}, {TEXT("redfish"), 3}, {TEXT("tarpon"), 2}, {TEXT("mangrove"), 3}, {TEXT("jackcrevalle"), 2}, {TEXT("ladyfish"), 2}});
			Z(TEXT("Flats"), {{TEXT("flounder"), 2}, {TEXT("sole"), 1.2}, {TEXT("bonefish"), 11}, {TEXT("permit"), 2}, {TEXT("barracuda"), 3}, {TEXT("tarpon"), 1}, {TEXT("ladyfish"), 2}, {TEXT("lemonshark"), 0.8}, {TEXT("blacktip"), 1}, {TEXT("palometa"), 1.5}});
			Z(TEXT("Backcountry"), {{TEXT("flounder"), 2.5}, {TEXT("sole"), 1.6}, {TEXT("snook"), 4}, {TEXT("redfish"), 4}, {TEXT("trout"), 5}, {TEXT("tarpon"), 2}, {TEXT("ladyfish"), 3}, {TEXT("jackcrevalle"), 2}, {TEXT("tripletail"), 0.8}, {TEXT("bullshark"), 0.6}});
			Z(TEXT("Bridge"), {{TEXT("flounder"), 1.5}, {TEXT("tarpon"), 11}, {TEXT("mangrove"), 4}, {TEXT("snook"), 2}, {TEXT("sheepshead"), 3}, {TEXT("jackcrevalle"), 2}, {TEXT("goliath"), 0.6}, {TEXT("bullshark"), 0.6}, {TEXT("lookdown"), 2}, {TEXT("bluerunner"), 2.5}});
			Z(TEXT("Hawk Channel"), {{TEXT("halibut"), 0.4}, {TEXT("mangrove"), 4}, {TEXT("yellowtail"), 3}, {TEXT("mutton"), 2}, {TEXT("redgrouper"), 1.2}, {TEXT("gag"), 0.8}, {TEXT("barracuda"), 2}, {TEXT("cero"), 2}, {TEXT("pompano"), 1.5}, {TEXT("cobia"), 1}, {TEXT("jackcrevalle"), 1.5}, {TEXT("graytrigger"), 1.5}, {TEXT("bluerunner"), 3}, {TEXT("lanesnapper"), 2.5}, {TEXT("spanishmack"), 2.5}, {TEXT("lookdown"), 1}, {TEXT("tarpon"), 0.5}, {TEXT("amberjack"), 0.5}, {TEXT("kingfish"), 1.2}, {TEXT("blacktip"), 0.3}});
			Z(TEXT("Reef"), {{TEXT("cubera"), 0.6}, {TEXT("yellowtail"), 5}, {TEXT("mutton"), 3}, {TEXT("hogfish"), 3}, {TEXT("grouper"), 2.5}, {TEXT("gag"), 2}, {TEXT("redgrouper"), 2}, {TEXT("barracuda"), 2}, {TEXT("cero"), 2.5}, {TEXT("kingfish"), 2}, {TEXT("lionfish"), 1.5}, {TEXT("graytrigger"), 2}, {TEXT("goliath"), 0.5}, {TEXT("amberjack"), 0.8}, {TEXT("cobia"), 0.6}, {TEXT("lemonshark"), 0.25}, {TEXT("schoolmaster"), 3}, {TEXT("lanesnapper"), 2.5}, {TEXT("vermilion"), 2.5}, {TEXT("bluerunner"), 2.5}, {TEXT("spanishmack"), 1.5}});
			Z(TEXT("Offshore"), {{TEXT("halibut"), 1}, {TEXT("mako"), 0.8}, {TEXT("tigershark"), 0.35}, {TEXT("mahi"), 6}, {TEXT("blackfin"), 4}, {TEXT("wahoo"), 2}, {TEXT("sailfish"), 2}, {TEXT("marlin"), 1.3}, {TEXT("blackmarlin"), 0.6}, {TEXT("swordfish"), 1.1}, {TEXT("yellowfin"), 0.8}, {TEXT("albacore"), 0.3}, {TEXT("bluefin"), 0.4}, {TEXT("kingfish"), 1.5}, {TEXT("amberjack"), 1.5}, {TEXT("cobia"), 0.5}, {TEXT("hammerhead"), 0.5}, {TEXT("skipjack"), 3}, {TEXT("bigeye"), 0.7}});
		}
		const FRow* R = T.Find(Zone);
		return R ? *R : T[TEXT("Hawk Channel")];
	}

	const TArray<FBoatSpec>& Boats()
	{
		static TArray<FBoatSpec> B;
		if (B.Num()) return B;
		auto Add = [](const TCHAR* Id, const TCHAR* Brand, const TCHAR* Name, const TCHAR* Power, double Len, double Beam, int32 Engines, double Top, double Accel, double Turn,
			const TCHAR* DraftFt, double Draft, bool bCat, bool bHardtop, uint32 Hull, uint32 Stripe, uint32 Bottom, uint32 Canvas, uint32 Engine, uint32 Deck, double Cap, double Spring, const TCHAR* Desc)
		{
			FBoatSpec S;
			S.Id = Id; S.Brand = Brand; S.Name = Name; S.Power = Power; S.DraftFt = DraftFt; S.Desc = Desc;
			S.Len = Len; S.Beam = Beam; S.Top = Top; S.Accel = Accel; S.Turn = Turn; S.Draft = Draft; S.Engines = Engines;
			S.bCat = bCat; S.bHardtop = bHardtop;
			S.Hull = Hex(Hull); S.Stripe = Hex(Stripe); S.Bottom = Hex(Bottom); S.Canvas = Hex(Canvas); S.Engine = Hex(Engine); S.Deck = Hex(Deck);
			S.CoolerCap = Cap; S.Spring = Spring;
			B.Add(S);
		};
		Add(TEXT("robalo"), TEXT("Robalo"), TEXT("R230"), TEXT("Single Mercury Racing 450R"), 7.0, 2.59, 1, 52, .62, 1.15, TEXT("1' 7\""), .6, false, true,
			0xffffff, 0x6fa8c9, 0xf4f4f2, 0xf4f4f2, 0xd8dadc, 0xdcddd8, 150, .38,
			TEXT("A nimble 23-footer that slips onto the flats and under the bridges. Gets wet and bouncy once you pass the reef."));
		Add(TEXT("grady"), TEXT("Grady-White"), TEXT("Canyon 306"), TEXT("Twin Mercury Racing 450R"), 9.3, 3.23, 2, 58, .5, .9, TEXT("1' 9\""), .72, false, false,
			0xfbfaf6, 0x23466e, 0xf8f7f2, 0x23466e, 0xd8dadc, 0xe0ded6, 350, .52,
			TEXT("The classic all-rounder. Its deep-V hull smooths out Hawk Channel chop and runs to blue water with confidence."));
		Add(TEXT("freeman"), TEXT("Freeman"), TEXT("42LR"), TEXT("Quad Mercury Racing 450R"), 12.8, 3.58, 4, 69, .44, .72, TEXT("2' 0\""), .82, true, true,
			0x74bfe6, 0x1f2a33, 0xf2f2f0, 0xf2f2ee, 0x17191c, 0xe6e2d8, 800, .3,
			TEXT("A long-range catamaran built for tournament fishing. Twin hulls ride flat and soft offshore, and it drafts shallow for its size."));
		Add(TEXT("midnight"), TEXT("Midnight Express"), TEXT("43 Open"), TEXT("Quad Mercury Racing 450R"), 13.1, 3.81, 4, 70, .42, .66, TEXT("2' 0\""), .9, false, true,
			0x17191d, 0xc8a24a, 0x2a2d31, 0xf2f2ee, 0x1c1f24, 0xe7e5df, 400, .35,
			TEXT("A twin-step deep-V luxury rocket with a very wide beam. Fast, stable, and dry, but keep it in real water."));
		Add(TEXT("mti"), TEXT("MTI"), TEXT("V42"), TEXT("Quad Mercury Racing 450R"), 12.8, 3.5, 4, 76, .46, .62, TEXT("3' 0\""), 1.15, false, true,
			0xc8102e, 0xf4f4f4, 0x15171a, 0x15171a, 0x15171a, 0x2a2d31, 300, .3,
			TEXT("The fastest center console here, with race-bred power. It covers the Keys in minutes, but its deeper draft keeps it off the flats."));
		Add(TEXT("yellowfin"), TEXT("Yellowfin"), TEXT("36 CC"), TEXT("Triple Mercury Racing 450R"), 11.0, 3.3, 3, 66, .48, .78, TEXT("1' 10\""), .76, false, true,
			0xe9d98a, 0x1f3b57, 0xf2efe2, 0xf2f2ee, 0xf4f4f2, 0xe2decd, 520, .46,
			TEXT("The Keys tournament favourite. A sharp 24-degree entry that cuts chop most boats pound through, with a famously dry ride for a boat this quick."));
		Add(TEXT("contender"), TEXT("Contender"), TEXT("32ST"), TEXT("Twin Mercury Racing 450R"), 9.8, 3.0, 2, 61, .52, .86, TEXT("1' 8\""), .68, false, false,
			0xf2f4f5, 0x2f7a52, 0xf4f6f5, 0xe8eae6, 0xf4f4f2, 0xdedfd9, 420, .44,
			TEXT("A no-nonsense fishing hull with a huge cockpit and almost nothing in it. Light, quick to plane, and happy running a long day offshore."));
		Add(TEXT("nortech"), TEXT("Nor-Tech"), TEXT("390 Sport"), TEXT("Twin Mercury Racing 500R"), 11.9, 2.9, 2, 82, .58, .58, TEXT("2' 8\""), 1.05, false, true,
			0x14161a, 0xff7a18, 0x14161a, 0x14161a, 0x14161a, 0x1b1e23, 180, .26,
			TEXT("Not a fishing boat. A stepped-hull go-fast that will out-run everything here in a straight line and punish you for asking it to turn."));

		// fit-out per hull, from HULLS[id].style (content/boats.ts) and BOAT_PRICE (sim/economy.ts)
		struct FFit { const TCHAR* Id; uint32 EngAcc, EngLow, Cove, Uph; bool bTTop, bRadar, bRodRack, bBowRail, bRiggerPoles, bSunpad, bLeaning, bBracket; int32 Steps; double Entry; int64 Price; };
		static const FFit Fits[] = {
			{TEXT("robalo"),    0x6fa8c9, 0x2a2d31, 0,        0x6fa8c9, false, false, false, true,  false, false, true,  false, 0, .42, 0},
			{TEXT("grady"),     0x23466e, 0x2a2d31, 0x23466e, 0xe8e4d8, true,  true,  false, true,  true,  false, true,  false, 0, .40, 0},
			{TEXT("freeman"),   0x1f2a33, 0x1f2126, 0,        0x9aa0a6, false, true,  true,  false, false, false, false, true,  1, .50, 0},
			{TEXT("midnight"),  0xc8a24a, 0x17191d, 0xc8a24a, 0xf3efe6, false, false, false, false, false, true,  false, true,  2, .46, 0},
			{TEXT("mti"),       0xf4f4f4, 0x15171a, 0xf4f4f4, 0x1d1f23, false, false, false, false, false, true,  false, true,  2, .48, 0},
			{TEXT("yellowfin"), 0x1f3b57, 0x2a2d31, 0x1f3b57, 0x1f3b57, true,  true,  true,  true,  true,  false, true,  true,  0, .56, 30000},
			{TEXT("contender"), 0x2f7a52, 0x2a2d31, 0x2f7a52, 0x2f7a52, true,  false, true,  true,  true,  false, true,  false, 0, .46, 20000},
			{TEXT("nortech"),   0xff7a18, 0x0b0c0e, 0xff7a18, 0xff7a18, false, false, false, false, false, true,  false, true,  2, .50, 85000},
		};
		for (FBoatSpec& S : B)
			for (const FFit& F : Fits)
			{
				if (S.Id != F.Id) continue;
				S.EngAcc = Hex(F.EngAcc); S.EngLow = Hex(F.EngLow); S.Uph = Hex(F.Uph);
				S.Cove = F.Cove ? Hex(F.Cove) : FLinearColor(0, 0, 0, 0);
				S.bTTop = F.bTTop; S.bRadar = F.bRadar; S.bRodRack = F.bRodRack; S.bBowRail = F.bBowRail; S.bRiggerPoles = F.bRiggerPoles;
				S.bSunpad = F.bSunpad; S.bLeaning = F.bLeaning; S.bBracket = F.bBracket; S.Steps = F.Steps; S.Entry = F.Entry; S.Price = F.Price;
			}
		return B;
	}

	int32 BoatIndexOf(const FString& Id)
	{
		const TArray<FBoatSpec>& B = Boats();
		for (int32 i = 0; i < B.Num(); i++) if (B[i].Id == Id) return i;
		return -1;
	}

	bool IsSounder(const FString& Key)
	{
		static const TSet<FString> S = {TEXT("blackfin"), TEXT("yellowfin"), TEXT("bluefin"), TEXT("albacore"), TEXT("amberjack"), TEXT("grouper"), TEXT("goliath"), TEXT("swordfish"),
			TEXT("cobia"), TEXT("bullshark"), TEXT("hammerhead"), TEXT("lemonshark"), TEXT("blacktip"), TEXT("mutton"), TEXT("kingfish"), TEXT("wahoo"),
			TEXT("tigershark"), TEXT("mako"), TEXT("warsaw"), TEXT("cubera")};
		return S.Contains(Key);
	}

	bool IsBillfish(const FString& Key) { return Key == TEXT("sailfish") || Key == TEXT("marlin") || Key == TEXT("blackmarlin"); }

	FString ReleaseOnlyReason(const FString& Key)
	{
		static const TMap<FString, FString> CR = {
			{TEXT("goliath"), TEXT("Goliath grouper are protected in Florida - release it in the water.")},
			{TEXT("hammerhead"), TEXT("Great hammerheads must be released in Florida waters.")},
			{TEXT("lemonshark"), TEXT("Lemon sharks are protected in Florida state waters.")},
			{TEXT("bonefish"), TEXT("Bonefish are catch-and-release only in Florida.")},
			{TEXT("tarpon"), TEXT("Tarpon are catch-and-release in Florida unless you hold a harvest tag.")},
			{TEXT("marlin"), TEXT("Billfish - release only.")}, {TEXT("blackmarlin"), TEXT("Billfish - release only.")}, {TEXT("sailfish"), TEXT("Billfish - release only.")}};
		const FString* R = CR.Find(Key);
		return R ? *R : FString();
	}
}
