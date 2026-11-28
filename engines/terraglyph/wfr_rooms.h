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

#ifndef TERRAGLYPH_WFR_ROOMS_H
#define TERRAGLYPH_WFR_ROOMS_H

#include "common/array.h"

#include "terraglyph/room_class.h"

namespace Terraglyph {

/**
 * Phantom's Wizard's Rule Room, ROOM_P30_WFR (0x41BEA0, vtable 0x45EE7C): a
 * rhythm game on the wizard's song (Wizards_Rule.Music). The song's play
 * position (0x4358D0, Sound::getClipMillis) at 1.3795 beats a second is the
 * clock; a beat counts in the first 0.49 of it. Each 16-beat phrase has eight
 * entries in the table at 0x46C5B0 (0-5 a bottle, 6 a gap): on beats 0-7 the
 * wizard dances and lights them while Scooby watches; on beats 8-15 the player
 * clicks the same bottles on the same beats while Scooby dances. A beat missed
 * breaks a glass and spends a life (Incorrect); a phrase played clean is a
 * snack (Correct). Out of lives, or the song over, back to the wizard's
 * bedroom, through movie 0xB while his clue is not yet the gang's (0x41BF30).
 *
 * Its slots: tick 0x41C3A0, enter 0x41C0C0, leave 0x41C350, claim 0x41CF50,
 * the ways out 0x41BF30. Scooby and the wizard are the dancers (0x41BAC0,
 * vtable 0x45EE54: update 0x41BC50, modes 0x41BC80).
 */
class WizardsRuleRoom : public RoomClass {
public:
	/** A dancer (0x41BAC0): ANIM_WFR_<name><suffix> by its mode (+0x5C). */
	struct Dancer {
		int sprite = -1;
		Common::String name; // +0x60: SCOOBY or WIZARD
		int mode = 0;        // +0x5C: 0 idle, 1 TURN_DANCE, 2 TURN_WATCH, 3 DANCE<value>, 4 WATCH
		int value = 0;       // +0x68: the dance DANCE<value> plays
	};

	explicit WizardsRuleRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override;                                // 0x41C0C0
	void roomLeft() override;                                                        // 0x41C350
	void tick() override;                                                            // 0x41C3A0
	void claimPlacement(const Common::String &object, Room::Claim &claim) override;  // 0x41CF50
	bool click(int sprite) override;
	void exitMovie(const Common::String &destination, int &movie) override;         // 0x41BF30

	enum { kBottles = 6, kGap = 6, kTableSize = 304 };
	static const char *const kColourClip[kBottles]; // Wizards_Rule.Red .. Brown
	static const char *const kColourStem[kBottles]; // RED .. BROWN
	/** 0x46C5B0: the bottle (0-5) or a gap (6) for @p beat. */
	static int bottleForBeat(uint32 beat);

	// The state a check reads.
	int getBottle(int i) const { return i >= 0 && i < kBottles ? _bottle[i] : -1; }
	const Dancer &getScooby() const { return _scooby; }
	const Dancer &getWizard() const { return _wizard; }
	int getSkullsWizard() const { return _skullsWizard; }
	int getSkullsScooby() const { return _skullsScooby; }
	int getTens() const { return _counter[0]; }
	int getOnes() const { return _counter[1]; }
	const Common::Array<int> &getLives() const { return _lives; }
	int getWrong() const { return _wrong; }
	bool isFailed() const { return _failed; }
	bool isAllGood() const { return _allGood; }
	uint32 getLastBeat() const { return _lastBeat; }
	const Common::String &getSounds() const { return _sounds; }
	void clearSounds() { _sounds.clear(); }
	/** For a check: the song's position in milliseconds (-1 over), -2 for the real one. */
	void setClockForCheck(int32 millis) { _clockForCheck = millis; }
	/** For a check: every rand() the room draws returns @p value (-1: the engine's). */
	void setRandomForCheck(int value) { _forcedRandom = value; }

private:
	void setMode(Dancer &d, int mode);          // 0x41BC80
	void dance(Dancer &d, int value);           // 0x41BBD0
	void turnWatch(Dancer &d);                  // 0x41BC10
	void updateDancer(Dancer &d);               // 0x41BC50
	void lightBottle(int bottle, bool lit);
	void allBottles(bool lit);
	void snack();
	void leaveToBedroom();
	int drawRandom();
	void playSound(const Common::String &name, bool restart = false);

	Dancer _scooby, _wizard;                    // +0x110, +0x114
	int _bottle[kBottles] = { -1, -1, -1, -1, -1, -1 }; // +0xF8..+0x10C: red, green, blue, pink, yellow, brown
	int _skullsWizard = -1, _skullsScooby = -1; // +0xF0, +0xF4: they mouth the song and the colours
	int _counter[2] = { -1, -1 };               // +0xE8, +0xEC: the snacks, tens the smaller x
	int _livesAnchor = -1;                      // +0x120: the placed life
	Common::Array<int> _lives;                  // +0x120, then +0x124..: 5 less the difficulty
	Common::String _colourClip;                 // +0xBC: the colour last called
	int _dances[8] = {};                        // +0xC0..+0xDC: a phrase's dances, rand() % 6 + 1
	bool _handled = true;                       // +0xE0: this beat done with
	bool _allGood = true;                       // +0xE1: no beat missed since the player's half began
	bool _off = true;                           // +0xE2: between beats
	uint32 _lastBeat = 0xFFFFFFFF;              // +0xE4
	int _wrong = 0;                             // +0x118: phrases missed
	bool _failed = false;                       // +0x11C: this phrase missed
	bool _canScore = true;                      // +0x11D: the phrase's snack not yet given
	int _lightTimer = 0;                        // +0x134: ticks a lit bottle stays lit
	int _clicked = -1;                          // the sprite clicked since the last tick
	int32 _clockForCheck = -2;
	int _forcedRandom = -1;
	Common::String _sounds;
};

/** The class for this room, or nullptr. */
RoomClass *createWizardsRuleRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_WFR_ROOMS_H
