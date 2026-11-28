/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// Phantom of the Knight's maze interior (WP-075): the centre, the fountain
// close-up and the well. The addresses are Phantom's.

#include "common/debug.h"

#include "terraglyph/character.h"
#include "terraglyph/inventory.h"
#include "terraglyph/maze_interior_rooms.h"
#include "terraglyph/room.h"
#include "terraglyph/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"

namespace Terraglyph {

static const char *const kMazeCenter = "ROOM_P23_Maze_Center";
static const char *const kFountain = "ROOM_P44_Fountain_Closeup";
static const char *const kWell = "ROOM_P22_Well_In_Maze";
static const char *const kGarden = "ROOM_P06_Garden";
static const char *const kBalcony = "ROOM_P27_Below_Balcony";

// 0x4070C0: the item is held but no longer in a toolbar slot, i.e. it has been
// used (the gallery's isMirrorPlaced idiom).
static bool used(TerraglyphEngine &engine, const char *item) {
	if (!engine.getInventory()->hasItem(item))
		return false;
	for (uint i = 0; i < Inventory::kSlotCount; i++)
		if (engine.getInventory()->getSlot(i) == item)
			return false;
	return true;
}

// ---------------------------------------------------------------------------
// The maze centre

const char *MazeCenterRoom::kWaterSfx = "SFX_P23_MAZE_CENTER_WATER";
const char *MazeCenterRoom::kTakeShears = "Maze_Center.Shaggy_Takes_Shears";
const char *MazeCenterRoom::kShearsScene = "Center_of_Maze.250B_";
const char *MazeCenterRoom::kHotspot4Lines[2] = { "Center_of_Maze.250X_", "Center_of_Maze.250X1_" };
const char *MazeCenterRoom::kHotspotLines[2] = { "Center_of_Maze.250_", "Center_of_Maze.250C_" };

int MazeCenterRoom::entryFlagFor(const Common::String &from) {
	if (from == kGarden)
		return 1;
	if (from == kBalcony)
		return 0;
	return -1;
}

int MazeCenterRoom::findSprite(const Common::String &object) const {
	const Room &room = *_engine.getRoom();
	for (uint s = 0; s < room.getSpriteCount(); s++)
		if (room.getObjectName(s) == object)
			return s;
	return -1;
}

void MazeCenterRoom::reset() {
	// 0x42A0F0, after the base reset: the HOTSPOT4 cycle and the Entrances key.
	_hotspot4 = 0;
	_entrances = false;
}

void MazeCenterRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x42A280: the babbling water is a looping sprite at depth -1500; the rusty
	// shears a requires object, gone once taken; the way to the fountain is the
	// default exit while the tokens are not held, and skipped once they are.
	if (object == "OBJ_P23_MAZE_CENTER_WATER") {
		claim.build = Room::Claim::kSprite;
		claim.depth = -1500;
	} else if (object == "OBJ_INV_RUSTY_SHEARS") {
		// Gone once taken (the history), a requires object until then.
		claim.build = _engine.getInventory()->hasItem("OBJ_INV_RUSTY_SHEARS") ? Room::Claim::kSkip : Room::Claim::kRequires;
	} else if (object == "OBJ_P23_TO_P44") {
		if (_engine.getInventory()->hasItem("OBJ_INV_TOKENS"))
			claim.build = Room::Claim::kSkip;
		// else the default path builds the exit.
	}
}

void MazeCenterRoom::enter(const Common::String &from) {
	// 0x42A580, after the base entry: the water babbles (SFX at volume 90); the
	// hedge-maze start flag is set from the maze location the gang came from
	// (0x42A640's game+0x289); the entrance list is registered.
	Sound &sound = *_engine.getSound();
	sound.play(kWaterSfx);
	const int flag = entryFlagFor(from);
	if (flag >= 0)
		_engine.setGameFlag("MazeCenterFromGarden", flag != 0);
	_entrances = true;
	_takingShears = false;
	_tookShears = false;
	_water = findSprite("OBJ_P23_MAZE_CENTER_WATER");
	debugC(1, kDebugScript, "maze centre: from %s, flag %d", from.c_str(), flag);
}

void MazeCenterRoom::roomLeft() {
	// 0x42A5F0, after the base leave: the taken shears are kept in the toolbar.
	if (_takingShears && !_engine.getInventory()->hasItem("OBJ_INV_RUSTY_SHEARS"))
		_engine.getInventory()->addItem("OBJ_INV_RUSTY_SHEARS", "ANIM_TOOLBARRUSTY_SHEARS", "ANIM_CURSORRUSTY_SHEARS");
}

void MazeCenterRoom::tick() {
	// 0x42A410, after the base tick: once the rusty shears are being taken,
	// Shaggy (idle) walks over and takes them, then they go to the toolbar and
	// the 250B_ scene plays.
	RoomScript &script = *_engine.getScript();
	if (!_takingShears)
		return;
	Character *shaggy = script.getCharacter("SHAGGY");
	const bool idle = shaggy && shaggy->getState() == Character::kStateIdle;
	if (!_tookShears) {
		if (idle && !script.isScenePlaying()) {
			_tookShears = true;
			shaggy->special(*_engine.getRoom(), *_engine.getSound(), "OBJ_SHAGGY_P23_TAKES_SHEARS");
			_engine.getSound()->play(kTakeShears);
			debugC(1, kDebugScript, "maze centre: Shaggy takes the shears");
		}
		return;
	}
	if (idle) {
		_takingShears = false;
		_tookShears = false;
		if (!_engine.getInventory()->hasItem("OBJ_INV_RUSTY_SHEARS"))
			_engine.getInventory()->addItem("OBJ_INV_RUSTY_SHEARS", "ANIM_TOOLBARRUSTY_SHEARS", "ANIM_CURSORRUSTY_SHEARS");
		script.startScene(kShearsScene, true);
		debugC(1, kDebugScript, "maze centre: the shears taken");
	}
}

void MazeCenterRoom::event(const Common::String &object, bool success) {
	// 0x42A110: HOTSPOT4 cycles its two lines; the rusty shears used starts the
	// taking.
	RoomScript &script = *_engine.getScript();
	if (object == "OBJ_P23_MAZE_CENTER_HOTSPOT4") {
		script.startScene(kHotspot4Lines[_hotspot4], true);
		_hotspot4 = _hotspot4 == 0 ? 1 : 0;
		return;
	}
	if (object == "OBJ_INV_RUSTY_SHEARS")
		_takingShears = true;
}

void MazeCenterRoom::saveKeys(Common::INIFile &profile) {
	// 0x42A200, after the base keys: the Entrances key in the room's section.
	profile.setKey("Entrances", kMazeCenter, _entrances ? "Yes" : "No");
}

void MazeCenterRoom::loadKeys(const Common::INIFile &profile) {
	// 0x42A240 -> 0x40C560: a missing key reads as false.
	Common::String v;
	_entrances = profile.getKey("Entrances", kMazeCenter, v) && v == "Yes";
}

// ---------------------------------------------------------------------------
// The fountain close-up

const char *FountainCloseupRoom::kOpening = "Fountain.250D_";
const char *FountainCloseupRoom::kClickLines[3] = { "Fountain.250D_", "Fountain.250D1_", "Fountain.250D2_" };
const char *FountainCloseupRoom::kExitLine = "Fountain.250F_";
const char *FountainCloseupRoom::kWater = "FOUNTAIN_CLOSEUP.WATER";
const char *FountainCloseupRoom::kSwimLoop = "Fountain_Closeup.Fountain_Closeup.Piranha_Swims";
const char *FountainCloseupRoom::kGrabTokens = "Fountain_Closeup.Shaggy_Takes_Tokens";
const char *FountainCloseupRoom::kLunge = "Fountain_Closeup.Piranha_Lunge_Attack";
const char *FountainCloseupRoom::kEats = "Fountain_Closeup.Piranha_Eats";
const char *FountainCloseupRoom::kPiranhaIdle = "ANIM_PIRANHAIDLE";
const char *FountainCloseupRoom::kPiranhaFull = "ANIM_PIRANHAFULL";
const char *FountainCloseupRoom::kEatStart = "ANIM_PIRANHALUNGE_EAT_START";
const char *FountainCloseupRoom::kEatFinish = "ANIM_PIRANHALUNGE_EAT_FINISH";
const char *FountainCloseupRoom::kLungeAttack = "ANIM_PIRANHALUNGE_ATTACK";
const char *FountainCloseupRoom::kCenter = "ROOM_P23_Maze_Center";

int FountainCloseupRoom::findSprite(const Common::String &object) const {
	const Room &room = *_engine.getRoom();
	for (uint s = 0; s < room.getSpriteCount(); s++)
		if (room.getObjectName(s) == object)
			return s;
	return -1;
}

void FountainCloseupRoom::reset() {
	// 0x4297A0, after the base reset: a lunge pending, the cycle from its first
	// line, nothing under way.
	_lunge = true;
	_clickCycle = 0;
	_grabbing = false;
	_fed = false;
}

void FountainCloseupRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x429CC0. The piranha a requires object (0x402D90: requiredanim=, the
	// burger) under layer 0 at depth -1, hit by its step polygon; its sequence
	// is set at entry. The tokens a requires object answering the burger at
	// depth -100 until taken. The way back to the centre an exit at depth 100.
	// Shaggy's grab a sprite under the second layer (game +0x5C; none here, so
	// the root) at depth 100, hidden and stopped.
	if (object == "OBJ_PIRANHA") {
		claim.build = Room::Claim::kRequires;
		claim.layer = 0;
		claim.depth = -1;
	} else if (object == "OBJ_INV_TOKENS") {
		if (!_engine.getInventory()->hasItem("OBJ_INV_TOKENS")) {
			claim.build = Room::Claim::kRequires;
			claim.depth = -100;
			claim.required = "ANIM_CURSORBURGER";
		} else {
			claim.build = Room::Claim::kSkip;
		}
	} else if (object == "OBJ_P44_TO_P23") {
		claim.build = Room::Claim::kExit;
		claim.depth = 100;
	} else if (object == "OBJ_SHAGGY_P44_GRAB_TOKENS") {
		claim.build = Room::Claim::kSprite;
		claim.layer = 1;
		claim.depth = 100;
		claim.hidden = true;
	}
}

void FountainCloseupRoom::enter(const Common::String &from) {
	// 0x4297D0, after the base entry: the water. The sprites the claim built;
	// the piranha full (looping) once the burger has been used, else its idle
	// played once with its swim (0x429CC0); the grab stopped on its first step.
	Room &room = *_engine.getRoom();
	_piranha = findSprite("OBJ_PIRANHA");
	_tokens = findSprite("OBJ_INV_TOKENS");
	_grab = findSprite("OBJ_SHAGGY_P44_GRAB_TOKENS");
	if (_piranha >= 0) {
		if (used(_engine, "OBJ_INV_BURGER")) {
			room.startAnim(_piranha, kPiranhaFull, 1, -1, 3);
		} else {
			room.startAnim(_piranha, kPiranhaIdle, 3, -1, 3);
			_engine.getSound()->play(kSwimLoop);
		}
	}
	if (_grab >= 0)
		room.setPlayer(_grab, 0, -1, 3);
	_engine.getSound()->play(kWater);
	debugC(1, kDebugScript, "fountain: from %s, lunge %s", from.c_str(), _lunge ? "pending" : "not pending");
}

void FountainCloseupRoom::roomLeft() {
	// 0x42A040, after the base leave: left during Shaggy's grab, the tokens are
	// his anyway.
	if (_grabbing && !_engine.getInventory()->hasItem("OBJ_INV_TOKENS"))
		_engine.getInventory()->addItem("OBJ_INV_TOKENS", "ANIM_TOOLBARTOKENS", "ANIM_CURSORTOKENS");
}

void FountainCloseupRoom::tick() {
	// 0x429800, after the base tick.
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	// The piranha's sequence over (shown, stopped):
	if (_piranha >= 0 && !room.isHidden(_piranha) && room.getMode(_piranha) == 0) {
		const Common::String &anim = room.getAnim(_piranha);
		if (_lunge) {
			// A lunge pending: it lunges at the gang.
			_lunge = false;
			room.startAnim(_piranha, kLungeAttack, 3, -1, 3);
			sound.play(kLunge);
			debugC(1, kDebugScript, "fountain: the piranha lunges");
		} else if (anim == kEatStart) {
			// It has the burger: it goes under, the tokens with it, and Shaggy
			// grabs them.
			room.setHidden(_piranha, true);
			if (_grab >= 0) {
				room.setHidden(_grab, false);
				room.setPlayer(_grab, 3, -1, 3);
			}
			if (_tokens >= 0)
				room.setHidden(_tokens, true);
			_grabbing = true;
			sound.play(kGrabTokens);
			debugC(1, kDebugScript, "fountain: Shaggy grabs the tokens");
		} else {
			// Its idle again with its swim; after the eat, full and looping.
			if (anim == kEatFinish)
				room.startAnim(_piranha, kPiranhaFull, 1, -1, 3);
			else
				room.startAnim(_piranha, kPiranhaIdle, 3, -1, 3);
			sound.play(kSwimLoop);
		}
	}
	// Shaggy's grab over (shown, stopped): the tokens into the toolbar, the
	// piranha back to finish eating, and the gang back to the centre with
	// Fountain.250F_ (the exit 0x404820 builds, through the room's +0x24).
	if (_grab >= 0 && !room.isHidden(_grab) && room.getMode(_grab) == 0) {
		_grabbing = false;
		Inventory &inventory = *_engine.getInventory();
		if (!inventory.hasItem("OBJ_INV_TOKENS"))
			inventory.addItem("OBJ_INV_TOKENS", "ANIM_TOOLBARTOKENS", "ANIM_CURSORTOKENS");
		room.setHidden(_grab, true);
		if (_piranha >= 0) {
			room.setHidden(_piranha, false);
			room.startAnim(_piranha, kEatFinish, 3, -1, 3);
		}
		RoomScript::Exit exit;
		exit.destination = kCenter;
		exit.scene = kExitLine;
		exit.movie = -1;
		_engine.getScript()->takeExit(exit);
		_fed = false;
		debugC(1, kDebugScript, "fountain: the tokens taken, back to the centre");
	}
}

void FountainCloseupRoom::event(const Common::String &object, bool success) {
	// 0x429B50: the piranha or the tokens.
	if (object != "OBJ_PIRANHA" && object != "OBJ_INV_TOKENS")
		return;
	if (success) {
		// The burger: spent; the piranha starts eating, Piranha_Eats; no lunge;
		// input held and the cursor hidden (game +0x09, 0x435E80).
		_engine.getInventory()->removeItem("OBJ_INV_BURGER");
		if (_piranha >= 0)
			_engine.getRoom()->startAnim(_piranha, kEatStart, 3, -1, 3);
		_engine.getSound()->play(kEats);
		_lunge = false;
		_fed = true;
		debugC(1, kDebugScript, "fountain: the piranha is fed");
		return;
	}
	// A plain click (the open hand, nothing held): the next of the three
	// lines, holding input, and the piranha lunges when its idle ends.
	if (!_engine.getInventory()->isDefaultCursor())
		return;
	_lunge = true;
	_engine.getScript()->startScene(kClickLines[_clickCycle], true);
	_clickCycle = _clickCycle == 2 ? 0 : _clickCycle + 1;
}

// ---------------------------------------------------------------------------
// The well

const char *WellInMazeRoom::kClickLines[3] = { "Well_in_Maze.290_", "Well_In_Maze.330X1_", "Well_In_Maze.330X2_" };
const char *WellInMazeRoom::kGallery = "ROOM_P07_Royal_Gallery";

int WellInMazeRoom::findSprite(const Common::String &object) const {
	const Room &room = *_engine.getRoom();
	for (uint s = 0; s < room.getSpriteCount(); s++)
		if (room.getObjectName(s) == object)
			return s;
	return -1;
}

void WellInMazeRoom::reset() {
	// 0x42C7D0, after the base reset.
	_clickCycle = 0;
	_returnPending = false;
}

void WellInMazeRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x42CA20: the yellow ball is gone once found (game+0x25d — the flag the well
	// sets on pickup and the Jester reads to take it, 0x412B80); the climb (the
	// well, its rope and the way to P21) waits on the rope being used — the rope
	// then shows as a sprite and the way out and the well become the clickables.
	const bool ballFound = _engine.getGameFlag("YellowBallFound");
	const bool ropeUsed = used(_engine, "OBJ_INV_ROPE");
	if (object == "OBJ_INV_BALL_YELLOW") {
		if (ballFound)
			claim.build = Room::Claim::kSkip;
	} else if (object == "OBJ_P22_TO_P21") {
		if (!ropeUsed)
			claim.build = Room::Claim::kSkip; // no way out until the rope is used
	} else if (object == "OBJ_P22_WELL_ROPE") {
		if (ropeUsed) {
			claim.build = Room::Claim::kSprite;
			claim.depth = 0;
		} else {
			claim.build = Room::Claim::kSkip;
		}
	} else if (object == "OBJ_P22_WELL") {
		if (ropeUsed)
			claim.build = Room::Claim::kSkip;
	}
}

void WellInMazeRoom::enter(const Common::String &from) {
	// 0x42C810, after the base entry: a queued return bounces to the gallery
	// (movie 54 from it, 57 from elsewhere), unless the full bottle has been used;
	// otherwise the Jester's entry (0x412B80, the member at +0xB8: the balls
	// found he takes, and his talk) and the well marked visited (game+0x262).
	_jesterHere = false;
	if (_returnPending) {
		_returnPending = false;
		if (!used(_engine, "OBJ_INV_BOTTLE_FULL")) {
			const int movie = from == kGallery ? kMovieReturnGallery : kMovieReturnOther;
			debugC(1, kDebugScript, "well: return to the gallery, movie %d", movie);
			_engine.requestRoom(kGallery, movie);
			return;
		}
	}
	_jester.enter(_engine);
	_jesterHere = true;
	_engine.setGameFlag("WellVisited", true);
	debugC(1, kDebugScript, "well: from %s, visited", from.c_str());
}

void WellInMazeRoom::tick() {
	// 0x42C7F0, after the base tick: the Jester's (0x4122E0). On a visit that
	// bounces to the gallery his entry didn't run, and the room's characters
	// were rebuilt since he last did, so he is left alone until the bounce.
	if (_jesterHere)
		_jester.tick(_engine);
}

void WellInMazeRoom::event(const Common::String &object, bool success) {
	// 0x42C8E0: the yellow ball shown plays movie 66 the first time (4 after) and
	// is marked found (game+0x25d), queuing the return; the rope used on the well
	// plays movie 59 and spends the rope; a plain click cycles its three lines.
	RoomScript &script = *_engine.getScript();
	if (object == "OBJ_INV_BALL_YELLOW") {
		const int movie = _engine.getGameFlag("YellowBallFound") ? kMovieBallAgain : kMovieBallFirst;
		_engine.requestRoom(kWell, movie);
		_engine.setGameFlag("YellowBallFound", true);
		_returnPending = true;
		debugC(1, kDebugScript, "well: the yellow ball, movie %d", movie);
		return;
	}
	if (object == "OBJ_P22_WELL") {
		if (success) {
			_engine.requestRoom(kWell, kMovieWellRope);
			_engine.getInventory()->removeItem("OBJ_INV_ROPE");
			debugC(1, kDebugScript, "well: the rope on the well, movie %d", kMovieWellRope);
			return;
		}
		script.startScene(kClickLines[_clickCycle], true);
		_clickCycle = _clickCycle == 2 ? 0 : _clickCycle + 1;
	}
}

// ---------------------------------------------------------------------------

RoomClass *createMazeInteriorRoom(TerraglyphEngine &engine, const Common::String &room) {
	if (room == kMazeCenter)
		return new MazeCenterRoom(engine);
	if (room == kFountain)
		return new FountainCloseupRoom(engine);
	if (room == kWell)
		return new WellInMazeRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
