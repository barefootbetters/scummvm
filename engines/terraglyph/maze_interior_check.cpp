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

// The script-level check of Phantom's maze interior (WP-075) — the centre, the
// fountain close-up and the well — against an independent reference written
// from the trace. Showdown has none of these rooms.

#include "common/debug.h"

#include "terraglyph/art_archive.h"
#include "terraglyph/detection.h"
#include "terraglyph/inventory.h"
#include "terraglyph/maze_interior_rooms.h"
#include "terraglyph/room.h"
#include "terraglyph/room_class.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"

namespace Terraglyph {

static const char *buildName(Room::Claim::Build b) {
	switch (b) {
	case Room::Claim::kDefault: return "default";
	case Room::Claim::kSkip: return "skip";
	case Room::Claim::kRequires: return "requires";
	case Room::Claim::kClickable: return "clickable";
	case Room::Claim::kSprite: return "sprite";
	case Room::Claim::kInventory: return "inventory";
	case Room::Claim::kExit: return "exit";
	}
	return "?";
}

void TerraglyphEngine::checkMazeInterior() {
	const char *const kMazeCenter = "ROOM_P23_Maze_Center";
	const char *const kFountain = "ROOM_P44_Fountain_Closeup";
	const char *const kWell = "ROOM_P22_Well_In_Maze";

	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kMazeCenter;
	if (!found) {
		debugC(3, kDebugScript, "maze interior check: no maze interior");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);

	// sfx and effect clips ship under their exact name; the numbered scene lines
	// ship with a speaker suffix (Center_of_Maze.250B_1_VELMA), so they match by
	// prefix — reading exactly would say "found 0" of a name that is right (the
	// WP-074 trap), so the voice lines are probed by prefix.
	auto has = [&](const char *name) { return _sound->findClip(name) != nullptr ? 1 : 0; };
	auto pfx = [&](const char *name) { return _sound->hasClipPrefix(name) ? 1 : 0; };

	// The classes the factory builds.
	{
		RoomClass *a = createRoomClass(*this, kMazeCenter);
		RoomClass *b = createRoomClass(*this, kFountain);
		RoomClass *c = createRoomClass(*this, kWell);
		debugC(3, kDebugScript, "maze interior check classes: Maze_Center %s, Fountain %s, Well %s",
		       dynamic_cast<MazeCenterRoom *>(a) ? "centre" : a ? "other" : "-",
		       dynamic_cast<FountainCloseupRoom *>(b) ? "fountain" : b ? "other" : "-",
		       dynamic_cast<WellInMazeRoom *>(c) ? "well" : c ? "other" : "-");
		delete a;
		delete b;
		delete c;
	}

	// A claim measured after setting the engine's inventory/flags.
	auto claimOf = [&](RoomClass *rc, const char *object) {
		Room::Claim claim;
		rc->claimPlacement(object, claim);
		return claim;
	};

	// -----------------------------------------------------------------------
	// The maze centre.
	{
		RoomClass *rc = createRoomClass(*this, kMazeCenter);
		debugC(3, kDebugScript, "maze interior check Maze_Center clips: water %s vol %d found %d; shears %s %d, scene %s %d; hotspot4 %s %d %s %d; hotspot %s %d %s %d",
		       MazeCenterRoom::kWaterSfx, (int)MazeCenterRoom::kWaterVolume, has(MazeCenterRoom::kWaterSfx),
		       MazeCenterRoom::kTakeShears, has(MazeCenterRoom::kTakeShears), MazeCenterRoom::kShearsScene, pfx(MazeCenterRoom::kShearsScene),
		       MazeCenterRoom::kHotspot4Lines[0], pfx(MazeCenterRoom::kHotspot4Lines[0]), MazeCenterRoom::kHotspot4Lines[1], pfx(MazeCenterRoom::kHotspot4Lines[1]),
		       MazeCenterRoom::kHotspotLines[0], pfx(MazeCenterRoom::kHotspotLines[0]), MazeCenterRoom::kHotspotLines[1], pfx(MazeCenterRoom::kHotspotLines[1]));

		newGameForCheck();
		const Room::Claim water = claimOf(rc, "OBJ_P23_MAZE_CENTER_WATER");
		const Room::Claim shearsLoose = claimOf(rc, "OBJ_INV_RUSTY_SHEARS");
		const Room::Claim p44None = claimOf(rc, "OBJ_P23_TO_P44");
		_inventory->addItem("OBJ_INV_RUSTY_SHEARS", "ANIM_TOOLBARRUSTY_SHEARS", "ANIM_CURSORRUSTY_SHEARS");
		const Room::Claim shearsHeld = claimOf(rc, "OBJ_INV_RUSTY_SHEARS");
		_inventory->addItem("OBJ_INV_TOKENS", "ANIM_TOOLBARTOKENS", "ANIM_CURSORTOKENS");
		const Room::Claim p44Tokens = claimOf(rc, "OBJ_P23_TO_P44");
		debugC(3, kDebugScript, "maze interior check Maze_Center claims: WATER %s d%d; RUSTY_SHEARS loose %s held %s; TO_P44 none %s tokens %s",
		       buildName(water.build), water.depth, buildName(shearsLoose.build), buildName(shearsHeld.build),
		       buildName(p44None.build), buildName(p44Tokens.build));

		// The hedge-maze start flag by entry source, and the Entrances save key.
		MazeCenterRoom *mc = dynamic_cast<MazeCenterRoom *>(rc);
		Common::INIFile yes;
		yes.setKey("Entrances", kMazeCenter, "Yes");
		mc->loadKeys(yes);
		const bool read = mc->hasEntrances();
		mc->reset();
		const bool afterReset = mc->hasEntrances();
		Common::INIFile none;
		mc->loadKeys(none);
		debugC(3, kDebugScript, "maze interior check Maze_Center state: garden %d, balcony %d, maze %d; Entrances read %d, reset %d, missing %d",
		       MazeCenterRoom::entryFlagFor("ROOM_P06_Garden"), MazeCenterRoom::entryFlagFor("ROOM_P27_Below_Balcony"),
		       MazeCenterRoom::entryFlagFor("ROOM_Hedge_Maze"), read ? 1 : 0, afterReset ? 1 : 0, mc->hasEntrances() ? 1 : 0);
		delete rc;
	}

	// -----------------------------------------------------------------------
	// The fountain close-up.
	{
		RoomClass *rc = createRoomClass(*this, kFountain);
		debugC(3, kDebugScript, "maze interior check Fountain clips: opening %s %d; cycle %s %d %s %d %s %d; exit %s %d; water %s %d; swim %s %d; grab %s %d; lunge %s %d; eats %s %d",
		       FountainCloseupRoom::kOpening, pfx(FountainCloseupRoom::kOpening),
		       FountainCloseupRoom::kClickLines[0], pfx(FountainCloseupRoom::kClickLines[0]), FountainCloseupRoom::kClickLines[1], pfx(FountainCloseupRoom::kClickLines[1]),
		       FountainCloseupRoom::kClickLines[2], pfx(FountainCloseupRoom::kClickLines[2]), FountainCloseupRoom::kExitLine, pfx(FountainCloseupRoom::kExitLine),
		       FountainCloseupRoom::kWater, has(FountainCloseupRoom::kWater), FountainCloseupRoom::kSwimLoop, has(FountainCloseupRoom::kSwimLoop),
		       FountainCloseupRoom::kGrabTokens, has(FountainCloseupRoom::kGrabTokens), FountainCloseupRoom::kLunge, has(FountainCloseupRoom::kLunge),
		       FountainCloseupRoom::kEats, has(FountainCloseupRoom::kEats));

		newGameForCheck();
		const Room::Claim piranhaIdle = claimOf(rc, "OBJ_PIRANHA");
		const Room::Claim tokensLoose = claimOf(rc, "OBJ_INV_TOKENS");
		const Room::Claim toP23 = claimOf(rc, "OBJ_P44_TO_P23");
		const Room::Claim grab = claimOf(rc, "OBJ_SHAGGY_P44_GRAB_TOKENS");
		_inventory->addItem("OBJ_INV_BURGER", "ANIM_TOOLBARBURGER", "ANIM_CURSORBURGER");
		_inventory->removeItem("OBJ_INV_BURGER"); // used: in the history, out of the slots
		const Room::Claim piranhaFull = claimOf(rc, "OBJ_PIRANHA");
		_inventory->addItem("OBJ_INV_TOKENS", "ANIM_TOOLBARTOKENS", "ANIM_CURSORTOKENS");
		const Room::Claim tokensHeld = claimOf(rc, "OBJ_INV_TOKENS");
		// The piranha's sequence is set at entry (WP-093): the Fountain piranha
		// and after lines read it.
		debugC(3, kDebugScript, "maze interior check Fountain claims: PIRANHA idle %s layer %d d%d, burger used %s layer %d d%d; TOKENS loose %s d%d cursor %s held %s; TO_P23 %s d%d; GRAB %s layer %d d%d hidden %d",
		       buildName(piranhaIdle.build), piranhaIdle.layer, piranhaIdle.depth, buildName(piranhaFull.build), piranhaFull.layer, piranhaFull.depth,
		       buildName(tokensLoose.build), tokensLoose.depth, tokensLoose.required.c_str(), buildName(tokensHeld.build),
		       buildName(toP23.build), toP23.depth, buildName(grab.build), grab.layer, grab.depth, grab.hidden ? 1 : 0);
		delete rc;
	}

	// -----------------------------------------------------------------------
	// The well.
	{
		RoomClass *rc = createRoomClass(*this, kWell);
		debugC(3, kDebugScript, "maze interior check Well clips: cycle %s %d %s %d %s %d",
		       WellInMazeRoom::kClickLines[0], pfx(WellInMazeRoom::kClickLines[0]), WellInMazeRoom::kClickLines[1], pfx(WellInMazeRoom::kClickLines[1]),
		       WellInMazeRoom::kClickLines[2], pfx(WellInMazeRoom::kClickLines[2]));

		newGameForCheck();
		const Room::Claim ballFresh = claimOf(rc, "OBJ_INV_BALL_YELLOW");
		const Room::Claim p21Dry = claimOf(rc, "OBJ_P22_TO_P21");
		const Room::Claim ropeDry = claimOf(rc, "OBJ_P22_WELL_ROPE");
		const Room::Claim wellDry = claimOf(rc, "OBJ_P22_WELL");
		setGameFlag("YellowBallFound", true);
		const Room::Claim ballUsed = claimOf(rc, "OBJ_INV_BALL_YELLOW");
		_inventory->addItem("OBJ_INV_ROPE", "ANIM_TOOLBARROPE", "ANIM_CURSORROPE");
		_inventory->removeItem("OBJ_INV_ROPE"); // used
		const Room::Claim p21Wet = claimOf(rc, "OBJ_P22_TO_P21");
		const Room::Claim ropeWet = claimOf(rc, "OBJ_P22_WELL_ROPE");
		const Room::Claim wellWet = claimOf(rc, "OBJ_P22_WELL");
		debugC(3, kDebugScript, "maze interior check Well claims: BALL_YELLOW fresh %s used %s; TO_P21 dry %s rope-used %s; WELL_ROPE dry %s rope-used %s d%d; WELL dry %s rope-used %s",
		       buildName(ballFresh.build), buildName(ballUsed.build), buildName(p21Dry.build), buildName(p21Wet.build),
		       buildName(ropeDry.build), buildName(ropeWet.build), ropeWet.depth, buildName(wellDry.build), buildName(wellWet.build));
		debugC(3, kDebugScript, "maze interior check Well movies: ball first %d again %d; well+rope %d; return gallery %d other %d; links %s",
		       (int)WellInMazeRoom::kMovieBallFirst, (int)WellInMazeRoom::kMovieBallAgain, (int)WellInMazeRoom::kMovieWellRope,
		       (int)WellInMazeRoom::kMovieReturnGallery, (int)WellInMazeRoom::kMovieReturnOther, WellInMazeRoom::kGallery);
		delete rc;
	}

	// The fountain's piranha (WP-093), played through the engine's tick:
	// 0x429CC0's requires object, 0x429800's idle, lunge, eat and grab,
	// 0x429B50's click and burger.
	{
		Common::String next;
		int movie;
		auto tick1 = [&]() {
			next.clear();
			movie = -1;
			return tickOnce(next, movie);
		};
		auto fountain = [&]() { return dynamic_cast<FountainCloseupRoom *>(getRoomClass(kFountain)); };
		auto visit = [&]() {
			newGameForCheck();
			_sound->stop();
			_mouseDown = false;
			fountain()->reset();
			showRoom(kFountain);
			_script->skip();
		};
		Room &room = *_currentRoom;

		// A fresh visit: the idle once, the lunge reset left pending, then the
		// idle again.
		visit();
		FountainCloseupRoom *f = fountain();
		int p = f->getPiranha();
		Common::String out = Common::String::format("requires %d hit %d depth %d, %s mode %d, lunge pending %d, grab hidden %d mode %d",
		                                            room.isRequiresObject(p) ? 1 : 0, room.getHitType(p), room.getDepth(p), room.getAnim(p).c_str(),
		                                            room.getMode(p), f->isLungePending() ? 1 : 0, room.isHidden(f->getGrab()) ? 1 : 0,
		                                            room.getMode(f->getGrab()));
		int lunge = -1, again = -1;
		for (int t = 1; t <= 400 && again < 0; t++) {
			tick1();
			if (lunge < 0 && room.getAnim(p) == FountainCloseupRoom::kLungeAttack) {
				lunge = t;
				out += Common::String::format("; lunges at tick %d", t);
			} else if (lunge > 0 && room.getAnim(p) == FountainCloseupRoom::kPiranhaIdle) {
				again = t;
				out += Common::String::format("; idle again at tick %d, lunge pending %d", t, f->isLungePending() ? 1 : 0);
			}
		}
		debugC(3, kDebugScript, "maze interior check Fountain piranha: %s", out.c_str());

		// A plain click: the next line, holding input, and a lunge pending.
		visit();
		f = fountain();
		tick1();
		const bool pendingBefore = f->isLungePending();
		f->event("OBJ_PIRANHA", false);
		out = Common::String::format("lunge pending %d then %d, scene %s, cycle %d", pendingBefore ? 1 : 0, f->isLungePending() ? 1 : 0,
		                             _script->getScene().c_str(), f->getClickCycle());
		_script->skip();
		f->event("OBJ_INV_TOKENS", false);
		out += Common::String::format("; on the tokens: scene %s, cycle %d", _script->getScene().c_str(), f->getClickCycle());
		debugC(3, kDebugScript, "maze interior check Fountain click: %s", out.c_str());

		// The burger: the eat, Shaggy's grab, the tokens, the way back.
		visit();
		f = fountain();
		p = f->getPiranha();
		_inventory->addItem("OBJ_INV_BURGER");
		f->event("OBJ_PIRANHA", true);
		bool burgerSlotted = false;
		for (uint i = 0; i < Inventory::kSlotCount; i++)
			burgerSlotted = burgerSlotted || _inventory->getSlot(i) == "OBJ_INV_BURGER";
		out = Common::String::format("burger in a slot %d, %s, busy %d cursor hidden %d", burgerSlotted ? 1 : 0, room.getAnim(p).c_str(),
		                             f->isBusy() ? 1 : 0, f->hidesCursor() ? 1 : 0);
		int grab = -1, tokens = -1, back = -1;
		Common::String scenes, last;
		for (int t = 1; t <= 2000 && back < 0; t++) {
			tick1();
			if (grab < 0 && !room.isHidden(f->getGrab())) {
				grab = t;
				out += Common::String::format("; grab at tick %d (piranha hidden %d, tokens hidden %d)", t, room.isHidden(p) ? 1 : 0,
				                              f->getTokens() >= 0 && room.isHidden(f->getTokens()) ? 1 : 0);
			}
			if (tokens < 0 && grab > 0 && _inventory->hasItem("OBJ_INV_TOKENS")) {
				tokens = t;
				out += Common::String::format("; tokens at tick %d (%s, grab hidden %d, busy %d)", t, room.getAnim(p).c_str(),
				                              room.isHidden(f->getGrab()) ? 1 : 0, f->isBusy() ? 1 : 0);
			}
			const Common::String s = _script->getScene();
			if (!s.empty() && s != last) {
				scenes += (scenes.empty() ? "" : " ") + s;
				last = s;
				_script->skip();
			}
			if (!next.empty())
				back = t;
		}
		out += Common::String::format("; scenes %s; to %s", scenes.empty() ? "none" : scenes.c_str(), next.empty() ? "-" : next.c_str());
		debugC(3, kDebugScript, "maze interior check Fountain burger: %s", out.c_str());

		// Back after: the piranha full, looping; the tokens gone.
		showRoom(kFountain);
		_script->skip();
		f = fountain();
		debugC(3, kDebugScript, "maze interior check Fountain after: %s mode %d, tokens placed %d",
		       room.getAnim(f->getPiranha()).c_str(), room.getMode(f->getPiranha()), f->getTokens() >= 0 ? 1 : 0);
	}

	// The well's Jester (WP-092): the member at +0xB8 is the Jester the
	// gallery and the room of doors keep (0x4122D0), entered by 0x42C810
	// (0x412B80) unless the visit bounces to the gallery, and ticked by
	// 0x42C7F0 (0x4122E0).
	{
		Common::String next;
		int movie;
		auto tick1 = [&]() {
			next.clear();
			movie = -1;
			return tickOnce(next, movie);
		};
		auto well = [&]() { return dynamic_cast<WellInMazeRoom *>(getRoomClass(kWell)); };
		auto jester = [&]() { return _script->getCharacter("JESTER"); };
		auto story = [&]() {
			newGameForCheck();
			_sound->stop();
			_mouseDown = false;
		};
		// Run ticks, skipping each scene as it starts; the scenes in order.
		auto scenes = [&](uint maxTicks) {
			Common::String out, last;
			for (uint t = 0; t < maxTicks; t++) {
				tick1();
				const Common::String s = _script->getScene();
				if (!s.empty() && s != last) {
					out += Common::String::format("%s%s", out.empty() ? "" : " ", s.c_str());
					last = s;
					_script->skip();
				} else if (s.empty()) {
					last.clear();
				}
				if (!next.empty())
					break;
			}
			return out.empty() ? Common::String("none") : out;
		};

		// A visit with no ball found: he stands, and a click on him gives one
		// of his five lines.
		story();
		showRoom(kWell);
		_script->skip();
		Common::String out = Common::String::format("present %d", jester() ? 1 : 0);
		if (jester()) {
			well()->click(jester()->getSprite());
			tick1();
			const Common::String line = _script->getScene();
			const bool isLine = line == "Global.1010J_" || line == "Global.1010J1_" || line == "Global.Jester1_" || line == "Global.Jester2_" ||
			                    line == "Global.Jester3_";
			out += Common::String::format("; a click: one of his five lines %d", isLine ? 1 : 0);
		}
		debugC(3, kDebugScript, "maze interior check Well jester: %s", out.c_str());

		// The yellow ball shown, the full bottle used: the well again, no
		// bounce; he takes it, and with the red and blue already his, gives
		// his clue, then thanks and the suspects and clues.
		story();
		setGameString("Whodunit", "MARIA");
		setGameFlag("RedBallFound", true);
		setGameFlag("RedBallTaken", true);
		setGameFlag("BlueBallFound", true);
		setGameFlag("BlueBallTaken", true);
		_inventory->addItem("OBJ_INV_BOTTLE_FULL");
		_inventory->removeItem("OBJ_INV_BOTTLE_FULL"); // used
		showRoom(kWell);
		_script->skip();
		well()->event("OBJ_INV_BALL_YELLOW", true);
		const bool pending = well()->isReturnPending();
		showRoom(kWell);
		const bool taken = getGameFlag("YellowBallTaken");
		const Common::String anim = jester() ? _currentRoom->getAnim(jester()->getSprite()) : Common::String("-");
		out = Common::String::format("bottle used: return pending %d, YellowBallTaken %d, Jester %s; ", pending ? 1 : 0, taken ? 1 : 0, anim.c_str());
		const Common::String clue = scenes(4000);
		out += Common::String::format("scenes %s; owner %d; then to %s", clue.c_str(), isClueOwner("JESTER") ? 1 : 0, next.c_str());
		debugC(3, kDebugScript, "maze interior check Well jester clue: %s", out.c_str());

		// The yellow ball shown, no full bottle: the visit bounces to the
		// gallery (movie 57 from the well) before he enters; he takes nothing
		// here, and the ticks before the bounce leave him alone.
		story();
		showRoom(kWell);
		_script->skip();
		well()->event("OBJ_INV_BALL_YELLOW", true);
		showRoom(kWell);
		bool bounced = false;
		int bounceMovie = -1;
		for (int t = 0; t < 10 && !bounced; t++) {
			tick1();
			if (next == WellInMazeRoom::kGallery) {
				bounced = true;
				bounceMovie = movie;
			}
		}
		debugC(3, kDebugScript, "maze interior check Well jester bounce: to %s movie %d, YellowBallTaken %d, busy %d",
		       bounced ? WellInMazeRoom::kGallery : "-", bounceMovie, getGameFlag("YellowBallTaken") ? 1 : 0, well()->isBusy() ? 1 : 0);
	}

	_checking = false;
}

} // End of namespace Terraglyph
