// Emulator.cpp : Defines the entry point for the console application.
//
#include <algorithm>
#include <string>
#include <stdlib.h>

#include "6502/cpu.h"
#include "6502/assemble.h"
#include "6502/video.h"
#include "6502/disk.h"
#include "debugger/debugger.h"
#include "utils/path_utils.h"

#pragma warning(disable:4996)   // disable the deprecated warnings for fopen

INITIALIZE_EASYLOGGINGPP

#define MEMORY_SIZE (64 * 1024 * 1024)
int8_t memory_buffer[MEMORY_SIZE];

static void configure_logging()
{
	// hacky for now, but remove the old logfile
	unlink("emulator.log");

	el::Configurations conf;

	conf.setToDefault();
	conf.set(el::Level::Global, el::ConfigurationType::Filename, "emulator.log");
	conf.set(el::Level::Global, el::ConfigurationType::Enabled, "true");
	conf.set(el::Level::Global, el::ConfigurationType::ToStandardOutput, "false");
	conf.set(el::Level::Global, el::ConfigurationType::Format, "%msg");
	el::Loggers::reconfigureLogger("default", conf);

	// set some options on the logger
	el::Loggers::addFlag(el::LoggingFlag::ImmediateFlush);
	el::Loggers::addFlag(el::LoggingFlag::NewLineForContainer);
	el::Loggers::addFlag(el::LoggingFlag::ColoredTerminalOutput);
}


static char *get_cmdline_option(char **start, char **end, const std::string &short_option, const std::string &long_option = "")
{
	char **iter = std::find(start, end, short_option);
	if (iter == end) {
		iter = std::find(start, end, long_option);
	}
	if ((iter != end) && (iter++ != end) ) {
		return *iter;
	}

	return nullptr;
}

static bool cmdline_option_exists(char **start, char **end, const std::string &short_option, const std::string &long_option = "")
{
	char **iter = std::find(start, end, short_option);
	if (iter == end) {
		iter = std::find(start, end, long_option);
	}
	if (iter == end) {
		return false;
	}
	return true;
}

bool dissemble_6502(const char *filename)
{
	return true;
}

int main(int argc, char* argv[])
{
	// configure logging before anything else
	configure_logging();

	memory mem(MEMORY_SIZE, memory_buffer);
	cpu_6502 cpu(mem);
	cpu.init();

	init_text_screen();
	debugger_init();
	disk_init(mem);

	//// get command line options
	//const char *assembly_file = nullptr;
	//const char *dissembly_file = nullptr;
	//assembly_file = get_cmdline_option(argv, argv+argc, "-a", "--assemble");

	//// let's test assembler
	//if (assembly_file != nullptr) {
	//	assemble_6502(assembly_file);
	//	return 0;
	//}

	//dissembly_file = get_cmdline_option(argv, argv+argc, "-d", "--disassemble");
	//if (dissembly_file != nullptr) {
	//	dissemble_6502(dissembly_file);
	//}
	
	//FILE *fp = fopen("test.bin", "rb");
	//fseek(fp, 0, SEEK_END);
	//uint32_t buffer_size = ftell(fp);
	//fseek(fp, 0, SEEK_SET);

	//// allocate the memory buffer
	//uint8_t *buffer = new uint8_t[buffer_size];
	//if (buffer == nullptr) {
	//	printf("Unable to allocate %d bytes for source code file buffer\n", buffer_size);
	//	fclose(fp);
	//	exit(-1);
	//}
	//fread(buffer, 1, buffer_size, fp);
	//fclose(fp);

	uint16_t prog_start = 0x600;
	uint16_t load_addr = 0x0;

	const char *prog_start_string = get_cmdline_option(argv, argv+argc, "-p", "--pc");
	if (prog_start_string != nullptr) {
		prog_start = (uint16_t)strtol(prog_start_string, nullptr, 16);
	}

	const char *load_addr_string = get_cmdline_option(argv, argv+argc, "-b", "--base");
	if (load_addr_string != nullptr) {
		load_addr = (uint16_t)strtol(load_addr_string, nullptr, 16);
	}

	const char *binary_file = nullptr;
	binary_file = get_cmdline_option(argv, argv+argc, "-b", "--binary");
	if (binary_file != nullptr) {
		FILE *fp = fopen(binary_file, "rb");
		if (fp == nullptr) {
			printf("Unable to open %s for reading: %d\n", binary_file, errno);
			exit(-1);
		}
		fseek(fp, 0, SEEK_END);
		uint32_t buffer_size = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		// allocate the memory buffer
		uint8_t *buffer = new uint8_t[buffer_size];
		if (buffer == nullptr) {
			printf("Unable to allocate %d bytes for source code file buffer\n", buffer_size);
			fclose(fp);
			exit(-1);
		}
		fread(buffer, 1, buffer_size, fp);
		fclose(fp);
		mem.load_memory(buffer, buffer_size, load_addr);
		cpu.set_pc(prog_start);

	// load up the ROMs
	} else {
		cpu.set_pc(mem[0xfffc] | mem[0xfffd] << 8);

		// see if there is disk imace
		const char *disk_image_filename = get_cmdline_option(argv, argv+argc, "-d", "--disk");  // will always go to drive one for now
		if (disk_image_filename != nullptr) {
			disk_insert(disk_image_filename, 1);
		}
	}
	clear_screen();
	set_raw(true);

	//cpu.load_program(0x600, buffer, buffer_size);
	while (1) {
		debugger_process(cpu, mem);
		cpu.process_opcode();
	}

	return 0;
}

