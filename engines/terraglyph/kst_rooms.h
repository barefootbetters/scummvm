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

#ifndef TERRAGLYPH_KST_ROOMS_H
#define TERRAGLYPH_KST_ROOMS_H

#include "common/array.h"

#include "terraglyph/room.h"
#include "terraglyph/room_class.h"
#include "terraglyph/room_script.h"

namespace Terraglyph {

/**
 * Phantom's Knights of the Snack Table, ROOM_P31_KST (0x419700, vtable
 * 0x45EDEC): the serving game Phantom gives in place of Showdown's Pie Noon
 * (its asserts still name Pie Noon). Three benches run from the knights' door
 * on the left to Shaggy's tap on the right. Shaggy follows the mouse up and
 * down to the nearest bench; a press fills a mug and the release slides it
 * along the bench. A knight walks in along a bench toward the tap, catches a
 * full mug that reaches him, and drinks while it pushes him back to the door;
 * there he drops the empty mug, which slides back to the tap for Shaggy to
 * catch, and he leaves or comes again. A full mug that passes the door end,
 * an empty one that falls off the tap end, or a knight that reaches the tap
 * costs a mug: the round starts again, one mug fewer. A round's knights all
 * served (one more each round) earns a snack. Out of mugs, the dining room.
 *
 * The room's four slots: tick 0x419E40, enter 0x419A80, leave 0x419CC0,
 * claim 0x41A970. Its sprites update themselves in the tree walk: Shaggy
 * (0x4192F0, vtable 0x45EDB4, update 0x419370), each knight (0x418CA0,
 * vtable 0x45ED78, update 0x418EE0), each full mug (0x418710, 0x45ED20,
 * update 0x418860) and each empty mug (0x4189B0, 0x45ED4C, update 0x418B10).
 * The room keeps three lists: the full mugs sliding (+0xCC), the empty mugs
 * (+0xD4) and the mugs left, drawn on the counter (+0xE4).
 */
class KstRoom : public RoomClass, public Room::Actor {
public:
	explicit KstRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override;                                // 0x419A80
	void roomLeft() override;                                                        // 0x419CC0
	void tick() override;                                                            // 0x419E40
	void claimPlacement(const Common::String &object, Room::Claim &claim) override;  // 0x41A970
	void actorUpdate(uint sprite) override;
	bool actorPlays(uint sprite) override;
	// 0x419E40: the cursor shown only over the top 100 rows (0x435EF0 / 0x435E80).
	bool hidesCursor() const override;

	enum { kKnightSlots = 6, kMarkers = 3, kPauseTicks = 36 };

	// The marker arrays (0x41A970), each sorted ascending by y (the primary
	// array keeps the y, the parallel one the x); the knights' x is the
	// placement x less 40, the gang entering from off the left. Bench-floor
	// keeps only its y.
	uint getKnightCount() const { return _knightCount; }
	float getKnightX(uint i) const { return _knightX[i]; }
	float getKnightY(uint i) const { return _knightY[i]; }
	float getBenchEndX(uint i) const { return _benchEndX[i]; }
	float getBenchEndY(uint i) const { return _benchEndY[i]; }
	float getBenchFloorY(uint i) const { return _benchFloorY[i]; }
	float getDrinkX(uint i) const { return _drinkX[i]; }
	float getDrinkY(uint i) const { return _drinkY[i]; }
	float getBenchStartX(uint i) const { return _benchStartX[i]; }
	float getBenchStartY(uint i) const { return _benchStartY[i]; }
	float getShaggyX() const { return _shaggyX; }
	float getShaggyY() const { return _shaggyY; }
	/** +0xEC after entry: Shaggy's x off the tap line. */
	float getShaggyOffset() const { return _offsetX; }

	// The sprites the claim built.
	int getOverlay(uint i) const { return i < 3 ? _overlays[i] : -1; }
	int getTens() const { return _tens; }
	int getOnes() const { return _ones; }
	const Common::String &getNarrator() const { return _narrator; }

	/** A knight (0x418CA0): two to a bench, slots i and i + 3 on bench i. */
	struct Knight {
		int sprite = -1;          // the body
		int arm = -1;             // +0x80: the arm, drawn at the body's step (0x419160)
		uint bench = 0;           // +0x68: slot % 3
		int state = 4;            // +0x60: 0 drinks, 1 walks, 2 starts drinking, 3 finishes, 4 gone
		float x = 0, y = 0;       // +0x0C, +0x10
		float reach = 0;          // +0x64: its step a tick, (bench-end x - door x) x 0.005
		Common::String walks[2];  // +0x70, +0x78: Knights.Knight_Walks.(1-5)
		uint walk = 0;            // +0x6C: which of the two plays
		bool stepLatch = true;    // +0x84: the walk's sound waits for step 0
		Common::String gulp;      // +0x8C: Knights.Knight_Gulp.(bench + 1)
	};
	/** A mug sliding: a full one (0x418710) or an empty one (0x4189B0). */
	struct Mug {
		int sprite = -1;
		uint bench = 0;           // full +0x60, empty +0x64
		int state = 0;            // full +0x64: 0 slides, 1 breaks, 2 broken; empty +0x60: 0 slides, 1 falls, 2 breaks, 3 broken
		float x = 0, y = 0;
		float speed = 0;          // +0x68: (bench-end x - bench-start x) x 0.01 a tick
		float fall = 0;           // empty +0x6C: (floor y - bench-end y) x 0.1 a tick
	};

	// The state the check reads.
	int getShaggy() const { return _shaggy; }
	float getShaggyNowX() const { return _sx; }
	float getShaggyNowY() const { return _sy; }
	int getShaggyState() const { return _shaggyState; }
	bool isGrabbing() const { return _grab; }
	const Knight &getKnight(uint slot) const { return _knights[slot]; }
	const Common::Array<Mug> &getFullMugs() const { return _full; }
	const Common::Array<Mug> &getEmptyMugs() const { return _empty; }
	const Common::Array<int> &getMugsLeft() const { return _icons; }
	int getLives() const { return _lives; }
	int getPause() const { return _pause; }
	int getRoundPause() const { return _roundPause; }
	int getRound() const { return _round; }
	int getSpawned() const { return _spawned; }
	bool isNarrating() const { return _narrating; }
	const Common::String &getSounds() const { return _sounds; }
	void clearSounds() { _sounds.clear(); }
	/** For a check: end the narrator's hold, as its line ending would. */
	void endIntroForCheck() { _narrating = false; }
	/** For a check: every rand() the room draws returns @p value (-1: the engine's). */
	void setRandomForCheck(int value) { _forcedRandom = value; }

private:
	void startRound();                    // 0x41A5C0
	void createKnight(uint slot);         // 0x418CA0
	void setKnightState(Knight &k, int state); // 0x4191B0
	void walkIn(Knight &k);               // 0x419110
	void updateKnight(Knight &k);         // 0x418EE0
	void syncArm(const Knight &k);        // 0x419160
	void updateShaggy();                  // 0x419370
	void shaggyDrop();                    // 0x419680
	void shaggySuccess();                 // 0x4196D0
	float shaggyX(float y) const;         // 0x41B2E0
	void slideMug(uint bench);            // 0x41A2B0
	void dropEmpty(float x, uint bench);  // 0x41A370
	bool takeFull(float x, uint bench);   // 0x41A410
	bool catchEmpty(uint bench);          // 0x41A820
	void updateFull(Mug &m);              // 0x418860
	void updateEmpty(Mug &m);             // 0x418B10
	void miss() { _missed = true; }       // 0x419E30
	void clearMugs(Common::Array<Mug> &mugs);
	void showCounter();
	void placeSprite(int sprite, float x, float y);
	/** The sorted insert of 0x41A970: @p primary kept ascending, @p parallel following. */
	void insertMarker(float *primary, float *parallel, uint &count, float key, float val);
	int layer0() const;
	int drawRandom();
	bool paused() const { return _pause > 0 || _roundPause > 0; }
	void playSound(const Common::String &name);

	// The marker arrays (claimPlacement): each primary holds its sorted y, the
	// parallel its x.
	float _knightY[kMarkers] = {}, _knightX[kMarkers] = {};        // +0x114, +0x108 (x less 40)
	uint _knightCount = 0;                                         // +0x18C
	float _benchEndY[kMarkers] = {}, _benchEndX[kMarkers] = {};    // +0x144, +0x138
	uint _benchEndCount = 0;                                       // +0x184
	float _benchFloorY[kMarkers] = {};                             // +0x150 (y only)
	uint _benchFloorCount = 0;                                     // +0x188
	float _drinkY[kMarkers] = {}, _drinkX[kMarkers] = {};          // +0x168, +0x15C
	uint _drinkCount = 0;                                          // +0x190
	float _benchStartY[kMarkers] = {}, _benchStartX[kMarkers] = {}; // +0x12C, +0x120
	uint _benchStartCount = 0;                                     // +0x180
	float _shaggyX = 0, _shaggyY = 0;                              // Shaggy's post
	bool _hasShaggy = false;
	float _offsetX = 0;                                            // +0xEC: Shaggy's x off the tap line

	// The sprites the claim built.
	int _overlays[3] = { -1, -1, -1 };  // +0x174, +0x178, +0x17C
	int _tens = -1, _ones = -1;         // +0xB8, +0xBC: the counter digits, tens = smaller x
	uint _numbers = 0;

	// Shaggy (+0xC4).
	int _shaggy = -1;
	float _sx = 0, _sy = 0;             // +0x0C, +0x10
	int _shaggyState = 0;               // +0x68: 0 free, 1 fills, 2 full, 3 slides, 4 back to idle
	int _dir = 2;                       // +0x60: 0 walks away, 1 walks forward, 2 idle
	bool _grab = false;                 // +0x64: his grab playing
	uint _fillBench = 0;                // +0x6C: the bench he fills for

	// The round.
	Knight _knights[kKnightSlots];      // +0xF0..+0x104
	Common::Array<Mug> _full, _empty;   // +0xCC, +0xD4: newest first
	Common::Array<int> _icons;          // +0xE4: the mugs left, on the counter
	int _lives = 0;                     // +0xE8: five less the difficulty
	int _pause = 0;                     // +0xC0: after a miss, then the round again
	bool _missed = false;               // +0x194
	int _round = 1;                     // +0x198: the round's knights
	int _spawned = 0;                   // +0x19C
	int _spawnTimer = 0;                // +0x1A0
	int _roundPause = 0;                // +0x1A4: after a round served
	bool _narrating = false;            // +0x1A8 set: the narrator's intro holds the game
	Common::String _narrator;           // Knights_of_the_Snack_Table.350X_1_NARRATOR
	int _forcedRandom = -1;
	Common::String _sounds;
};

/** Knights of the Snack Table's room class for @p room, or nullptr. */
RoomClass *createKstRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_KST_ROOMS_H
