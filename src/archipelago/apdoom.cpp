//
// Copyright(C) 2023 David St-Louis
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
// *Source file for interfacing with archipelago*
//

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#ifdef _MSC_VER
#include <direct.h>
#endif
#else
#include <sys/types.h>
#include <sys/stat.h>
#endif


#include "apdoom.h"
#include "Archipelago.h"
#include <json/json.h>
#include <memory.h>
#include <chrono>
#include <thread>
#include <vector>
#include <fstream>
#include <sstream>
#include <set>

// DEBUG
#include <iostream>

#if defined(_WIN32)
static wchar_t *ConvertMultiByteToWide(const char *str, UINT code_page)
{
    wchar_t *wstr = NULL;
    int wlen = 0;

    wlen = MultiByteToWideChar(code_page, 0, str, -1, NULL, 0);

    if (!wlen)
    {
        errno = EINVAL;
        printf("APDOOM: ConvertMultiByteToWide: Failed to convert path to wide encoding.\n");
        return NULL;
    }

    wstr = (wchar_t*)malloc(sizeof(wchar_t) * wlen);

    if (!wstr)
    {
        printf("APDOOM: ConvertMultiByteToWide: Failed to allocate new string.");
        return NULL;
    }

    if (MultiByteToWideChar(code_page, 0, str, -1, wstr, wlen) == 0)
    {
        errno = EINVAL;
        printf("APDOOM: ConvertMultiByteToWide: Failed to convert path to wide encoding.\n");
        free(wstr);
        return NULL;
    }

    return wstr;
}

static wchar_t *AP_ConvertUtf8ToWide(const char *str)
{
    return ConvertMultiByteToWide(str, CP_UTF8);
}
#endif



//
// Create a directory
//

static void AP_MakeDirectory(const char *path)
{
#ifdef _WIN32
    wchar_t *wdir;

    wdir = AP_ConvertUtf8ToWide(path);

    if (!wdir)
    {
        return;
    }

    _wmkdir(wdir);

    free(wdir);
#else
    mkdir(path, 0755);
#endif
}


static FILE *AP_fopen(const char *filename, const char *mode)
{
#ifdef _WIN32
    FILE *file;
    wchar_t *wname = NULL;
    wchar_t *wmode = NULL;

    wname = AP_ConvertUtf8ToWide(filename);

    if (!wname)
    {
        return NULL;
    }

    wmode = AP_ConvertUtf8ToWide(mode);

    if (!wmode)
    {
        free(wname);
        return NULL;
    }

    file = _wfopen(wname, wmode);

    free(wname);
    free(wmode);

    return file;
#else
    return fopen(filename, mode);
#endif
}


static int AP_FileExists(const char *filename)
{
    FILE *fstream;

    fstream = AP_fopen(filename, "r");

    if (fstream != NULL)
    {
        fclose(fstream);
        return true;
    }
    else
    {
        // If we can't open because the file is a directory, the 
        // "file" exists at least!

        return errno == EISDIR;
    }
}


enum class ap_game_t
{
	doom,
	doom2,
	heretic
};


ap_state_t ap_state;
int ap_is_in_game = 0;
int ap_episode_count = -1;

static ap_game_t ap_base_game;
static int ap_weapon_count = -1;
static int ap_ammo_count = -1;
static int ap_powerup_count = -1;
static int ap_inventory_count = -1;
static int max_map_count = -1;
static ap_settings_t ap_settings;
static AP_RoomInfo ap_room_info;
static std::vector<int64_t> ap_item_queue; // We queue when we're in the menu.
static bool ap_was_connected = false; // Got connected at least once. That means the state is valid
static std::set<int64_t> ap_progressive_locations;
static bool ap_initialized = false;
static std::vector<std::string> ap_cached_messages;
static std::string ap_save_dir_name;
static std::vector<ap_notification_icon_t> ap_notification_icons;
//static bool ap_check_sanity = false; // Unreferenced?


void f_itemclr();
void f_itemrecv(int64_t item_id, int player_id, bool notify_player);
void f_locrecv(int64_t loc_id);
void f_locinfo(std::vector<AP_NetworkItem> loc_infos);
void f_goal(int);
void f_difficulty(int);
void f_random_monsters(int);
void f_random_items(int);
void f_random_music(int);
void f_flip_levels(int);
void f_check_sanity(int);
void f_reset_level_on_death(int);
void f_episode1(int);
void f_episode2(int);
void f_episode3(int);
void f_episode4(int);
void f_episode5(int);
void f_two_ways_keydoors(int);
void f_ammo1start(int);
void f_ammo2start(int);
void f_ammo3start(int);
void f_ammo4start(int);
void f_ammo5start(int);
void f_ammo6start(int);
void f_ammo1add(int);
void f_ammo2add(int);
void f_ammo3add(int);
void f_ammo4add(int);
void f_ammo5add(int);
void f_ammo6add(int);
void load_state();
void save_state();
void APSend(std::string msg);

// ===== PWAD SUPPORT =========================================================
// All of these are loaded from json on game startup

// The game name that gets passed to Archipelago when connecting.
static std::string archipelago_game_name;

// The name of the IWAD that this game definition requires
static std::string iwad_name;

// Every PWAD that needs to be loaded to play the given PWAD (includes APDOOM.WAD/APHERETIC.WAD)
static std::vector<std::string> pwad_names;

// Names of maps in the PWAD, because we need to keep a const char *pointer around for ap_level_info_t
// Stored in a deque so we can quickly add a new name to the back, do back().c_str(), and never think about it again
static std::deque<std::string> individual_map_names;

// Stores the name of a lump into a 9-byte char array. Does nothing if src is not a string.
static void store_lump_name(char *dest, Json::Value src)
{
	if (!src.isString())
		return;

	strncpy(dest, src.asCString(), 8);
	dest[8] = 0;
}

// ----------------------------------------------------------------------------
// Definitions for each level select screen, made generic.
// See ap_levelselect_t in apdoom.h for a lot more detailed information about this.
static std::vector<ap_levelselect_t> level_select_screens;

// Parses one mapinfo structure from a JSON blob, while taking care to not overwrite
// any default options that may have already been set.
static void json_parse_single_mapinfo(ap_levelselect_map_t *info, const Json::Value json)
{
	info->x = json.get("x", info->x).asInt();
	info->y = json.get("y", info->y).asInt();

	if (!json["cursor"].isNull())
	{
		store_lump_name(info->cursor.graphic, json["cursor"]["graphic"]);
		info->cursor.x = json["cursor"].get("x", info->cursor.x).asInt();
		info->cursor.y = json["cursor"].get("y", info->cursor.y).asInt();
	}

	if (!json["image"].isNull())
	{
		store_lump_name(info->image.graphic, json["image"]["graphic"]);
		info->image.x = json["image"].get("x", info->image.x).asInt();
		info->image.y = json["image"].get("y", info->image.y).asInt();
	}

	if (!json["keys"].isNull())
	{
		if (!json["keys"]["relative_to"].isNull())
		{
			std::string result = json["keys"]["relative_to"].asString();
			if (result == "map")              info->keys.relative_to = 0;
			else if (result == "image")       info->keys.relative_to = 1;
			else if (result == "image-right") info->keys.relative_to = 2;
		}
		info->keys.x = json["keys"].get("x", info->keys.x).asInt();
		info->keys.y = json["keys"].get("y", info->keys.y).asInt();
		info->keys.spacing_x = json["keys"].get("spacing_x", info->keys.spacing_x).asInt();
		info->keys.spacing_y = json["keys"].get("spacing_y", info->keys.spacing_y).asInt();
		info->keys.checkmark_x = json["keys"].get("checkmark_x", info->keys.checkmark_x).asInt();
		info->keys.checkmark_y = json["keys"].get("checkmark_y", info->keys.checkmark_y).asInt();		
		info->keys.use_checkmark = json["keys"].get("use_checkmark", info->keys.use_checkmark).asBool();
	}

	if (!json["checks"].isNull())
	{
		if (!json["checks"]["relative_to"].isNull())
		{
			std::string result = json["checks"]["relative_to"].asString();
			if (result == "map")              info->checks.relative_to = 0;
			else if (result == "image")       info->checks.relative_to = 1;
			else if (result == "image-right") info->checks.relative_to = 2;
			else if (result == "keys")        info->checks.relative_to = 3;
			else if (result == "keys-last")   info->checks.relative_to = 4;
		}
		info->checks.x = json["checks"].get("x", info->checks.x).asInt();
		info->checks.y = json["checks"].get("y", info->checks.y).asInt();
	}
}

static int json_parse_level_select(Json::Value json)
{
	if (json.isNull())
	{
		printf("APDOOM: Definitions missing required 'level_select'.\n");
		return 0;
	}

	// Defaults for level select mapinfo, if not specified anywhere else.
	char default_map_image[9] = "INTERPIC";
	ap_levelselect_map_t default_mapinfo;
	memset(&default_mapinfo, 0, sizeof(ap_levelselect_map_t));

	// Specifying defaults?
	if (!json["defaults"].isNull())
	{
		json_parse_single_mapinfo(&default_mapinfo, json["defaults"]["maps"]);
		store_lump_name(default_map_image, json["defaults"]["background_image"]);
	}

	const int ep_count = (int)json["episodes"].size();
	level_select_screens.resize(ep_count);

	for (int idx = 0; idx < ep_count; ++idx)
	{
		Json::Value episode_defs = json["episodes"][idx];

		if (!episode_defs["background_image"].isNull())
			store_lump_name(level_select_screens[idx].background_image, episode_defs["background_image"]);
		else
			memcpy(level_select_screens[idx].background_image, default_map_image, 9);

		const int map_count = (int)episode_defs["maps"].size();
		for (int map_idx = 0; map_idx < map_count; ++map_idx)
		{
			memcpy(&level_select_screens[idx].map_info[map_idx], &default_mapinfo, sizeof(ap_levelselect_map_t));
			json_parse_single_mapinfo(&level_select_screens[idx].map_info[map_idx], episode_defs["maps"][map_idx]);
		}
	}
	return 1;
}

// ----------------------------------------------------------------------------
// Tweaks that we make for each map, to remove softlocks and improve AP experiences.
// <episode, <map, <tweaks>>>
static std::map<int, std::map<int, std::vector<ap_maptweak_t>>> map_tweak_list;

static ap_level_index_t get_index_from_map_name(std::string lump_name)
{
	int game_ep = 0;
	int game_map = std::stoi(lump_name.substr(3));

	if (!lump_name.compare(0, 3, "MAP"))
		game_ep = 1;
	else if (lump_name[0] == 'E' && lump_name[2] == 'M')
		game_ep = (lump_name[1] - '0');
	else
		return {-1, -1};

	return ap_make_level_index(game_ep, game_map);
}

static void insert_new_tweak(std::vector<ap_maptweak_t> &tweak_list, allowed_tweaks_t type, int target, Json::Value value)
{
	if (value.isNull())
		return;

	ap_maptweak_t new_tweak = {type, target, 0, ""};
	if (value.isString())
		store_lump_name(new_tweak.string, value);
	else if (value.isInt())
		new_tweak.value = value.asInt();
	else if (value.isBool())
		new_tweak.value = value.asBool();

	tweak_list.emplace_back(new_tweak);
}

static void parse_hub_tweak_block(Json::Value json, std::vector<ap_maptweak_t> &tweak_list)
{
	// There's only one thing that can be tweaked with the hub, so the target is ignored
	insert_new_tweak(tweak_list, TWEAK_HUB_X, 0, json["x"]);
	insert_new_tweak(tweak_list, TWEAK_HUB_Y, 0, json["y"]);	
}

static void parse_things_tweak_block(Json::Value json, std::vector<ap_maptweak_t> &tweak_list)
{
	for (std::string &key_target : json.getMemberNames())
	{
		const int target = std::stoi(key_target);
		insert_new_tweak(tweak_list, TWEAK_MAPTHING_X,     target, json[key_target]["x"]);
		insert_new_tweak(tweak_list, TWEAK_MAPTHING_Y,     target, json[key_target]["y"]);
		insert_new_tweak(tweak_list, TWEAK_MAPTHING_TYPE,  target, json[key_target]["type"]);
		insert_new_tweak(tweak_list, TWEAK_MAPTHING_ANGLE, target, json[key_target]["angle"]);
	}
}

static void parse_sectors_tweak_block(Json::Value json, std::vector<ap_maptweak_t> &tweak_list)
{
	for (std::string &key_target : json.getMemberNames())
	{
		const int target = std::stoi(key_target);
		insert_new_tweak(tweak_list, TWEAK_SECTOR_SPECIAL,     target, json[key_target]["special"]);
		insert_new_tweak(tweak_list, TWEAK_SECTOR_TAG,         target, json[key_target]["tag"]);
		insert_new_tweak(tweak_list, TWEAK_SECTOR_FLOOR_PIC,   target, json[key_target]["floor_pic"]);
		insert_new_tweak(tweak_list, TWEAK_SECTOR_CEILING_PIC, target, json[key_target]["ceiling_pic"]);
	}
}

static void parse_linedefs_tweak_block(Json::Value json, std::vector<ap_maptweak_t> &tweak_list)
{
	for (std::string &key_target : json.getMemberNames())
	{
		const int target = std::stoi(key_target);
		insert_new_tweak(tweak_list, TWEAK_LINEDEF_SPECIAL, target, json[key_target]["special"]);
		insert_new_tweak(tweak_list, TWEAK_LINEDEF_TAG,     target, json[key_target]["tag"]);
		insert_new_tweak(tweak_list, TWEAK_LINEDEF_FLAGS,   target, json[key_target]["flags"]);
	}
}

static void parse_sidedefs_tweak_block(Json::Value json, std::vector<ap_maptweak_t> &tweak_list)
{
	for (std::string &key_target : json.getMemberNames())
	{
		const int target = std::stoi(key_target);
		insert_new_tweak(tweak_list, TWEAK_SIDEDEF_LOWER,  target, json[key_target]["lower"]);
		insert_new_tweak(tweak_list, TWEAK_SIDEDEF_MIDDLE, target, json[key_target]["middle"]);
		insert_new_tweak(tweak_list, TWEAK_SIDEDEF_UPPER,  target, json[key_target]["upper"]);
		insert_new_tweak(tweak_list, TWEAK_SIDEDEF_X,      target, json[key_target]["x"]);
		insert_new_tweak(tweak_list, TWEAK_SIDEDEF_Y,      target, json[key_target]["y"]);
	}
}

static int json_parse_map_tweaks(Json::Value json)
{
	if (json.isNull())
		return 1; // Optional

	for (std::string &map_lump_name : json.getMemberNames())
	{
		ap_level_index_t idx = get_index_from_map_name(map_lump_name);
		if (idx.ep == -1)
		{
			printf("APDOOM: 'map_tweaks' contains invalid map name '%s'.\n", map_lump_name.c_str());
			return 0;
		}

		map_tweak_list.insert({idx.ep, {}});
		map_tweak_list[idx.ep].insert({idx.map, {}});

		for (std::string &tweak_type : json[map_lump_name].getMemberNames())
		{
			if (tweak_type == "hub")
				parse_hub_tweak_block(json[map_lump_name]["hub"], map_tweak_list[idx.ep][idx.map]);
			else if (tweak_type == "things")
				parse_things_tweak_block(json[map_lump_name]["things"], map_tweak_list[idx.ep][idx.map]);
			else if (tweak_type == "sectors")
				parse_sectors_tweak_block(json[map_lump_name]["sectors"], map_tweak_list[idx.ep][idx.map]);
			else if (tweak_type == "linedefs")
				parse_linedefs_tweak_block(json[map_lump_name]["linedefs"], map_tweak_list[idx.ep][idx.map]);
			else if (tweak_type == "sidedefs")
				parse_sidedefs_tweak_block(json[map_lump_name]["sidedefs"], map_tweak_list[idx.ep][idx.map]);
			else
				printf("APDOOM: Unknown tweak type '%s', ignoring\n", tweak_type.c_str());
		}
	}

	for (auto &it_ep : map_tweak_list)
	{
		for (auto &it_map : it_ep.second)
		{
			for (ap_maptweak_t &it : it_map.second)
			{
				printf("(%i, %i): [%02x] %i %i %s\n", it_ep.first, it_map.first, it.type, it.target, it.value, it.string);
			}
		}
	}
	return 1;
}

// ----------------------------------------------------------------------------
// outer vector is episode, inner is map
static std::vector<std::vector<ap_level_info_t>> preloaded_level_info;

static int json_parse_level_info(Json::Value json)
{
	if (json.isNull())
	{
		printf("APDOOM: Definitions missing required 'level_info'.\n");
		return 0;
	}

	const int episode_count = (int)json.size();
	preloaded_level_info.resize(episode_count);

	ap_level_info_t new_level;
	for (int ep = 0; ep < episode_count; ++ep)
	{
		const int map_count = (int)json[ep].size();
		preloaded_level_info[ep].resize(map_count);

		for (int map = 0; map < map_count; ++map)
		{
			Json::Value map_info = json[ep][map];

			individual_map_names.emplace_back(map_info["_name"].asString());
			new_level.name = individual_map_names.back().c_str();

			new_level.game_episode = map_info["game_map"][0].asInt();
			new_level.game_map = map_info["game_map"][1].asInt();
			new_level.keys[0] = map_info["key"][0].asBool();
			new_level.keys[1] = map_info["key"][1].asBool();
			new_level.keys[2] = map_info["key"][2].asBool();
			new_level.use_skull[0] = map_info["use_skull"][0].asBool();
			new_level.use_skull[1] = map_info["use_skull"][1].asBool();
			new_level.use_skull[2] = map_info["use_skull"][2].asBool();

			// These used to be stored in the structures, but are now recalculated as we load.
			new_level.thing_count = map_info["thing_list"].size();
			new_level.check_count = 0;
			new_level.sanity_check_count = 0;

			if (new_level.thing_count > AP_MAX_THING)
			{
				printf("APDOOM: %s: Too many things! The max is %i\n", new_level.name, AP_MAX_THING);
				return 0;
			}

			Json::Value map_things = map_info["thing_list"];
			for (int idx = 0; idx < new_level.thing_count; ++idx)
			{
				new_level.thing_infos[idx].index = idx;
				if (map_things[idx].isInt())
				{
					// Things which are not AP items are only stored as their doomednum.
					new_level.thing_infos[idx].doom_type = map_things[idx].asInt();
					new_level.thing_infos[idx].check_sanity = false;
					new_level.thing_infos[idx].unreachable = true;
				}
				else
				{
					// Things which _are_ AP items are stored as an array.
					// [0] is the doomednum, [1] is the checksanity boolean.
					new_level.thing_infos[idx].doom_type = map_things[idx][0].asInt();
					new_level.thing_infos[idx].check_sanity = map_things[idx][1].asBool();
					new_level.thing_infos[idx].unreachable = false;
					++new_level.check_count;
					// sanity_check_count was always handled later, anyway.
				}
			}

			// Copy structure into our vector
			preloaded_level_info[ep][map] = new_level;
		}
	}
	return 1;
}


// ----------------------------------------------------------------------------
// <doomednum>
static std::set<int> preloaded_location_types;

static int json_parse_location_types(Json::Value json)
{
	if (json.isNull())
	{
		printf("APDOOM: Definitions missing required 'location_types'.\n");
		return 0;
	}

	for (auto &doomednum : json)
		preloaded_location_types.emplace(doomednum.asInt());
	return 1;
}

// ----------------------------------------------------------------------------
// <episode, <map, <index, ap-location-num>>> 
static std::map<int, std::map<int, std::map<int, int64_t>>> preloaded_location_table;

static int json_parse_location_table(Json::Value json)
{
	if (json.isNull())
	{
		printf("APDOOM: Definitions missing required 'location_table'.\n");
		return 0;
	}

	for (std::string &key_episode : json.getMemberNames())
	{
		const int episode_num = std::stoi(key_episode);
		preloaded_location_table.insert({episode_num, {}});

		for (std::string &key_map : json[key_episode].getMemberNames())
		{
			const int map_num = std::stoi(key_map);
			preloaded_location_table[episode_num].insert({map_num, {}});

			Json::Value items_in_map = json[key_episode][key_map];
			for (std::string &key_item_idx : items_in_map.getMemberNames())
			{
				const int item_idx = std::stoi(key_item_idx);
				const int64_t ap_item_id = items_in_map[key_item_idx].asInt64();
				preloaded_location_table[episode_num][map_num].insert({item_idx, ap_item_id});
			}
		}
	}
	return 1;
}

// ----------------------------------------------------------------------------
// <ap-item-num, <doomednum, episode, map>>
static std::map<int64_t, ap_item_t> preloaded_item_table;

static int json_parse_item_table(Json::Value json)
{
	if (json.isNull())
	{
		printf("APDOOM: Definitions missing required 'item_table'.\n");
		return 0;
	}

	for (std::string &json_key : json.getMemberNames())
	{
		const int64_t ap_item_id = std::stoll(json_key);

		Json::Value json_value = json[json_key];
		const int doomednum = json_value[0].asInt();
		const int ep = json_value.get(1, -1).asInt();
		const int map = json_value.get(2, -1).asInt();

		preloaded_item_table.insert({ap_item_id, {doomednum, ep, map}});
	}	
	return 1;
}

// ----------------------------------------------------------------------------
// <doomednum, sprite-lump-name>
static std::map<int, std::string> preloaded_type_sprites;

static int json_parse_type_sprites(Json::Value json)
{
	if (json.isNull())
	{
		printf("APDOOM: Definitions missing required 'type_sprites'.\n");
		return 0;
	}

	for (std::string &json_key : json.getMemberNames())
	{
		const int doomednum = std::stoi(json_key);
		preloaded_type_sprites.insert({doomednum, json[json_key].asString()});
	}
	return 1;
}

// ----------------------------------------------------------------------------

// Returns positive on successful load, 0 for failure.
int ap_preload_defs_for_game(const char *game_name)
{
	std::string filename = std::string("defs/") + game_name + std::string(".json");
	std::ifstream f(filename);
	if (!f.is_open())
	{
		printf("APDOOM: Can't find a definitions file for \"%s\" in the \"defs\" folder\n", game_name);
		return 0;
	}

	Json::Value defs_json;
	f >> defs_json;
	f.close();

	archipelago_game_name = defs_json["_game_name"].asString();

	// Recognize supported IWADs, and set up game info for them automatically.
	iwad_name = defs_json["_iwad"].asString();
	if (iwad_name == "HERETIC.WAD")
		ap_base_game = ap_game_t::heretic;
	else if (iwad_name == "DOOM.WAD" || iwad_name == "CHEX.WAD")
		ap_base_game = ap_game_t::doom;
	else // All others are Doom 2 based, I think?
		ap_base_game = ap_game_t::doom2;

	// Track PWADs that we need to force load later, if this is a PWAD game def.
	if (!defs_json["_pwads"].isNull())
	{
		for (auto &pwad : defs_json["_pwads"])
			pwad_names.emplace_back(pwad.asString());
	}

	// Load location type table (used to determine which items are AP items)
	if (!json_parse_location_types(defs_json["ap_location_types"]))
		return 0;
	if (!json_parse_type_sprites(defs_json["type_sprites"]))
		return 0;
	if (!json_parse_item_table(defs_json["item_table"]))
		return 0;
	if (!json_parse_location_table(defs_json["location_table"]))
		return 0;
	if (!json_parse_level_info(defs_json["level_info"]))
		return 0;
	if (!json_parse_map_tweaks(defs_json["map_tweaks"]))
		return 0;
	if (!json_parse_level_select(defs_json["level_select"]))
		return 0;

	return 1;
} 

// Returns the name of the IWAD to load.
const char *ap_get_iwad_name()
{
	return iwad_name.c_str();
}

// Returns an array of all PWADs to load, ending with "-".
const char *ap_get_pwad_name(unsigned int id)
{
	if (id >= pwad_names.size())
		return NULL;
	return pwad_names[id].c_str();
}

int ap_is_location_type(int doom_type)
{
	return preloaded_location_types.count(doom_type);
}


ap_levelselect_t *ap_get_level_select_info(unsigned int ep)
{
	if (ep >= level_select_screens.size())
		return NULL;
	return &level_select_screens[ep];
}

// ----------------------------------------------------------------------------

// These are used to do iteration with ap_get_map_tweaks
static ap_level_index_t gmt_level;
static allowed_tweaks_t gmt_type_mask;
static unsigned int gmt_i;

void ap_init_map_tweaks(ap_level_index_t idx, allowed_tweaks_t type_mask)
{
	gmt_i = 0;
	gmt_level.ep = idx.ep;
	gmt_level.map = idx.map;
	gmt_type_mask = type_mask;
}

ap_maptweak_t *ap_get_map_tweaks()
{
	// If map isn't present (has no tweaks), do nothing.
	if (map_tweak_list.count(gmt_level.ep) == 0
		|| map_tweak_list[gmt_level.ep].count(gmt_level.map) == 0)
	{
		return NULL;		
	}

	std::vector<ap_maptweak_t> &tweak_list = map_tweak_list[gmt_level.ep][gmt_level.map];
	while (gmt_i < tweak_list.size())
	{
		ap_maptweak_t *tweak = &tweak_list[gmt_i++];
		if ((tweak->type & TWEAK_TYPE_MASK) != gmt_type_mask)
			continue;
		return tweak;
	}
	return NULL;
}

// ============================================================================

static int get_original_music_for_level(int ep, int map)
{
	switch (ap_base_game)
	{
		case ap_game_t::doom:
		{
			int ep4_music[] = {
				// Song - Who? - Where?

				2 * 9 + 3 + 1, //mus_e3m4,        // American     e4m1
				2 * 9 + 1 + 1, //mus_e3m2,        // Romero       e4m2
				2 * 9 + 2 + 1, //mus_e3m3,        // Shawn        e4m3
				0 * 9 + 4 + 1, //mus_e1m5,        // American     e4m4
				1 * 9 + 6 + 1, //mus_e2m7,        // Tim          e4m5
				1 * 9 + 3 + 1, //mus_e2m4,        // Romero       e4m6
				1 * 9 + 5 + 1, //mus_e2m6,        // J.Anderson   e4m7 CHIRON.WAD
				1 * 9 + 4 + 1, //mus_e2m5,        // Shawn        e4m8
				0 * 9 + 8 + 1  //mus_e1m9,        // Tim          e4m9
			};

			if (ep == 4) return ep4_music[map - 1];
			return 1 + (ep - 1) * ap_get_map_count(ep) + (map - 1);
		}
		case ap_game_t::doom2:
		{
			return 52 + ap_index_to_map({ep - 1, map - 1}) - 1;
		}
		case ap_game_t::heretic:
		{
			return (ep - 1) * ap_get_map_count(ep) + (map - 1);
		}
	}

	// For now for doom and heretic
	return -1;
}

static std::vector<std::vector<ap_level_info_t>>& get_level_info_table()
{
#if 1
	return preloaded_level_info;
#else
	switch (ap_base_game)
	{
		default: // (Indeterminate state?)
		case ap_game_t::doom: return ap_doom_level_infos;
		case ap_game_t::doom2: return ap_doom2_level_infos;
		case ap_game_t::heretic: return ap_heretic_level_infos;
	}
#endif
}


int ap_get_map_count(int ep)
{
	--ep;
	auto& level_info_table = get_level_info_table();
	if (ep < 0 || ep >= (int)level_info_table.size()) return -1;
	return (int)level_info_table[ep].size();
}


ap_level_info_t* ap_get_level_info(ap_level_index_t idx)
{
	auto& level_info_table = get_level_info_table();
	if (idx.ep < 0 || idx.ep >= (int)level_info_table.size()) return nullptr;
	if (idx.map < 0 || idx.map >= (int)level_info_table[idx.ep].size()) return nullptr;
	return &level_info_table[idx.ep][idx.map];
}


ap_level_state_t* ap_get_level_state(ap_level_index_t idx)
{
	return &ap_state.level_states[idx.ep * max_map_count + idx.map];
}


static const std::map<int64_t, ap_item_t>& get_item_type_table()
{
#if 1
	return preloaded_item_table;
#else
	switch (ap_base_game)
	{
		default: // (Indeterminate state?)
		case ap_game_t::doom: return ap_doom_item_table;
		case ap_game_t::doom2: return ap_doom2_item_table;
		case ap_game_t::heretic: return ap_heretic_item_table;
	}
#endif
}


static const std::map<int /* ep */, std::map<int /* map */, std::map<int /* index */, int64_t /* loc id */>>>& get_location_table()
{
#if 1
	return preloaded_location_table;
#else
	switch (ap_base_game)
	{
		default: // (Indeterminate state?)
		case ap_game_t::doom: return ap_doom_location_table;
		case ap_game_t::doom2: return ap_doom2_location_table;
		case ap_game_t::heretic: return ap_heretic_location_table;
	}
#endif
}


std::string string_to_hex(const char* str)
{
    static const char hex_digits[] = "0123456789ABCDEF";

	std::string out;
	std::string in = str;

    out.reserve(in.length() * 2);
    for (unsigned char c : in)
    {
        out.push_back(hex_digits[c >> 4]);
        out.push_back(hex_digits[c & 15]);
    }

    return out;
}


static const int doom_max_ammos[] = {200, 50, 300, 50};
static const int doom2_max_ammos[] = {200, 50, 300, 50};
static const int heretic_max_ammos[] = {100, 50, 200, 200, 20, 150};


static unsigned long long hash_seed(const char *str)
{
    unsigned long long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}


const int* get_default_max_ammos()
{
	switch (ap_base_game)
	{
		default: // (Indeterminate state?)
		case ap_game_t::doom: return doom_max_ammos;
		case ap_game_t::doom2: return doom2_max_ammos;
		case ap_game_t::heretic: return heretic_max_ammos;
	}
}

void recalc_max_ammo()
{
	for (int i = 0; i < ap_ammo_count; ++i)
	{
		const int recalc_max = ap_state.max_ammo_start[i]
		    + (ap_state.max_ammo_add[i] * ap_state.player_state.capacity_upgrades[i]);

		ap_state.player_state.max_ammo[i] = (recalc_max > 999) ? 999 : recalc_max;
	}
}

int validate_doom_location(ap_level_index_t idx, int index)
{
    ap_level_info_t* level_info = ap_get_level_info(idx);
    if (index >= level_info->thing_count) return 0;
	if (level_info->thing_infos[index].unreachable) return 0;
    return level_info->thing_infos[index].check_sanity == 0 || ap_state.check_sanity == 1;
}


int apdoom_init(ap_settings_t* settings)
{
	printf("%s\n", APDOOM_VERSION_FULL_TEXT);

	ap_notification_icons.reserve(4096); // 1MB. A bit exessive, but I got a crash with invalid strings and I cannot figure out why. Let's not take any chances...
	memset(&ap_state, 0, sizeof(ap_state));

	settings->game = archipelago_game_name.c_str();
	if (ap_base_game == ap_game_t::heretic)
	{
		ap_weapon_count = 9;
		ap_ammo_count = 6;
		ap_powerup_count = 9;
		ap_inventory_count = 14;
	}
	else // Doom or Doom 2, both use the same variables here
	{
		ap_weapon_count = 9;
		ap_ammo_count = 4;
		ap_powerup_count = 6;
		ap_inventory_count = 0;
	}

	const auto& level_info_table = get_level_info_table();
	ap_episode_count = (int)level_info_table.size();
	max_map_count = 0; // That's really the map count
	for (const auto& episode_level_info : level_info_table)
	{
		max_map_count = std::max(max_map_count, (int)episode_level_info.size());
	}

	printf("APDOOM: Initializing Game: \"%s\", Server: %s, Slot: %s\n", settings->game, settings->ip, settings->player_name);

	ap_state.level_states = new ap_level_state_t[ap_episode_count * max_map_count];
	ap_state.episodes = new int[ap_episode_count];
	ap_state.player_state.powers = new int[ap_powerup_count];
	ap_state.player_state.weapon_owned = new int[ap_weapon_count];
	ap_state.player_state.ammo = new int[ap_ammo_count];
	ap_state.player_state.max_ammo = new int[ap_ammo_count];
	ap_state.player_state.inventory = ap_inventory_count ? new ap_inventory_slot_t[ap_inventory_count] : nullptr;

	memset(ap_state.level_states, 0, sizeof(ap_level_state_t) * ap_episode_count * max_map_count);
	memset(ap_state.episodes, 0, sizeof(int) * ap_episode_count);
	memset(ap_state.player_state.powers, 0, sizeof(int) * ap_powerup_count);
	memset(ap_state.player_state.weapon_owned, 0, sizeof(int) * ap_weapon_count);
	memset(ap_state.player_state.ammo, 0, sizeof(int) * ap_ammo_count);
	memset(ap_state.player_state.max_ammo, 0, sizeof(int) * ap_ammo_count);
	if (ap_inventory_count)
		memset(ap_state.player_state.inventory, 0, sizeof(ap_inventory_slot_t) * ap_inventory_count);

	ap_state.player_state.health = 100;
	ap_state.player_state.ready_weapon = 1;
	ap_state.player_state.weapon_owned[0] = 1; // Fist
	ap_state.player_state.weapon_owned[1] = 1; // Pistol
	ap_state.player_state.ammo[0] = 50; // Clip

	// Ammo capacity management
	ap_state.max_ammo_start = new int[ap_ammo_count];
	ap_state.max_ammo_add = new int[ap_ammo_count];
	ap_state.player_state.capacity_upgrades = new int[ap_ammo_count];

	// default to regular max ammos for games without custom max ammo set
	auto max_ammos = get_default_max_ammos();
	for (int i = 0; i < ap_ammo_count; ++i)
	{
		ap_state.max_ammo_start[i] = max_ammos[i];
		ap_state.max_ammo_add[i] = max_ammos[i];
	}
	memset(ap_state.player_state.capacity_upgrades, 0, sizeof(int) * ap_ammo_count);

	for (int ep = 0; ep < ap_episode_count; ++ep)
	{
		int map_count = ap_get_map_count(ep + 1);
		for (int map = 0; map < map_count; ++map)
		{
			for (int k = 0; k < AP_CHECK_MAX; ++k)
			{
				ap_state.level_states[ep * max_map_count + map].checks[k] = -1;
			}
			auto level_info = ap_get_level_info(ap_level_index_t{ep, map});
			level_info->sanity_check_count = 0;
			for (int k = 0; k < level_info->thing_count; ++k)
			{
				if (level_info->thing_infos[k].check_sanity)
					level_info->sanity_check_count++;
			}
		}
	}

	ap_settings = *settings;

	if (ap_settings.override_skill)
		ap_state.difficulty = ap_settings.skill;
	if (ap_settings.override_monster_rando)
		ap_state.random_monsters = ap_settings.monster_rando;
	if (ap_settings.override_item_rando)
		ap_state.random_items = ap_settings.item_rando;
	if (ap_settings.override_music_rando)
		ap_state.random_music = ap_settings.music_rando;
	if (ap_settings.override_flip_levels)
		ap_state.flip_levels = ap_settings.flip_levels;
	if (ap_settings.override_reset_level_on_death)
		ap_state.reset_level_on_death = ap_settings.reset_level_on_death;

	AP_NetworkVersion version = {0, 4, 1};
	AP_SetClientVersion(&version);
    AP_Init(ap_settings.ip, ap_settings.game, ap_settings.player_name, ap_settings.passwd);
	AP_SetDeathLinkSupported(ap_settings.force_deathlink_off ? false : true);
	AP_SetItemClearCallback(f_itemclr);
	AP_SetItemRecvCallback(f_itemrecv);
	AP_SetLocationCheckedCallback(f_locrecv);
	AP_SetLocationInfoCallback(f_locinfo);
	AP_RegisterSlotDataIntCallback("goal", f_goal);
	AP_RegisterSlotDataIntCallback("difficulty", f_difficulty);
	AP_RegisterSlotDataIntCallback("random_monsters", f_random_monsters);
	AP_RegisterSlotDataIntCallback("random_pickups", f_random_items);
	AP_RegisterSlotDataIntCallback("random_music", f_random_music);
	AP_RegisterSlotDataIntCallback("flip_levels", f_flip_levels);
	AP_RegisterSlotDataIntCallback("check_sanity", f_check_sanity);
	AP_RegisterSlotDataIntCallback("reset_level_on_death", f_reset_level_on_death);
	AP_RegisterSlotDataIntCallback("episode1", f_episode1);
	AP_RegisterSlotDataIntCallback("episode2", f_episode2);
	AP_RegisterSlotDataIntCallback("episode3", f_episode3);
	AP_RegisterSlotDataIntCallback("episode4", f_episode4);
	AP_RegisterSlotDataIntCallback("episode5", f_episode5);
	AP_RegisterSlotDataIntCallback("ammo1start", f_ammo1start);
	AP_RegisterSlotDataIntCallback("ammo2start", f_ammo2start);
	AP_RegisterSlotDataIntCallback("ammo3start", f_ammo3start);
	AP_RegisterSlotDataIntCallback("ammo4start", f_ammo4start);
	AP_RegisterSlotDataIntCallback("ammo5start", f_ammo5start);
	AP_RegisterSlotDataIntCallback("ammo6start", f_ammo6start);
	AP_RegisterSlotDataIntCallback("ammo1add", f_ammo1add);
	AP_RegisterSlotDataIntCallback("ammo2add", f_ammo2add);
	AP_RegisterSlotDataIntCallback("ammo3add", f_ammo3add);
	AP_RegisterSlotDataIntCallback("ammo4add", f_ammo4add);
	AP_RegisterSlotDataIntCallback("ammo5add", f_ammo5add);
	AP_RegisterSlotDataIntCallback("ammo6add", f_ammo6add);
	AP_RegisterSlotDataIntCallback("two_ways_keydoors", f_two_ways_keydoors);
    AP_Start();

	// Block DOOM until connection succeeded or failed
	auto start_time = std::chrono::steady_clock::now();
	while (true)
	{
		bool should_break = false;
		switch (AP_GetConnectionStatus())
		{
			case AP_ConnectionStatus::Authenticated:
			{
				printf("APDOOM: Authenticated\n");
				AP_GetRoomInfo(&ap_room_info);

				printf("APDOOM: Room Info:\n");
				printf("  Network Version: %i.%i.%i\n", ap_room_info.version.major, ap_room_info.version.minor, ap_room_info.version.build);
				printf("  Tags:\n");
				for (const auto& tag : ap_room_info.tags)
					printf("    %s\n", tag.c_str());
				printf("  Password required: %s\n", ap_room_info.password_required ? "true" : "false");
				printf("  Permissions:\n");
				for (const auto& permission : ap_room_info.permissions)
					printf("    %s = %i:\n", permission.first.c_str(), permission.second);
				printf("  Hint cost: %i\n", ap_room_info.hint_cost);
				printf("  Location check points: %i\n", ap_room_info.location_check_points);
				printf("  Data package checksums:\n");
				for (const auto& kv : ap_room_info.datapackage_checksums)
					printf("    %s = %s:\n", kv.first.c_str(), kv.second.c_str());
				printf("  Seed name: %s\n", ap_room_info.seed_name.c_str());
				printf("  Time: %f\n", ap_room_info.time);

				ap_was_connected = true;

				std::string this_game_save_folder = "AP_" + ap_room_info.seed_name + "_" + string_to_hex(ap_settings.player_name);
				if (ap_settings.save_dir != NULL)
					ap_save_dir_name = std::string(ap_settings.save_dir) + "/" + this_game_save_folder;
				else
					ap_save_dir_name = this_game_save_folder;

				// Create a directory where saves will go for this AP seed.
				printf("APDOOM: Save directory: %s\n", ap_save_dir_name.c_str());
				if (!AP_FileExists(ap_save_dir_name.c_str()))
				{
					printf("  Doesn't exist, creating...\n");
					AP_MakeDirectory(ap_save_dir_name.c_str());
				}

				// Make sure that ammo starts at correct base values no matter what
				recalc_max_ammo();

				load_state();
				should_break = true;
				break;
			}
			case AP_ConnectionStatus::ConnectionRefused:
				printf("APDOOM: Failed to connect, connection refused\n");
				return 0;
			default: // Explicitly do not handle
				break;
		}
		if (should_break) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(10))
		{
			printf("APDOOM: Failed to connect, timeout 10s\n");
			return 0;
		}
	}

	// If none episode is selected, select the first one.
	int ep_count = 0;
	for (int i = 0; i < ap_episode_count; ++i)
		if (ap_state.episodes[i])
			ep_count++;
	if (!ep_count)
	{
		printf("APDOOM: No episode selected, selecting episode 1\n");
		ap_state.episodes[0] = 1;
	}

	// Seed for random features
	auto ap_seed = apdoom_get_seed();
	unsigned long long seed = hash_seed(ap_seed);
	srand(seed);

	// Randomly flip levels based on the seed
	if (ap_state.flip_levels == 1)
	{
		printf("APDOOM: All levels flipped\n");
		for (int ep = 0; ep < ap_episode_count; ++ep)
		{
			int map_count = ap_get_map_count(ep + 1);
			for (int map = 0; map < map_count; ++map)
				ap_state.level_states[ep * max_map_count + map].flipped = 1;
		}
	}
	else if (ap_state.flip_levels == 2)
	{
		printf("APDOOM: Levels randomly flipped\n");
		for (int ep = 0; ep < ap_episode_count; ++ep)
		{
			int map_count = ap_get_map_count(ep + 1);
			for (int map = 0; map < map_count; ++map)
				ap_state.level_states[ep * max_map_count + map].flipped = rand() % 2;
		}
	}

	// Map original music to every level to start
	for (int ep = 0; ep < ap_episode_count; ++ep)
	{
		int map_count = ap_get_map_count(ep + 1);
		for (int map = 0; map < map_count; ++map)
			ap_state.level_states[ep * max_map_count + map].music = get_original_music_for_level(ep + 1, map + 1);
	}

	// Randomly shuffle music 
	if (ap_state.random_music > 0)
	{
		// Collect music for all selected levels
		std::vector<int> music_pool;
		for (int ep = 0; ep < ap_episode_count; ++ep)
		{
			if (ap_state.episodes[ep] || ap_state.random_music == 2)
			{
				int map_count = ap_get_map_count(ep + 1);
				for (int map = 0; map < map_count; ++map)
					music_pool.push_back(ap_state.level_states[ep * max_map_count + map].music);
			}
		}

		// Shuffle
		printf("APDOOM: Random Music:\n");
		for (int ep = 0; ep < ap_episode_count; ++ep)
		{
			if (ap_state.episodes[ep])
			{
				int map_count = ap_get_map_count(ep + 1);
				for (int map = 0; map < map_count; ++map)
				{
					int rnd = rand() % (int)music_pool.size();
					int mus = music_pool[rnd];
					music_pool.erase(music_pool.begin() + rnd);
					ap_state.level_states[ep * max_map_count + map].music = mus;

					switch (ap_base_game)
					{
						case ap_game_t::doom:
							printf("  E%iM%i = E%iM%i\n", ep + 1, map + 1, ((mus - 1) / max_map_count) + 1, ((mus - 1) % max_map_count) + 1);
							break;
						case ap_game_t::doom2:
							printf("  MAP%02i = MAP%02i\n", map + 1, mus);
							break;
						case ap_game_t::heretic:
							printf("  E%iM%i = E%iM%i\n", ep + 1, map + 1, (mus / max_map_count) + 1, (mus % max_map_count) + 1);
							break;
					}
				}
			}
		}
	}

	// Scout locations to see which are progressive
	if (ap_progressive_locations.empty())
	{
		std::vector<int64_t> location_scouts;

		const auto& loc_table = get_location_table();
		for (const auto& kv1 : loc_table)
		{
			if (!ap_state.episodes[kv1.first - 1])
				continue;
			for (const auto& kv2 : kv1.second)
			{
				for (const auto& kv3 : kv2.second)
				{
					if (kv3.first == -1) continue;
#if 0 // Was this used to debug something in the past? Either way, it does nothing anymore
					if (kv3.second == 371349)
					{
						int tmp;
						tmp = 5;
					}
#endif
					if (validate_doom_location({kv1.first - 1, kv2.first - 1}, kv3.first))
					{
						location_scouts.push_back(kv3.second);
					}
				}
			}
		}
		
		printf("APDOOM: Scouting for %i locations...\n", (int)location_scouts.size());
		AP_SendLocationScouts(location_scouts, 0);

		// Wait for location infos
		start_time = std::chrono::steady_clock::now();
		while (ap_progressive_locations.empty())
		{
			apdoom_update();
		
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(10))
			{
				printf("APDOOM: Timeout waiting for LocationScouts. 10s\n  Do you have a VPN active?\n  Checks will all look non-progression.");
				break;
			}
		}
	}
	else
	{
		printf("APDOOM: Scout locations cached loaded\n");
	}
	
	printf("APDOOM: Initialized\n");
	ap_initialized = true;
	return 1;
}


static bool is_loc_checked(ap_level_index_t idx, int index)
{
	auto level_state = ap_get_level_state(idx);
	for (int i = 0; i < level_state->check_count; ++i)
	{
		if (level_state->checks[i] == index) return true;
	}
	return false;
}


void apdoom_shutdown()
{
	if (ap_was_connected)
		save_state();
}


void apdoom_save_state()
{
	if (ap_was_connected)
		save_state();
}


static void json_get_int(const Json::Value& json, int& out_or_default)
{
	if (json.isInt())
		out_or_default = json.asInt();
}


static void json_get_bool_or(const Json::Value& json, int& out_or_default)
{
	if (json.isInt())
		out_or_default |= json.asInt();
}


const char* get_weapon_name(int weapon)
{
	switch (weapon)
	{
		case 0: return "Fist";
		case 1: return "Pistol";
		case 2: return "Shotgun";
		case 3: return "Chaingun";
		case 4: return "Rocket launcher";
		case 5: return "Plasma gun";
		case 6: return "BFG9000";
		case 7: return "Chainsaw";
		case 8: return "Super shotgun";
		default: return "UNKNOWN";
	}
}


const char* get_power_name(int weapon)
{
	switch (weapon)
	{
		case 0: return "Invulnerability";
		case 1: return "Strength";
		case 2: return "Invisibility";
		case 3: return "Hazard suit";
		case 4: return "Computer area map";
		case 5: return "Infrared";
		default: return "UNKNOWN";
	}
}


const char* get_ammo_name(int weapon)
{
	switch (weapon)
	{
		case 0: return "Bullets";
		case 1: return "Shells";
		case 2: return "Cells";
		case 3: return "Rockets";
		default: return "UNKNOWN";
	}
}


void load_state()
{
	printf("APDOOM: Load state\n");

	std::string filename = ap_save_dir_name + "/apstate.json";
	std::ifstream f(filename);
	if (!f.is_open())
	{
		printf("  None found.\n");
		return; // Could be no state yet, that's fine
	}
	Json::Value json;
	f >> json;
	f.close();

	// Player state
	json_get_int(json["player"]["health"], ap_state.player_state.health);
	json_get_int(json["player"]["armor_points"], ap_state.player_state.armor_points);
	json_get_int(json["player"]["armor_type"], ap_state.player_state.armor_type);
	json_get_int(json["player"]["ready_weapon"], ap_state.player_state.ready_weapon);
	json_get_int(json["player"]["kill_count"], ap_state.player_state.kill_count);
	json_get_int(json["player"]["item_count"], ap_state.player_state.item_count);
	json_get_int(json["player"]["secret_count"], ap_state.player_state.secret_count);
	for (int i = 0; i < ap_powerup_count; ++i)
		json_get_int(json["player"]["powers"][i], ap_state.player_state.powers[i]);
	for (int i = 0; i < ap_weapon_count; ++i)
		json_get_bool_or(json["player"]["weapon_owned"][i], ap_state.player_state.weapon_owned[i]);
	for (int i = 0; i < ap_ammo_count; ++i)
	{
		json_get_int(json["player"]["ammo"][i], ap_state.player_state.ammo[i]);

		// This will get overwritten later,
		// but it must be saved if the player is in game before all their items have been re-received.
		json_get_int(json["player"]["max_ammo"][i], ap_state.player_state.max_ammo[i]);		
	}
	for (int i = 0; i < ap_inventory_count; ++i)
	{
		const auto& inventory_slot = json["player"]["inventory"][i];
		json_get_int(inventory_slot["type"], ap_state.player_state.inventory[i].type);
		json_get_int(inventory_slot["count"], ap_state.player_state.inventory[i].count);
	}

	printf("  Player State:\n");
	printf("    Health %i:\n", ap_state.player_state.health);
	printf("    Armor points %i:\n", ap_state.player_state.armor_points);
	printf("    Armor type %i:\n", ap_state.player_state.armor_type);
	printf("    Ready weapon: %s\n", get_weapon_name(ap_state.player_state.ready_weapon));
	printf("    Kill count %i:\n", ap_state.player_state.kill_count);
	printf("    Item count %i:\n", ap_state.player_state.item_count);
	printf("    Secret count %i:\n", ap_state.player_state.secret_count);
	printf("    Active powerups:\n");
	for (int i = 0; i < ap_powerup_count; ++i)
		if (ap_state.player_state.powers[i])
			printf("    %s\n", get_power_name(i));
	printf("    Owned weapons:\n");
	for (int i = 0; i < ap_weapon_count; ++i)
		if (ap_state.player_state.weapon_owned[i])
			printf("      %s\n", get_weapon_name(i));
	printf("    Ammo:\n");
	for (int i = 0; i < ap_ammo_count; ++i)
		printf("      %s = %i / %i\n", get_ammo_name(i),
			ap_state.player_state.ammo[i],
			ap_state.player_state.max_ammo[i]);

	// Level states
	for (int i = 0; i < ap_episode_count; ++i)
	{
		int map_count = ap_get_map_count(i + 1);
		for (int j = 0; j < map_count; ++j)
		{
			auto level_state = ap_get_level_state(ap_level_index_t{i, j});
			json_get_bool_or(json["episodes"][i][j]["completed"], level_state->completed);
			json_get_bool_or(json["episodes"][i][j]["keys0"], level_state->keys[0]);
			json_get_bool_or(json["episodes"][i][j]["keys1"], level_state->keys[1]);
			json_get_bool_or(json["episodes"][i][j]["keys2"], level_state->keys[2]);
			//json_get_bool_or(json["episodes"][i][j]["check_count"], level_state->check_count);
			json_get_bool_or(json["episodes"][i][j]["has_map"], level_state->has_map);
			json_get_bool_or(json["episodes"][i][j]["unlocked"], level_state->unlocked);
			json_get_bool_or(json["episodes"][i][j]["special"], level_state->special);

			//int k = 0;
			//for (const auto& json_check : json["episodes"][i][j]["checks"])
			//{
			//	json_get_bool_or(json_check, level_state->checks[k++]);
			//}
		}
	}

	// Item queue
	for (const auto& item_id_json : json["item_queue"])
	{
		ap_item_queue.push_back(item_id_json.asInt64());
	}

	json_get_int(json["ep"], ap_state.ep);
	printf("  Enabled episodes: ");
	int first = 1;
	for (int i = 0; i < ap_episode_count; ++i)
	{
		json_get_int(json["enabled_episodes"][i], ap_state.episodes[i]);
		if (ap_state.episodes[i])
		{
			if (!first) printf(", ");
			first = 0;
			printf("%i", i + 1);
		}
	}
	printf("\n");
	json_get_int(json["map"], ap_state.map);
	printf("  Episode: %i\n", ap_state.ep);
	printf("  Map: %i\n", ap_state.map);

	for (const auto& prog_json : json["progressive_locations"])
	{
		ap_progressive_locations.insert(prog_json.asInt64());
	}
	
	json_get_bool_or(json["victory"], ap_state.victory);
	printf("  Victory state: %s\n", ap_state.victory ? "true" : "false");
}


static Json::Value serialize_level(int ep, int map)
{
	auto level_state = ap_get_level_state(ap_level_index_t{ep - 1, map - 1});

	Json::Value json_level;

	json_level["completed"] = level_state->completed;
	json_level["keys0"] = level_state->keys[0];
	json_level["keys1"] = level_state->keys[1];
	json_level["keys2"] = level_state->keys[2];
	json_level["check_count"] = level_state->check_count;
	json_level["has_map"] = level_state->has_map;
	json_level["unlocked"] = level_state->unlocked;
	json_level["special"] = level_state->special;

	Json::Value json_checks(Json::arrayValue);
	for (int k = 0; k < AP_CHECK_MAX; ++k)
	{
		if (level_state->checks[k] == -1)
			continue;
		json_checks.append(level_state->checks[k]);
	}
	json_level["checks"] = json_checks;

	return json_level;
}


std::vector<ap_level_index_t> get_level_indices()
{
	std::vector<ap_level_index_t> ret;

	for (int i = 0; i < ap_episode_count; ++i)
	{
		int map_count = ap_get_map_count(i + 1);
		for (int j = 0; j < map_count; ++j)
		{
			ret.push_back({i + 1, j + 1});
		}
	}

	return ret;
}


void save_state()
{
	std::string filename = ap_save_dir_name + "/apstate.json";
	std::ofstream f(filename);
	if (!f.is_open())
	{
		printf("Failed to save AP state.\n");
#if WIN32
		MessageBoxA(nullptr, "Failed to save player state. That's bad.", "Error", MB_OK);
#endif
		return; // Ok that's bad. we won't save player state
	}

	// Player state
	Json::Value json;
	Json::Value json_player;
	json_player["health"] = ap_state.player_state.health;
	json_player["armor_points"] = ap_state.player_state.armor_points;
	json_player["armor_type"] = ap_state.player_state.armor_type;
	json_player["ready_weapon"] = ap_state.player_state.ready_weapon;
	json_player["kill_count"] = ap_state.player_state.kill_count;
	json_player["item_count"] = ap_state.player_state.item_count;
	json_player["secret_count"] = ap_state.player_state.secret_count;

	Json::Value json_powers(Json::arrayValue);
	for (int i = 0; i < ap_powerup_count; ++i)
		json_powers.append(ap_state.player_state.powers[i]);
	json_player["powers"] = json_powers;

	Json::Value json_weapon_owned(Json::arrayValue);
	for (int i = 0; i < ap_weapon_count; ++i)
		json_weapon_owned.append(ap_state.player_state.weapon_owned[i]);
	json_player["weapon_owned"] = json_weapon_owned;

	Json::Value json_ammo(Json::arrayValue);
	for (int i = 0; i < ap_ammo_count; ++i)
		json_ammo.append(ap_state.player_state.ammo[i]);
	json_player["ammo"] = json_ammo;

	Json::Value json_max_ammo(Json::arrayValue);
	for (int i = 0; i < ap_ammo_count; ++i)
		json_max_ammo.append(ap_state.player_state.max_ammo[i]);
	json_player["max_ammo"] = json_max_ammo;

	Json::Value json_inventory(Json::arrayValue);
	for (int i = 0; i < ap_inventory_count; ++i)
	{
		if (ap_state.player_state.inventory[i].type == 9) // Don't include wings to player inventory, they are per level
			continue;
		Json::Value json_inventory_slot;
		json_inventory_slot["type"] = ap_state.player_state.inventory[i].type;
		json_inventory_slot["count"] = ap_state.player_state.inventory[i].count;
		json_inventory.append(json_inventory_slot);
	}
	json_player["inventory"] = json_inventory;

	json["player"] = json_player;

	// Level states
	Json::Value json_episodes(Json::arrayValue);
	for (int i = 0; i < ap_episode_count; ++i)
	{
		Json::Value json_levels(Json::arrayValue);
		int map_count = ap_get_map_count(i + 1);
		for (int j = 0; j < map_count; ++j)
		{
			json_levels.append(serialize_level(i + 1, j + 1));
		}
		json_episodes.append(json_levels);
	}
	json["episodes"] = json_episodes;

	// Item queue
	Json::Value json_item_queue(Json::arrayValue);
	for (auto item_id : ap_item_queue)
	{
		json_item_queue.append(item_id);
	}
	json["item_queue"] = json_item_queue;

	json["ep"] = ap_state.ep;
	for (int i = 0; i < ap_episode_count; ++i)
		json["enabled_episodes"][i] = ap_state.episodes[i] ? true : false;
	json["map"] = ap_state.map;

	// Progression items (So we don't scout everytime we connect)
	for (auto loc_id : ap_progressive_locations)
	{
		json["progressive_locations"].append(loc_id);
	}

	json["victory"] = ap_state.victory;

	json["version"] = APDOOM_VERSION_FULL_TEXT;

	f << json;
}


void f_itemclr()
{
	// Not sure what use this would have here.
}


static const std::map<int, int> doom_keys_map = {{5, 0}, {40, 0}, {6, 1}, {39, 1}, {13, 2}, {38, 2}};
static const std::map<int, int> doom2_keys_map = {{5, 0}, {40, 0}, {6, 1}, {39, 1}, {13, 2}, {38, 2}};
static const std::map<int, int> heretic_keys_map = {{80, 0}, {73, 1}, {79, 2}};


const std::map<int, int>& get_keys_map()
{
	switch (ap_base_game)
	{
		default: // (Indeterminate state?)
		case ap_game_t::doom: return doom_keys_map;
		case ap_game_t::doom2: return doom2_keys_map;
		case ap_game_t::heretic: return heretic_keys_map;
	}
}


int get_map_doom_type()
{
	switch (ap_base_game)
	{
		default: // (Indeterminate state?)
		case ap_game_t::doom: return 2026;
		case ap_game_t::doom2: return 2026;
		case ap_game_t::heretic: return 35;
	}
}


static const std::map<int, int> doom_weapons_map = {{2001, 2}, {2002, 3}, {2003, 4}, {2004, 5}, {2006, 6}, {2005, 7}};
static const std::map<int, int> doom2_weapons_map = {{2001, 2}, {2002, 3}, {2003, 4}, {2004, 5}, {2006, 6}, {2005, 7}, {82, 8}};
static const std::map<int, int> heretic_weapons_map = {{2005, 7}, {2001, 2}, {53, 3}, {2003, 5}, {2002, 6}, {2004, 4}};


const std::map<int, int>& get_weapons_map()
{
	switch (ap_base_game)
	{
		default: // (Indeterminate state?)
		case ap_game_t::doom: return doom_weapons_map;
		case ap_game_t::doom2: return doom2_weapons_map;
		case ap_game_t::heretic: return heretic_weapons_map;
	}
}


const std::map<int, std::string>& get_sprites()
{
#if 1
	return preloaded_type_sprites;
#else
	switch (ap_base_game)
	{
		case ap_game_t::doom: return ap_doom_type_sprites;
		case ap_game_t::doom2: return ap_doom2_type_sprites;
		case ap_game_t::heretic: return ap_heretic_type_sprites;
	}
#endif
}


std::string get_exmx_name(const std::string& name)
{
	auto pos = name.find_first_of('(');
	if (pos == std::string::npos) return name;
	return name.substr(pos);
}


// Split from f_itemrecv so that the item queue can call it without side-effects
// This handles everything that requires us be in game, notification icons included
static void process_received_item(int64_t item_id)
{
	const auto& item_type_table = get_item_type_table();
	auto it = item_type_table.find(item_id);
	if (it == item_type_table.end())
		return; // Skip -- This is probably redundant, but whatever

	ap_item_t item = it->second;
	std::string notif_text;

	// If the item has an associated episode/map, note that
	if (item.ep != -1)
	{
		ap_level_index_t idx = {item.ep - 1, item.map - 1};
		ap_level_info_t* level_info = ap_get_level_info(idx);

		notif_text = get_exmx_name(level_info->name);
	}

	// Give item to in-game player
	ap_settings.give_item_callback(item.doom_type, item.ep, item.map);

	// Add notification icon
	const auto& sprite_map = get_sprites();
	auto sprite_it = sprite_map.find(item.doom_type);
	if (sprite_it != sprite_map.end())
	{
		ap_notification_icon_t notif;
		snprintf(notif.sprite, 9, "%s", sprite_it->second.c_str());
		notif.t = 0;
		notif.text[0] = '\0'; // For now
		if (notif_text != "")
		{
			snprintf(notif.text, 260, "%s", notif_text.c_str());
		}
		notif.xf = AP_NOTIF_SIZE / 2 + AP_NOTIF_PADDING;
		notif.yf = -200.0f + AP_NOTIF_SIZE / 2;
		notif.state = AP_NOTIF_STATE_PENDING;
		notif.velx = 0.0f;
		notif.vely = 0.0f;
		notif.x = (int)notif.xf;
		notif.y = (int)notif.yf;
		ap_notification_icons.push_back(notif);
	}
}

void f_itemrecv(int64_t item_id, int player_id, bool notify_player)
{
	const auto& item_type_table = get_item_type_table();
	auto it = item_type_table.find(item_id);
	if (it == item_type_table.end())
		return; // Skip
	ap_item_t item = it->second;

	ap_level_index_t idx = {item.ep - 1, item.map - 1};
	auto level_state = ap_get_level_state(idx);

	// Backpack?
	if (item.doom_type == 8)
	{
		for (int i = 0; i < ap_ammo_count; ++i)
			++ap_state.player_state.capacity_upgrades[i];
		recalc_max_ammo();
	}

	// Single ammo capacity upgrade?
	if (item.doom_type >= 65001 && item.doom_type <= 65006)
	{
		int ammo_num = item.doom_type - 65001;
		if (ammo_num < ap_ammo_count)
			++ap_state.player_state.capacity_upgrades[ammo_num];
		recalc_max_ammo();
	}

	// Key?
	const auto& keys_map = get_keys_map();
	auto key_it = keys_map.find(item.doom_type);
	if (key_it != keys_map.end())
		level_state->keys[key_it->second] = 1;

	// Weapon?
	const auto& weapons_map = get_weapons_map();
	auto weapon_it = weapons_map.find(item.doom_type);
	if (weapon_it != weapons_map.end())
		ap_state.player_state.weapon_owned[weapon_it->second] = 1;

	// Map?
	if (item.doom_type == get_map_doom_type())
		level_state->has_map = 1;

	// Level unlock?
	if (item.doom_type == -1)
		level_state->unlocked = 1;

	// Level complete?
	if (item.doom_type == -2)
		level_state->completed = 1;

	// Ignore inventory items, the game will add them up

	if (!notify_player) return;

	if (!ap_is_in_game)
		ap_item_queue.push_back(item_id);
	else
		process_received_item(item_id);
}


bool find_location(int64_t loc_id, int &ep, int &map, int &index)
{
	ep = -1;
	map = -1;
	index = -1;

	const auto& loc_table = get_location_table();
	for (const auto& loc_map_table : loc_table)
	{
		for (const auto& loc_index_table : loc_map_table.second)
		{
			for (const auto& loc_index : loc_index_table.second)
			{
				if (loc_index.second == loc_id)
				{
					ep = loc_map_table.first;
					map = loc_index_table.first;
					index = loc_index.first;
					break;
				}
			}
			if (ep != -1) break;
		}
		if (ep != -1) break;
	}
	return (ep > 0);
}


void f_locrecv(int64_t loc_id)
{
	// Find where this location is
	int ep = -1;
	int map = -1;
	int index = -1;
	if (!find_location(loc_id, ep, map, index))
	{
		printf("APDOOM: In f_locrecv, loc id not found: %i\n", (int)loc_id);
		return; // Loc not found
	}

	ap_level_index_t idx = {ep - 1, map - 1};

	// Make sure we didn't already check it
	if (is_loc_checked(idx, index)) return;
	if (index < 0) return;

	auto level_state = ap_get_level_state(idx);
	level_state->checks[level_state->check_count] = index;
	level_state->check_count++;
}


void f_locinfo(std::vector<AP_NetworkItem> loc_infos)
{
	for (const auto& loc_info : loc_infos)
	{
		if (loc_info.flags & 1)
			ap_progressive_locations.insert(loc_info.location);
	}
}


void f_goal(int goal)
{
	ap_state.goal = goal;
}


void f_difficulty(int difficulty)
{
	if (ap_settings.override_skill)
		return;

	ap_state.difficulty = difficulty;
}


void f_random_monsters(int random_monsters)
{
	if (ap_settings.override_monster_rando)
		return;

	ap_state.random_monsters = random_monsters;
}


void f_random_items(int random_items)
{
	if (ap_settings.override_item_rando)
		return;

	ap_state.random_items = random_items;
}


void f_random_music(int random_music)
{
	if (ap_settings.override_music_rando)
		return;

	ap_state.random_music = random_music;
}


void f_flip_levels(int flip_levels)
{
	if (ap_settings.override_flip_levels)
		return;

	ap_state.flip_levels = flip_levels;
}


void f_check_sanity(int check_sanity)
{
	ap_state.check_sanity = check_sanity;
}


void f_reset_level_on_death(int reset_level_on_death)
{
	if (ap_settings.override_reset_level_on_death)
		return;

	ap_state.reset_level_on_death = reset_level_on_death;
}


void f_episode1(int ep)
{
	ap_state.episodes[0] = ep;
}


void f_episode2(int ep)
{
	ap_state.episodes[1] = ep;
}


void f_episode3(int ep)
{
	ap_state.episodes[2] = ep;
}


void f_episode4(int ep)
{
	ap_state.episodes[3] = ep;
}


void f_episode5(int ep)
{
	ap_state.episodes[4] = ep;
}


void f_two_ways_keydoors(int two_ways_keydoors)
{
	ap_state.two_ways_keydoors = two_ways_keydoors;
}


void f_ammo1start(int ammo_amt)
{
	if (ammo_amt > 0)
		ap_state.max_ammo_start[0] = ammo_amt;
}


void f_ammo2start(int ammo_amt)
{
	if (ammo_amt > 0)
		ap_state.max_ammo_start[1] = ammo_amt;
}


void f_ammo3start(int ammo_amt)
{
	if (ammo_amt > 0)
		ap_state.max_ammo_start[2] = ammo_amt;
}


void f_ammo4start(int ammo_amt)
{
	if (ammo_amt > 0)
		ap_state.max_ammo_start[3] = ammo_amt;
}


void f_ammo5start(int ammo_amt)
{
	if (ammo_amt > 0)
		ap_state.max_ammo_start[4] = ammo_amt;
}


void f_ammo6start(int ammo_amt)
{
	if (ammo_amt > 0)
		ap_state.max_ammo_start[5] = ammo_amt;
}


void f_ammo1add(int ammo_amt)
{
	if (ammo_amt > 0)
		ap_state.max_ammo_add[0] = ammo_amt;
}


void f_ammo2add(int ammo_amt)
{
	if (ammo_amt > 0)
		ap_state.max_ammo_add[1] = ammo_amt;
}


void f_ammo3add(int ammo_amt)
{
	if (ammo_amt > 0)
		ap_state.max_ammo_add[2] = ammo_amt;
}


void f_ammo4add(int ammo_amt)
{
	if (ammo_amt > 0)
		ap_state.max_ammo_add[3] = ammo_amt;
}


void f_ammo5add(int ammo_amt)
{
	if (ammo_amt > 0)
		ap_state.max_ammo_add[4] = ammo_amt;
}


void f_ammo6add(int ammo_amt)
{
	if (ammo_amt > 0)
		ap_state.max_ammo_add[5] = ammo_amt;
}


const char* apdoom_get_seed()
{
	return ap_save_dir_name.c_str();
}


void apdoom_check_location(ap_level_index_t idx, int index)
{
	int64_t id = 0;
	const auto& loc_table = get_location_table();

	auto it1 = loc_table.find(idx.ep + 1);
	if (it1 == loc_table.end()) return;

	auto it2 = it1->second.find(idx.map + 1);
	if (it2 == it1->second.end()) return;

	auto it3 = it2->second.find(index);
	if (it3 == it2->second.end()) return;

	id = it3->second;

	if (index >= 0)
	{
		if (is_loc_checked(idx, index))
		{
			printf("APDOOM: Location already checked\n");
		}
		else
		{ // We get back from AP
			//auto level_state = ap_get_level_state(ep, map);
			//level_state->checks[level_state->check_count] = index;
			//level_state->check_count++;
		}
	}
	AP_SendItem(id);
}


int apdoom_is_location_progression(ap_level_index_t idx, int index)
{
	const auto& loc_table = get_location_table();

	auto it1 = loc_table.find(idx.ep + 1);
	if (it1 == loc_table.end()) return 0;

	auto it2 = it1->second.find(idx.map + 1);
	if (it2 == it1->second.end()) return 0;

	auto it3 = it2->second.find(index);
	if (it3 == it2->second.end()) return 0;

	int64_t id = it3->second;

	return (ap_progressive_locations.find(id) != ap_progressive_locations.end()) ? 1 : 0;
}

void apdoom_complete_level(ap_level_index_t idx)
{
	//if (ap_state.level_states[ep - 1][map - 1].completed) return; // Already completed
    ap_get_level_state(idx)->completed = 1;
	apdoom_check_location(idx, -1); // -1 is complete location
}


ap_level_index_t ap_make_level_index(int gameepisode, int gamemap)
{
	// For PWAD support: Level info struct has gameepisode/gamemap, don't make assumptions
	const auto& table = get_level_info_table();
	for (int ep = 0; ep < (int)table.size(); ++ep)
	{
		for (int map = 0; map < (int)table[ep].size(); ++map)
		{
			if (table[ep][map].game_episode == gameepisode && table[ep][map].game_map == gamemap)
				return {ep, map};
		}
	}
	// Requested level isn't in the level table; this will probably crash but whatever
	printf("APDOOM: Episode %d, Map %d isn't in the Archipelago level table!\n", gameepisode, gamemap);
	return {0, 0};
}


int ap_index_to_ep(ap_level_index_t idx)
{
	const auto& table = get_level_info_table();
	return table[idx.ep][idx.map].game_episode;
}


int ap_index_to_map(ap_level_index_t idx)
{
	const auto& table = get_level_info_table();
	return table[idx.ep][idx.map].game_map;
}


void apdoom_check_victory()
{
	if (ap_state.victory) return;

	if (ap_state.goal == 1 && (ap_base_game == ap_game_t::doom || ap_base_game == ap_game_t::heretic))
	{
		for (int ep = 0; ep < ap_episode_count; ++ep)
		{
			if (!ap_state.episodes[ep]) continue;
			if (!ap_get_level_state(ap_level_index_t{ep, 7})->completed) return;
		}
	}
	else
	{
		// All levels
		for (int ep = 0; ep < ap_episode_count; ++ep)
		{
			if (!ap_state.episodes[ep]) continue;
		
			int map_count = ap_get_map_count(ep + 1);
			for (int map = 0; map < map_count; ++map)
			{
				if (!ap_get_level_state(ap_level_index_t{ep, map})->completed) return;
			}
		}
	}

	ap_state.victory = 1;

	AP_StoryComplete();
	ap_settings.victory_callback();
}


void apdoom_send_message(const char* msg)
{
	std::string smsg = msg;
#if 0 // TODO This needs a major rework.
	if (strnicmp(msg, "!hint ", 6) == 0)
	{
		// Make the hint easier.
		// Find an E#M# in the text
		for (size_t i = 6; i < smsg.size() - 4; ++i)
		{
			ap_level_info_t* level_info = nullptr;

			if (toupper(smsg[i]) == 'E' &&
				toupper(smsg[i + 2]) == 'M' &&
				(smsg[i + 1] - '0') >= 1 && (smsg[i + 1] - '0') <= 9 &&
				(smsg[i + 3] - '0') >= 1 && (smsg[i + 3] - '0') <= 9)
			{
				// Find the level name
				int ep = smsg[i + 1] - '1';
				int map = smsg[i + 3] - '1';
				level_info = ap_get_level_info(ap_level_index_t{ep, map});
			}

			if (toupper(smsg[i]) == 'M' &&
				toupper(smsg[i + 1]) == 'A' &&
				toupper(smsg[i + 2]) == 'P' &&
				(smsg[i + 3] - '0') >= 0 && (smsg[i + 3] - '0') <= 3 &&
				(smsg[i + 4] - '0') >= 0 && (smsg[i + 4] - '0') <= 9)
			{
				// Find the level name
				int ep = 1;
				int map = smsg[i + 4] + (smsg[i + 3] - '0') * 10;
				level_info = ap_get_level_info(ap_make_level_index(ep, map));
			}

			if (level_info)
			{
				std::string level_name = level_info->name;

				// Check what it's looking for. Computer area map? map scroll?
				if (smsg.find("map") != std::string::npos || smsg.find("MAP") != std::string::npos)
				{
					switch (ap_base_game)
					{
						case ap_game_t::doom:
						case ap_game_t::doom2:
							smsg = "!hint " + level_name + " - Computer area map";
							break;
						case ap_game_t::heretic:
							smsg = "!hint " + level_name + " - Map scroll";
							break;
					}
				}
				else if (smsg.find("blue") != std::string::npos || smsg.find("BLUE") != std::string::npos)
				{
					switch (ap_base_game)
					{
						case ap_game_t::doom:
						case ap_game_t::doom2:
							smsg = "!hint " + level_name + " - Blue " + (level_info->use_skull[0] ? "skull key" : "keycard");
							break;
						case ap_game_t::heretic:
							smsg = "!hint " + level_name + " - Blue key";
							break;
					}
				}
				else if (smsg.find("yellow") != std::string::npos || smsg.find("YELLOW") != std::string::npos)
				{
					switch (ap_base_game)
					{
						case ap_game_t::doom:
						case ap_game_t::doom2:
							smsg = "!hint " + level_name + " - Yellow " + (level_info->use_skull[1] ? "skull key" : "keycard");
							break;
						case ap_game_t::heretic:
							smsg = "!hint " + level_name + " - Yellow key";
							break;
					}
				}
				else if (smsg.find("red") != std::string::npos || smsg.find("RED") != std::string::npos)
				{
					switch (ap_base_game)
					{
						case ap_game_t::doom:
						case ap_game_t::doom2:
							smsg = "!hint " + level_name + " - Red " + (level_info->use_skull[0] ? "skull key" : "keycard");
							break;
						case ap_game_t::heretic:
							break;
					}
				}
				else if (smsg.find("green") != std::string::npos || smsg.find("GREEN") != std::string::npos)
				{
					switch (ap_base_game)
					{
						case ap_game_t::doom:
						case ap_game_t::doom2:
							break;
						case ap_game_t::heretic:
							smsg = "!hint " + level_name + " - Green key";
							break;
					}
				}

				break;
			}
		}
	}
#endif

	Json::Value say_packet;
	say_packet[0]["cmd"] = "Say";
	say_packet[0]["text"] = smsg;
	Json::FastWriter writer;
	APSend(writer.write(say_packet));
}


void apdoom_on_death()
{
	AP_DeathLinkSend();
}


void apdoom_clear_death()
{
	AP_DeathLinkClear();
}


int apdoom_should_die()
{
	return AP_DeathLinkPending() ? 1 : 0;
}


const ap_notification_icon_t* ap_get_notification_icons(int* count)
{
	*count = (int)ap_notification_icons.size();
	return ap_notification_icons.data();
}


int ap_get_highest_episode()
{
	int highest = 0;
	for (int i = 0; i < ap_episode_count; ++i)
		if (ap_state.episodes[i])
			highest = i;
	return highest;
}


int ap_validate_doom_location(ap_level_index_t idx, int doom_type, int index)
{
	ap_level_info_t* level_info = ap_get_level_info(idx);
    if (index >= level_info->thing_count) return -1;
	if (level_info->thing_infos[index].doom_type != doom_type) return -1;
	if (level_info->thing_infos[index].unreachable) return 0;
    return level_info->thing_infos[index].check_sanity == 0 || ap_state.check_sanity == 1;
}


/*
    black: "000000"
    red: "EE0000"
    green: "00FF7F"  # typically a location
    yellow: "FAFAD2"  # typically other slots/players
    blue: "6495ED"  # typically extra info (such as entrance)
    magenta: "EE00EE"  # typically your slot/player
    cyan: "00EEEE"  # typically regular item
    slateblue: "6D8BE8"  # typically useful item
    plum: "AF99EF"  # typically progression item
    salmon: "FA8072"  # typically trap item
    white: "FFFFFF"  # not used, if you want to change the generic text color change color in Label

    (byte *) &cr_none, // 0 (RED)
    (byte *) &cr_dark, // 1 (DARK RED)
    (byte *) &cr_gray, // 2 (WHITE) normal text
    (byte *) &cr_green, // 3 (GREEN) location
    (byte *) &cr_gold, // 4 (YELLOW) player
    (byte *) &cr_red, // 5 (RED, same as cr_none)
    (byte *) &cr_blue, // 6 (BLUE) extra info such as Entrance
    (byte *) &cr_red2blue, // 7 (BLUE) items
    (byte *) &cr_red2green // 8 (DARK EDGE GREEN)
*/
void apdoom_update()
{
	if (ap_initialized)
	{
		if (!ap_cached_messages.empty())
		{
			for (const auto& cached_msg : ap_cached_messages)
				ap_settings.message_callback(cached_msg.c_str());
			ap_cached_messages.clear();
		}
	}

	while (AP_IsMessagePending())
	{
		AP_Message* msg = AP_GetLatestMessage();

		std::string colored_msg;

		switch (msg->type)
		{
			case AP_MessageType::ItemSend:
			{
				AP_ItemSendMessage* o_msg = static_cast<AP_ItemSendMessage*>(msg);
				colored_msg = "~9" + o_msg->item + "~2 was sent to ~4" + o_msg->recvPlayer;
				break;
			}
			case AP_MessageType::ItemRecv:
			{
				AP_ItemRecvMessage* o_msg = static_cast<AP_ItemRecvMessage*>(msg);
				colored_msg = "~2Received ~9" + o_msg->item + "~2 from ~4" + o_msg->sendPlayer;
				break;
			}
			case AP_MessageType::Hint:
			{
				AP_HintMessage* o_msg = static_cast<AP_HintMessage*>(msg);
				colored_msg = "~9" + o_msg->item + "~2 from ~4" + o_msg->sendPlayer + "~2 to ~4" + o_msg->recvPlayer + "~2 at ~3" + o_msg->location + (o_msg->checked ? " (Checked)" : " (Unchecked)");
				break;
			}
			default:
			{
				colored_msg = "~2" + msg->text;
				break;
			}
		}

		printf("APDOOM: %s\n", msg->text.c_str());

		if (ap_initialized)
			ap_settings.message_callback(colored_msg.c_str());
		else
			ap_cached_messages.push_back(colored_msg);

		AP_ClearLatestMessage();
	}

	// Check if we're in game, then dequeue the items
	if (ap_is_in_game)
	{
		while (!ap_item_queue.empty())
		{
			auto item_id = ap_item_queue.front();
			ap_item_queue.erase(ap_item_queue.begin());
			process_received_item(item_id);
		}
	}

	// Update notification icons
	float previous_y = 2.0f;
	for (auto it = ap_notification_icons.begin(); it != ap_notification_icons.end();)
	{
		auto& notification_icon = *it;

		if (notification_icon.state == AP_NOTIF_STATE_PENDING && previous_y > -100.0f)
		{
			notification_icon.state = AP_NOTIF_STATE_DROPPING;
		}
		if (notification_icon.state == AP_NOTIF_STATE_PENDING)
		{
			++it;
			continue;
		}

		if (notification_icon.state == AP_NOTIF_STATE_DROPPING)
		{
			notification_icon.vely += 0.15f + (float)(ap_notification_icons.size() / 4) * 0.25f;
			if (notification_icon.vely > 8.0f) notification_icon.vely = 8.0f;
			notification_icon.yf += notification_icon.vely;
			if (notification_icon.yf >= previous_y - AP_NOTIF_SIZE - AP_NOTIF_PADDING)
			{
				notification_icon.yf = previous_y - AP_NOTIF_SIZE - AP_NOTIF_PADDING;
				notification_icon.vely *= -0.3f / ((float)(ap_notification_icons.size() / 4) * 0.05f + 1.0f);

				notification_icon.t += ap_notification_icons.size() / 4 + 1; // Faster the more we have queued (4 can display on screen)
				if (notification_icon.t > 350 * 3 / 4) // ~7.5sec
				{
					notification_icon.state = AP_NOTIF_STATE_HIDING;
				}
			}
		}

		if (notification_icon.state == AP_NOTIF_STATE_HIDING)
		{
			notification_icon.velx -= 0.14f + (float)(ap_notification_icons.size() / 4) * 0.1f;
			notification_icon.xf += notification_icon.velx;
			if (notification_icon.xf < -AP_NOTIF_SIZE / 2)
			{
				it = ap_notification_icons.erase(it);
				continue;
			}
		}

		notification_icon.x = (int)notification_icon.xf;
		notification_icon.y = (int)notification_icon.yf;
		previous_y = notification_icon.yf;

		++it;
	}
}
