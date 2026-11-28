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

// Phantom of the Knight's Wizard's Rule Room (WP-083, the game WP-094). The
// addresses are Phantom's; Showdown has no such room.

#include "common/debug.h"
#include "common/util.h"

#include "terraglyph/detection.h"
#include "terraglyph/inventory.h"
#include "terraglyph/room.h"
#include "terraglyph/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"
#include "terraglyph/wfr_rooms.h"

namespace Terraglyph {

static const char *const kWFR = "ROOM_P30_WFR";
static const char *const kBedroom = "ROOM_P24_Wizards_Bedroom";
static const char *const kMusic = "Wizards_Rule.Music";

// The song's beats a second (0x45EED0, a double) and the part of each beat
// that counts (0x45EEC0, 0.49 as a float widened).
static const double kBeatsPerSecond = 1.3795;
static const double kBeatWindow = (double)0.49f;
// A lit bottle stays lit 12 ticks (+0x134).
static const int kLightTicks = 12;
// The movie the ways out take while the wizard's clue is not the gang's (0x41BF30).
static const int kMovieClueNotOwned = 0xB;

const char *const WizardsRuleRoom::kColourClip[kBottles] = {
	"Wizards_Rule.Red", "Wizards_Rule.Green", "Wizards_Rule.Blue",
	"Wizards_Rule.Pink", "Wizards_Rule.Yellow", "Wizards_Rule.Brown"
};
const char *const WizardsRuleRoom::kColourStem[kBottles] = { "RED", "GREEN", "BLUE", "PINK", "YELLOW", "BROWN" };

// 0x46C5B0: eight entries a phrase of 16 beats, the bottle (0-5) or a gap (6)
// for beats 0-3 and 4-7, which the player repeats on 8-11 and 12-15.
static const int kTable[WizardsRuleRoom::kTableSize] = {
	0, 1, 2, 6, 6, 6, 6, 6,
	3, 4, 0, 6, 6, 6, 6, 6,
	5, 2, 4, 6, 6, 6, 6, 6,
	0, 0, 5, 6, 6, 6, 6, 6,
	1, 2, 3, 6, 6, 6, 6, 6,
	4, 0, 1, 6, 6, 6, 6, 6,
	2, 3, 5, 4, 6, 6, 6, 6,
	5, 0, 2, 3, 6, 6, 6, 6,
	4, 2, 2, 0, 6, 6, 6, 6,
	3, 1, 4, 5, 6, 6, 6, 6,
	0, 3, 4, 2, 6, 6, 6, 6,
	5, 5, 0, 1, 6, 6, 6, 6,
	4, 4, 1, 0, 2, 6, 6, 6,
	2, 0, 2, 0, 5, 6, 6, 6,
	3, 1, 2, 5, 0, 6, 6, 6,
	5, 2, 0, 4, 1, 6, 6, 6,
	0, 0, 5, 2, 4, 6, 6, 6,
	1, 1, 0, 2, 3, 6, 6, 6,
	2, 2, 0, 0, 4, 4, 6, 6,
	3, 2, 3, 2, 3, 0, 6, 6,
	5, 2, 1, 1, 0, 3, 6, 6,
	4, 2, 1, 3, 0, 5, 6, 6,
	0, 5, 2, 2, 5, 1, 6, 6,
	1, 0, 0, 1, 4, 2, 6, 6,
	0, 0, 2, 2, 4, 1, 3, 6,
	3, 4, 4, 3, 1, 0, 2, 6,
	5, 2, 4, 2, 2, 0, 0, 6,
	2, 1, 3, 0, 1, 1, 5, 6,
	4, 3, 4, 3, 3, 3, 4, 6,
	1, 2, 0, 5, 5, 5, 1, 6,
	1, 4, 3, 5, 4, 3, 5, 1,
	0, 3, 0, 2, 0, 1, 0, 2,
	3, 1, 3, 2, 3, 0, 4, 3,
	4, 4, 4, 4, 0, 2, 3, 5,
	5, 1, 1, 5, 2, 2, 5, 4,
	2, 4, 5, 1, 1, 5, 4, 0,
	6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6,
};

int WizardsRuleRoom::bottleForBeat(uint32 beat) {
	// The bar's beat, the phrase's half, the phrase.
	const uint32 index = (beat & 3) + (((beat >> 2) & 1) + (beat >> 4) * 2) * 4;
	return index < (uint32)kTableSize ? kTable[index] : (int)kGap;
}

int WizardsRuleRoom::drawRandom() {
	return _forcedRandom >= 0 ? _forcedRandom : (int)_engine.getRandomNumber(0x7FFF);
}

void WizardsRuleRoom::playSound(const Common::String &name, bool restart) {
	Sound &sound = *_engine.getSound();
	if (restart)
		sound.stopClip(name);
	const bool found = sound.play(name);
	if (!_sounds.empty())
		_sounds += ", ";
	_sounds += Common::String::format("%s found %d", name.c_str(), found ? 1 : 0);
}

void WizardsRuleRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x41CF50. The six bottles (one OBJ_WFR_BOTTLES placement each, told apart
	// by the colour after ANIM_WFR_BOTTLES) are plain sprites at depth 10,
	// stopped, hit by their step polygon (+4 = 2): the room reads a click on
	// one itself. Scooby, the wizard, the skulls, the lives and the counter
	// keep the base path, as WP-083 drew them.
	if (object == "OBJ_WFR_BOTTLES") {
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = 10;
		claim.hitType = 2;
	}
}

// ---------------------------------------------------------------------------
// The dancers

void WizardsRuleRoom::setMode(Dancer &d, int mode) {
	// 0x41BC80: ANIM_WFR_<name>IDLE looping; TURN_DANCE, TURN_WATCH,
	// DANCE<value> and WATCH once; 3 ticks a step.
	d.mode = mode;
	if (d.sprite < 0)
		return;
	Common::String anim = "ANIM_WFR_" + d.name;
	switch (mode) {
	case 0: anim += "IDLE"; break;
	case 1: anim += "TURN_DANCE"; break;
	case 2: anim += "TURN_WATCH"; break;
	case 3: anim += Common::String::format("DANCE%d", d.value); d.value = 0; break;
	case 4: anim += "WATCH"; break;
	default: return;
	}
	_engine.getRoom()->startAnim(d.sprite, anim, mode == 0 ? 1 : 3, -1, 3);
}

void WizardsRuleRoom::dance(Dancer &d, int value) {
	// 0x41BBD0: idle or watching, he turns to dance it; dancing, the next once
	// the last is over.
	if (d.mode == 0 || d.mode == 4) {
		d.value = value;
		setMode(d, 1);
	} else if (d.mode == 3 && d.sprite >= 0 && _engine.getRoom()->getMode(d.sprite) == 0) {
		d.value = value;
		setMode(d, 3);
	}
}

void WizardsRuleRoom::turnWatch(Dancer &d) {
	// 0x41BC10: idle or dancing, he turns to watch.
	if (d.mode == 0 || d.mode == 3)
		setMode(d, 2);
}

void WizardsRuleRoom::updateDancer(Dancer &d) {
	// 0x41BC50: a turn over, the dance or the watching.
	if (d.sprite < 0 || _engine.getRoom()->getMode(d.sprite) != 0)
		return;
	if (d.mode == 1)
		setMode(d, 3);
	else if (d.mode == 2)
		setMode(d, 4);
}

// ---------------------------------------------------------------------------
// The room

void WizardsRuleRoom::lightBottle(int bottle, bool lit) {
	if (bottle >= 0 && bottle < kBottles && _bottle[bottle] >= 0)
		_engine.getRoom()->setPlayer(_bottle[bottle], 0, lit ? 1 : 0, 3);
}

void WizardsRuleRoom::allBottles(bool lit) {
	for (int b = 0; b < kBottles; b++)
		lightBottle(b, lit);
}

void WizardsRuleRoom::snack() {
	// A phrase played clean: a snack (0x407E00), the counter, and a Correct line.
	Room &room = *_engine.getRoom();
	Inventory &inventory = *_engine.getInventory();
	_canScore = false;
	inventory.setSnacks(inventory.getSnacks() + 1);
	const uint n = inventory.getSnacks();
	if (_counter[0] >= 0)
		room.setPlayer(_counter[0], 0, ((n / 10) % 10 + 9) % 10, 3);
	if (_counter[1] >= 0)
		room.setPlayer(_counter[1], 0, (n % 10 + 9) % 10, 3);
	playSound(Common::String::format("Wizards_Rule.Correct%d_1_NARRATOR", drawRandom() % 5 + 1));
	debugC(1, kDebugScript, "wfr: a phrase played clean, %u snacks", n);
}

void WizardsRuleRoom::leaveToBedroom() {
	// The song over or the lives gone: an exit to the wizard's bedroom through
	// the room's ways out (0x404820, +0x24), which pick its movie. Once.
	if (_engine.getScript()->isBusy())
		return;
	RoomScript::Exit exit;
	exit.destination = kBedroom;
	exit.movie = -1;
	_engine.getScript()->takeExit(exit);
	debugC(1, kDebugScript, "wfr: back to the wizard's bedroom");
}

void WizardsRuleRoom::exitMovie(const Common::String &destination, int &movie) {
	// 0x41BF30: any way out takes movie 0xB while the wizard's clue isn't owned.
	if (!_engine.isClueOwner("WIZARD"))
		movie = kMovieClueNotOwned;
}

void WizardsRuleRoom::enter(const Common::String &from) {
	// 0x41C0C0, after the base entry.
	Room &room = *_engine.getRoom();
	_scooby = Dancer();
	_scooby.name = "SCOOBY";
	_wizard = Dancer();
	_wizard.name = "WIZARD";
	for (int b = 0; b < kBottles; b++)
		_bottle[b] = -1;
	_skullsWizard = _skullsScooby = -1;
	_counter[0] = _counter[1] = -1;
	_livesAnchor = -1;
	_lives.clear();
	_sounds.clear();
	_clicked = -1;
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		const Common::String &obj = room.getObjectName(s);
		const Common::String &anim = room.getAnim(s);
		if (room.isEngineSprite(s))
			continue;
		if (obj == "OBJ_WFR_SCOOBY") {
			_scooby.sprite = s;
		} else if (obj == "OBJ_WFR_WIZARD") {
			_wizard.sprite = s;
		} else if (obj == "OBJ_WFR_BOTTLES") {
			for (int b = 0; b < kBottles; b++) {
				if (anim == Common::String("ANIM_WFR_BOTTLES") + kColourStem[b])
					_bottle[b] = s;
			}
		} else if (obj == "OBJ_WFR_SKULLS") {
			if (anim == "ANIM_WFR_SKULLSWIZARD")
				_skullsWizard = s;
			else if (anim == "ANIM_WFR_SKULLSSCOOBY")
				_skullsScooby = s;
		} else if (obj == "OBJ_WFR_LIVES") {
			_livesAnchor = s;
		} else if (obj == "OBJ_WFR_COUNTER" && anim == "ANIM_WFR_COUNTERNUMBERS") {
			if (_counter[0] < 0) {
				_counter[0] = s;
			} else if (_counter[1] < 0) {
				_counter[1] = s;
				if (room.getOffsetX(_counter[1]) < room.getOffsetX(_counter[0]))
					SWAP(_counter[0], _counter[1]);
			}
		}
	}
	for (int b = 0; b < kBottles; b++) {
		if (_bottle[b] < 0) {
			warning("There is a bottle missing from Wizard's Rule Room");
			continue;
		}
		room.setPlayer(_bottle[b], 0, 0, 3);
		// The cursor opens over a bottle (0x41C3A0, below row 240).
		room.setRollover(_bottle[b], "ANIM_CURSOROPEN");
		int bx, by, bw, bh;
		if (room.getFrameRect(_bottle[b], bx, by, bw, bh))
			debugC(2, kDebugScript, "wfr: the %s bottle at %d,%d, %dx%d", kColourStem[b], bx, by, bw, bh);
	}
	if (_wizard.sprite < 0)
		warning("WFR: Where's the wizard?");
	if (_scooby.sprite < 0)
		warning("WFR: Where's Scooby?");
	if (_counter[1] < 0)
		warning("2 numbers should be placed in WFR");
	// The skulls stopped on their first step, to mouth the song and the colours.
	if (_skullsWizard >= 0)
		room.setPlayer(_skullsWizard, 0, 0, 3);
	if (_skullsScooby >= 0)
		room.setPlayer(_skullsScooby, 0, 0, 3);
	setMode(_scooby, 0);
	setMode(_wizard, 0);

	// The counter shows the snacks, each cel one back (cel 0 draws a 1).
	const uint n = _engine.getInventory()->getSnacks();
	if (_counter[0] >= 0)
		room.setPlayer(_counter[0], 0, ((n / 10) % 10 + 9) % 10, 3);
	if (_counter[1] >= 0)
		room.setPlayer(_counter[1], 0, (n % 10 + 9) % 10, 3);

	_handled = _allGood = _off = true;
	_canScore = true;
	_lastBeat = 0xFFFFFFFF;
	_colourClip.clear();
	_wrong = 0;
	_failed = false;
	_lightTimer = 0;
	for (int i = 0; i < 8; i++)
		_dances[i] = 0;

	// The lives: the placed one, and four less the difficulty more, each under
	// the last and a life's width to its left; five less the difficulty in all.
	if (_livesAnchor >= 0) {
		_lives.push_back(_livesAnchor);
		int w = 0, h = 0;
		room.getSize(_livesAnchor, w, h);
		const int extra = 4 - _engine.getDifficulty();
		int parent = _livesAnchor;
		for (int i = 0; i < extra; i++) {
			const int s = room.addEngineSprite(parent, "OBJ_WFR_LIVES", "ANIM_WFR_LIVESA", 0, -w, 0, 1, "wfr life");
			room.startAnim(s, "ANIM_WFR_LIVESA", 1, 0, 3);
			_lives.push_back(s);
			parent = s;
		}
	}

	playSound(kMusic);
	debugC(1, kDebugScript, "wfr: from %s, %u lives", from.c_str(), _lives.size());
}

void WizardsRuleRoom::roomLeft() {
	// 0x41C350, after the base leave: the placements to be read again.
	for (int b = 0; b < kBottles; b++)
		_bottle[b] = -1;
	_scooby.sprite = _wizard.sprite = -1;
	_skullsWizard = _skullsScooby = -1;
	_counter[0] = _counter[1] = -1;
	_livesAnchor = -1;
	_lives.clear();
}

bool WizardsRuleRoom::click(int sprite) {
	// The room's tick reads a click on a bottle (its hover, +0x08, and the
	// click, game +0x20 bit 2).
	for (int b = 0; b < kBottles; b++) {
		if (sprite >= 0 && sprite == _bottle[b]) {
			_clicked = sprite;
			return true;
		}
	}
	return false;
}

void WizardsRuleRoom::tick() {
	// 0x41C3A0, after the base tick.
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	const int clicked = _clicked;
	_clicked = -1;
	updateDancer(_scooby);
	updateDancer(_wizard);

	// The skulls mouth the song and the colour last called (0x435A70).
	uint shape = 0;
	if (_skullsWizard >= 0 && sound.getLipShape(kMusic, shape) && shape != 0)
		room.setPlayer(_skullsWizard, 0, shape - 1, 3);
	if (_skullsScooby >= 0 && !_colourClip.empty() && sound.getLipShape(_colourClip, shape) && shape != 0)
		room.setPlayer(_skullsScooby, 0, shape - 1, 3);

	// The clock: the song's position (0x4358D0). Over, back to the bedroom.
	const int32 millis = _clockForCheck != -2 ? _clockForCheck : sound.getClipMillis(kMusic);
	if (millis < 0) {
		leaveToBedroom();
		return;
	}
	const double beatf = millis / 1000.0 * kBeatsPerSecond;
	const uint32 beat = (uint32)(int64)beatf;
	if (--_lightTimer == 0)
		allBottles(false);
	// Late in a beat: the bottles dark, once.
	if (ABS(beatf - (double)beat) >= kBeatWindow) {
		if (!_off) {
			allBottles(false);
			_off = true;
		}
		return;
	}
	// A new beat: was the last one done with?
	if (_off || _lastBeat != beat) {
		_allGood = _allGood && _handled;
		allBottles(false);
		_off = false;
		_handled = false;
		_lastBeat = beat;
		debugC(2, kDebugScript, "wfr: beat %u at %d ms, %s, bottle %d", beat, millis, ((beat >> 3) & 1) ? "the player's" : "the wizard's",
		       bottleForBeat(beat));
	}
	if (_handled)
		return;

	const uint32 bar = beat & 3;           // the beat in its bar
	const bool second = (beat >> 2) & 1;   // the phrase's second bar
	const bool repeat = (beat >> 3) & 1;   // the player's half
	const int bottle = bottleForBeat(beat);
	const int danced = _dances[(second ? 4 : 0) + bar];

	// A phrase's first beat: its dances drawn; a phrase missed counts, and
	// enough of them end the game. Each bar's first beat, the dancers idle.
	if (bar == 0) {
		if (!repeat && !second) {
			for (int i = 0; i < 8; i++)
				_dances[i] = drawRandom() % 6 + 1;
			if (_failed) {
				_failed = false;
				_wrong++;
				debugC(1, kDebugScript, "wfr: %d phrases missed of %d", _wrong, 5 - _engine.getDifficulty());
				if (_wrong >= 5 - _engine.getDifficulty())
					leaveToBedroom();
			}
		}
		setMode(_wizard, 0);
		setMode(_scooby, 0);
	}

	bool wizardsBeat = false;
	if (!second || bar < 2) {
		if (repeat) {
			// The player's beats: the wizard dances on; Scooby dances along, or
			// a beat has been missed, and he stops and a glass breaks.
			dance(_wizard, danced);
			if (!_allGood) {
				setMode(_scooby, 0);
				if (!_failed) {
					_failed = true;
					const int life = 4 - _engine.getDifficulty() - _wrong;
					if (life >= 0 && life < (int)_lives.size())
						room.setHidden(_lives[life], true);
					playSound("Knights.Glass_Breaks.1");
					const int r = drawRandom() % 3;
					playSound(r == 0 ? "Wizards_Rule.Incorrect_1_NARRATOR" : r == 1 ? "Wizards_Rule.Incorrect2_1_NARRATOR"
					                                                         : "Wizards_Rule.Incorrect3_1_NARRATOR");
					debugC(1, kDebugScript, "wfr: a beat missed, a glass breaks");
				}
			} else {
				dance(_scooby, danced);
			}
		} else {
			// The wizard's beats: he dances, Scooby turns to watch.
			dance(_wizard, danced);
			turnWatch(_scooby);
			wizardsBeat = true;
		}
	} else {
		// The second bar's last two beats: the dances over, idle.
		if (_wizard.sprite >= 0 && room.getMode(_wizard.sprite) == 0)
			setMode(_wizard, 0);
		if (_scooby.sprite >= 0 && room.getMode(_scooby.sprite) == 0)
			setMode(_scooby, 0);
		wizardsBeat = !repeat;
	}

	if (!wizardsBeat) {
		// The player's beat: a gap is done with (and, nothing missed, the
		// phrase's snack); else the beat's bottle clicked lights it and calls
		// its colour and a potion.
		if (bottle == kGap) {
			_handled = true;
			if (_canScore && !_failed)
				snack();
			return;
		}
		if (clicked < 0 || clicked != _bottle[bottle])
			return;
		_handled = true;
		_lightTimer = kLightTicks;
		lightBottle(bottle, true);
		_colourClip = kColourClip[bottle];
		playSound(Common::String::format("WFR.Potion_%d", bottle + 1), true);
		playSound(_colourClip, true);
		return;
	}

	// The wizard's beat: the phrase's first beat, nothing missed, is the last
	// phrase's snack if not yet given; the bar before the player's turn starts
	// it clean; the beat's bottle lit.
	if (bar == 0) {
		if (!second && _allGood && beat != 0) {
			if (_canScore && !_failed)
				snack();
			_canScore = true;
		}
	} else if (bar == 3 && second) {
		_allGood = true;
	}
	if (bottle != kGap) {
		_lightTimer = kLightTicks;
		lightBottle(bottle, true);
	}
	_handled = true;
}

RoomClass *createWizardsRuleRoom(TerraglyphEngine &engine, const Common::String &room) {
	if (room == kWFR)
		return new WizardsRuleRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
