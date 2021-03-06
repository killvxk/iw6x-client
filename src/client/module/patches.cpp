#include <std_include.hpp>

#include "command.hpp"
#include "game_console.hpp"
#include "game/game.hpp"
#include "game/dvars.hpp"

#include "utils/hook.hpp"
#include "utils/nt.hpp"

namespace
{
	utils::hook::detour live_get_local_client_name_hook;

	const char* live_get_local_client_name()
	{
		return game::native::Dvar_FindVar("name")->current.string;
	}

	utils::hook::detour dvar_register_int_hook;

	game::native::dvar_t* dvar_register_int(const char* dvarName, int value, int min, int max, unsigned int flags,
		const char* description)
	{
		// enable map selection in extinction
		if (!strcmp(dvarName, "extinction_map_selection_enabled"))
		{
			value = true;
		}

		// enable extra loadouts
		else if (!strcmp(dvarName, "extendedLoadoutsEnable"))
		{
			value = true;
		}

		// show all in-game store items
		else if (strstr(dvarName, "igs_"))
		{
			value = true;
		}

		return dvar_register_int_hook.invoke<game::native::dvar_t*>(dvarName, value, min, max, flags, description);
	}

	const auto g_gravity_stub = utils::hook::assemble([](utils::hook::assembler& a)
	{
		a.push(rax);

		a.mov(rax, qword_ptr(reinterpret_cast<int64_t>(&dvars::g_gravity)));
		a.mov(rax, dword_ptr(rax, 0x10));
		a.mov(dword_ptr(rbx, 0x5C), eax);
		a.mov(eax, ptr(rbx, 0x33E8));
		a.mov(ptr(rbx, 0x25C), eax);

		a.pop(rax);

		a.jmp(0x1403828D5);
	});

	const auto g_speed_stub = utils::hook::assemble([](utils::hook::assembler& a)
	{
		a.push(rax);

		a.mov(rax, qword_ptr(reinterpret_cast<int64_t>(&dvars::g_speed)));
		a.mov(rax, dword_ptr(rax, 0x10));
		a.mov(dword_ptr(rdi, 0x60), eax);
		a.add(eax, ptr(rdi, 0xEA0));

		a.pop(rax);

		a.jmp(0x140383796);
	});
}

class patches final : public module
{
public:
	void post_unpack() override
	{
		// add quit command
		command::add("quit", [](command::params&)
		{
			utils::hook::invoke<void>(SELECT_VALUE(0x1403BDDD0, 0x140414920));
		});

		// add quit_hard command
		command::add("quit_hard", [](command::params&)
		{
			utils::nt::raise_hard_exception();
		});

		//Patch r_znear "does not like being forced to be set at its default value fucks with rendering"
		//game::native::Dvar_RegisterInt("r_znear", 4, 0, 4, 0x1, "near Z clip plane distance"); // keeping it at for as it can be used as wall hacks if set more

		// Patch bg_compassshowenemies
		// Keeping it so it cant be used for uav cheats for people
		game::native::Dvar_RegisterInt("bg_compassShowEnemies", 0, 0, 0, 0x1, "Whether enemies are visible on the compass at all times");

		// igs_announcer
		// set it to 3 to display both voice dlc announcers did only show 1
		game::native::Dvar_RegisterInt("igs_announcer", 3, 3, 3, 0x1, "Show Announcer Packs. (Bitfield representing which announcer paks to show)");

		// patch com_maxfps
		// changed max value from 85 -> 1000
		game::native::Dvar_RegisterInt("com_maxfps", 85, 0, 1000, 0x1, "Cap frames per second");

		// patch cg_fov
		// changed max value from 80.0f -> 120.f
		game::native::Dvar_RegisterFloat("cg_fov", 65.0f, 65.0f, 120.0f, 0x1, "The field of view angle in degrees");

		// patch r_vsync
		// changed default value from true -> false. There are some fps issues with vsync at least on 144hz monitors.
		game::native::Dvar_RegisterBool("r_vsync", false, 0x1,
			"Enable v-sync before drawing the next frame to avoid 'tearing' artifacts.");

		// add dvarDump command
		command::add("dvarDump", [](command::params&)
		{
			game_console::print(
				7, "================================ DVAR DUMP ========================================\n");
			int i;
			for (i = 0; i < *game::native::dvarCount; i++)
			{
				if (game::native::sortedDvars[i] && game::native::sortedDvars[i]->name)
				{
					game_console::print(7, "%s\n", game::native::sortedDvars[i]->name);
				}
			}
			game_console::print(7, "\n%i dvar indexes\n", i);
			game_console::print(
				7, "================================ END DVAR DUMP ====================================\n");
		});

		// add commandDump command
		command::add("commandDump", [](command::params&)
		{
			game_console::print(
				7, "================================ COMMAND DUMP =====================================\n");
			game::native::cmd_function_s* cmd = (*game::native::cmd_functions);
			int i = 0;
			while (cmd)
			{
				if (cmd->name)
				{
					game_console::print(7, "%s\n", cmd->name);
					i++;
				}
				cmd = cmd->next;
			}
			game_console::print(7, "\n%i command indexes\n", i);
			game_console::print(
				7, "================================ END COMMAND DUMP =================================\n");
		});

		if (game::is_mp())
		{
			patch_mp();
		}
		else if (game::is_sp())
		{
			patch_sp();
		}
	}

	void patch_mp() const
	{
		// Use name dvar and add "saved" flags to it
		utils::hook::set<uint8_t>(0x1402C836D, 0x01);
		live_get_local_client_name_hook.create(0x1404FDAA0, &live_get_local_client_name);

		// Unlock all patches/cardtitles and exclusive items/camos
		utils::hook::set(0x140402B10, 0xC301B0); // LiveStorage_IsItemUnlockedFromTable_LocalClient
		utils::hook::set(0x140402360, 0xC301B0); // LiveStorage_IsItemUnlockedFromTable
		utils::hook::set(0x1404A94E0, 0xC301B0); // GetIsCardTitleUnlocked

		// Enable DLC items, extra loadouts and map selection in extinction
		dvar_register_int_hook.create(0x1404EE270, &dvar_register_int);

		// Implement gravity dvar
		utils::hook::nop(0x1403828C8, 13);
		utils::hook::jump(0x1403828C8, g_gravity_stub, true);
		dvars::g_gravity = game::native::Dvar_RegisterInt("g_gravity", 800, 0, 1000, 0, "Game gravity in inches per second per second");

		// Implement speed dvar
		utils::hook::nop(0x140383789, 13);
		utils::hook::jump(0x140383789, g_speed_stub, true);
		dvars::g_speed = game::native::Dvar_RegisterInt("g_speed", 190, 0, 999, 0, "Maximum player speed");
	}

	void patch_sp() const
	{
		// SP doesn't initialize WSA
		WSADATA wsa_data;
		WSAStartup(MAKEWORD(2, 2), &wsa_data);
	}
};

REGISTER_MODULE(patches);
