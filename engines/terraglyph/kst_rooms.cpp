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

#include "common/debug.h"
#include "common/util.h"

#include "terraglyph/detection.h"
#include "terraglyph/inventory.h"
#include "terraglyph/kst_rooms.h"
#include "terraglyph/room.h"
#include "terraglyph/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"

namespace Terraglyph {

static const char *const kKstRoom = "ROOM_P31_KST";

static const int kStopped = 0;
static const int kLoop = 1;
static const int kOnce = 3;

// The knight's x enters 40 off the placement, from off the left (0x45EE50).
static const float kKnightXOffset = 40.0f;
// The mugs left: from (550, 40) stepping left by 35 (0x41A5C0, 0x45EE48).
static const float kMugsLeftX = 550.0f;
static const float kMugsLeftY = 40.0f;
static const float kMugsLeftStep = 35.0f;
// A knight's step a tick, of the bench's length (0x45ED74); pushed back at half (0x45EDA4).
static const float kReachScale = 0.005f;
static const float kPushBack = 0.5f;
// A mug's slide a tick, of the bench's length (0x45ED1C); an empty mug's fall, of its drop (0x45ED48).
static const float kSlideScale = 0.01f;
static const float kFallScale = 0.1f;
// A knight takes a full mug within 5 of him (0x45EE40, a double).
static const double kTakeReach = 5.0;
// Shaggy steps at most 10 a tick toward his bench (0x45EDDC, 0x45EDE0).
static const float kShaggyStep = 10.0f;
// The cursor shows over the top 100 rows (0x45EDAC).
static const float kCursorRows = 100.0f;

int KstRoom::layer0() const {
	const int l = _engine.getRoom()->getLayerSprite(0);
	return l >= 0 ? l : 0;
}

int KstRoom::drawRandom() {
	// The CRT's rand(), 0 to 0x7FFF.
	return _forcedRandom >= 0 ? _forcedRandom : (int)_engine.getRandomNumber(0x7FFF);
}

void KstRoom::playSound(const Common::String &name) {
	const bool found = _engine.getSound()->play(name);
	if (!_sounds.empty())
		_sounds += ", ";
	_sounds += Common::String::format("%s found %d", name.c_str(), found ? 1 : 0);
}

void KstRoom::placeSprite(int sprite, float x, float y) {
	// The sprite's place (+0x0C, +0x10), drawn through __ftol.
	if (sprite >= 0)
		_engine.getRoom()->setOffset(sprite, (int)x, (int)y);
}

void KstRoom::insertMarker(float *primary, float *parallel, uint &count, float key, float val) {
	// 0x41A970's insertion: the primary array stays sorted ascending by its key
	// (the placement's y), the parallel (the placement's x) following it.
	if (count >= kMarkers) {
		warning("More than 3 objects of the same type specified in Knights Table Room");
		return;
	}
	for (uint k = 0; k < count; k++) {
		if (key < primary[k]) {
			SWAP(key, primary[k]);
			if (parallel)
				SWAP(val, parallel[k]);
		}
	}
	primary[count] = key;
	if (parallel)
		parallel[count] = val;
	count++;
}

void KstRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x41A970. Shaggy's post, the knights' doors and the benches' rows are
	// marker placements the room keeps; the three table overlays and the two
	// counter digits are sprites.
	const float x = (float)claim.x, y = (float)claim.y;
	const Common::String &anim = claim.placedAnim;
	if (object == "OBJ_KST_SHAGGY") {
		// Shaggy (0x4192F0) at the post; his x off the tap line from it (+0xEC).
		_shaggyX = x;
		_shaggyY = y;
		_offsetX = x;
		_hasShaggy = true;
		claim.build = Room::Claim::kSkip;
	} else if (object == "OBJ_KST_KNIGHT1") {
		// The door's y sorts; its x (less 40, off the left) follows.
		insertMarker(_knightY, _knightX, _knightCount, y, x - kKnightXOffset);
		claim.build = Room::Claim::kSkip;
	} else if (object == "OBJ_KST_OVERLAY1" || object == "OBJ_KST_OVERLAY2" || object == "OBJ_KST_OVERLAY3") {
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = 0;
	} else if (object == "OBJ_KST_MUG") {
		if (anim == "ANIM_KST_MUGEMPTY")
			insertMarker(_benchEndY, _benchEndX, _benchEndCount, y, x);
		else if (anim == "ANIM_KST_MUGEMPTY_BREAKS")
			insertMarker(_benchFloorY, nullptr, _benchFloorCount, y, x);
		else if (anim == "ANIM_KST_MUGFULL")
			insertMarker(_drinkY, _drinkX, _drinkCount, y, x);
		else if (anim == "ANIM_KST_MUGFULL_BREAKS")
			insertMarker(_benchStartY, _benchStartX, _benchStartCount, y, x);
		claim.build = Room::Claim::kSkip;
	} else if (anim == "ANIM_KST_COUNTERNUMBERS") {
		if (_numbers < 2) {
			claim.build = Room::Claim::kSprite;
			claim.layer = 10; // past the ten layer slots: the root
			claim.depth = 100;
			_numbers++;
		} else {
			warning("Not enough character start positions specified in Pie Noon");
		}
	}
	// OBJ_KST_COUNTER's box, OBJ_KST_EXIT and OBJ_KST_FLAME fall to the base path.
}

// ---------------------------------------------------------------------------
// The room

void KstRoom::enter(const Common::String &from) {
	// 0x419A80, after the base entry.
	Room &room = *_engine.getRoom();
	_sounds.clear();
	_offsetX -= _drinkX[0];
	// The room is freshly loaded: none of the last visit's sprites is in it,
	// even when the visit ended without roomLeft (as some checks end it).
	for (uint i = 0; i < kKnightSlots; i++)
		_knights[i] = Knight();
	_full.clear();
	_empty.clear();
	_icons.clear();

	// The sprites the claim built: the three overlays and the two counter digits.
	_tens = _ones = -1;
	for (uint i = 0; i < 3; i++)
		_overlays[i] = -1;
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		const Common::String &obj = room.getObjectName(s);
		const Common::String &anim = room.getAnim(s);
		if (room.isEngineSprite(s))
			continue;
		if (obj == "OBJ_KST_OVERLAY1")
			_overlays[0] = s;
		else if (obj == "OBJ_KST_OVERLAY2")
			_overlays[1] = s;
		else if (obj == "OBJ_KST_OVERLAY3")
			_overlays[2] = s;
		else if (anim == "ANIM_KST_COUNTERNUMBERS" && room.getDepth(s) == 100) {
			if (_tens < 0) {
				_tens = s;
			} else if (_ones < 0) {
				_ones = s;
				if (room.getOffsetX(_ones) < room.getOffsetX(_tens))
					SWAP(_tens, _ones);
			}
		}
	}
	if (_ones < 0)
		warning("2 numbers should be placed in Knights Table Room");
	if (_benchEndCount != kMarkers)
		warning("3 bench end markers should be placed in Knights Table Room");
	if (_drinkCount != kMarkers)
		warning("3 drink spawn markers should be placed in Knights Table Room");
	if (_benchStartCount != kMarkers)
		warning("3 bench start markers should be placed in Knights Table Room");
	if (_knightCount != kMarkers)
		warning("3 knight markers should be placed in Knights Table Room");
	if (_benchFloorCount != kMarkers)
		warning("3 bench floor markers should be placed in Knights Table Room");

	// The counter shows the snacks; each overlay a row in front of its bench.
	showCounter();
	for (uint i = 0; i < 3; i++) {
		if (_overlays[i] >= 0)
			room.setDepth(_overlays[i], (int)(_benchEndY[i] - 1.0f));
	}

	// Shaggy at his post, idle (0x4192F0: once, 3 ticks a step), free.
	_shaggy = -1;
	_sx = _shaggyX;
	_sy = _shaggyY;
	_shaggyState = 0;
	_dir = 2;
	_grab = false;
	_fillBench = 0;
	if (_hasShaggy) {
		_shaggy = room.addEngineSprite(layer0(), "OBJ_KST_SHAGGY", "ANIM_KST_SHAGGYIDLE", 0, 0, 0, 0, "kst Shaggy");
		room.startAnim(_shaggy, "ANIM_KST_SHAGGYIDLE", kOnce, 0, 3);
		placeSprite(_shaggy, _sx, _sy);
		room.setActor(_shaggy, this);
	} else {
		warning("Shaggy's post not specified in Knights Table Room");
	}

	// The mugs: five less the difficulty. The round laid out.
	_lives = 5 - _engine.getDifficulty();
	startRound();

	// The narrator's intro holds the game, only before the opening has played.
	_narrator = "Knights_of_the_Snack_Table.350X_1_NARRATOR";
	_narrating = !_engine.getScript()->getMemory(kKstRoom).openingPlayed;
	// The first tick starts the round again (+0xC0 = 1).
	_pause = 1;
	playSound("Knights.Ambience");
}

void KstRoom::roomLeft() {
	// 0x419CC0, after the base leave: the markers to be read again, the round
	// torn down with the room.
	_knightCount = _benchEndCount = _benchFloorCount = _drinkCount = _benchStartCount = 0;
	for (uint i = 0; i < kMarkers; i++) {
		_knightX[i] = 320.0f;
		_knightY[i] = 240.0f;
	}
	_hasShaggy = false;
	_offsetX = 0;
	_tens = _ones = -1;
	_numbers = 0;
	_shaggy = -1;
	for (uint i = 0; i < 3; i++)
		_overlays[i] = -1;
	for (uint i = 0; i < kKnightSlots; i++)
		_knights[i] = Knight();
	_full.clear();
	_empty.clear();
	_icons.clear();
	_narrating = false;
}

void KstRoom::showCounter() {
	// The snacks (inventory +0x30), each digit's cel one back: cel 0 shows a 1.
	Room &room = *_engine.getRoom();
	const uint n = _engine.getInventory()->getSnacks();
	if (_tens >= 0)
		room.setPlayer(_tens, kStopped, ((n / 10) % 10 + 9) % 10);
	if (_ones >= 0)
		room.setPlayer(_ones, kStopped, (n % 10 + 9) % 10);
}

void KstRoom::clearMugs(Common::Array<Mug> &mugs) {
	Room &room = *_engine.getRoom();
	for (uint i = 0; i < mugs.size(); i++) {
		if (mugs[i].sprite >= 0)
			room.detachSprite(mugs[i].sprite);
	}
	mugs.clear();
}

void KstRoom::startRound() {
	// 0x41A5C0: the mugs on the benches gone; one knight to serve; six knights
	// gone (made the first time, two to a bench); the mugs left on the counter.
	Room &room = *_engine.getRoom();
	clearMugs(_full);
	clearMugs(_empty);
	_missed = false;
	_round = 1;
	_spawned = 0;
	_spawnTimer = 0;
	for (uint i = 0; i < kKnightSlots; i++) {
		if (_knights[i].sprite < 0)
			createKnight(i);
		else
			setKnightState(_knights[i], 4);
	}
	for (uint i = 0; i < _icons.size(); i++)
		room.detachSprite(_icons[i]);
	_icons.clear();
	// Under the second layer (game +0x5C; none here, so the root), at depth
	// 100, looping, 3 ticks a step; each pushed at the front of the list.
	float x = kMugsLeftX;
	for (int n = 0; n < _lives; n++) {
		const int s = room.addEngineSprite(room.getLayerSprite(1), "OBJ_KST_MUG", "ANIM_KST_MUGFULL", 100, 0, 0, 0, "kst mug left");
		room.startAnim(s, "ANIM_KST_MUGFULL", kLoop, 0, 3);
		placeSprite(s, x, kMugsLeftY);
		_icons.insert_at(0, s);
		x -= kMugsLeftStep;
	}
}

// ---------------------------------------------------------------------------
// The knights

void KstRoom::createKnight(uint slot) {
	// 0x418CA0: the body and its arm, each once at 9 ticks a step from the
	// bench's parity; the body 5 behind the bench row, the arm 32 in front.
	Room &room = *_engine.getRoom();
	Knight &k = _knights[slot];
	k = Knight();
	k.bench = slot % 3;
	const uint b = k.bench;
	k.sprite = room.addEngineSprite(layer0(), "OBJ_KST_KNIGHT1", "ANIM_KST_KNIGHT1WALKS", (int)(_benchEndY[b] - 5.0f), 0, 0, 0, "kst knight");
	room.startAnim(k.sprite, "ANIM_KST_KNIGHT1WALKS", kOnce, b & 1, 9);
	k.arm = room.addEngineSprite(layer0(), "OBJ_KST_KNIGHT1", "ANIM_KST_KNIGHT1ARMWALKS", (int)_benchEndY[b] + 0x20, 0, 0, 0, "kst knight arm");
	room.startAnim(k.arm, "ANIM_KST_KNIGHT1ARMWALKS", kOnce, b & 1, 9);
	k.reach = (float)(((double)_benchEndX[b] - _knightX[b]) * (double)kReachScale);
	setKnightState(k, 4);
	k.walks[0] = Common::String::format("Knights.Knight_Walks.%d", drawRandom() % 5 + 1);
	k.walks[1] = Common::String::format("Knights.Knight_Walks.%d", drawRandom() % 5 + 1);
	k.walk = 0;
	k.stepLatch = true;
	k.gulp = Common::String::format("Knights.Knight_Gulp.%u", b + 1);
	room.setActor(k.sprite, this);
}

void KstRoom::setKnightState(Knight &k, int state) {
	// 0x4191B0: the body's sequence at 9 ticks a step, the arm's alongside.
	Room &room = *_engine.getRoom();
	switch (state) {
	case 0:
		room.startAnim(k.sprite, "ANIM_KST_KNIGHT1DRINKS", kOnce, -1, 9);
		room.startAnim(k.arm, "ANIM_KST_KNIGHT1ARMDRINKS", kOnce, -1, 3);
		break;
	case 1:
		room.startAnim(k.sprite, "ANIM_KST_KNIGHT1WALKS", kLoop, k.bench & 1, 9);
		room.startAnim(k.arm, "ANIM_KST_KNIGHT1ARMWALKS", kLoop, k.bench & 1, 9);
		break;
	case 2:
		room.startAnim(k.sprite, "ANIM_KST_KNIGHT1STARTS_DRINKS", kOnce, -1, 9);
		room.startAnim(k.arm, "ANIM_KST_KNIGHT1ARMSTARTS_DRINKS", kOnce, -1, 3);
		break;
	case 3:
		room.startAnim(k.sprite, "ANIM_KST_KNIGHT1FINISH_DRINKS", kOnce, -1, 9);
		room.startAnim(k.arm, "ANIM_KST_KNIGHT1ARMFINISH_DRINKS", kOnce, -1, 3);
		break;
	case 4:
		// Gone: off the screen.
		room.startAnim(k.sprite, "ANIM_KST_KNIGHT1STARTS_DRINKS", kOnce, -1, 9);
		room.startAnim(k.arm, "ANIM_KST_KNIGHT1ARMSTARTS_DRINKS", kOnce, -1, 3);
		k.x = k.y = -5000.0f;
		break;
	default:
		break;
	}
	k.state = state;
	placeSprite(k.sprite, k.x, k.y);
	syncArm(k);
}

void KstRoom::syncArm(const Knight &k) {
	// 0x419160: the arm at the body's place and step, its player stopped.
	Room &room = *_engine.getRoom();
	if (k.arm < 0 || k.sprite < 0)
		return;
	placeSprite(k.arm, k.x, k.y);
	room.setPlayer(k.arm, kStopped, room.getStep(k.sprite), 3);
}

void KstRoom::walkIn(Knight &k) {
	// 0x419110: walking, from the bench's door on its row.
	setKnightState(k, 1);
	k.x = _knightX[k.bench];
	k.y = _benchEndY[k.bench];
	debugC(1, kDebugScript, "KST: a knight walks in on bench %u (round %d, %d of %d)", k.bench, _round, _spawned + 1, _round);
	placeSprite(k.sprite, k.x, k.y);
	syncArm(k);
}

void KstRoom::updateKnight(Knight &k) {
	// 0x418EE0, held while the room pauses.
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	const uint b = k.bench;
	switch (k.state) {
	case 1:
		// Toward the tap; his footsteps on step 0, the two in turn.
		k.x = (float)((double)k.x + k.reach);
		if (room.getStep(k.sprite) == 0 && k.stepLatch) {
			k.walk ^= 1;
			sound.stopClip(k.walks[k.walk]);
			playSound(k.walks[k.walk]);
			k.stepLatch = false;
		}
		if (room.getStep(k.sprite) != 0)
			k.stepLatch = true;
		placeSprite(k.sprite, k.x, k.y);
		if (_drinkX[b] <= k.x) {
			// At the tap: a mug lost.
			if (!_missed)
				debugC(1, kDebugScript, "KST: a knight reaches the tap on bench %u", b);
			miss();
			break;
		}
		if (takeFull(k.x, b)) {
			debugC(1, kDebugScript, "KST: the knight on bench %u takes a mug at x %.1f", b, k.x);
			setKnightState(k, 2);
		}
		break;
	case 2:
	case 0:
		// Drinking (and starting to): pushed back to the door at half his step.
		if (k.state == 2 && room.getMode(k.sprite) == 0)
			setKnightState(k, 0);
		if (k.state == 0 && !sound.isClipPlaying(k.gulp))
			playSound(k.gulp);
		k.x = (float)((double)k.x - (double)k.reach * kPushBack);
		placeSprite(k.sprite, k.x, k.y);
		if (k.x <= _knightX[b]) {
			// At the door: done, and the empty mug down.
			debugC(1, kDebugScript, "KST: the knight on bench %u drinks up at the door; the empty mug slides back", b);
			setKnightState(k, 3);
			dropEmpty(k.x, b);
			sound.stopClip(k.gulp);
		}
		break;
	case 3:
		// Finished: one in three comes again, from where he is.
		if (room.getMode(k.sprite) == 0)
			setKnightState(k, drawRandom() % 3 == 0 ? 1 : 4);
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// The mugs

void KstRoom::slideMug(uint bench) {
	// 0x41A2B0 -> 0x418710: Knights.Mug_Slide; a full mug at the bench's tap,
	// once at 3 ticks a step, at the bench row's depth, pushed at the front.
	Room &room = *_engine.getRoom();
	playSound("Knights.Mug_Slide");
	Mug m;
	m.bench = bench;
	m.state = 0;
	m.x = _drinkX[bench];
	m.y = _benchEndY[bench];
	m.speed = (float)(((double)_benchEndX[bench] - _benchStartX[bench]) * (double)kSlideScale);
	m.sprite = room.addEngineSprite(layer0(), "OBJ_KST_MUG", "ANIM_KST_MUGFULL", (int)m.y, 0, 0, 0, "kst full mug");
	room.startAnim(m.sprite, "ANIM_KST_MUGFULL", kOnce, 0, 3);
	placeSprite(m.sprite, m.x, m.y);
	room.setActor(m.sprite, this);
	_full.insert_at(0, m);
	playSound(Common::String::format("Knights.Mugdown.%d", drawRandom() % 3 + 1));
	debugC(1, kDebugScript, "KST: Shaggy slides a full mug down bench %u", bench);
}

void KstRoom::dropEmpty(float x, uint bench) {
	// 0x41A370 -> 0x4189B0: an empty mug where the knight finished, 31 in
	// front of the bench row, sliding back to the tap; pushed at the front.
	Room &room = *_engine.getRoom();
	Mug m;
	m.bench = bench;
	m.state = 0;
	m.x = x;
	m.y = _benchEndY[bench];
	m.speed = (float)(((double)_benchEndX[bench] - _benchStartX[bench]) * (double)kSlideScale);
	m.fall = (float)(((double)_benchFloorY[bench] - _benchEndY[bench]) * (double)kFallScale);
	m.sprite = room.addEngineSprite(layer0(), "OBJ_KST_MUG", "ANIM_KST_MUGEMPTY", (int)_benchEndY[bench] + 0x1F, 0, 0, 0, "kst empty mug");
	room.startAnim(m.sprite, "ANIM_KST_MUGEMPTY", kOnce, 0, 3);
	placeSprite(m.sprite, m.x, m.y);
	room.setActor(m.sprite, this);
	_empty.insert_at(0, m);
	playSound(Common::String::format("Knights.Mugdown.%d", drawRandom() % 3 + 1));
}

bool KstRoom::takeFull(float x, uint bench) {
	// 0x41A410: the newest full mug on the bench, sliding, within 5 of the
	// knight, out of the list and deleted.
	for (uint i = 0; i < _full.size(); i++) {
		const Mug &m = _full[i];
		if (m.bench == bench && ABS((double)m.x - x) < kTakeReach && m.state == 0) {
			_engine.getRoom()->setPaused(m.sprite, true);
			_full.remove_at(i);
			return true;
		}
	}
	return false;
}

bool KstRoom::catchEmpty(uint bench) {
	// 0x41A820: the newest empty mug on the bench falling off the tap end, or
	// past the tap, caught: out of the list and done.
	for (uint i = 0; i < _empty.size(); i++) {
		const Mug &m = _empty[i];
		if (m.bench != bench)
			continue;
		if (m.state == 1 || (m.state == 0 && m.x > _drinkX[bench])) {
			debugC(1, kDebugScript, "KST: Shaggy catches the empty mug on bench %u", bench);
			_engine.getRoom()->setPaused(m.sprite, true);
			_empty.remove_at(i);
			return true;
		}
	}
	return false;
}

void KstRoom::updateFull(Mug &m) {
	// 0x418860, held while the room pauses: along the bench toward the door;
	// past its end, it breaks; broken, a mug lost.
	Room &room = *_engine.getRoom();
	if (m.state == 0) {
		const double x = (double)m.x - m.speed;
		m.x = (float)x;
		placeSprite(m.sprite, m.x, m.y);
		if (x < _benchStartX[m.bench]) {
			debugC(1, kDebugScript, "KST: a full mug passes the door end of bench %u and breaks", m.bench);
			room.startAnim(m.sprite, "ANIM_KST_MUGFULL_BREAKS", kOnce, -1, 3);
			playSound(Common::String::format("Knights.Glass_Breaks.%d", drawRandom() % 4 + 1));
			m.state = 1;
		}
	} else if (m.state == 1 && room.getMode(m.sprite) == 0) {
		m.state = 2;
		miss();
	}
}

void KstRoom::updateEmpty(Mug &m) {
	// 0x418B10, held while the room pauses: back toward the tap; past the
	// bench's end it falls to the floor and breaks; broken, a mug lost.
	Room &room = *_engine.getRoom();
	if (m.state == 0) {
		const double x = (double)m.speed + m.x;
		m.x = (float)x;
		placeSprite(m.sprite, m.x, m.y);
		if (_benchEndX[m.bench] <= x)
			m.state = 1;
	} else if (m.state == 1) {
		const double y = (double)m.fall + m.y;
		m.y = (float)y;
		placeSprite(m.sprite, m.x, m.y);
		if (_benchFloorY[m.bench] <= y) {
			debugC(1, kDebugScript, "KST: an empty mug falls off the tap end of bench %u and breaks", m.bench);
			m.state = 2;
			room.startAnim(m.sprite, "ANIM_KST_MUGEMPTY_BREAKS", kOnce, -1, 3);
			playSound(Common::String::format("Knights.Glass_Breaks.%d", drawRandom() % 4 + 1));
		}
	} else if (m.state == 2 && room.getMode(m.sprite) == 0) {
		m.state = 3;
		miss();
	}
}

// ---------------------------------------------------------------------------
// Shaggy

float KstRoom::shaggyX(float y) const {
	// 0x41B2E0: the tap line through the three drink markers, by the bench
	// rows (the lower segment past the middle row), off it by +0xEC.
	const uint seg = _benchEndY[1] < y ? 1 : 0;
	const double slope = ((double)_drinkX[seg + 1] - _drinkX[seg]) / ((double)_benchEndY[seg + 1] - _benchEndY[seg]);
	return (float)(((double)y - _benchEndY[seg]) * slope + _drinkX[seg] + _offsetX);
}

void KstRoom::shaggyDrop() {
	// 0x419680: a mug lost; he drops one, Zoinks.
	_shaggyState = 0;
	_engine.getRoom()->startAnim(_shaggy, "ANIM_KST_SHAGGYDROP_MUG", kOnce, -1, 3);
	_dir = 2;
	playSound("Knights.Zoinks");
}

void KstRoom::shaggySuccess() {
	// 0x4196D0: a round served.
	_shaggyState = 0;
	_engine.getRoom()->startAnim(_shaggy, "ANIM_KST_SHAGGYSUCCESS", kOnce, -1, 3);
	_dir = 2;
}

void KstRoom::updateShaggy() {
	// 0x419370, held while the room pauses.
	if (paused())
		return;
	Room &room = *_engine.getRoom();
	// The bench row nearest the mouse (game +0x1E), and his step toward it.
	const float my = (float)(uint)(int)(int16)_engine.getMousePos().y;
	uint target = 0;
	float best = ABS(my - _benchEndY[0]);
	for (uint i = 1; i < kMarkers; i++) {
		const float d = ABS(my - _benchEndY[i]);
		if (d < best) {
			target = i;
			best = d;
		}
	}
	float step = _benchEndY[target] - _sy;
	if (step > kShaggyStep)
		step = kShaggyStep;
	else if (step < -kShaggyStep)
		step = -kShaggyStep;

	// The button (game +0x20): pressed at his bench he fills a mug; let go,
	// he slides it; the slide over, the mug goes down the bench he filled for.
	const bool down = _engine.isMouseDown();
	bool wasFree = false;
	switch (_shaggyState) {
	case 0:
		wasFree = true;
		if (-kShaggyStep < step && step < kShaggyStep && down) {
			room.startAnim(_shaggy, "ANIM_KST_SHAGGYFILL_MUG", kOnce, -1, 3);
			_shaggyState = 1;
			_fillBench = target;
		}
		break;
	case 1:
		if (room.getMode(_shaggy) == 0)
			_shaggyState = 2;
		// Let go while filling, he slides it as it is.
		// fall through
	case 2:
		if (!down) {
			room.startAnim(_shaggy, "ANIM_KST_SHAGGYSLIDE_MUG", kOnce, -1, 3);
			_shaggyState = 3;
		}
		break;
	case 3:
		if (room.getMode(_shaggy) == 0) {
			slideMug(_fillBench);
			room.startAnim(_shaggy, "ANIM_KST_SHAGGYSLIDE_MUG_TO_IDLE", kOnce, -1, 3);
			_shaggyState = 4;
		}
		break;
	case 4:
		if (room.getMode(_shaggy) == 0)
			_shaggyState = 0;
		break;
	default:
		break;
	}

	// Free, he walks toward the bench, forward (down) or away (up); there, idle.
	if (_shaggyState == 0) {
		if (step == 0.0f) {
			_dir = 2;
			if (!_grab || room.getMode(_shaggy) == 0) {
				_grab = false;
				room.startAnim(_shaggy, "ANIM_KST_SHAGGYIDLE", kLoop, -1, 3);
			}
		} else {
			_sy = (float)((double)step + _sy);
			_sx = shaggyX(_sy);
			placeSprite(_shaggy, _sx, _sy);
			if (step > 0.0f && (_grab || _dir != 1)) {
				_dir = 1;
				room.startAnim(_shaggy, "ANIM_KST_SHAGGYWALK_FORWARD", kLoop, -1, 3);
				_grab = false;
			} else {
				if (step < 0.0f && (_grab || _dir != 0)) {
					_dir = 0;
					room.startAnim(_shaggy, "ANIM_KST_SHAGGYWALK_AWAY", kLoop, -1, 3);
				}
				_grab = false;
			}
		}
	}

	// The bench he is nearest catches its empty mug.
	uint here = 0;
	best = ABS(_sy - _benchEndY[0]);
	for (uint i = 1; i < kMarkers; i++) {
		if (ABS(_sy - _benchEndY[i]) < best) {
			here = i;
			best = ABS(_sy - _benchEndY[i]);
		}
	}
	if (_shaggyState == 0 && catchEmpty(here)) {
		if (!wasFree) {
			_grab = false;
			return;
		}
		room.startAnim(_shaggy, "ANIM_KST_SHAGGYGRAB_MUG", kOnce, -1, 3);
		_grab = true;
	}
	if (!wasFree)
		_grab = false;
}

// ---------------------------------------------------------------------------
// The walk and the tick

void KstRoom::actorUpdate(uint sprite) {
	if ((int)sprite == _shaggy) {
		updateShaggy();
		return;
	}
	for (uint i = 0; i < kKnightSlots; i++) {
		Knight &k = _knights[i];
		if (k.sprite != (int)sprite)
			continue;
		if (!paused())
			updateKnight(k);
		syncArm(k);
		return;
	}
	if (paused())
		return;
	for (uint i = 0; i < _full.size(); i++) {
		if (_full[i].sprite == (int)sprite) {
			updateFull(_full[i]);
			return;
		}
	}
	for (uint i = 0; i < _empty.size(); i++) {
		if (_empty[i].sprite == (int)sprite) {
			updateEmpty(_empty[i]);
			return;
		}
	}
}

bool KstRoom::actorPlays(uint sprite) {
	// The knights' and the mugs' players (0x419140, 0x418990, 0x418C80) are
	// held while the room pauses; Shaggy's (0x4315C0) is not.
	return (int)sprite == _shaggy || !paused();
}

bool KstRoom::hidesCursor() const {
	return (float)_engine.getMousePos().y > kCursorRows;
}

void KstRoom::tick() {
	// 0x419E40, after the base tick.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	// Shaggy 30 in front of his row; a falling empty mug 15 in front of its bench.
	if (_shaggy >= 0)
		room.setDepth(_shaggy, (int)_sy + 0x1E);
	for (uint i = 0; i < _empty.size(); i++) {
		if (_empty[i].state == 1)
			room.setDepth(_empty[i].sprite, (int)_benchEndY[_empty[i].bench] + 0xF);
	}
	// The narrator's intro holds the game until the opening is over and the
	// line has ended.
	if (_narrating) {
		if (script.isScenePlaying() || !script.isOpeningDone())
			return;
		if (_engine.getSound()->isClipPlaying(_narrator))
			return;
		_narrating = false;
		debugC(1, kDebugScript, "KST: the narrator's intro over, the game on (%d mugs)", _lives);
	}
	// A mug lost: Shaggy drops one, the room pauses, one mug fewer.
	if (_missed) {
		shaggyDrop();
		_missed = false;
		_pause = kPauseTicks;
		_lives--;
		debugC(1, kDebugScript, "KST: a mug lost, %d left", _lives);
	}
	// Out of mugs, the pause over: the dining room.
	if (_lives == 0 && _pause == 0) {
		debugC(1, kDebugScript, "KST: out of mugs: to ROOM_P12_Dining_Room");
		_engine.requestRoom("ROOM_P12_Dining_Room");
	}
	// The pause over: the round again.
	if (_pause != 0 && --_pause == 0)
		startRound();
	if (_roundPause != 0)
		_roundPause--;
	// The round's knights walk in one at a time, each into the first slot free.
	if (_spawned < _round) {
		if (--_spawnTimer > 0)
			return;
		for (uint i = 0; i < kKnightSlots; i++) {
			if (_knights[i].sprite >= 0 && _knights[i].state == 4) {
				walkIn(_knights[i]);
				_spawnTimer = drawRandom() % 0x48 + 0x24;
				_spawned++;
				return;
			}
		}
		return;
	}
	// The round served once every knight has gone and no mug is on a bench:
	// one more knight next round, a pause, a snack and a narrator's line.
	for (uint i = 0; i < kKnightSlots; i++) {
		if (_knights[i].state != 4)
			return;
	}
	if (!_full.empty() || !_empty.empty())
		return;
	_spawned = 0;
	_round++;
	const int r = drawRandom();
	_roundPause = kPauseTicks;
	_spawnTimer = r % 100 + 0x32;
	shaggySuccess();
	Inventory &inventory = *_engine.getInventory();
	inventory.setSnacks(inventory.getSnacks() + 1);
	showCounter();
	debugC(1, kDebugScript, "KST: round %d served: %u snacks; round %d next", _round - 1, inventory.getSnacks(), _round);
	playSound(Common::String::format("Knights_of_the_Snack_Table.Correct%d_NARRATOR", drawRandom() % 6 + 1));
}

RoomClass *createKstRoom(TerraglyphEngine &engine, const Common::String &room) {
	if (room == kKstRoom)
		return new KstRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
