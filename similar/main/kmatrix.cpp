/*
 * Portions of this file are copyright Rebirth contributors and licensed as
 * described in COPYING.txt.
 * Portions of this file are copyright Parallax Software and licensed
 * according to the Parallax license below.
 * See COPYING.txt for license details.

THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/*
 *
 * Kill matrix displayed at end of level.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "dxxerror.h"
#include "pstypes.h"
#include "gr.h"
#include "window.h"
#include "key.h"
#include "palette.h"
#include "game.h"
#include "gameseq.h"
#include "window.h"
#include "physfsx.h"
#include "gamefont.h"
#include "u_mem.h"
#include "newmenu.h"
#include "menu.h"
#include "player.h"
#include "screens.h"
#include "cntrlcen.h"
#include "mouse.h"
#include "joy.h"
#include "timer.h"
#include "text.h"
#include "rbaudio.h"
#include "multi.h"
#include "kmatrix.h"
#include "gauges.h"
#include "pcx.h"
#include "object.h"
#include "args.h"

#include "compiler-range_for.h"
#include "d_levelstate.h"

#if DXX_USE_OGL
#include "ogl_init.h"
#endif

#define CENTERING_OFFSET(x) ((300 - (70 + (x)*25 ))/2)
#define CENTERSCREEN (SWIDTH/2)
#define KMATRIX_VIEW_SEC 7 // Time after reactor explosion until new level - in seconds
static void kmatrix_redraw_coop(fvcobjptr &vcobjptr);

static void kmatrix_draw_item(fvcobjptr &vcobjptr, grs_canvas &canvas, const grs_font &cv_font, const int i, const playernum_array_t &sorted)
{
	int x, y;

	y = FSPACY(50+i*9);
	const auto &&fspacx = FSPACX();
	auto &p = *vcplayerptr(sorted[i]);
	gr_string(canvas, cv_font, fspacx(CENTERING_OFFSET(N_players)), y, static_cast<const char *>(p.callsign));

	const auto &&rgb10 = BM_XRGB(10, 10, 10);
	const auto &&rgb25 = BM_XRGB(25, 25, 25);
	for (int j=0; j<N_players; j++)
	{
		x = fspacx(70 + CENTERING_OFFSET(N_players) + j * 25);

		const auto kmij = kill_matrix[sorted[i]][sorted[j]];
		if (sorted[i]==sorted[j])
		{
			if (kmij == 0)
			{
				gr_set_fontcolor(canvas, rgb10, -1);
				gr_string(canvas, cv_font, x, y, "0");
			}
			else
			{
				gr_set_fontcolor(canvas, rgb25, -1);
				gr_printf(canvas, cv_font, x, y, "-%hu", kmij);
			}
		}
		else
		{
			gr_set_fontcolor(canvas, kmij <= 0 ? rgb10 : rgb25, -1);
			gr_printf(canvas, cv_font, x, y, "%hu", kmij);
		}
	}

		auto &player_info = vcobjptr(p.objnum)->ctype.player_info;
		const int eff = (player_info.net_killed_total + player_info.net_kills_total <= 0)
			? 0
			: static_cast<int>(
				static_cast<float>(player_info.net_kills_total) / (
					static_cast<float>(player_info.net_killed_total) + static_cast<float>(player_info.net_kills_total)
				) * 100.0
			);

	x = fspacx(60 + CENTERING_OFFSET(N_players) + N_players * 25);
	gr_set_fontcolor(canvas, rgb25, -1);
	gr_printf(canvas, cv_font, x, y, "%4d/%i%%", player_info.net_kills_total, eff <= 0 ? 0 : eff);
}

static void kmatrix_draw_names(grs_canvas &canvas, const grs_font &cv_font, const playernum_array_t &sorted)
{
	int x;

	const auto &&fspacx = FSPACX();
	const auto &&fspacy = FSPACY();
	const auto &&rgb31 = BM_XRGB(31, 31, 31);
	for (int j=0; j<N_players; j++)
	{
		x = fspacx(70 + CENTERING_OFFSET(N_players) + j * 25);

		color_t c;
		auto &p = *vcplayerptr(sorted[j]);
		if (p.connected==CONNECT_DISCONNECTED)
			c = rgb31;
		else
		{
			const auto color = get_player_or_team_color(sorted[j]);
			const auto &rgb = player_rgb[color];
			c = BM_XRGB(rgb.r, rgb.g, rgb.b);
		}
		gr_set_fontcolor(canvas, c, -1);
		gr_printf(canvas, cv_font, x, fspacy(40), "%c", p.callsign[0u]);
	}

	x = fspacx(72 + CENTERING_OFFSET(N_players) + N_players * 25);
	gr_set_fontcolor(canvas, rgb31, -1);
	gr_string(canvas, cv_font, x, fspacy(40), "K/E");
}

static void kmatrix_draw_coop_names(grs_canvas &canvas, const grs_font &cv_font)
{
	gr_set_fontcolor(canvas, BM_XRGB(63, 31, 31),-1);
	const auto &&fspacy40 = FSPACY(40);
	const auto centerscreen = CENTERSCREEN;
	gr_string(canvas, cv_font, centerscreen, fspacy40, "SCORE");
	gr_string(canvas, cv_font, centerscreen + FSPACX(50), fspacy40, "DEATHS");
}

static void kmatrix_status_msg(grs_canvas &canvas, const fix time, const int reactor)
{
	gr_set_fontcolor(canvas, gr_find_closest_color(255, 255, 255),-1);
	auto &game_font = *GAME_FONT;
	gr_printf(canvas, game_font, 0x8000, SHEIGHT - LINE_SPACING(game_font, game_font), reactor
		? "Waiting for players to finish level. Reactor time: T-%d"
		: "Level finished. Wait (%d) to proceed or ESC to Quit."
	, time);
}

namespace dcx {

namespace {

struct kmatrix_window : window
{
	using window::window;
	grs_main_bitmap background;
	fix64 end_time = -1;
	kmatrix_network network;
	kmatrix_result result;
};

}

}

namespace dsx {

namespace {

struct kmatrix_window : ::dcx::kmatrix_window
{
	using ::dcx::kmatrix_window::kmatrix_window;
	virtual window_event_result event_handler(const d_event &) override;
};

static void kmatrix_redraw(kmatrix_window *const km)
{
	auto &Objects = LevelUniqueObjectState.Objects;
	auto &vcobjptr = Objects.vcptr;
	playernum_array_t sorted;

	gr_set_default_canvas();
	auto &canvas = *grd_curcanv;
	show_fullscr(canvas, km->background);
	
	if (Game_mode & GM_MULTI_COOP)
	{
		kmatrix_redraw_coop(vcobjptr);
	}
	else
	{
		multi_sort_kill_list();
		const auto title =
#if defined(DXX_BUILD_DESCENT_II)
			game_mode_capture_flag()
			? "CAPTURE THE FLAG SUMMARY"
			: game_mode_hoard()
				? "HOARD SUMMARY"
				:
#endif
				TXT_KILL_MATRIX_TITLE;
		auto &medium3_font = *MEDIUM3_FONT;
		gr_string(canvas, medium3_font, 0x8000, FSPACY(10), title);

		auto &game_font = *GAME_FONT;
		multi_get_kill_list(sorted);
		kmatrix_draw_names(canvas, game_font, sorted);

		for (int i=0; i<N_players; i++ )
		{
			if (vcplayerptr(sorted[i])->connected == CONNECT_DISCONNECTED)
				gr_set_fontcolor(canvas, gr_find_closest_color(31, 31, 31),-1);
			else
			{
				const auto color = get_player_or_team_color(sorted[i]);
				gr_set_fontcolor(canvas, BM_XRGB(player_rgb[color].r, player_rgb[color].g, player_rgb[color].b),-1);
			}
			kmatrix_draw_item(vcobjptr, canvas, game_font, i, sorted);
		}
	}

	gr_palette_load(gr_palette);
}

}

}

static void kmatrix_redraw_coop(fvcobjptr &vcobjptr)
{
	playernum_array_t sorted;

	multi_sort_kill_list();
	auto &canvas = *grd_curcanv;
	auto &medium3_font = *MEDIUM3_FONT;
	gr_string(canvas, medium3_font,  0x8000, FSPACY(10), "COOPERATIVE SUMMARY");
	multi_get_kill_list(sorted);
	auto &game_font = *GAME_FONT;
	kmatrix_draw_coop_names(canvas, game_font);
	const auto &&fspacx = FSPACX();
	const auto &&fspacy = FSPACY();
	const auto x_callsign = fspacx(CENTERING_OFFSET(N_players));
	const auto x_centerscreen = CENTERSCREEN;
	const auto &&fspacx50 = fspacx(50);
	const auto rgb60_40_10 = BM_XRGB(60, 40, 10);

	for (playernum_t i = 0; i < N_players; ++i)
	{
		auto &plr = *vcplayerptr(sorted[i]);
		int r, g, b;
		if (plr.connected == CONNECT_DISCONNECTED)
			r = g = b = 31;
		else
		{
			auto &color = player_rgb_normal[get_player_color(sorted[i])];
			r = color.r * 2;
			g = color.g * 2;
			b = color.b * 2;
		}
		gr_set_fontcolor(canvas, gr_find_closest_color(r, g, b), -1);

		const auto &&y = fspacy(50 + i * 9);
		gr_string(canvas, game_font, x_callsign, y, static_cast<const char *>(plr.callsign));
		gr_set_fontcolor(canvas, rgb60_40_10, -1);
		auto &player_info = vcobjptr(plr.objnum)->ctype.player_info;
		gr_printf(canvas, game_font, x_centerscreen, y, "%d", player_info.mission.score);
		gr_printf(canvas, game_font, x_centerscreen + fspacx50, y, "%d", player_info.net_killed_total);
	}

	gr_palette_load(gr_palette);
}

namespace dsx {

window_event_result kmatrix_window::event_handler(const d_event &event)
{
	auto &LevelUniqueControlCenterState = LevelUniqueObjectState.ControlCenterState;
	int k = 0, choice = 0;
	
	switch (event.type)
	{
		case EVENT_KEY_COMMAND:
			k = event_key_get(event);
			switch( k )
			{
				case KEY_ESC:
					{
						std::array<newmenu_item, 2> nm_message_items{{
							nm_item_menu(TXT_YES),
							nm_item_menu(TXT_NO),
						}};
						choice = newmenu_do(nullptr, TXT_ABORT_GAME, nm_message_items, network != kmatrix_network::offline ? get_multi_endlevel_poll2() : unused_newmenu_subfunction, unused_newmenu_userdata);
					}
					
					if (choice==0)
					{
						get_local_player().connected=CONNECT_DISCONNECTED;
						
						if (network != kmatrix_network::offline)
							multi_send_endlevel_packet();
						
						multi_leave_game();
						this->result = kmatrix_result::abort;

						return window_event_result::close;
					}
					return window_event_result::handled;
					
				default:
					break;
			}
			break;
			
		case EVENT_WINDOW_DRAW:
			{
			timer_delay2(50);

			if (network != kmatrix_network::offline)
				multi_do_protocol_frame(0, 1);
			
			uint8_t playing = 0;

			// Check if all connected players are also looking at this screen ...
			range_for (auto &i, Players)
				if (i.connected)
					if (i.connected != CONNECT_END_MENU && i.connected != CONNECT_DIED_IN_MINE)
					{
						playing = 1;
						break;
					}
			
			// ... and let the reactor blow sky high!
			if (!playing)
				LevelUniqueControlCenterState.Countdown_seconds_left = -1;
			
			// If Reactor is finished and end_time not inited, set the time when we will exit this loop
			const auto Countdown_seconds_left = LevelUniqueControlCenterState.Countdown_seconds_left;
			if (end_time == -1 && Countdown_seconds_left < 0 && !playing)
				end_time = timer_query() + (KMATRIX_VIEW_SEC * F1_0);
			
			// Check if end_time has been reached and exit loop
			if (timer_query() >= end_time && end_time != -1)
			{
				if (network != kmatrix_network::offline)
					multi_send_endlevel_packet();  // make sure
				
#if defined(DXX_BUILD_DESCENT_II)
				if (is_D2_OEM)
				{
					if (Current_level_num==8)
					{
						get_local_player().connected=CONNECT_DISCONNECTED;
						
						if (network != kmatrix_network::offline)
							multi_send_endlevel_packet();
						
						multi_leave_game();
						this->result = kmatrix_result::abort;
					}
				}
#endif
				return window_event_result::close;
			}

			kmatrix_redraw(this);
			kmatrix_status_msg(*grd_curcanv, playing ? Countdown_seconds_left : f2i(timer_query() - end_time), playing);
			break;
			}
			
		case EVENT_WINDOW_CLOSE:
			game_flush_inputs(Controls);
			newmenu_free_background();
			break;
			
		default:
			break;
	}
	return window_event_result::ignored;
}

kmatrix_result kmatrix_view(const kmatrix_network network, control_info &Controls)
{
	auto &Objects = LevelUniqueObjectState.Objects;
	auto &vcobjptridx = Objects.vcptridx;
	const auto pkm = std::make_unique<kmatrix_window>(grd_curscreen->sc_canvas, 0, 0, SWIDTH, SHEIGHT);
	auto &km = *pkm;
	if (pcx_read_bitmap(STARS_BACKGROUND, km.background, gr_palette) != pcx_result::SUCCESS)
	{
		return kmatrix_result::abort;
	}
	gr_palette_load(gr_palette);

	km.network = network;
	km.result = kmatrix_result::proceed;
	
	set_screen_mode( SCREEN_MENU );
	game_flush_inputs(Controls);

	range_for (auto &i, Players)
		if (i.objnum != object_none)
			digi_kill_sound_linked_to_object(vcobjptridx(i.objnum));

	km.send_creation_events();
	event_process_all();
	return km.result;
}

}
