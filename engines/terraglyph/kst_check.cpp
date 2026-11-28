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

// The script-level check of Phantom's Knights of the Snack Table (WP-073,
// the serving game WP-090), against an independent reference written from
// the trace. The game is driven through the engine's tick with the mouse set
// directly, every rand() the room draws pinned to 1.

#include "common/debug.h"

#include "terraglyph/art_archive.h"
#include "terraglyph/detection.h"
#include "terraglyph/inventory.h"
#include "terraglyph/kst_rooms.h"
#include "terraglyph/room.h"
#include "terraglyph/room_class.h"
#include "terraglyph/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"

namespace Terraglyph {

void TerraglyphEngine::checkKST() {
	const char *const kKST = "ROOM_P31_KST";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kKST;
	if (!found) {
		debugC(3, kDebugScript, "kst check: no kst");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);

	Common::String next;
	int movie;
	auto tick1 = [&]() {
		next.clear();
		movie = -1;
		return tickOnce(next, movie);
	};
	auto kstRoom = [&]() { return dynamic_cast<KstRoom *>(getRoomClass(kKST)); };
	auto story = [&](int level) {
		newGameForCheck();
		setDifficulty(level);
		_inventory->setSnacks(0);
		_sound->stop();
		_mouseDown = false;
	};
	// A game at @p level, the room entered with rand() pinned to 1 and the
	// narrator's hold ended.
	auto begin = [&](int level) {
		story(level);
		KstRoom *kst = kstRoom();
		kst->setRandomForCheck(1);
		showRoom(kKST);
		kst->endIntroForCheck();
		kst->clearSounds();
		_mousePos = Common::Point(320, (int)kst->getBenchEndY(0));
		return kst;
	};
	auto has = [&](const Common::String &name) {
		return _sound->findClip(name) != nullptr ? 1 : 0;
	};
	// The sounds the room played, each once, in the order first heard.
	auto heard = [&](KstRoom *kst) {
		Common::Array<Common::String> names;
		const Common::String &all = kst->getSounds();
		uint32 from = 0;
		while (from < all.size()) {
			const size_t to = all.find(", ", from);
			Common::String item = to == Common::String::npos ? all.substr(from) : all.substr(from, to - from);
			from = to == Common::String::npos ? all.size() : (uint32)to + 2;
			const size_t cut = item.find(" found ");
			if (cut != Common::String::npos)
				item = item.substr(0, cut);
			bool seen = false;
			for (uint k = 0; k < names.size() && !seen; k++)
				seen = names[k] == item;
			if (!seen)
				names.push_back(item);
		}
		Common::String out;
		for (uint k = 0; k < names.size(); k++)
			out += (k ? " " : "") + names[k];
		return out;
	};
	// Shaggy fills at @p bench once he stands there, and lets go when full.
	struct Server {
		bool pressed = false;
	};
	auto serve = [&](KstRoom *kst, Server &s, uint bench) {
		if (!s.pressed && kst->getShaggyState() == 0 && kst->getShaggyNowY() == kst->getBenchEndY(bench)) {
			_mouseDown = true;
			s.pressed = true;
		} else if (_mouseDown && kst->getShaggyState() == 2) {
			_mouseDown = false;
		}
	};

	// The class.
	{
		RoomClass *a = createRoomClass(*this, kKST);
		debugC(3, kDebugScript, "kst check classes: KST %s", dynamic_cast<KstRoom *>(a) ? "knights of the snack table" : "-");
		delete a;
	}

	// The marker placements (0x41A970), each sorted ascending by y.
	{
		story(0);
		showRoom(kKST);
		KstRoom *kst = kstRoom();
		Common::String out = Common::String::format("Shaggy %d %d", (int)kst->getShaggyX(), (int)kst->getShaggyY());
		out += "; knights";
		for (uint i = 0; i < kst->getKnightCount(); i++)
			out += Common::String::format("%s %d %d", i ? "," : "", (int)kst->getKnightX(i), (int)kst->getKnightY(i));
		out += "; bench-end";
		for (uint i = 0; i < KstRoom::kMarkers; i++)
			out += Common::String::format("%s %d %d", i ? "," : "", (int)kst->getBenchEndX(i), (int)kst->getBenchEndY(i));
		out += "; bench-floor";
		for (uint i = 0; i < KstRoom::kMarkers; i++)
			out += Common::String::format("%s %d", i ? "," : "", (int)kst->getBenchFloorY(i));
		out += "; drink-spawn";
		for (uint i = 0; i < KstRoom::kMarkers; i++)
			out += Common::String::format("%s %d %d", i ? "," : "", (int)kst->getDrinkX(i), (int)kst->getDrinkY(i));
		out += "; bench-start";
		for (uint i = 0; i < KstRoom::kMarkers; i++)
			out += Common::String::format("%s %d %d", i ? "," : "", (int)kst->getBenchStartX(i), (int)kst->getBenchStartY(i));
		debugC(3, kDebugScript, "kst check markers: %s", out.c_str());
	}

	// The sprites the claim built: the three table overlays, each a row in
	// front of its bench, and the two counter digits.
	{
		story(0);
		showRoom(kKST);
		Room &room = *_currentRoom;
		KstRoom *kst = kstRoom();
		Common::String out;
		for (uint i = 0; i < 3; i++) {
			const int s = kst->getOverlay(i);
			out += Common::String::format("%soverlay%u depth %d", i ? ", " : "", i + 1, s >= 0 ? room.getDepth(s) : -9999);
		}
		const int tens = kst->getTens(), ones = kst->getOnes();
		out += Common::String::format("; counter tens %d ones %d depth %d",
		                              tens >= 0 ? room.getOffsetX(tens) : -9999, ones >= 0 ? room.getOffsetX(ones) : -9999,
		                              tens >= 0 ? room.getDepth(tens) : -9999);
		debugC(3, kDebugScript, "kst check sprites: %s", out.c_str());
	}

	// Entry (0x419A80): the mugs by level, laid on the counter; Shaggy and
	// his offset; the six knights gone; the counter's cels for the snacks; the
	// narrator's intro, the ambience and the music.
	{
		Common::String out = "mugs";
		for (int l = 0; l < 3; l++) {
			story(l);
			showRoom(kKST);
			out += Common::String::format(" %d", kstRoom()->getLives());
		}
		story(0);
		showRoom(kKST);
		Room &room = *_currentRoom;
		KstRoom *kst = kstRoom();
		out += "; level 0 mugs at";
		const Common::Array<int> &icons = kst->getMugsLeft();
		for (uint i = 0; i < icons.size(); i++)
			out += Common::String::format("%s %d %d", i ? "," : "", room.getOffsetX(icons[i]), room.getOffsetY(icons[i]));
		out += Common::String::format(" depth %d", icons.empty() ? -9999 : room.getDepth(icons[0]));
		out += Common::String::format("; Shaggy at %d %d offset %.2f; knights", room.getOffsetX(kst->getShaggy()), room.getOffsetY(kst->getShaggy()),
		                              kst->getShaggyOffset());
		for (uint i = 0; i < KstRoom::kKnightSlots; i++) {
			const KstRoom::Knight &k = kst->getKnight(i);
			out += Common::String::format("%s %u state %d reach %.4f depth %d arm %d", i ? "," : "", k.bench, k.state, k.reach,
			                              room.getDepth(k.sprite), room.getDepth(k.arm));
		}
		const int tens = kst->getTens(), ones = kst->getOnes();
		out += Common::String::format("; counter cels %u %u", room.getStep(tens), room.getStep(ones));
		_inventory->setSnacks(13);
		showRoom(kKST);
		out += Common::String::format(", 13 snacks %u %u", room.getStep(kstRoom()->getTens()), room.getStep(kstRoom()->getOnes()));
		_inventory->setSnacks(0);
		const Common::String &narrator = kstRoom()->getNarrator();
		out += Common::String::format("; narrator %s found %d holds %d; ambience Knights.Ambience found %d; music Knights.Music found %d",
		                              narrator.c_str(), has(narrator), kstRoom()->isNarrating() ? 1 : 0, has("Knights.Ambience"), has("Knights.Music"));
		debugC(3, kDebugScript, "kst check enter: %s", out.c_str());
	}

	// Shaggy (0x419370) after the mouse: down to the bottom bench, then up to
	// the top one, ten a tick at most, on the tap line.
	{
		KstRoom *kst = begin(0);
		Room &room = *_currentRoom;
		Common::String out;
		const uint benches[2] = { 2, 0 };
		int t = 0;
		for (uint leg = 0; leg < 2; leg++) {
			const uint b = benches[leg];
			_mousePos = Common::Point(320, (int)kst->getBenchEndY(b));
			const float y0 = kst->getShaggyNowY();
			Common::String walk = "-";
			int arrive = -1;
			for (int n = 0; n < 200 && arrive < 0; n++) {
				tick1();
				t++;
				if (walk == "-" && kst->getShaggyNowY() != y0)
					walk = room.getAnim(kst->getShaggy());
				if (kst->getShaggyNowY() == kst->getBenchEndY(b))
					arrive = t;
			}
			tick1();
			t++;
			out += Common::String::format("%sto bench %u: %s, there at tick %d, x %.2f y %.2f, then %s", leg ? "; " : "", b, walk.c_str(), arrive,
			                              kst->getShaggyNowX(), kst->getShaggyNowY(), room.getAnim(kst->getShaggy()).c_str());
		}
		debugC(3, kDebugScript, "kst check shaggy: %s", out.c_str());
	}

	// A round served on the top bench: Shaggy fills and slides a mug; the
	// knight takes it, drinks back to the door and drops the empty; Shaggy
	// catches it; the round served, a snack.
	{
		KstRoom *kst = begin(0);
		Room &room = *_currentRoom;
		Server s;
		int fill = -1, full = -1, slide = -1, take = -1, drink = -1, door = -1, gone = -1, caught = -1, served = -1;
		Common::String out;
		for (int t = 1; t <= 3000 && served < 0; t++) {
			serve(kst, s, 0);
			tick1();
			const KstRoom::Knight &k = kst->getKnight(0);
			if (fill < 0 && kst->getShaggyState() == 1)
				fill = t;
			if (full < 0 && kst->getShaggyState() == 2)
				full = t;
			if (slide < 0 && !kst->getFullMugs().empty()) {
				slide = t;
				const KstRoom::Mug &m = kst->getFullMugs()[0];
				out += Common::String::format("slid at %d: bench %u x %.2f y %.2f speed %.4f", t, m.bench, m.x, m.y, m.speed);
			}
			if (take < 0 && k.state == 2) {
				take = t;
				out += Common::String::format("; taken at %d: knight x %.2f, mugs on benches %u", t, k.x, kst->getFullMugs().size());
			}
			if (drink < 0 && take > 0 && k.state == 0)
				drink = t;
			if (door < 0 && k.state == 3) {
				door = t;
				const KstRoom::Mug &m = kst->getEmptyMugs()[0];
				out += Common::String::format("; at the door at %d: knight x %.2f, empty x %.2f speed %.4f fall %.4f", t, k.x, m.x, m.speed, m.fall);
			}
			if (gone < 0 && door > 0 && k.state == 4)
				gone = t;
			if (caught < 0 && door > 0 && kst->getEmptyMugs().empty()) {
				caught = t;
				out += Common::String::format("; caught at %d: grab %d %s", t, kst->isGrabbing() ? 1 : 0, room.getAnim(kst->getShaggy()).c_str());
			}
			if (served < 0 && kst->getRound() == 2) {
				served = t;
				out += Common::String::format("; served at %d: snacks %u, cels %u %u, %s, pause %d, next knight in %d", t, _inventory->getSnacks(),
				                              room.getStep(kst->getTens()), room.getStep(kst->getOnes()), room.getAnim(kst->getShaggy()).c_str(),
				                              kst->getRoundPause(), kst->getSpawned());
			}
		}
		out += Common::String::format("; fill %d, full %d, drinks %d, gone %d; mugs %d; heard %s", fill, full, drink, gone, kst->getLives(), heard(kst).c_str());
		debugC(3, kDebugScript, "kst check serve: %s", out.c_str());
	}

	// Mugs lost, three mugs: one slid down the middle bench breaks at the
	// door end; then the top bench's knight, unserved, reaches the tap twice;
	// out of mugs, the dining room.
	{
		KstRoom *kst = begin(2);
		Server s;
		Common::String out = Common::String::format("mugs %d", kst->getLives());
		_mousePos = Common::Point(320, (int)kst->getBenchEndY(1));
		int lives = kst->getLives(), broke = -1, dining = -1;
		bool slid = false, wasPaused = false;
		for (int t = 1; t <= 6000 && dining < 0; t++) {
			serve(kst, s, 1);
			tick1();
			if (!slid && !kst->getFullMugs().empty()) {
				slid = true;
				out += Common::String::format("; slid at %d on bench %u", t, kst->getFullMugs()[0].bench);
			}
			if (broke < 0 && !kst->getFullMugs().empty() && kst->getFullMugs()[0].state == 1) {
				broke = t;
				out += Common::String::format("; broke at %d: x %.2f", t, kst->getFullMugs()[0].x);
			}
			if (kst->getLives() != lives) {
				lives = kst->getLives();
				out += Common::String::format("; lost at %d: mugs %d, knight x %.2f", t, lives, kst->getKnight(0).x);
			}
			if (wasPaused && kst->getPause() == 0)
				out += Common::String::format("; again at %d: mugs left %u", t, kst->getMugsLeft().size());
			wasPaused = kst->getPause() > 0;
			if (next == "ROOM_P12_Dining_Room")
				dining = t;
		}
		out += Common::String::format("; to %s at %d; heard %s", dining > 0 ? "ROOM_P12_Dining_Room" : "-", dining, heard(kst).c_str());
		debugC(3, kDebugScript, "kst check misses: %s", out.c_str());
	}

	// An empty mug not caught: Shaggy slides one down the top bench, then
	// walks to the bottom one; the knight's empty falls off the tap end and
	// breaks.
	{
		KstRoom *kst = begin(0);
		Server s;
		Common::String out;
		int door = -1, falls = -1, breaks = -1, lost = -1;
		bool away = false;
		for (int t = 1; t <= 3000 && lost < 0; t++) {
			serve(kst, s, 0);
			if (s.pressed && !_mouseDown && !away) {
				away = true;
				_mousePos = Common::Point(320, (int)kst->getBenchEndY(2));
			}
			tick1();
			const Common::Array<KstRoom::Mug> &e = kst->getEmptyMugs();
			if (door < 0 && !e.empty()) {
				door = t;
				out += Common::String::format("dropped at %d: x %.2f", t, e[0].x);
			}
			if (falls < 0 && !e.empty() && e[0].state == 1) {
				falls = t;
				out += Common::String::format("; falls at %d: x %.2f", t, e[0].x);
			}
			if (breaks < 0 && !e.empty() && e[0].state == 2) {
				breaks = t;
				out += Common::String::format("; breaks at %d: y %.2f", t, e[0].y);
			}
			if (lost < 0 && kst->getLives() == 4) {
				lost = t;
				out += Common::String::format("; lost at %d: mugs %d, Shaggy y %.2f", t, kst->getLives(), kst->getShaggyNowY());
			}
		}
		debugC(3, kDebugScript, "kst check empty: %s", out.c_str());
	}

	// The cursor (0x419E40): shown over the top 100 rows.
	{
		KstRoom *kst = begin(0);
		_mousePos = Common::Point(320, 100);
		const bool at100 = kst->hidesCursor();
		_mousePos = Common::Point(320, 101);
		const bool at101 = kst->hidesCursor();
		debugC(3, kDebugScript, "kst check cursor: hidden at y 100 %d, at y 101 %d", at100 ? 1 : 0, at101 ? 1 : 0);
	}

	// The narrator's round-served lines, the sounds the game plays, and the exit.
	{
		Common::String out;
		for (int n = 1; n <= 6; n++)
			out += Common::String::format("%sCorrect%d found %d", n > 1 ? ", " : "", n, has(Common::String::format("Knights_of_the_Snack_Table.Correct%d_NARRATOR", n)));
		out += Common::String::format("; Zoinks found %d, Mug_Slide found %d", has("Knights.Zoinks"), has("Knights.Mug_Slide"));
		for (int n = 1; n <= 3; n++)
			out += Common::String::format(", Mugdown.%d found %d", n, has(Common::String::format("Knights.Mugdown.%d", n)));
		for (int n = 1; n <= 4; n++)
			out += Common::String::format(", Glass_Breaks.%d found %d", n, has(Common::String::format("Knights.Glass_Breaks.%d", n)));
		for (int n = 1; n <= 3; n++)
			out += Common::String::format(", Knight_Gulp.%d found %d", n, has(Common::String::format("Knights.Knight_Gulp.%d", n)));
		for (int n = 1; n <= 5; n++)
			out += Common::String::format(", Knight_Walks.%d found %d", n, has(Common::String::format("Knights.Knight_Walks.%d", n)));
		out += "; exit to ROOM_P12_Dining_Room";
		debugC(3, kDebugScript, "kst check correct: %s", out.c_str());
	}

	_mouseDown = false;
	_checking = false;
}

} // End of namespace Terraglyph
