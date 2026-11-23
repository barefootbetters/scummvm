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
#include "terraglyph/room.h"
#include "terraglyph/room_class.h"
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
	// apart by the colour in their ANIM); it is clickable. The position table
	// maps a spinner stop to an ingredient (0-5) or 6 for a gap.
	{
		RoomClass *rc = createRoomClass(*this, kWFR);
		Room::Claim bottles;
		rc->claimPlacement("OBJ_WFR_BOTTLES", bottles);
		debugC(3, kDebugScript, "wizard's rule room check claim: OBJ_WFR_BOTTLES %s; table[0..9] %d %d %d %d %d %d %d %d %d %d (6=gap); leads to ROOM_P24_Wizards_Bedroom",
		       wfrBuildName(bottles.build),
		       WizardsRuleRoom::positionIngredient(0), WizardsRuleRoom::positionIngredient(1),
		       WizardsRuleRoom::positionIngredient(2), WizardsRuleRoom::positionIngredient(3),
		       WizardsRuleRoom::positionIngredient(4), WizardsRuleRoom::positionIngredient(5),
		       WizardsRuleRoom::positionIngredient(6), WizardsRuleRoom::positionIngredient(7),
		       WizardsRuleRoom::positionIngredient(8), WizardsRuleRoom::positionIngredient(9));
		delete rc;
	}

	_checking = false;
}

} // End of namespace Terraglyph
