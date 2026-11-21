# R4: Free Fish spots in the mangroves, and snook and tarpon charters

Written, not done: both live in hub files. Coordinates are game metres on the real map (p1).

## Free Fish spots

File: `Source/KeysRun/KeysBoatPawn.cpp`, `AKeysBoatPawn::RunDests()` (the list the Free Fish "run to" menu and spot
discovery use). Add, on the real map only:

| Name | Note | x | z | What is there |
|---|---|---|---|---|
| Sister Creek | mangrove creek | -272 | 426 | calm bend of the creek, mangrove both banks; snook and mangrove snapper under the roots; a manatee haunt |
| Whiskey Creek | mangrove tunnel | -292 | 462 | the turning out of Sister Creek into the roofed tunnel (tunnel mouth `tp=-290,462,1.4317`); the pond at its head is at -518, 498 |
| Sister Creek mouth | tarpon at first and last light | -342 | 792 | the ocean mouth, beside the marked channel (daybeacons 6 and 8): tarpon rolling at dawn and dusk, barracuda and sharks on the edge |
| Boot Key flat | mangrove flat | -1150 | 452 | the flat off Boot Key's west shore: bonefish and permit, tails up on the flood, waders along the roots |

```cpp
	if (RealMap())
	{
		Out.Add({TEXT("Sister Creek"), TEXT("mangrove creek"), -272, 426});
		Out.Add({TEXT("Whiskey Creek"), TEXT("mangrove tunnel"), -292, 462});
		Out.Add({TEXT("Sister Creek mouth"), TEXT("tarpon at first and last light"), -342, 792});
		Out.Add({TEXT("Boot Key flat"), TEXT("mangrove flat"), -1150, 452});
	}
```

One thing to handle with it: `TeleportSpot()` / the `tpcheck` test wants the landing more than 6 m from shore
(`ShoreInfo().D`). Inside a creek the baked shore distance is small or negative (-4 m at "Sister Creek", 4 m at
"Whiskey Creek"), so those two would be shifted or reported BAD. The same exemption R3 gives KeysLife fits here: treat
`CreekDist(X, Z) < -3` as far enough from shore. "Sister Creek mouth" (42 m) and "Boot Key flat" (38 m) pass as they are.

## Charter requests

File: `Content/KeysRun/Data/CharterRequests.json` (and `DT_CharterRequests.uasset` if the table asset is what ships;
`KeysCharter.cpp` loads the asset first and seeds from the JSON when it is missing). There is no snook or tarpon
request today. Rows in the file's own format:

```json
{"Name": "CreekSnook", "ClientName": "Captain Odalys Ferrer", "RequestText": "My nephew wants his first snook. Work the roots in Sister Creek on the moving tide - two slot fish, and they go back.", "TargetSpecies": ["snook"], "MinWeight": 4, "RequiredCount": 2, "Method": "Any", "CatchAndRelease": true, "TimeLimitMinutes": 120, "BasePay": 550},
{"Name": "DuskTarpon", "ClientName": "The Halvorsen brothers", "RequestText": "One silver king, jumped and let go. They roll at the creek mouth and under the bridges at first and last light.", "TargetSpecies": ["tarpon"], "MinWeight": 40, "RequiredCount": 1, "Method": "Any", "CatchAndRelease": true, "TimeLimitMinutes": 150, "BasePay": 900},
{"Name": "MangroveSupper", "ClientName": "Tide Table Supper Club", "RequestText": "Four mangrove snapper for the pan. Go light on the leader and fish the shade under the roots.", "TargetSpecies": ["mangrove"], "MinWeight": 1, "RequiredCount": 4, "Method": "Any", "CatchAndRelease": false, "TimeLimitMinutes": 120, "BasePay": 450}
```

`AKeysCharter::WhereLine()` already names "Creek" for snook (its largest share of a table), and "Bridge" for tarpon.
The client names are invented.

## Related (R5, one number)

File: `Source/KeysRun/KeysData.cpp`, the `Creek` zone table: `{TEXT("mangrove"), 3}` -> `{TEXT("mangrove"), 4}`.
Mangrove snapper and redfish both weigh 3 there, so second place behind snook is a coin toss at midday (see criterion
4 in the status file). At 4 the brief's "snook and mangrove snapper lead in the creeks" holds at every hour.
