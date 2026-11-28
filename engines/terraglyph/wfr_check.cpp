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

// The script-level check of Phantom's Wizard's Rule Room (WP-083) against an
// independent reference written from the trace. Showdown has no such room.

#include "common/debug.h"

#include "terraglyph/art_archive.h"
#include "terraglyph/detection.h"
#include "terraglyph/inventory.h"
#include "terraglyph/room.h"
#include "terraglyph/room_class.h"
#include "terraglyph/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"
#include "terraglyph/wfr_rooms.h"

namespace Terraglyph {

static const char *wfrBuildName(Room::Claim::Build b) {
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

void TerraglyphEngine::checkWizardsRule() {
	const char *const kWFR = "ROOM_P30_WFR";

	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kWFR;
	if (!found) {
		debugC(3, kDebugScript, "wizard's rule room check: no wizard's rule room");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);

	// The dotted voice lines ship with a speaker suffix, so they match by prefix
	// (reading exactly would say "found 0" of a name that is right — the WP-074
	// trap). WFR.Potion_%i and Wizards_Rule.Correct%i carry a number, so they
	// are probed by their stem.
	auto pfx = [&](const char *name) { return _sound->hasClipPrefix(name) ? 1 : 0; };

	// The class the factory builds.
	{
		RoomClass *a = createRoomClass(*this, kWFR);
		debugC(3, kDebugScript, "wizard's rule room check class: WFR %s",
		       dynamic_cast<WizardsRuleRoom *>(a) ? "wfr" : a ? "other" : "-");
		delete a;
	}

	// The music and the Simon lines.
	debugC(3, kDebugScript, "wizard's rule room check clips: music %s %d; red %s %d; green %s %d; blue %s %d; pink %s %d; yellow %s %d; brown %s %d",
	       "Wizards_Rule.Music", pfx("Wizards_Rule.Music"),
	       "Wizards_Rule.Red", pfx("Wizards_Rule.Red"),
	       "Wizards_Rule.Green", pfx("Wizards_Rule.Green"),
	       "Wizards_Rule.Blue", pfx("Wizards_Rule.Blue"),
	       "Wizards_Rule.Pink", pfx("Wizards_Rule.Pink"),
	       "Wizards_Rule.Yellow", pfx("Wizards_Rule.Yellow"),
	       "Wizards_Rule.Brown", pfx("Wizards_Rule.Brown"));
	debugC(3, kDebugScript, "wizard's rule room check lines: potion %s %d; correct %s %d; incorrect %s %d; incorrect2 %s %d; incorrect3 %s %d; glass %s %d",
	       "WFR.Potion_", pfx("WFR.Potion_"),
	       "Wizards_Rule.Correct", pfx("Wizards_Rule.Correct"),
	       "Wizards_Rule.Incorrect_1_NARRATOR", pfx("Wizards_Rule.Incorrect_1_NARRATOR"),
	       "Wizards_Rule.Incorrect2_1_NARRATOR", pfx("Wizards_Rule.Incorrect2_1_NARRATOR"),
	       "Wizards_Rule.Incorrect3_1_NARRATOR", pfx("Wizards_Rule.Incorrect3_1_NARRATOR"),
	       "Knights.Glass_Breaks.1", pfx("Knights.Glass_Breaks.1"));

	// The six coloured bottles share one OBJ_WFR_BOTTLES placement each (told
	// apart by the colour in their ANIM): plain sprites the room reads a click
	// on (WP-094). The table at 0x46C5B0 gives each beat's bottle.
	static const char *const kNames[7] = { "RED", "GREEN", "BLUE", "PINK", "YELLOW", "BROWN", "-" };
	{
		RoomClass *rc = createRoomClass(*this, kWFR);
		Room::Claim bottles;
		rc->claimPlacement("OBJ_WFR_BOTTLES", bottles);
		Common::String out = Common::String::format("OBJ_WFR_BOTTLES %s layer %d depth %d hit %d", wfrBuildName(bottles.build), bottles.layer,
		                                            bottles.depth, bottles.hitType);
		for (uint p = 0; p < 2; p++) {
			out += Common::String::format("; phrase %u:", p);
			for (uint32 b = 0; b < 16; b++)
				out += Common::String(" ") + kNames[WizardsRuleRoom::bottleForBeat(p * 16 + b)];
		}
		out += "; bottles a phrase:";
		for (uint p = 0; p < WizardsRuleRoom::kTableSize / 8; p++) {
			int n = 0;
			for (uint32 b = 0; b < 8; b++)
				n += WizardsRuleRoom::bottleForBeat(p * 16 + b) != WizardsRuleRoom::kGap ? 1 : 0;
			out += Common::String::format(" %d", n);
		}
		out += "; leads to ROOM_P24_Wizards_Bedroom";
		debugC(3, kDebugScript, "wizard's rule room check claim: %s", out.c_str());
		delete rc;
	}

	// The game played through the engine's tick (WP-094), the song's clock
	// set directly: on a beat at 0.2 of it, then late in it at 0.7; a click on
	// a bottle as the player would make it.
	{
		Common::String next;
		int movie;
		auto tick1 = [&]() {
			next.clear();
			movie = -1;
			return tickOnce(next, movie);
		};
		auto wfr = [&]() { return dynamic_cast<WizardsRuleRoom *>(getRoomClass(kWFR)); };
		auto begin = [&](int level) {
			newGameForCheck();
			setDifficulty(level);
			_inventory->setSnacks(0);
			_sound->stop();
			_mouseDown = false;
			WizardsRuleRoom *w = wfr();
			w->setRandomForCheck(1);
			w->setClockForCheck(0);
			showRoom(kWFR);
			_script->skip();
			w->clearSounds();
			return w;
		};
		auto at = [&](uint32 beat, double part) { return (int32)((beat + part) / 1.3795 * 1000.0); };
		auto lit = [&](WizardsRuleRoom *w) {
			Common::String s;
			for (int b = 0; b < WizardsRuleRoom::kBottles; b++) {
				if (w->getBottle(b) >= 0 && _currentRoom->getStep(w->getBottle(b)) == 1)
					s += (s.empty() ? "" : "+") + Common::String(kNames[b]);
			}
			return s.empty() ? Common::String("-") : s;
		};
		auto dark = [&](WizardsRuleRoom *w) {
			for (int b = 0; b < WizardsRuleRoom::kBottles; b++) {
				if (w->getBottle(b) >= 0 && _currentRoom->getStep(w->getBottle(b)) != 0)
					return false;
			}
			return true;
		};
		// The sounds the room played, each once, in the order first heard.
		auto heard = [&](WizardsRuleRoom *w) {
			Common::Array<Common::String> names;
			const Common::String &all = w->getSounds();
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
			Common::String s;
			for (uint k = 0; k < names.size(); k++)
				s += (k ? " " : "") + names[k];
			return s;
		};
		// One beat: the click the player makes on it (-1 none), the tick on it,
		// the tick late in it. Returns the bottles lit on the beat.
		auto beatOf = [&](WizardsRuleRoom *w, uint32 beat, int clickBottle, bool &darkAfter) {
			w->setClockForCheck(at(beat, 0.2));
			if (clickBottle >= 0)
				w->click(w->getBottle(clickBottle));
			tick1();
			const Common::String l = lit(w);
			w->setClockForCheck(at(beat, 0.7));
			tick1();
			darkAfter = dark(w);
			return l;
		};

		// Entry, by level: the bottles stopped and dark, opening the cursor; the
		// lives; the counter's cels for the snacks; the dancers idle.
		{
			Common::String out = "lives";
			for (int l = 0; l < 3; l++)
				out += Common::String::format(" %u", begin(l)->getLives().size());
			WizardsRuleRoom *w = begin(0);
			Room &room = *_currentRoom;
			int bottlesFound = 0, dark0 = 0, open = 0, hit = 0;
			for (int b = 0; b < WizardsRuleRoom::kBottles; b++) {
				const int s = w->getBottle(b);
				if (s < 0)
					continue;
				bottlesFound++;
				dark0 += room.getStep(s) == 0 && room.getMode(s) == 0 ? 1 : 0;
				open += room.getRollover(s) == "ANIM_CURSOROPEN" ? 1 : 0;
				hit += room.getHitType(s) == 2 ? 1 : 0;
			}
			out += Common::String::format("; bottles %d, stopped dark %d, cursor open %d, polygon %d", bottlesFound, dark0, open, hit);
			out += Common::String::format("; counter cels %u %u", room.getStep(w->getTens()), room.getStep(w->getOnes()));
			_inventory->setSnacks(13);
			showRoom(kWFR);
			w = wfr();
			out += Common::String::format(", 13 snacks %u %u", room.getStep(w->getTens()), room.getStep(w->getOnes()));
			out += Common::String::format("; %s, %s; skulls %d %d", room.getAnim(w->getScooby().sprite).c_str(),
			                              room.getAnim(w->getWizard().sprite).c_str(), w->getSkullsWizard() >= 0 ? 1 : 0,
			                              w->getSkullsScooby() >= 0 ? 1 : 0);
			debugC(3, kDebugScript, "wizard's rule room check entry: %s", out.c_str());
		}

		// A phrase played clean: the wizard lights his bottles on beats 0-7; the
		// player clicks the same on 8-15; the first gap after is a snack.
		{
			WizardsRuleRoom *w = begin(0);
			Room &room = *_currentRoom;
			Common::String out = "lit";
			bool allDark = true;
			for (uint32 beat = 0; beat <= 16; beat++) {
				const int b = WizardsRuleRoom::bottleForBeat(beat);
				const bool players = ((beat >> 3) & 1) != 0;
				bool d = true;
				out += " " + beatOf(w, beat, players && b != WizardsRuleRoom::kGap ? b : -1, d);
				allDark = allDark && d;
				if (beat == 0 || beat == 8)
					out += Common::String::format(" (%s %s)", room.getAnim(w->getWizard().sprite).c_str(), room.getAnim(w->getScooby().sprite).c_str());
				if (beat == 11)
					out += Common::String::format(" (snacks %u)", _inventory->getSnacks());
			}
			out += Common::String::format("; late in each beat dark %d; snacks %u, cels %u %u, missed %d, phrases missed %d; heard %s", allDark ? 1 : 0,
			                              _inventory->getSnacks(), room.getStep(w->getTens()), room.getStep(w->getOnes()), w->isFailed() ? 1 : 0,
			                              w->getWrong(), heard(w).c_str());
			debugC(3, kDebugScript, "wizard's rule room check clean: %s", out.c_str());
		}

		// A beat missed (9): on the next, a glass breaks and a life goes; the
		// phrase earns nothing; the next phrase counts it.
		{
			WizardsRuleRoom *w = begin(0);
			Room &room = *_currentRoom;
			Common::String out;
			for (uint32 beat = 0; beat <= 16; beat++) {
				const int b = WizardsRuleRoom::bottleForBeat(beat);
				const bool click = ((beat >> 3) & 1) != 0 && b != WizardsRuleRoom::kGap && beat != 9;
				bool d = true;
				beatOf(w, beat, click ? b : -1, d);
				if (beat == 10) {
					int hidden = -1;
					for (uint i = 0; i < w->getLives().size(); i++) {
						if (room.isHidden(w->getLives()[i]))
							hidden = i;
					}
					out += Common::String::format("beat 10: missed %d, life %d of %u hidden, %s", w->isFailed() ? 1 : 0, hidden, w->getLives().size(),
					                              room.getAnim(w->getScooby().sprite).c_str());
				}
				if (beat == 11)
					out += Common::String::format("; beat 11: snacks %u", _inventory->getSnacks());
				if (beat == 16)
					out += Common::String::format("; beat 16: phrases missed %d, missed %d", w->getWrong(), w->isFailed() ? 1 : 0);
			}
			out += "; heard " + heard(w);
			debugC(3, kDebugScript, "wizard's rule room check missed: %s", out.c_str());
		}

		// Nothing clicked, Spookier (three lives): a life a phrase, and out of
		// them the bedroom, through movie 0xB while the wizard's clue isn't owned.
		{
			WizardsRuleRoom *w = begin(2);
			Room &room = *_currentRoom;
			Common::String out = Common::String::format("lives %u; hidden at beat", w->getLives().size());
			uint hiddenBefore = 0;
			int out3 = -1;
			for (uint32 beat = 0; beat < 80 && next.empty(); beat++) {
				bool d = true;
				beatOf(w, beat, -1, d);
				uint hidden = 0;
				for (uint i = 0; i < w->getLives().size(); i++)
					hidden += room.isHidden(w->getLives()[i]) ? 1 : 0;
				if (hidden != hiddenBefore) {
					out += Common::String::format(" %u", beat);
					hiddenBefore = hidden;
				}
				if (out3 < 0 && w->getWrong() >= 3)
					out3 = beat;
			}
			for (int t = 0; t < 2000 && next.empty(); t++)
				tick1();
			out += Common::String::format("; three missed at beat %d; to %s movie %d", out3, next.empty() ? "-" : next.c_str(), movie);
			debugC(3, kDebugScript, "wizard's rule room check lives: %s", out.c_str());
		}

		// The song over, the wizard's clue owned: the bedroom, no movie.
		{
			WizardsRuleRoom *w = begin(0);
			giveClue("WIZARD");
			w->setClockForCheck(-1);
			next.clear();
			movie = -1;
			for (int t = 0; t < 2000 && next.empty(); t++)
				tick1();
			debugC(3, kDebugScript, "wizard's rule room check song over: clue owned %d; to %s movie %d", isClueOwner("WIZARD") ? 1 : 0,
			       next.empty() ? "-" : next.c_str(), movie);
			w->setClockForCheck(-2);
		}
	}

	// The sounds the game plays.
	{
		Common::String out;
		for (int n = 1; n <= 6; n++)
			out += Common::String::format("%sWFR.Potion_%d %d", n > 1 ? ", " : "", n, pfx(Common::String::format("WFR.Potion_%d", n).c_str()));
		for (int n = 1; n <= 5; n++)
			out += Common::String::format(", Correct%d %d", n, pfx(Common::String::format("Wizards_Rule.Correct%d_1_NARRATOR", n).c_str()));
		debugC(3, kDebugScript, "wizard's rule room check sounds: %s", out.c_str());
	}

	_checking = false;
}

} // End of namespace Terraglyph
