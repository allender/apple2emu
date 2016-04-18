#include "debugger/debugger.h"
#include "6502/video.h"
#include "6502/curses.h"
#include "panel.h"


bool Debugger_active = false;
bool Debugger_stopped = false;
PANEL *Debugger_panel;

// processes a command for the debugger
static void debugger_process_commands()
{
}

void debugger_enter()
{
	Debugger_active = true;
	Debugger_stopped = true;
	set_raw(false);

	// show the debugger panel
	int err = show_panel(Debugger_panel);
	err = top_panel(Debugger_panel);
	refresh();
}

void debugger_exit()
{
	Debugger_active = false;
	set_raw(true);
}

void debugger_init()
{
	Debugger_active = false;
	Debugger_stopped = false;
	Debugger_panel = new_panel(stdscr);
	int err = hide_panel(Debugger_panel);
}

void debugger_process()
{
	if (Debugger_active == false) {
		return;
	}

	// if we are stopped, process any commands here in tight loop
	if (Debugger_stopped) {
		debugger_process_commands();
	}
}