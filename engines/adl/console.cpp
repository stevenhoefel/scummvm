/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "common/debug-channels.h"

#include "adl/console.h"
#include "adl/display.h"
#include "adl/adl.h"

namespace Adl {

Console::Console(AdlEngine *engine) : GUI::Debugger() {
	_engine = engine;

	registerCmd("nouns", WRAP_METHOD(Console, Cmd_Nouns));
	registerCmd("verbs", WRAP_METHOD(Console, Cmd_Verbs));
	registerCmd("dump_scripts", WRAP_METHOD(Console, Cmd_DumpScripts));
	registerCmd("valid_cmds", WRAP_METHOD(Console, Cmd_ValidCommands));
	registerCmd("region", WRAP_METHOD(Console, Cmd_Region));
	registerCmd("room", WRAP_METHOD(Console, Cmd_Room));
	registerCmd("items", WRAP_METHOD(Console, Cmd_Items));
	registerCmd("give_item", WRAP_METHOD(Console, Cmd_GiveItem));
	registerCmd("vars", WRAP_METHOD(Console, Cmd_Vars));
	registerCmd("var", WRAP_METHOD(Console, Cmd_Var));
}

Common::String Console::toAscii(const Common::String &str) {
	Common::String ascii(str);

	for (uint i = 0; i < ascii.size(); ++i)
		ascii.setChar(ascii[i] & 0x7f, i);

	return ascii;
}

Common::String Console::toAppleWord(const Common::String &str) {
	Common::String apple(str);

	if (apple.size() > IDI_WORD_SIZE)
		apple.erase(IDI_WORD_SIZE);
	apple.toUppercase();

	for (uint i = 0; i < apple.size(); ++i)
		apple.setChar(APPLECHAR(apple[i]), i);

	while (apple.size() < IDI_WORD_SIZE)
		apple += APPLECHAR(' ');

	return apple;
}

bool Console::Cmd_Verbs(int argc, const char **argv) {
	if (argc != 1) {
		debugPrintf("Usage: %s\n", argv[0]);
		return true;
	}

	debugPrintf("Verbs in alphabetical order:\n");
	printWordMap(_engine->_verbs);
	return true;
}

bool Console::Cmd_Nouns(int argc, const char **argv) {
	if (argc != 1) {
		debugPrintf("Usage: %s\n", argv[0]);
		return true;
	}

	debugPrintf("Nouns in alphabetical order:\n");
	printWordMap(_engine->_nouns);
	return true;
}

bool Console::Cmd_ValidCommands(int argc, const char **argv) {
	if (argc != 1) {
		debugPrintf("Usage: %s\n", argv[0]);
		return true;
	}

	WordMap::const_iterator verb, noun;
	bool is_any;

	for (verb = _engine->_verbs.begin(); verb != _engine->_verbs.end(); ++verb) {
		for (noun = _engine->_nouns.begin(); noun != _engine->_nouns.end(); ++noun) {
			if (_engine->isInputValid(verb->_value, noun->_value, is_any) && !is_any)
				debugPrintf("%s %s\n", toAscii(verb->_key).c_str(), toAscii(noun->_key).c_str());
		}
		if (_engine->isInputValid(verb->_value, IDI_ANY, is_any))
			debugPrintf("%s *\n", toAscii(verb->_key).c_str());
	}
	if (_engine->isInputValid(IDI_ANY, IDI_ANY, is_any))
		debugPrintf("* *\n");

	return true;
}

void Console::dumpScripts(const Common::String &prefix) {
	for (byte roomNr = 1; roomNr <= _engine->_state.rooms.size(); ++roomNr) {
		_engine->loadRoom(roomNr);
		if (_engine->_roomData.commands.size() != 0) {
			_engine->_dumpFile->open(prefix + Common::String::format("%03d.ADL", roomNr).c_str());
			_engine->doAllCommands(_engine->_roomData.commands, IDI_ANY, IDI_ANY);
			_engine->_dumpFile->close();
		}
	}
	_engine->loadRoom(_engine->_state.room);

	_engine->_dumpFile->open(prefix + "GLOBAL.ADL");
	_engine->doAllCommands(_engine->_globalCommands, IDI_ANY, IDI_ANY);
	_engine->_dumpFile->close();

	_engine->_dumpFile->open(prefix + "RESPONSE.ADL");
	_engine->doAllCommands(_engine->_roomCommands, IDI_ANY, IDI_ANY);
	_engine->_dumpFile->close();
}

bool Console::Cmd_DumpScripts(int argc, const char **argv) {
	if (argc != 1) {
		debugPrintf("Usage: %s\n", argv[0]);
		return true;
	}

	bool oldFlag = DebugMan.isDebugChannelEnabled(kDebugChannelScript);

	DebugMan.enableDebugChannel("Script");

	_engine->_dumpFile = new Common::DumpFile();

	if (_engine->_state.regions.empty()) {
		dumpScripts();
	} else {
		const byte oldRegion = _engine->_state.region;
		const byte oldPrevRegion = _engine->_state.prevRegion;
		const byte oldRoom = _engine->_state.room;

		for (byte regionNr = 1; regionNr <= _engine->_state.regions.size(); ++regionNr) {
			_engine->switchRegion(regionNr);
			dumpScripts(Common::String::format("%03d-", regionNr));
		}

		_engine->switchRegion(oldRegion);
		_engine->_state.prevRegion = oldPrevRegion;
		_engine->_state.room = oldRoom;
		_engine->loadRoom(oldRoom);
	}

	delete _engine->_dumpFile;
	_engine->_dumpFile = nullptr;

	if (!oldFlag)
		DebugMan.disableDebugChannel("Script");

	return true;
}

void Console::prepareGame() {
	_engine->clearScreen();
	_engine->loadRoom(_engine->_state.room);
	_engine->showRoom();
	_engine->_display->updateTextScreen();
	_engine->_display->updateHiResScreen();
}

bool Console::Cmd_Region(int argc, const char **argv) {
	if (argc > 2) {
		debugPrintf("Usage: %s [<new_region>]\n", argv[0]);
		return true;
	}

	if (argc == 2) {
		if (!_engine->_canRestoreNow) {
			debugPrintf("Cannot change regions right now\n");
			return true;
		}

		uint regionCount = _engine->_state.regions.size();
		uint region = strtoul(argv[1], NULL, 0);
		if (region < 1 || region > regionCount) {
			debugPrintf("Region %u out of valid range [1, %u]\n", region, regionCount);
			return true;
		}

		_engine->switchRegion(region);
		prepareGame();
	}

	debugPrintf("Current region: %u\n", _engine->_state.region);

	return true;
}

bool Console::Cmd_Room(int argc, const char **argv) {
	if (argc > 2) {
		debugPrintf("Usage: %s [<new_room>]\n", argv[0]);
		return true;
	}

	if (argc == 2) {
		if (!_engine->_canRestoreNow) {
			debugPrintf("Cannot change rooms right now\n");
			return true;
		}

		uint roomCount = _engine->_state.rooms.size();
		uint room = strtoul(argv[1], NULL, 0);
		if (room < 1 || room > roomCount) {
			debugPrintf("Room %u out of valid range [1, %u]\n", room, roomCount);
			return true;
		}

		_engine->switchRoom(room);
		prepareGame();
	}

	debugPrintf("Current room: %u\n", _engine->_state.room);

	return true;
}

bool Console::Cmd_Items(int argc, const char **argv) {
	if (argc != 1) {
		debugPrintf("Usage: %s\n", argv[0]);
		return true;
	}

	Common::List<Item>::const_iterator item;

	for (item = _engine->_state.items.begin(); item != _engine->_state.items.end(); ++item)
		printItem(*item);

	return true;
}

bool Console::Cmd_GiveItem(int argc, const char **argv) {
	if (argc != 2) {
		debugPrintf("Usage: %s <ID | name>\n", argv[0]);
		return true;
	}

	Common::List<Item>::iterator item;

	char *end;
	uint id = strtoul(argv[1], &end, 0);

	if (*end != 0) {
		Common::Array<Item *> matches;

		Common::String name = toAppleWord(argv[1]);

		if (!_engine->_nouns.contains(name)) {
			debugPrintf("Item '%s' not found\n", argv[1]);
			return true;
		}

		byte noun = _engine->_nouns[name];

		for (item = _engine->_state.items.begin(); item != _engine->_state.items.end(); ++item) {
			if (item->noun == noun)
				matches.push_back(&*item);
		}

		if (matches.size() == 0) {
			debugPrintf("Item '%s' not found\n", argv[1]);
			return true;
		}

		if (matches.size() > 1) {
			debugPrintf("Multiple matches found, please use item ID:\n");
			for (uint i = 0; i < matches.size(); ++i)
				printItem(*matches[i]);
			return true;
		}

		matches[0]->room = IDI_ANY;
		debugPrintf("OK\n");
		return true;
	}

	for (item = _engine->_state.items.begin(); item != _engine->_state.items.end(); ++item)
		if (item->id == id) {
			item->room = IDI_ANY;
			debugPrintf("OK\n");
			return true;
		}

	debugPrintf("Item %i not found\n", id);
	return true;
}

bool Console::Cmd_Vars(int argc, const char **argv) {
	if (argc != 1) {
		debugPrintf("Usage: %s\n", argv[0]);
		return true;
	}

	Common::StringArray vars;
	for (uint i = 0; i < _engine->_state.vars.size(); ++i)
		vars.push_back(Common::String::format("%3d: %3d", i, _engine->_state.vars[i]));

	debugPrintf("Variables:\n");
	debugPrintColumns(vars);

	return true;
}

bool Console::Cmd_Var(int argc, const char **argv) {
	if (argc < 2 || argc > 3) {
		debugPrintf("Usage: %s <index> [<value>]\n", argv[0]);
		return true;
	}

	uint varCount = _engine->_state.vars.size();
	uint var = strtoul(argv[1], NULL, 0);

	if (var >= varCount) {
		debugPrintf("Variable %u out of valid range [0, %u]\n", var, varCount - 1);
		return true;
	}

	if (argc == 3) {
		uint value = strtoul(argv[2], NULL, 0);
		_engine->_state.vars[var] = value;
	}

	debugPrintf("%3d: %3d\n", var, _engine->_state.vars[var]);

	return true;
}

void Console::printItem(const Item &item) {
	Common::String name, desc, state;

	if (item.noun > 0)
		name = _engine->_priNouns[item.noun - 1];

	desc = toAscii(_engine->getItemDescription(item));
	if (desc.lastChar() == '\r')
		desc.deleteLastChar();

	switch (item.state) {
	case IDI_ITEM_NOT_MOVED:
		state = "PLACED";
		break;
	case IDI_ITEM_DROPPED:
		state = "DROPPED";
		break;
	case IDI_ITEM_DOESNT_MOVE:
		state = "FIXED";
		break;
	}

	debugPrintf("%3d %s %-30s %-10s %-8s (%3d, %3d)\n", item.id, name.c_str(), desc.c_str(), _engine->itemRoomStr(item.room).c_str(), state.c_str(), item.position.x, item.position.y);
}

void Console::printWordMap(const WordMap &wordMap) {
	Common::StringArray words;
	WordMap::const_iterator verb;

	for (verb = wordMap.begin(); verb != wordMap.end(); ++verb)
		words.push_back(Common::String::format("%s: %3d", toAscii(verb->_key).c_str(), wordMap[verb->_key]));

	Common::sort(words.begin(), words.end());

	debugPrintColumns(words);
}

} // End of namespace Adl
