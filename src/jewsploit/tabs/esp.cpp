#include "esp.h"
#include "helpers.h"
#include "../widgets/child.h"
#include "../widgets/checkbox.h"
#include "../widgets/colorpicker.h"
#include "app/Settings.h"
#include "app/Graphics.h"
#include "features/visuals/boxfill/BoxFill.h"
#include "features/visuals/ShaderChams.h"
#include "features/visuals/MeshDxShader.h"
#include "features/visuals/MeshChams.h"
#include "features/visuals/havoc/HavocWorldEsp.h"
#include "features/games/PhantomForces.h"
#include <imgui.h>

void ng_tabs::draw_esp_tab()
{
	using namespace Cheat;

	float side_gap = 10.f;
	float avail_w = ImGui::GetContentRegionAvail().x;
	float avail_h = ImGui::GetContentRegionAvail().y;
	float cw = (avail_w - side_gap) * 0.5f;

	if (ng::child_begin("##esp_child1", "esp", cw, avail_h, 10.f, true))
	{
		pad();
		ng::checkbox("enabled", &g_Settings.esp.enabled);
		gap();

		pad();
		ng::checkbox("draw local", &g_Settings.esp.draw_local);
		gap();

		row_cb_color("bounding box", &g_Settings.esp.box,
		             g_Settings.esp.box_color, "esp_box_color");
		if (g_Settings.esp.box)
			g_Settings.esp.box_fill = false;
		gap();

		if (!g_Settings.esp.box)
		{
			if (g_Settings.esp.box_fill_mode == 1)
			{
				pad();
				ng::checkbox("box fill", &g_Settings.esp.box_fill);
			}

			else
			{
				row_cb_color("box fill", &g_Settings.esp.box_fill,
				             g_Settings.esp.box_fill_color, "esp_box_fill_color");
			}
			gap();

			if (g_Settings.esp.box_fill)
			{
				static const char* k_fill_modes[] = { "color", "image" };
				row_select("##fill_type", "fill type", &g_Settings.esp.box_fill_mode,
				           k_fill_modes, 2);
				gap();

				if (g_Settings.esp.box_fill_mode == 1)
				{
					static const char* k_fill_images[] = {
						"lebrone", "peter", "stewie", "2minion",
						"on_baby", "pink_floyd", "didi_bop", "herobrine",
						"vape", "dok", "panta",
						"charlie-binladen", "kostya", "loshad",
						"elsa", "vovka", "egor", "diddy_kirk", "a-kirk",
						"i_cant_fly", "patriot", "agatha_kirk"
					};
					row_select("##fill_image", "fill image",
					           &g_Settings.esp.box_fill_image,
					           k_fill_images, 22);
					if (g_Settings.esp.box_fill_image < 0)
						g_Settings.esp.box_fill_image = 0;
					if (g_Settings.esp.box_fill_image > 21)
						g_Settings.esp.box_fill_image = 21;
					gap();

					row_slider("##fill_opacity", "fill opacity",
					           &g_Settings.esp.box_fill_image_alpha, 0.0f, 1.0f);
					gap();

					pad();
					ng::checkbox("remove background",
					             &g_Settings.esp.box_fill_remove_bg);
					Visuals::BoxFill::SetRemoveBg(
						Core::g_Device, g_Settings.esp.box_fill_remove_bg);
					gap();
				}
			}
		}

		row_cb_color("name", &g_Settings.esp.name,
		             g_Settings.esp.name_color, "esp_name_color");
		gap();

		if (g_Settings.esp.name)
		{
			static const char* k_name_modes[] = { "display name", "username" };
			row_select("##name_type", "name type", &g_Settings.esp.name_mode,
			           k_name_modes, 2);
			gap();
		}

		row_cb_color("skeleton", &g_Settings.esp.skeleton,
		             g_Settings.esp.skeleton_color, "esp_skeleton_color");
		gap();

		if (g_Settings.esp.skeleton)
		{
			static const char* k_skel_types[] = {
				"funny skeleton", "anton", "unfunny_skeleton", "egor"
			};
			row_select("##skeleton_type", "skeleton type",
			           &g_Settings.esp.skeleton_type, k_skel_types, 4);
			if (g_Settings.esp.skeleton_type < 0)
				g_Settings.esp.skeleton_type = 0;
			if (g_Settings.esp.skeleton_type > 3)
				g_Settings.esp.skeleton_type = 3;
			gap();
		}

		if (g_Settings.esp.chams_mode != 3 && g_Settings.esp.chams_mode != 4)
		{
			row_cb_color2(
				"chams",
				&g_Settings.esp.chams,
				g_Settings.esp.chams_outline_color,
				g_Settings.esp.chams_fill_color,
				"esp_chams_outline_color",
				"esp_chams_fill_color",
				"outline",
				"fill",
				true
			);
		}

		else
		{
			pad();
			ng::checkbox("chams", &g_Settings.esp.chams);
		}
		gap();

		{
			static const char* k_chams_modes[] = {
				"box", "box filled", "clipper", "shader", "engine", "mesh"
			};
			row_select("##chams_mode", "chams mode", &g_Settings.esp.chams_mode,
			           k_chams_modes, 6);
		}
		gap();

		if (g_Settings.esp.chams_mode == 3)
		{
			row_select("##chams_shader", "shader", &g_Settings.esp.chams_shader,
			           Visuals::ShaderChams::StyleNames(),
			           Visuals::ShaderChams::StyleNameCount());
			gap();
		}

		if (g_Settings.esp.chams_mode == 5)
		{
			{
				static const char* k_mesh_style[] = { "flat", "shader" };
				row_select("##mesh_style", "mesh style",
				           &g_Settings.esp.mesh_chams_style,
				           k_mesh_style, 2);
			}
			if (g_Settings.esp.mesh_chams_style < 0)
				g_Settings.esp.mesh_chams_style = 0;
			if (g_Settings.esp.mesh_chams_style > 1)
				g_Settings.esp.mesh_chams_style = 1;
			gap();

			if (g_Settings.esp.mesh_chams_style == 1)
			{
				row_select("##mesh_shader", "mesh shader",
				           &g_Settings.esp.mesh_chams_dx_mode,
				           Visuals::MeshDxShader::ModeNames(),
				           Visuals::MeshDxShader::ModeNameCount());
				if (g_Settings.esp.mesh_chams_dx_mode < 0)
					g_Settings.esp.mesh_chams_dx_mode = 0;
				if (g_Settings.esp.mesh_chams_dx_mode > 13)
					g_Settings.esp.mesh_chams_dx_mode = 13;
				gap();
			}

			row_cb_color("mesh outline", &g_Settings.esp.mesh_chams_outline,
			             g_Settings.esp.mesh_chams_outline_color,
			             "esp_mesh_outline_color");
			gap();

			if (g_Settings.esp.mesh_chams_outline)
			{
				row_select("##outline_shader", "outline shader",
				           &g_Settings.esp.mesh_chams_outline_style,
				           Visuals::MeshChams::OutlineStyleNames(),
				           Visuals::MeshChams::OutlineStyleNameCount());
				if (g_Settings.esp.mesh_chams_outline_style < 0)
					g_Settings.esp.mesh_chams_outline_style = 0;
				if (g_Settings.esp.mesh_chams_outline_style >
				    Visuals::MeshChams::OutlineStyleNameCount() - 1)
					g_Settings.esp.mesh_chams_outline_style =
						Visuals::MeshChams::OutlineStyleNameCount() - 1;
				gap();

				row_slider("##outline_fade", "outline fade",
				           &g_Settings.esp.mesh_chams_outline_fade, 0.35f, 3.0f);
				gap();
			}

			row_cb_color("occluded", &g_Settings.esp.mesh_chams_occlusion,
			             g_Settings.esp.mesh_chams_occluded_color,
			             "esp_mesh_occluded_color");
			gap();

			if (g_Settings.esp.mesh_chams_occlusion)
			{
				row_select("##occluded_shader", "occluded shader",
				           &g_Settings.esp.mesh_chams_occluded_dx_mode,
				           Visuals::MeshDxShader::ModeNames(),
				           Visuals::MeshDxShader::ModeNameCount());
				if (g_Settings.esp.mesh_chams_occluded_dx_mode < 0)
					g_Settings.esp.mesh_chams_occluded_dx_mode = 0;
				if (g_Settings.esp.mesh_chams_occluded_dx_mode > 13)
					g_Settings.esp.mesh_chams_occluded_dx_mode = 13;
				gap();
			}
		}

		if (g_Settings.esp.chams_mode == 4)
		{
			{
				static const char* engine_styles[] = {
					"default", "ghost", "wireframe", "mesh", "charwire"
				};
				row_select("##engine_style", "engine style",
				           &g_Settings.esp.engine_chams_style,
				           engine_styles, 5);
			}
			gap();

			const int est = g_Settings.esp.engine_chams_style;
			if (est == 1 || est == 3 || est == 4)
			{
				static const char* engine_colors[] = {
					"red", "green", "orange", "blue", "pink", "cyan", "white"
				};
				row_select("##engine_color", "engine color",
				           &g_Settings.esp.engine_ghost_color_idx,
				           engine_colors, 7);
				gap();
			}
		}

		const bool pf_place = Games::PhantomForces::IsActivePlace();
		const bool havoc_place = Visuals::HavocWorldEsp::IsActivePlace();

		if (pf_place)
		{
			g_Settings.esp.healthbar = false;
			g_Settings.esp.health_text = false;
			g_Settings.esp.distance = false;
			g_Settings.esp.tool = false;
			g_Settings.esp.flags = false;
			g_Settings.esp.dead_check = false;
			g_Settings.esp.body_corpse = false;
		}

		if (!pf_place)
		{
			pad();
			ng::checkbox("health bar", &g_Settings.esp.healthbar);
			gap();

			pad();
			ng::checkbox("health text", &g_Settings.esp.health_text);
			gap();

			row_cb_color("distance", &g_Settings.esp.distance,
			             g_Settings.esp.distance_color, "esp_distance_color");
			gap();

			row_cb_color("tool", &g_Settings.esp.tool,
			             g_Settings.esp.tool_color, "esp_tool_color");
			gap();

			pad();
			ng::checkbox("flags", &g_Settings.esp.flags);
			gap();
		}

		if (havoc_place)
		{
			g_Settings.esp.body_corpse = false;

			pad();
			ng::checkbox("dead check", &g_Settings.esp.dead_check);
			gap();

			pad();
			ng::checkbox("bots", &g_Settings.esp.bots);
			if (g_Settings.esp.bots)
			{
				auto& be = g_Settings.esp.bot_esp;

				row_cb_color("bot box", &be[Settings::BOT_BOX],
				             g_Settings.esp.bot_box_color, "esp_bot_box_color");
				gap();

				row_cb_color("bot name", &be[Settings::BOT_NAME],
				             g_Settings.esp.bot_name_color, "esp_bot_name_color");
				gap();

				row_cb_color("bot skeleton", &be[Settings::BOT_SKELETON],
				             g_Settings.esp.bot_skeleton_color, "esp_bot_skel_color");
				gap();

				if (g_Settings.esp.bot_chams_mode != 3)
				{
					row_cb_color2(
						"bot chams",
						&be[Settings::BOT_CHAMS],
						g_Settings.esp.bot_chams_outline_color,
						g_Settings.esp.bot_chams_fill_color,
						"esp_bot_chams_outline",
						"esp_bot_chams_fill",
						"outline",
						"fill",
						false
					);
				}

				else
				{
					pad();
					ng::checkbox("bot chams", &be[Settings::BOT_CHAMS]);
				}
				gap();

				if (be[Settings::BOT_CHAMS])
				{
					{
						static const char* k_bot_chams_modes[] = {
							"box", "box filled", "clipper", "shader"
						};
						if (g_Settings.esp.bot_chams_mode > 3)
							g_Settings.esp.bot_chams_mode = 3;
						row_select("##bot_chams_mode", "bot chams mode",
						           &g_Settings.esp.bot_chams_mode,
						           k_bot_chams_modes, 4);
					}
					gap();

					if (g_Settings.esp.bot_chams_mode == 3)
					{
						row_select("##bot_shader", "bot shader",
						           &g_Settings.esp.bot_chams_shader,
						           Visuals::ShaderChams::StyleNames(),
						           Visuals::ShaderChams::StyleNameCount());
						gap();
					}
				}

				pad();
				ng::checkbox("bot health bar", &be[Settings::BOT_HEALTHBAR]);
				gap();

				pad();
				ng::checkbox("bot health text", &be[Settings::BOT_HEALTH_TEXT]);
				gap();

				row_cb_color("bot distance", &be[Settings::BOT_DISTANCE],
				             g_Settings.esp.bot_distance_color, "esp_bot_dist_color");
				gap();

				row_cb_color("bot tool", &be[Settings::BOT_TOOL],
				             g_Settings.esp.bot_tool_color, "esp_bot_tool_color");
				gap();

				pad();
				ng::checkbox("bot flags", &be[Settings::BOT_FLAGS]);
				gap();
			}

			row_cb_color("corpses", &g_Settings.esp.corpses,
			             g_Settings.esp.corpse_color, "esp_corpse_color");
			gap();

			row_cb_color("ground loot", &g_Settings.esp.ground_loot,
			             g_Settings.esp.ground_loot_color, "esp_ground_loot_color");
			gap();

			if (g_Settings.esp.ground_loot)
			{
				pad();
				ng::checkbox("loot chams", &g_Settings.esp.loot_chams);
				gap();

				if (g_Settings.esp.loot_chams)
				{
					row_select("##loot_shader", "loot shader",
					           &g_Settings.esp.loot_chams_shader,
					           Visuals::ShaderChams::StyleNames(),
					           Visuals::ShaderChams::StyleNameCount());
					gap();
				}

				{
					static const char* k_loot_filters[] = {
						"weapons", "mags", "ammo", "attachments", "medical",
						"valuables", "tools", "electronics", "households",
						"documents", "other"
					};
					row_dropdown("##loot_filters", "loot filters",
					             g_Settings.esp.loot_filter,
					             k_loot_filters, Settings::LOOT_FILTER_COUNT);
				}
				gap();
			}

			row_cb_color("containers", &g_Settings.esp.containers,
			             g_Settings.esp.containers_color, "esp_containers_color");
			gap();

			if (g_Settings.esp.containers)
			{
				pad();
				ng::checkbox("container chams", &g_Settings.esp.containers_chams);
				gap();

				if (g_Settings.esp.containers_chams)
				{
					row_select("##crate_shader", "crate shader",
					           &g_Settings.esp.containers_chams_shader,
					           Visuals::ShaderChams::StyleNames(),
					           Visuals::ShaderChams::StyleNameCount());
					gap();
				}
			}
		}

		else if (!pf_place)
		{
			g_Settings.esp.bots = true;
			g_Settings.esp.corpses = false;
			g_Settings.esp.ground_loot = false;
			g_Settings.esp.containers = false;

			if (!g_Settings.esp.body_corpse)
			{
				pad();
				ng::checkbox("dead check", &g_Settings.esp.dead_check);
				gap();
			}

			else
			{
				g_Settings.esp.dead_check = false;
			}

			if (!g_Settings.esp.dead_check)
			{
				row_cb_color("body corpse", &g_Settings.esp.body_corpse,
				             g_Settings.esp.corpse_color, "esp_corpse_color");
				gap();
			}

			else
			{
				g_Settings.esp.body_corpse = false;
			}
		}

		row_cb_color("tracer", &g_Settings.esp.tracer,
		             g_Settings.esp.tracer_color, "esp_tracer_color");
		gap();

		if (g_Settings.esp.tracer)
		{
			static const char* k_tracer_origin[] = {
				"bottom", "center", "mouse", "top"
			};
			row_select("##tracer_origin", "tracer origin",
			           &g_Settings.esp.tracer_origin, k_tracer_origin, 4);
			gap();
		}

		row_cb_color("offscreen arrows", &g_Settings.esp.offscreen_arrows,
		             g_Settings.esp.arrow_color, "esp_arrow_color");
		gap();

		if (g_Settings.esp.offscreen_arrows)
		{
			{
				static const char* k_arrow_info[] = {
					"name", "distance", "health", "tool"
				};
				row_dropdown("##arrow_info", "arrow info",
				             g_Settings.esp.arrow_info,
				             k_arrow_info, Settings::ARROW_INFO_COUNT);
			}
			gap();

			row_slider("##arrow_size", "arrow size", &g_Settings.esp.arrow_size,
			           6.0f, 32.0f);
			gap();
			row_slider("##arrow_radius", "arrow radius",
			           &g_Settings.esp.arrow_radius, 40.0f, 500.0f);
			gap();
		}
	}
	ng::child_end();

	ImGui::SameLine(0.f, side_gap);

	if (ng::child_begin("##esp_child2", "settings", cw, avail_h, 10.f, true))
	{
		{
			static const char* k_esp_fonts[] = {
				"fredoka one", "tahoma bold", "proggy clean", "visitor", "verdana", "imgui"
			};
			row_select("##esp_font", "esp font", &g_Settings.esp.font,
			           k_esp_fonts, 6);
		}
		gap();

		row_slider("##esp_font_size", "esp font size",
		           &g_Settings.esp.font_size, 8.0f, 24.0f);
		gap();

		{
			static const char* k_box_modes[] = { "bounding", "corner", "3d" };
			row_select("##box_style", "box style", &g_Settings.esp.box_mode,
			           k_box_modes, 3);
		}
		gap();

		{
			static const char* k_bound_types[] = { "parts", "mesh" };
			row_select("##bounding_type", "bounding type",
			           &g_Settings.esp.bounding_type, k_bound_types, 2);
			if (g_Settings.esp.bounding_type < 0)
				g_Settings.esp.bounding_type = 0;
			if (g_Settings.esp.bounding_type > 1)
				g_Settings.esp.bounding_type = 1;
		}
		gap();

		{
			static const char* k_esp_outline[] = { "skeleton", "box" };
			row_dropdown("##esp_outline", "esp outline",
			             g_Settings.esp.esp_outline,
			             k_esp_outline, Settings::ESP_OUTLINE_COUNT);
		}
		gap();

		if (g_Settings.esp.skeleton_type == 0)
		{
			row_slider("##skel_thick", "skeleton thickness",
			           &g_Settings.esp.skeleton_thickness, 1.0f, 6.0f);
			gap();
		}

		row_slider("##box_thick", "box thickness",
		           &g_Settings.esp.box_thickness, 0.5f, 6.0f);
		gap();

		if (Visuals::HavocWorldEsp::IsActivePlace())
		{
			pad();
			ImGui::TextUnformatted("distance: meters (max 400)");
		}

		else
		{
			static const char* k_dist_units[] = { "studs", "meters" };
			const int prev_unit = g_Settings.esp.distance_unit;
			row_select("##dist_unit", "distance unit",
			           &g_Settings.esp.distance_unit, k_dist_units, 2);
			if (prev_unit != g_Settings.esp.distance_unit)
			{
				float k = 0.28f;
				if (g_Settings.esp.distance_unit == 1)
					g_Settings.esp.max_distance *= k;

				else if (k > 0.f)
					g_Settings.esp.max_distance /= k;
			}
			gap();

			pad();
			ng::checkbox("distance check", &g_Settings.esp.distance_check);
			gap();

			{
				bool meters = g_Settings.esp.distance_unit == 1;
				float lo = meters ? 10.f : 50.f;
				float hi = meters ? 1400.f : 5000.f;
				float d = g_Settings.esp.max_distance;
				if (d < lo) d = lo;
				if (d > hi) d = hi;
				g_Settings.esp.max_distance = d;
				row_slider(meters ? "##max_dist_m" : "##max_dist_s",
				           meters ? "max distance (meters)" : "max distance (studs)",
				           &g_Settings.esp.max_distance, lo, hi);
			}
			gap();
		}
	}
	ng::child_end();
}
