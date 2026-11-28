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

// Phantom of the Knight's Wizard's Rule Room (WP-083). The addresses are
// Phantom's; Showdown has no such room. See wfr_rooms.h for the mechanic and
// the one porting judgment (the spinner runs on a tick counter, not the
// original's scene-position clock).

#include "common/debug.h"

#include "terraglyph/detection.h"
#include "terraglyph/room.h"
#include "terraglyph/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"
#include "terraglyph/wfr_rooms.h"

namespace Terraglyph {

static const char *const kWFR = "ROOM_P30_WFR";
static const char *const kBedroom = "ROOM_P24_Wizards_Bedroom";
static const char *const kWizardSuspect = "WIZARD";
static const char *const kMusic = "Wizards_Rule.Music";

// The six colours, in order: the OBJ_WFR_<name> stem matched in a placement's
// name (0x41CF50), and the Wizards_Rule.<Colour> line (0x41C470 cases).
static const char *const kColourStem[6] = { "RED", "GREEN", "BLUE", "PINK", "YELLOW", "BROWN" };
const char *const WizardsRuleRoom::kColourClip[6] = {
	"Wizards_Rule.Red", "Wizards_Rule.Green", "Wizards_Rule.Blue",
	"Wizards_Rule.Pink", "Wizards_Rule.Yellow", "Wizards_Rule.Brown"
};

// 0x46C5B0: the spinner position (0..47) -> ingredient (0..5), 6 a gap. Groups
// of eight: three live stops then five gaps.
const int WizardsRuleRoom::kPositionTable[WizardsRuleRoom::kPositions] = {
	0, 1, 2, 6, 6, 6, 6, 6,
	3, 4, 0, 6, 6, 6, 6, 6,
	5, 2, 4, 6, 6, 6, 6, 6,
	0, 0, 5, 6, 6, 6, 6, 6,
	1, 2, 3, 6, 6, 6, 6, 6,
	4, 0, 1, 6, 6, 6, 6, 6
};

int WizardsRuleRoom::positionIngredient(int pos) {
	if (pos < 0 || pos >= kPositions)
		return 6;
	return kPositionTable[pos];
}

void WizardsRuleRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x41CF50 classifies every WFR placement. The six coloured bottles (one
	// OBJ_WFR_BOTTLES placement each, told apart by their ANIM colour) are what
	// the player clicks to repeat the run, so they are clickable; the rest
	// (Scooby, the wizard, the counter, the table, the skulls, the lives and the
	// exit) are display sprites the base builds by default.
	if (object == "OBJ_WFR_BOTTLES")
		claim.build = Room::Claim::kClickable;
}

void WizardsRuleRoom::playSound(const Common::String &name) {
	Sound &sound = *_engine.getSound();
	sound.stopClip(name);
	sound.play(name);
}

int WizardsRuleRoom::colourAt(int sprite) const {
	for (int c = 0; c < 6; c++) {
		if (_colour[c].sprite == sprite)
			return c;
	}
	return -1;
}

void WizardsRuleRoom::setObjMode(Obj &obj, int mode) {
	// 0x41BC80, the dancers (Scooby, the wizard): play ANIM_WFR_<name><suffix>;
	// suffix by mode: 0 the idle, 1 TURN_DANCE, 2 TURN_WATCH, 3 DANCE<value>, 4
	// WATCH.
	if (obj.sprite < 0)
		return;
	obj.mode = mode;
	Common::String anim = "ANIM_WFR_" + obj.name;
	switch (mode) {
	case 1: anim += "TURN_DANCE"; break;
	case 2: anim += "TURN_WATCH"; break;
	case 3: anim += Common::String::format("DANCE%d", obj.value); obj.value = 0; break;
	case 4: anim += "WATCH"; break;
	default: break;
	}
	_engine.getRoom()->startAnim(obj.sprite, anim, 1, 3);
}

void WizardsRuleRoom::lightBottle(int colour) {
	// A bottle is a plain sprite (not a dancer); lighting it replays its colour
	// ANIM, and the colour's line (Wizards_Rule.<Colour>) and WFR.Potion play.
	if (colour < 0 || colour >= 6 || _colour[colour].sprite < 0)
		return;
	Room &room = *_engine.getRoom();
	if (!_colour[colour].anim.empty())
		room.startAnim(_colour[colour].sprite, _colour[colour].anim, 1, 0);
}

void WizardsRuleRoom::showCounter() {
	// 0x41C0C0 / 0x41C6xx: the two counter digits show the round number (its
	// tens and ones), the way the original seeds them from the game's rule.
	Room &room = *_engine.getRoom();
	if (_counter[0] >= 0)
		room.setPlayer(_counter[0], 0, (_round / 10) % 10, 3);
	if (_counter[1] >= 0)
		room.setPlayer(_counter[1], 0, _round % 10, 3);
}

void WizardsRuleRoom::startRound() {
	// The wizard's run grows with the round. Each colour is rand() % 6; the
	// wizard will DANCE them in turn (kShow) while the gang WATCHes.
	_sequence.clear();
	const int len = _round + 1;
	for (int i = 0; i < len; i++)
		_sequence.push_back((int)_engine.getRandomNumber(5));
	_phase = kShow;
	_showAt = 0;
	_inputAt = 0;
	_pos = 0;
	_stepTimer = 12; // +0x4D: the original holds each stop a dozen ticks.
	_busy = true;
	setObjMode(_wizard, 1); // TURN_DANCE
	setObjMode(_scooby, 2); // TURN_WATCH
	showCounter();
	debugC(1, kDebugScript, "wfr: round %d, run of %u", _round, _sequence.size());
}

void WizardsRuleRoom::loseLife() {
	// A wrong run: Incorrect (one of three) and Knights.Glass_Breaks, and a life
	// spent (0x41C7xx). Out of lives ends the game, back to the bedroom.
	Room &room = *_engine.getRoom();
	const int which = (int)_engine.getRandomNumber(2);
	playSound(which == 0 ? "Wizards_Rule.Incorrect_1_NARRATOR"
	        : which == 1 ? "Wizards_Rule.Incorrect2_1_NARRATOR"
	                     : "Wizards_Rule.Incorrect3_1_NARRATOR");
	playSound("Knights.Glass_Breaks.1");
	if (!_lives.empty()) {
		room.setHidden(_lives.back(), true);
		_lives.pop_back();
	}
	_wrong++;
	debugC(1, kDebugScript, "wfr: wrong run %d of %d", _wrong, _wrongMax);
	if (_wrong >= _wrongMax || _lives.empty())
		_phase = kDone;
	else
		startRound();
}

void WizardsRuleRoom::leaveToBedroom() {
	// 0x41C481 / 0x41C6xx: the run finished, or the lives gone, returns the gang
	// to the wizard's bedroom, where the clue is given and the accusation armed
	// (WP-081). The transition-movie variant by clue ownership (0x41BF30) is not
	// reproduced; see the header.
	RoomScript::Exit exit;
	exit.destination = kBedroom;
	exit.movie = -1;
	_engine.getScript()->takeExit(exit);
	_phase = kShow;
	debugC(1, kDebugScript, "wfr: leaving to the wizard's bedroom");
}

void WizardsRuleRoom::enter(const Common::String &from) {
	// 0x41C0C0, after the base entry: the required placements wired and checked,
	// the lives built (difficulty-scaled), the counter seeded and the music up,
	// and the wizard's first run begun.
	Room &room = *_engine.getRoom();
	for (int c = 0; c < 6; c++)
		_colour[c] = Obj();
	_scooby = Obj();
	_scooby.name = "SCOOBY";
	_wizard = Obj();
	_wizard.name = "WIZARD";
	_counter[0] = _counter[1] = -1;

	// The six bottles share OBJ_WFR_BOTTLES; the colour is in each one's ANIM
	// (0x41CF50 strips it to compare RED/GREEN/BLUE/PINK/YELLOW/BROWN). Scooby
	// and the wizard are the dancers; two OBJ_WFR_COUNTER sprites with the
	// ANIM_WFR_COUNTERNUMBERS animation are the number digits.
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		const Common::String &obj = room.getObjectName(s);
		if (obj == "OBJ_WFR_SCOOBY") {
			_scooby.sprite = s;
		} else if (obj == "OBJ_WFR_WIZARD") {
			_wizard.sprite = s;
		} else if (obj == "OBJ_WFR_BOTTLES") {
			const Common::String anim = room.getAnim(s);
			for (int c = 0; c < 6; c++) {
				if (_colour[c].sprite < 0 && anim.contains(kColourStem[c])) {
					_colour[c].sprite = s;
					_colour[c].name = kColourStem[c];
					_colour[c].anim = anim;
					break;
				}
			}
		} else if (obj == "OBJ_WFR_COUNTER" && room.getAnim(s).contains("COUNTERNUMBERS")) {
			if (_counter[0] < 0)
				_counter[0] = s;
			else if (_counter[1] < 0)
				_counter[1] = s;
		}
	}

	// 0x41C0C0's asserts (FUN_00434A70): every required placement must be here.
	int bottles = 0;
	for (int c = 0; c < 6; c++)
		if (_colour[c].sprite >= 0)
			bottles++;
	if (bottles < 6)
		debugC(1, kDebugScript, "wfr: a bottle is missing (%d of 6)", bottles);
	if (_wizard.sprite < 0)
		debugC(1, kDebugScript, "wfr: the wizard is missing");
	if (_scooby.sprite < 0)
		debugC(1, kDebugScript, "wfr: Scooby is missing");
	if (!hasCounter())
		debugC(1, kDebugScript, "wfr: the two counter numbers are missing");

	// 0x41C1xx: beyond the placed OBJ_WFR_LIVES, difficulty-scaled extras — four
	// less the difficulty, at least one. Clones of the base lives sprite.
	_lives.clear();
	int baseLives = -1;
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s).contains("OBJ_WFR_LIVES")) {
			baseLives = s;
			break;
		}
	}
	const int lives = MAX(1, 4 - (int)_engine.getDifficulty());
	_wrongMax = MAX(1, 5 - (int)_engine.getDifficulty());
	if (baseLives >= 0) {
		_lives.push_back(baseLives);
		for (int i = 1; i < lives; i++) {
			const int sp = room.addEngineSprite(room.getLayerSprite(0), "OBJ_WFR_LIVES", "ANIM_WFR_LIVESA",
			                                    0, 20 + i * 24, 440, 1, "OBJ_WFR_LIVES");
			_lives.push_back(sp);
		}
	}

	_round = 1;
	_wrong = 0;
	playSound(kMusic);
	startRound();
	debugC(1, kDebugScript, "wfr: from %s, %d bottles, %d lives, wrongMax %d", from.c_str(), bottles, (int)_lives.size(), _wrongMax);
}

void WizardsRuleRoom::reset() {
	// 0x41C350: back to how the factory built it — the run and the wired objects
	// cleared, the counter and the state zeroed.
	for (int c = 0; c < 6; c++)
		_colour[c] = Obj();
	_scooby = Obj();
	_wizard = Obj();
	_counter[0] = _counter[1] = -1;
	_lives.clear();
	_sequence.clear();
	_phase = kShow;
	_showAt = _inputAt = _pos = _stepTimer = 0;
	_round = _wrong = 0;
	_busy = false;
	_click = false;
	_clicked = -1;
}

bool WizardsRuleRoom::click(int sprite) {
	// The player repeats the run by clicking the coloured ingredients. A click
	// on one is the game's (taken); the room reads it on the next tick.
	_click = true;
	_clicked = sprite;
	return colourAt(sprite) >= 0;
}

void WizardsRuleRoom::tick() {
	// 0x41C3A0, after the base tick. The wizard DANCEs the run a colour at a
	// time (kShow); then the player repeats it by clicking (kRepeat). The cursor
	// opens while the player may click.
	Room &room = *_engine.getRoom();
	const bool click = _click;
	const int clicked = _clicked;
	_click = false;
	_clicked = -1;

	if (_phase == kDone) {
		leaveToBedroom();
		return;
	}

	// The open-hand cursor over the ingredients while the player may click.
	for (int c = 0; c < 6; c++)
		if (_colour[c].sprite >= 0)
			room.setRollover(_colour[c].sprite, _phase == kRepeat ? "ANIM_CURSOROPEN" : "");

	if (_phase == kShow) {
		_busy = true; // input held while the wizard dances
		if (--_stepTimer > 0)
			return;
		_stepTimer = 12;
		if (_showAt < (int)_sequence.size()) {
			const int c = _sequence[_showAt++];
			_wizard.value = _showAt;
			setObjMode(_wizard, 3); // the wizard DANCEs the step
			lightBottle(c);         // the bottle lights
			playSound(kColourClip[c]);
			playSound(Common::String::format("WFR.Potion_%d", _showAt));
			// Advance the spinner over its position table in step (0x46C5B0).
			_pos = (_pos + 1) % kPositions;
			debugC(2, kDebugScript, "wfr: show %d/%u colour %s (pos %d -> %d)", _showAt, _sequence.size(), kColourStem[c], _pos, positionIngredient(_pos));
		} else {
			// The run shown: the gang WATCHes and the player may repeat it.
			_phase = kRepeat;
			_inputAt = 0;
			_busy = false;
			setObjMode(_wizard, 2); // TURN_WATCH
			setObjMode(_scooby, 4); // WATCH
			debugC(1, kDebugScript, "wfr: repeat the run of %u", _sequence.size());
		}
		return;
	}

	// kRepeat: a click on a colour is the player's next answer.
	if (click && clicked >= 0) {
		const int c = colourAt(clicked);
		if (c >= 0) {
			lightBottle(c);
			playSound(kColourClip[c]);
			if (c == _sequence[_inputAt]) {
				_inputAt++;
				if (_inputAt >= (int)_sequence.size()) {
					// The whole run right: Wizards_Rule.Correct and the next,
					// longer round, until the game is won.
					playSound(Common::String::format("Wizards_Rule.Correct%d_1_NARRATOR", _round));
					_round++;
					debugC(1, kDebugScript, "wfr: run matched, round %d", _round);
					if (_round > 3 + (int)_engine.getDifficulty())
						_phase = kDone;
					else
						startRound();
				}
			} else {
				loseLife();
			}
		}
	}
}

RoomClass *createWizardsRuleRoom(TerraglyphEngine &engine, const Common::String &room) {
	if (room == kWFR)
		return new WizardsRuleRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
