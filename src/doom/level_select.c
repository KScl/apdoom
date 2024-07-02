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
// *Level select feature for archipelago*
//

#include "level_select.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_main.h"
#include "s_sound.h"
#include "z_zone.h"
#include "v_video.h"
#include "d_player.h"
#include "doomkeys.h"
#include "apdoom.h"
#include "i_video.h"
#include "g_game.h"
#include "m_misc.h"
#include "hu_lib.h"
#include "hu_stuff.h"
#include <math.h>
#include "m_controls.h"


void WI_initAnimatedBack(void);
void WI_updateAnimatedBack(void);
void WI_drawAnimatedBack(void);
void WI_initVariables(wbstartstruct_t* wbstartstruct);
void WI_loadData(void);


#if 0
typedef struct
{
    int x;
    int y;
    int keys_offset;
    char* urhere_lump_name;
    int urhere_x_offset;
    int urhere_y_offset;
    char* img;
    int img_x_offset;
    int img_y_offset;
    int h_key_alignement;
    int h_key_x_offset;
    int h_key_y_offset;
} level_pos_t;


extern int cursor_x;
extern int cursor_y;


typedef struct
{
    int x, y;
    int right_align;
} legende_t;


static legende_t legendes[4] = {
    {0, 200 - 8 * 3, 0},
    {0, 200 - 8 * 3, 0},
    {0, 200 - 8 * 3, 0},
    {320, 200 - 8 * 3, 1}
};


static level_pos_t level_pos_infos[4][9] =
{
    // Episode 1
    {
        { 185, 164 + 10, 22, "WIURH0", 0, 0, 0, 0, 0 },
        { 148, 143, 18, "WIURH0", 0, 0, 0, 0, 0 },
        { 69, 122, 18, "WIURH1", 0, 0, 0, 0, 0 },
        { 209 + 20, 102, 22, "WIURH0", 0, 0, 0, 0, 0 },
        { 116, 89, 26, "WIURH2", 0, 0, 0, 0, 0 },
        { 166 + 10, 55 - 2, 22, "WIURH0", 0, 0, 0, 0, 0 },
        { 71, 56, 18, "WIURH1", 0, 0, 0, 0, 0 },
        { 135, 29, 18, "WIURH0", -2, 4, 0, 0, 0 },
        { 70, 24, 22, "WIURH1", 0, 0, 0, 0, 0 }
    },

#ifndef FIRST_EP_ONLY
    // Episode 2
    {
	    { 254, 25, 18, "WIURH2", 0, 0, 0, 0, 0 },	// location of level 0 (CJ)
	    { 97, 50, 22, "WIURH0", 0, 0, 0, 0, 0 },	// location of level 1 (CJ)
	    { 188, 64, 18, "WIURH0", 0, 0, 0, 0, 0 },	// location of level 2 (CJ)
	    { 128, 83, 22, "WIURH3", 0, 0, 0, 0, 0 },	// location of level 3 (CJ)
	    { 214, 92, 22, "WIURH0", 0, 0, 0, 0, 0 },	// location of level 4 (CJ)
	    { 133, 130, 20, "WIURH0", 0, 0, 0, 0, 0 },	// location of level 5 (CJ)
	    { 208, 135, 18, "WIURH0", 0, 0, 0, 0, 0 },	// location of level 6 (CJ)
	    { 148, 160, 22, "WIURH2", 0, 0, 0, 0, 0 },	// location of level 7 (CJ)
	    { 235, 168, 18, "WIURH2", 0, 0, 0, 0, 0 }	// location of level 8 (CJ)
    },

    // Episode 3
    {
	    { 156, 168, 22, "WIURH0", 0, 0, 0, 0, 0 },	// location of level 0 (CJ)
	    { 48, 154, 22, "WIURH0", 0, 0, 0, 0, 0 },	// location of level 1 (CJ)
	    { 174, 95, -26, "WIURH0", 0, 0, 0, 0, 0 },	// location of level 2 (CJ)
	    { 265, 75, 22, "WIURH3", 0, 0, 0, 0, 0 },	// location of level 3 (CJ)
	    { 130, 52, -24, "WIURH3", 0, 0, 0, 0, 0 },	// location of level 4 (CJ)
	    { 279, 23, -26, "WIURH1", 8, 0, 0, 0, 0 },	// location of level 5 (CJ)
	    { 198, 48, 18, "WIURH3", 0, 0, 0, 0, 0 },	// location of level 6 (CJ)
	    { 140, 25, 22, "WIURH1", 0, 0, 0, 0, 0 },	// location of level 7 (CJ)
	    { 281, 136, -26, "WIURH3", 0, 0, 0, 0, 0 }	// location of level 8 (CJ)
    },

    // Episode 4
    {
        { 101, 177, 22, "WIURH1", 0, 0, 0, 0, 0 },	// location of level 0 (CJ)
	    { 183, 148, 18, "WIURH0", 0, 0, 0, 0, 0 },	// location of level 1 (CJ)
	    { 172, 97, 18, "WIURH2", 0, 0, 0, 0, 0 },	// location of level 2 (CJ)
	    { 78, 86, 22, "WIURH2", 0, 0, 0, 0, 0 },	// location of level 3 (CJ)
	    { 251, 85, 26, "WIURH2", 0, 0, 0, 0, 0 },	// location of level 4 (CJ)
	    { 37, 24, -24, "WIURH2", 3, -20, 0, 0, 0 },	// location of level 5 (CJ)
	    { 260, 47, 18, "WIURH0", -5, -4, 0, 0, 0 },	// location of level 6 (CJ)
	    { 194, 25, 24, "WIURH3", 0, 0, 0, 0, 0 },	// location of level 7 (CJ)
	    { 88, 58, 17, "WIURH0", 0, 0, 0, 0, 0 }	// location of level 8 (CJ)
    }
#endif
};

static level_pos_t level_pos_infos_d2[4][11] =
{
    // Episode 1
    {
        { 30, 15 + 17 * 0, 0, "M_SKULL1", -32, -9, "CWILV00", 10, -5, 1 },
        { 30, 15 + 17 * 1, 0, "M_SKULL1", -32, -9, "CWILV01", 10, -5, 1 },
        { 30, 15 + 17 * 2, 0, "M_SKULL1", -32, -9, "CWILV02", 10, -5, 1 },
        { 30, 15 + 17 * 3, 0, "M_SKULL1", -32, -9, "CWILV03", 10, -5, 1 },
        { 30, 15 + 17 * 4, 0, "M_SKULL1", -32, -9, "CWILV04", 10, -5, 1 },
        { 30, 15 + 17 * 5, 0, "M_SKULL1", -32, -9, "CWILV05", 10, -5, 1 },
        { 30, 15 + 17 * 6, 0, "M_SKULL1", -32, -9, "CWILV06", 10, -5, 1 },
        { 30, 15 + 17 * 7, 0, "M_SKULL1", -32, -9, "CWILV07", 10, -5, 1 },
        { 30, 15 + 17 * 8, 0, "M_SKULL1", -32, -9, "CWILV08", 10, -5, 1 },
        { 30, 15 + 17 * 9, 0, "M_SKULL1", -32, -9, "CWILV09", 10, -5, 1 },
        { 30, 15 + 17 *10, 0, "M_SKULL1", -32, -9, "CWILV10", 10, -5, 1 }
    },

    // Episode 2
    {
        { 40, 15 + 21 * 0, 0, "M_SKULL1", -32, -9, "CWILV11", 10, -5, 1 },
        { 40, 15 + 21 * 1, 0, "M_SKULL1", -32, -9, "CWILV12", 10, -5, 1 },
        { 40, 15 + 21 * 2, 0, "M_SKULL1", -32, -9, "CWILV13", 10, -5, 1 },
        { 40, 15 + 21 * 3, 0, "M_SKULL1", -32, -9, "CWILV14", 10, -5, 1 },
        { 40, 15 + 21 * 4, 0, "M_SKULL1", -32, -9, "CWILV15", 10, -5, 1 },
        { 40, 15 + 21 * 5, 0, "M_SKULL1", -32, -9, "CWILV16", 10, -5, 1 },
        { 40, 15 + 21 * 6, 0, "M_SKULL1", -32, -9, "CWILV17", 10, -5, 1 },
        { 40, 15 + 21 * 7, 0, "M_SKULL1", -32, -9, "CWILV18", 10, -5, 1 },
        { 40, 15 + 21 * 8, 0, "M_SKULL1", -32, -9, "CWILV19", 10, -5, 1 }
    },

    // Episode 3
    {
        { 30, 15 + 18 * 0, 0, "M_SKULL1", -32, -9, "CWILV20", 10, -5, 1 },
        { 30, 15 + 18 * 1, 0, "M_SKULL1", -32, -9, "CWILV21", 10, -5, 1 },
        { 30, 15 + 18 * 2, 0, "M_SKULL1", -32, -9, "CWILV22", 10, -5, 1 },
        { 30, 15 + 18 * 3, 0, "M_SKULL1", -32, -9, "CWILV23", 10, -5, 1 },
        { 30, 15 + 18 * 4, 0, "M_SKULL1", -32, -9, "CWILV24", 10, -5, 1 },
        { 30, 15 + 18 * 5, 0, "M_SKULL1", -32, -9, "CWILV25", 10, -5, 1, -38, -12 },
        { 30, 15 + 18 * 6, 0, "M_SKULL1", -32, -9, "CWILV26", 10, -5, 1 },
        { 30, 15 + 18 * 7, 0, "M_SKULL1", -32, -9, "CWILV27", 10, -5, 1 },
        { 30, 15 + 18 * 8, 0, "M_SKULL1", -32, -9, "CWILV28", 10, -5, 1 },
        { 30, 15 + 18 * 9, 0, "M_SKULL1", -32, -9, "CWILV29", 10, -5, 1 }
    },

    // Episode 4
    {
        { 100, 50, 0, "M_SKULL1", -32, -9, "CWILV30", 10, -5, 1 },
        { 130, 110, 0, "M_SKULL1", -32, -9, "CWILV31", 10, -5, 1 }
    }
};


const level_pos_t* get_level_pos_info(int ep, int map)
{
    if (gamemode == commercial) return &level_pos_infos_d2[ep][map];
    return &level_pos_infos[ep][map];
}
#endif

static wbstartstruct_t wiinfo;

extern int bcnt;

int selected_level[4] = {0};
int selected_ep = 0;
int prev_ep = 0;
int ep_anim = 0;
int urh_anim = 0;

static const char* YELLOW_DIGIT_LUMP_NAMES[] = {
    "STYSNUM0", "STYSNUM1", "STYSNUM2", "STYSNUM3", "STYSNUM4", 
    "STYSNUM5", "STYSNUM6", "STYSNUM7", "STYSNUM8", "STYSNUM9"
};


void print_right_aligned_yellow_digit(int x, int y, int digit)
{
    x -= 4;

    if (!digit)
    {
        V_DrawPatch(x, y, W_CacheLumpName(YELLOW_DIGIT_LUMP_NAMES[0], PU_CACHE));
        return;
    }

    while (digit)
    {
        int i = digit % 10;
        V_DrawPatch(x, y, W_CacheLumpName(YELLOW_DIGIT_LUMP_NAMES[i], PU_CACHE));
        x -= 4;
        digit /= 10;
    }
}


void print_left_aligned_yellow_digit(int x, int y, int digit)
{
    if (!digit)
    {
        x += 4;
    }

    int len = 0;
    int d = digit;
    while (d)
    {
        len++;
        d /= 10;
    }
    print_right_aligned_yellow_digit(x + len * 4, y, digit);
}


void restart_wi_anims()
{
    wiinfo.epsd = selected_ep;
    WI_initVariables(&wiinfo);
    WI_loadData();
    WI_initAnimatedBack();
}

void HU_ClearAPMessages();

void play_level(int ep, int lvl)
{
    ap_level_index_t idx = { ep, lvl };
    ep = ap_index_to_ep(idx);
    lvl = ap_index_to_map(idx);

    // Check if level has a save file first
    char filename[260];
    if (gamemode != commercial)
        snprintf(filename, 260, "%s/save_E%iM%i.dsg", apdoom_get_seed(), ep, lvl);
    else
        snprintf(filename, 260, "%s/save_MAP%02i.dsg", apdoom_get_seed(), lvl);

    if (M_FileExists(filename))
    {
        // We load
        extern char savename[256];
        snprintf(savename, 256, "%s", filename);
        ap_state.player_state.powers[pw_strength] = 0;
        gameaction = ga_loadgame;
        //G_DoLoadGame();
    }
    else
    {
        // If none, load it fresh
        G_DeferedInitNew(gameskill, ep, lvl);
    }

    HU_ClearAPMessages();

    // This breaks everything, if we're starting a new save file on an already cleared game
    //apdoom_check_victory(); // In case we had pending victory
}


void select_map_dir(int dir)
{
    const ap_levelselect_t* screen_defs = ap_get_level_select_info(selected_ep);

    int from = selected_level[selected_ep];
    float fromx = (float)screen_defs->map_info[from].x;
    float fromy = (float)screen_defs->map_info[from].y;

    int best = from;
    int top_most = 200;
    int top_most_idx = -1;
    int bottom_most = 0;
    int bottom_most_idx = -1;
    float best_score = 0.0f;

    int map_count = ap_get_map_count(selected_ep + 1);
    for (int i = 0; i < map_count; ++i)
    {
        if (screen_defs->map_info[i].y < top_most)
        {
            top_most = screen_defs->map_info[i].y;
            top_most_idx = i;
        }
        if (screen_defs->map_info[i].y > bottom_most)
        {
            bottom_most = screen_defs->map_info[i].y;
            bottom_most_idx = i;
        }
        if (i == from) continue;

        float tox = (float)screen_defs->map_info[i].x;
        float toy = (float)screen_defs->map_info[i].y;
        float score = 0.0f;
        float dist = 10000.0f;

        switch (dir)
        {
            case 0: // Left
                if (tox >= fromx) continue;
                dist = fromx - tox;
                break;
            case 1: // Right
                if (tox <= fromx) continue;
                dist = tox - fromx;
                break;
            case 2: // Up
                if (toy >= fromy) continue;
                dist = fromy - toy;
                break;
            case 3: // Down
                if (toy <= fromy) continue;
                dist = toy - fromy;
                break;
        }
        score = 1.0f / dist;

        if (score > best_score)
        {
            best_score = score;
            best = i;
        }
    }

    // Are we at the top? go to the bottom
    if (from == top_most_idx && dir == 2)
    {
        best = bottom_most_idx;
    }
    else if (from == bottom_most_idx && dir == 3)
    {
        best = top_most_idx;
    }

    if (best != from)
    {
        urh_anim = 0;
        S_StartSoundOptional(NULL, sfx_mnusli, sfx_stnmov);
        selected_level[selected_ep] = best;
    }
}


static void level_select_nav_left()
{
    select_map_dir(0);
}


static void level_select_nav_right()
{
    select_map_dir(1);
}


static void level_select_nav_up()
{
    select_map_dir(2);
}


static void level_select_nav_down()
{
    select_map_dir(3);
}


static int get_episode_count()
{
    int ep_count = 0;
    for (int i = 0; i < ap_episode_count; ++i)
        if (ap_state.episodes[i])
            ep_count++;
    return ep_count;
}


static void level_select_prev_episode()
{
    if (gamemode != shareware && get_episode_count() > 1)
    {
        prev_ep = selected_ep;
        ep_anim = -10;
        selected_ep--;
        if (selected_ep < 0) selected_ep = ap_episode_count - 1;
        while (!ap_state.episodes[selected_ep])
        {
            selected_ep--;
            if (selected_ep < 0) selected_ep = ap_episode_count - 1;
            if (selected_ep == prev_ep) // oops;
                break;
        }
        restart_wi_anims();
        urh_anim = 0;
        S_StartSoundOptional(NULL, sfx_mnucls, sfx_swtchx);
    }
}


static void level_select_next_episode()
{
    if (gamemode != shareware && get_episode_count() > 1)
    {
        prev_ep = selected_ep;
        ep_anim = 10;
        selected_ep = (selected_ep + 1) % ap_episode_count;
        while (!ap_state.episodes[selected_ep])
        {
            selected_ep = (selected_ep + 1) % ap_episode_count;
            if (selected_ep == prev_ep) // oops;
                break;
        }
        restart_wi_anims();
        urh_anim = 0;
        S_StartSoundOptional(NULL, sfx_mnucls, sfx_swtchx);
    }
}


static void level_select_nav_enter()
{
    ap_level_index_t idx = {selected_ep, selected_level[selected_ep]};
    if (ap_get_level_state(idx)->unlocked)
    {
        S_StartSoundOptional(NULL, sfx_mnusli, sfx_swtchn);
        play_level(selected_ep, selected_level[selected_ep]);
    }
    else
    {
        S_StartSoundOptional(NULL, sfx_mnusli, sfx_noway);
    }
}


boolean LevelSelectResponder(event_t* ev)
{
    if (ep_anim) return true;

    int ep_count = get_episode_count();

    switch (ev->type)
    {
        case ev_joystick:
        {
            if (ev->data4 < 0 || ev->data2 < 0)
            {
                level_select_nav_left();
                joywait = I_GetTime() + 5;
            }
            else if (ev->data4 > 0 || ev->data2 > 0)
            {
                level_select_nav_right();
                joywait = I_GetTime() + 5;
            }
            else if (ev->data3 < 0)
            {
                level_select_nav_up();
                joywait = I_GetTime() + 5;
            }
            else if (ev->data3 > 0)
            {
                level_select_nav_down();
                joywait = I_GetTime() + 5;
            }

#define JOY_BUTTON_MAPPED(x) ((x) >= 0)
#define JOY_BUTTON_PRESSED(x) (JOY_BUTTON_MAPPED(x) && (ev->data1 & (1 << (x))) != 0)

            if (JOY_BUTTON_PRESSED(joybfire)) level_select_nav_enter();

            if (JOY_BUTTON_PRESSED(joybprevweapon)) level_select_prev_episode();
            else if (JOY_BUTTON_PRESSED(joybnextweapon)) level_select_next_episode();

            break;
        }
        case ev_keydown:
        {
            if (ev->data1 == key_left || ev->data1 == key_alt_strafeleft || ev->data1 == key_strafeleft) level_select_prev_episode();
            if (ev->data1 == key_right || ev->data1 == key_alt_straferight || ev->data1 == key_straferight) level_select_next_episode();
            if (ev->data1 == key_up || ev->data1 == key_alt_up) level_select_nav_up();
            if (ev->data1 == key_down || ev->data1 == key_alt_down) level_select_nav_down();
            if (ev->data1 == key_menu_forward || ev->data1 == key_use) level_select_nav_enter();
            break;
        }
    }

    return true;
}


void ShowLevelSelect()
{
    HU_ClearAPMessages();

    // If in a level, save current level
    if (gamestate == GS_LEVEL)
        G_DoSaveGame(); 

    if (crispy->ap_levelselectmusic)
        S_ChangeMusic(mus_read_m, true);
    else
        S_StopMusic();

    gameaction = ga_nothing;
    gamestate = GS_LEVEL_SELECT;
    viewactive = false;
    automapactive = false;

    ap_state.ep = 0;
    ap_state.map = 0;

    while (!ap_state.episodes[selected_ep])
    {
        selected_ep = (selected_ep + 1) % ap_episode_count;
        if (selected_ep == 0) // oops;
            break;
    }

    wiinfo.epsd = selected_ep;
    wiinfo.didsecret = false;
    wiinfo.last = -1;
    wiinfo.next = -1;
    wiinfo.maxkills = 0;
    wiinfo.maxitems = 0;
    wiinfo.maxsecret = 0;
    wiinfo.maxfrags = 0;
    wiinfo.partime = 0;
    wiinfo.pnum = 0;
    
    restart_wi_anims();
    bcnt = 0;
}


void TickLevelSelect()
{
    if (ep_anim > 0)
        ep_anim -= 1;
    else if (ep_anim < 0)
        ep_anim += 1;
    bcnt++;
    urh_anim = (urh_anim + 1) % 35;
    WI_updateAnimatedBack();
}


void draw_legend_line_right_aligned(const char* text, int x, int y)
{
    int w = HULib_measureText(text, strlen(text));
    HUlib_drawText(text, x - w, y);
}


void draw_legend_line(const char* text, int x, int y)
{
    HUlib_drawText(text, x, y);
}


void DrawEpisodicLevelSelectStats()
{
    int x, y;
    const int key_spacing = 8;
    const int key_h_spacing = 12;
    const int start_y_offset = 10;

    const ap_levelselect_t* screen_defs = ap_get_level_select_info(selected_ep);

    int map_count = ap_get_map_count(selected_ep + 1);
    for (int i = 0; i < map_count; ++i)
    {
        const ap_levelselect_map_t* mapinfo = &screen_defs->map_info[i];

        ap_level_index_t idx = {selected_ep, i};
        ap_level_info_t* ap_level_info = ap_get_level_info(idx);
        ap_level_state_t* ap_level_state = ap_get_level_state(idx);

        x = mapinfo->x;
        y = mapinfo->y;

        int key_count = 0;
        for (int i = 0; i < 3; ++i)
            if (ap_level_info->keys[i])
                key_count++;

        const int key_start_offset = -key_spacing * key_count / 2;

        int img_w = 0;

        // Level custom icon
        if (mapinfo->image.graphic[0])
        {
            patch_t* patch = W_CacheLumpName(mapinfo->image.graphic, PU_CACHE);
            img_w = patch->width;
            V_DrawPatch(x + mapinfo->image.x, y + mapinfo->image.y, patch);
        }
        
        // Level complete splash
        if (ap_level_state->completed)
            V_DrawPatch(x, y, W_CacheLumpName("WISPLAT", PU_CACHE));

        // Lock
        if (!ap_level_state->unlocked)
            V_DrawPatch(x, y, W_CacheLumpName("WILOCK", PU_CACHE));

        // Keys
        const char* key_lump_names[] = {"STKEYS0", "STKEYS1", "STKEYS2"};
        const char* key_skull_lump_names[] = {"STKEYS3", "STKEYS4", "STKEYS5"};

        int key_x = x + mapinfo->keys.x;
        int key_y = y + mapinfo->keys.y;
        switch (mapinfo->keys.relative_to)
        {
            default:
                break;
            case 2:
                key_x += img_w;
                // fall through
            case 1:
                key_x += mapinfo->image.x;
                key_y += mapinfo->image.y;
                break;
        }

        for (int k = 0; k < 3; ++k)
        {
            if (!ap_level_info->keys[k])
                continue;

            const char* key_lump_name = (ap_level_info->use_skull[k]) ? key_skull_lump_names[k] : key_lump_names[k];

            V_DrawPatch(key_x, key_y, W_CacheLumpName("KEYBG", PU_CACHE));
            if (mapinfo->keys.use_checkmark)
            {
                const int checkmark_x = key_x + mapinfo->keys.checkmark_x;
                const int checkmark_y = key_x + mapinfo->keys.checkmark_y;

                V_DrawPatch(key_x + 2, key_y + 1, W_CacheLumpName(key_lump_name, PU_CACHE));
                if (ap_level_state->keys[k])
                    V_DrawPatch(checkmark_x, checkmark_y, W_CacheLumpName("CHECKMRK", PU_CACHE));
            }
            else
            {
                if (ap_level_state->keys[k])
                    V_DrawPatch(key_x + 2, key_y + 1, W_CacheLumpName(key_lump_name, PU_CACHE));                
            }

            key_x += mapinfo->keys.spacing_x;
            key_y += mapinfo->keys.spacing_y;
        }
#if 0
        if (level_pos->h_key_alignement)
        {
            key_x = x + img_w + 3 + level_pos->h_key_x_offset;
            key_y = y - 3 + level_pos->h_key_y_offset;
            for (int k = 0; k < 3; ++k)
            {
                if (ap_level_info->keys[k])
                {
                    const char* key_lump_name = key_lump_names[k];
                    if (ap_level_info->use_skull[k])
                        key_lump_name = key_skull_lump_names[k];
                    V_DrawPatch(key_x, key_y, W_CacheLumpName("KEYBG", PU_CACHE));
                    if (ap_level_state->keys[k])
                        V_DrawPatch(key_x + 2, key_y + 1, W_CacheLumpName(key_lump_name, PU_CACHE));
                    key_x += key_h_spacing;
                }
            }
        }
        else
        {
            key_x = x + level_pos->keys_offset;
            key_y = y + key_start_offset;
            for (int k = 0; k < 3; ++k)
            {
                if (ap_level_info->keys[k])
                {
                    const char* key_lump_name = key_lump_names[k];
                    if (ap_level_info->use_skull[k])
                        key_lump_name = key_skull_lump_names[k];
                    V_DrawPatch(key_x, key_y, W_CacheLumpName("KEYBG", PU_CACHE));
                    V_DrawPatch(key_x + 2, key_y + 1, W_CacheLumpName(key_lump_name, PU_CACHE));
                    if (ap_level_state->keys[k])
                    {
                        if (level_pos->keys_offset < 0)
                        {
                            V_DrawPatch(key_x - 12, key_y - 1, W_CacheLumpName("CHECKMRK", PU_CACHE));
                        }
                        else
                        {
                            V_DrawPatch(key_x + 12, key_y - 1, W_CacheLumpName("CHECKMRK", PU_CACHE));
                        }
                    }
                    key_y += key_spacing;
                }
            }
        }
#endif

        // Progress
#if 0
        int progress_x = x - 4;
        int progress_y = y + start_y_offset;
        if (level_pos->h_key_alignement)
        {
            progress_x = key_x + 8;
            progress_y = key_y + 2;
        }
        print_right_aligned_yellow_digit(progress_x, progress_y, ap_level_state->check_count);
        V_DrawPatch(progress_x + 1, progress_y, W_CacheLumpName("STYSLASH", PU_CACHE));
        print_left_aligned_yellow_digit(progress_x + 8, progress_y, ap_level_info->check_count - ap_level_info->sanity_check_count);
#else
        int progress_x = x + mapinfo->checks.x;
        int progress_y = y + mapinfo->checks.y;
        switch (mapinfo->checks.relative_to)
        {
            default:
                break;
            case 2:
                progress_x += img_w;
                // fall through
            case 1:
                progress_x += mapinfo->image.x;
                progress_y += mapinfo->image.y;
                break;
            case 3:
                progress_x += mapinfo->keys.x;
                progress_y += mapinfo->keys.y;
                break;
            case 4:
                progress_x = key_x + mapinfo->checks.x;
                progress_y = key_y + mapinfo->checks.y;
                break;
        }
        print_right_aligned_yellow_digit(progress_x, progress_y, ap_level_state->check_count);
        V_DrawPatch(progress_x + 1, progress_y, W_CacheLumpName("STYSLASH", PU_CACHE));
        print_left_aligned_yellow_digit(progress_x + 8, progress_y, ap_level_info->check_count - ap_level_info->sanity_check_count);

#endif

#if 0
        // "You are here"
        if (i == selected_level[selected_ep] && urh_anim < 25)
        {
            int x_offset = 2;
            int y_offset = -2;
            if (level_pos->urhere_lump_name[5] == '1')
            {
                x_offset = -2;
            }
            if ((level_pos->urhere_lump_name[5] == '0' && level_pos->keys_offset > 0) ||
                (level_pos->urhere_lump_name[5] == '1' && level_pos->keys_offset < 0))
            {
                y_offset += key_start_offset;
            }
            if (level_pos->urhere_lump_name[5] == '2' ||
                level_pos->urhere_lump_name[5] == '3')
            {
                y_offset = 16;
            }
            V_DrawPatch(x + x_offset + level_pos->urhere_x_offset, 
                        y + y_offset + level_pos->urhere_y_offset, 
                        W_CacheLumpName(level_pos->urhere_lump_name, PU_CACHE));
        }
#else
        if (i == selected_level[selected_ep] && urh_anim < 25)
        {
            V_DrawPatch(x + mapinfo->cursor.x, 
                        y + mapinfo->cursor.y, 
                        W_CacheLumpName(mapinfo->cursor.graphic, PU_CACHE));
        }
#endif
    }

    // Level name
    if (gamemode != commercial)
    {
        char name[9];
        if (gamemode == commercial)
        {
            int map = 0;
            for (int ep = 0; ep < ap_episode_count; ++ep)
            {
                if (ep == selected_ep)
                {
                    map += selected_level[selected_ep];
                    break;
                }
                map += ap_get_map_count(ep + 1);
            }
            snprintf(name, 9, "CWILV%02i", map);
        }
        else
            snprintf(name, 9, "WILV%d%d", selected_ep, selected_level[selected_ep]);
        if (W_CheckNumForName(name) != -1)
        {
            patch_t* finished = W_CacheLumpName(name, PU_STATIC);
            V_DrawPatch((ORIGWIDTH - finished->width) / 2, 2, finished);
        }
    }

    // Mouse Cursor
    //V_DrawPatch(cursor_x, cursor_y, W_CacheLumpName("CURSOR", PU_STATIC));

    // Legend
    //int lx = legendes[selected_ep].x;
    //int ly = legendes[selected_ep].y;
    //typedef void (*draw_legend_line_fn_t)(const char* text, int x, int y);
    //draw_legend_line_fn_t draw_legend_line_fn = draw_legend_line;
    //if (legendes[selected_ep].right_align) draw_legend_line_fn = draw_legend_line_right_aligned;
    //draw_legend_line_fn("~2Change map: ~3Arrows", lx, ly);
    //draw_legend_line_fn("~2Change episode: ~3[~2, ~3]", lx, ly + 8);
    //draw_legend_line_fn("~2Enter map: ~3Enter", lx, ly + 16);
}


void DrawLevelSelectStats()
{
    DrawEpisodicLevelSelectStats();
}


static const char* WIN_MAPS[4] = {
    "WIMAP0",
    "WIMAP1",
    "WIMAP2",
    "WIMAP3"
};


static const char* WIN_MAPS_D2[4] = {
    "INTERPIC",
    "D2INTER2",
    "BOSSBACK",
    "D2INTER3"
};


const char* get_win_map(int ep)
{
    if (gamemode == commercial) return WIN_MAPS_D2[ep];
    else return WIN_MAPS[ep];
}


void DrawLevelSelect()
{
    int x_offset = ep_anim * 32;

    char lump_name[9];
    snprintf(lump_name, 9, "%s", ap_get_level_select_info(selected_ep)->background_image);
    
    // [crispy] fill pillarboxes in widescreen mode
    if (SCREENWIDTH != NONWIDEWIDTH)
    {
        V_DrawFilledBox(0, 0, SCREENWIDTH, SCREENHEIGHT, 0);
    }

    V_DrawPatch(x_offset, 0, W_CacheLumpName(lump_name, PU_CACHE));
    if (ep_anim == 0)
    {
        WI_drawAnimatedBack();

        DrawLevelSelectStats();
    }
    else
    {
        snprintf(lump_name, 9, "%s", ap_get_level_select_info(prev_ep)->background_image);
        if (ep_anim > 0)
            x_offset = -(10 - ep_anim) * 32;
        else
            x_offset = (10 + ep_anim) * 32;
        V_DrawPatch(x_offset, 0, W_CacheLumpName(lump_name, PU_CACHE));
    }
}
