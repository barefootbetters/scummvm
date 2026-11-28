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
 * Phantom's Wizard's Rule Room (WP-083), ROOM_P30_WFR (class 0x41BEA0, vtable
 * 0x45EE7C, the game's largest at 1022 lines) — the fourth and last of the
 * audit's group-8 (Upstairs) rooms. Showdown has no such room.
 *
 * It is a Simon-says potion game. The wizard sets a rule: a number (on a
 * two-digit counter) and a run of coloured ingredients he DANCEs in turn while
 * the gang WATCHes; the player repeats it by clicking the ingredients. A right
 * run plays Wizards_Rule.Correct and advances; a wrong one plays
 * Wizards_Rule.Incorrect (one of three) and Knights.Glass_Breaks and spends a
 * life (the lives are difficulty-scaled). Out of lives, or the run finished,
 * returns the gang to the wizard's bedroom (ROOM_P24_Wizards_Bedroom), where —
 * on the return from here — the wizard's clue is given and the accusation armed
 * (WP-081). The leave direction is gated on owning the wizard's clue.
 *
 * The original drives the spinner off a lip-sync scene's fractional playback
 * position (0x4358D0); the fork's scene player exposes no fractional position,
 * so the spinner here advances on its own tick counter over the same
 * position->ingredient table (0x46C5B0), the way Maria's game (WP-064) drives
 * its physics on a tick counter rather than the original's scene clock. The
 * contract — the classes, the clips, the claims, the music, the required
 * placements, the exit and the clue gate — is what the self-test checks; the
 * moment-to-moment feel of the reconstructed loop is for a human to confirm
 * live (as with WP-081's from-WFR arming path).
 */
class WizardsRuleRoom : public RoomClass {
public:
	/**
	 * A WFR object. Scooby and the wizard are the animated sub-class (0x41BAC0,
	 * vtable 0x45EE54) whose mode (+0x5C) plays an ANIM_WFR_<name><suffix>
	 * (0x41BC80): 0 the idle, 1 TURN_DANCE, 2 TURN_WATCH, 3 DANCE<value>, 4
	 * WATCH. The six coloured bottles share one OBJ_WFR_BOTTLES placement each,
	 * told apart by the colour in their ANIM (the original's claim strips the
	 * anim to compare RED/GREEN/BLUE/PINK/YELLOW/BROWN); they are plain lit
	 * sprites, not dancers.
	 */
	struct Obj {
		int sprite = -1;
		Common::String name; // the stem used for the dancers: SCOOBY or WIZARD
		Common::String anim; // a bottle's display ANIM (its colour), lit on cue
		int mode = 0;        // +0x5C (the dancers)
		int value = 0;       // +0x68: the step DANCE<value> plays
	};

	explicit WizardsRuleRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override;                                // 0x41C0C0
	void tick() override;                                                            // 0x41C3A0
	void reset() override;                                                           // 0x41C350
	void claimPlacement(const Common::String &object, Room::Claim &claim) override;  // 0x41CF50
	bool click(int sprite) override;
	bool isBusy() const override { return _busy; }

	// 0x41BF30 (+0x24, the leave builder) wraps the base walk-off only to pick a
	// transition-movie variant (movie 0xb) when the wizard's clue is not yet
	// owned. The engine's leaving() hook exposes the scene, not that transition
	// movie, and the destination (the bedroom) and every gameplay contract are
	// the same either way, so the variant is left to a later pass rather than
	// faked here; see the WP-083 Result.

	// For the check.
	int getColour(int i) const { return (i >= 0 && i < 6) ? _colour[i].sprite : -1; }
	int getScooby() const { return _scooby.sprite; }
	int getWizard() const { return _wizard.sprite; }
	int getLives() const { return (int)_lives.size(); }
	bool hasCounter() const { return _counter[0] >= 0 && _counter[1] >= 0; }
	int getRound() const { return _round; }
	/** 0x46C5B0: the spinner position's ingredient (0-5), or 6 for a gap. */
	static int positionIngredient(int pos);

private:
	enum Phase { kShow, kRepeat, kDone };

	void setObjMode(Obj &obj, int mode);
	void lightBottle(int colour);
	void playSound(const Common::String &name);
	void showCounter();
	void startRound();
	void loseLife();
	void leaveToBedroom();
	int colourAt(int sprite) const;

	static const int kPositions = 48;
	static const int kPositionTable[kPositions];
	static const char *const kColourClip[6]; // Wizards_Rule.Red .. Brown

	// The six ingredients in colour order: Red, Green, Blue, Pink, Yellow, Brown.
	Obj _colour[6];
	Obj _scooby;
	Obj _wizard;
	int _counter[2] = { -1, -1 };  // +0xE8/+0xEC: the two counter-number sprites
	Common::Array<int> _lives;     // +0x124..: OBJ_WFR_LIVES, difficulty-scaled

	// The Simon run. The wizard DANCEs _sequence in turn (kShow) over the
	// position table's cadence, then the player repeats it by clicking (kRepeat).
	Common::Array<int> _sequence;
	Phase _phase = kShow;
	int _showAt = 0;       // the sequence index being danced
	int _inputAt = 0;      // the sequence index awaited from the player
	int _pos = 0;          // the spinner's position (0..47)
	int _stepTimer = 0;    // +0x4D: ticks the current position holds
	int _round = 0;        // the run's length / the rule number on the counter
	int _wrong = 0;        // +0x46: wrong runs so far
	int _wrongMax = 5;     // 5 - difficulty: wrong runs that end the game
	bool _busy = false;    // game +0x24: input held (the wizard dancing)
	bool _click = false;
	int _clicked = -1;
};

/** The class for this room, or nullptr. */
RoomClass *createWizardsRuleRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_WFR_ROOMS_H
