#include "ModuleSystem.h"
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif
#include "OptUtils.h"
#include "StringUtils.h"

int main(int argc, char **argv)
{
#ifdef _WIN32
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 0x71);
	SetConsoleTitle("MS++ -- Building");
	system("cls");
#else
	system("clear");
#endif

	OptUtils opt(argc, argv);
	unsigned long long flags = 0;

	if (opt.Has("-strict")) flags |= MSF_STRICT;
	if (opt.Has("-skip-id-files")) flags |= MSF_SKIP_ID_FILES;
	if (opt.Has("-list-resources")) flags |= MSF_LIST_RESOURCES;
	if (opt.Has("-hide-global-vars")) flags |= MSF_OBFUSCATE_GLOBAL_VARS;
	if (opt.Has("-hide-scripts")) flags |= MSF_OBFUSCATE_SCRIPTS;
	if (opt.Has("-list-obfuscated-scripts")) flags |= MSF_LIST_OBFUSCATED_SCRIPTS;
	if (opt.Has("-hide-dialog-states")) flags |= MSF_OBFUSCATE_DIALOG_STATES;
	if (opt.Has("-hide-tags")) flags |= MSF_OBFUSCATE_TAGS;
	if (opt.Has("-compile-data")) flags |= MSF_COMPILE_MODULE_DATA;
	if (opt.Has("-list-unreferenced-scripts")) flags |= MSF_LIST_UNREFERENCED_SCRIPTS;
	if (opt.Has("-no-warnings")) flags |= MSF_DISABLE_WARNINGS;
	if (opt.Has("-rusmod_rebalanser")) flags |= MSF_RUSMOD_REBALANSER;

	std::string in_path;

	if (opt.Has("-in-path")) in_path = opt.Get("-in-path");
	else
	{
		char buf[1024];

#if defined _WIN32
		if (!GetCurrentDirectory(1024, buf))
#else
		if (!getcwd(buf, 1024))
#endif
			std::cout << "Error getting current directory." << std::endl;

		in_path = buf;
	}

	//in_path = "E:\\WarbandModuleSystem";

	std::string out_path;

	if (opt.Has("-out-path")) out_path = opt.Get("-out-path");

	auto leftover = opt.Leftover();

	for (auto & it : leftover) std::cout << "Unrecognized option: " << it << std::endl;

	if (!leftover.empty()) return EXIT_FAILURE;

	ModuleSystem ms(in_path, out_path);

	ms.Compile(flags);
	return EXIT_SUCCESS;
}
