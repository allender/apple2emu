# Apple2Emu - An Apple ][ Emulator

Apple2Emu is an Apple ][ emulator.  Having started programming on the Apple ][ in the early 80's, I have always wanted to write an emulator for that system.  One goal for this emulator is to be cross platform (Windows, Mac OS, and Linux).  Given modern hardware, the need to write obfustaced code for the sake of speed is really not an issue anymore, so a second goal is to provide readable code that is more easily understood by someone looking to see how emulators can be built.  

## Building  
On Windows, Apple2Emu will build under Visual Studio 2015.  A Visual Studio 2015 project file is included.  There is also a Makefile that I have used to build under Mac OS.  I have used all command line tools on the Mac so if you want to build on the Mac, you will need to make sure that you have the command line tools installed through XCode.  I have not yet built the emulator under Linux but the provided Makefile should work (perhaps with minor modifications).

# Required Libraries
The following external libraries are currently required to build apple2emu.
* [SDL 2.X](https://www.libsdl.org/) - For cross platform graphics, key, gamepad, handling
* [SDL Image 2.X](https://www.libsdl.org/projects/SDL_image/) - For loading texture images for fonts
* [pdcurses](http://pdcurses.sourceforge.net/) - for Windows only.  Uses ncurses on mac and linux.  Used in Debugger
* [ImGUI](https://github.com/ocornut/imgui) - currently included in apple2emu repo. Used for ingame menus
* [NativeFileDialog](https://github.com/mlabbe/nativefiledialog) - currently included in apple2emu repo.  Used for ingame menus
* [Glew](http://glew.sourceforge.net/) -- GL Extension wrangler library.  Needed to provide runtime glue for application to host GL function calls.

I am considering moving to Visual Studio Code for all development as that IDE is available on all the platforms that I am targeting and having a common way of editing and building on those platforms seems desirable to me.  Until that point in time, those using VS may need to adjust paths in the project configuration to their SDL, etc. installations.  The Makefile will also likely need to be adjusted for other platforms.

## Running the emulator
Currenlty, the emulator needs to be started with the appropriate command line options to load a disk into the first disk drive.  Using the --disk <filename> argument on the command line, the emulator will start with that disk image in the disk drive.

Once in the game, the small number of currently supported features can be changed by using the in-game menu.  I have decided to use imgui as the interface as it's (again) cross platform, small, fast, and just works.  Hitting F1 in the emulator will bring up a small nubmer of options that you can change (screen color and mounted disks).  F2 will bring up just a menu that allows you to control which disks are mounted.

## Integrated 6502 Debugger
There is an integrated 6502 debugger in apple2emu.  Currently, this debugger will run in the console window from which the emulator was started.  I intent to move the debugger to imgui in the future, but for now, it will remain in the console.  In order to support the features I wanted, I require pdcurses (or ncurses) for screen/window maniuplation for the debugger.  Hitting F12 in the emulator will start the debugger.  Currently there is a memory view, register view, breakpoint list, and diassembly.  The debugger currently supports the following commands (curently modelled after gcc:

* 's' or 'step' to single step when broken
* 'c' or 'cont' to continue execution
* 'b' or 'break'  to set read/write breakpoint on memory location
* 'ww' or 'wwatch' to set write watchpoint at a memory location
* 'rw' or 'rwatch' to set read watchpoint at a memory location
* 'enable'/'disable' to enable or disable break or watchpoints
* 'del' to delete break or watch points
* 'x' or 'examine' to view memory content of the specified address
* 'trace' to start a trace dump to file (could create metric ton of output)
* 'q' or 'quit' quits the emulator
* 


## License
Apple2Emu is covered under the MIT license.  A copy of this license is available in the same directory as this file under the name License.txt  

