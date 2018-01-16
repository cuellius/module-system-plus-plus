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

	if (opt.Has("-strict")) flags |= msf_strict;
	if (opt.Has("-skip-id-files")) flags |= msf_skip_id_files;
	if (opt.Has("-list-resources")) flags |= msf_list_resources;
	if (opt.Has("-hide-global-vars")) flags |= msf_obfuscate_global_vars;
	if (opt.Has("-hide-scripts")) flags |= msf_obfuscate_scripts;
	if (opt.Has("-list-obfuscated-scripts")) flags |= msf_list_obfuscated_scripts;
	if (opt.Has("-hide-dialog-states")) flags |= msf_obfuscate_dialog_states;
	if (opt.Has("-hide-tags")) flags |= msf_obfuscate_tags;
	if (opt.Has("-compile-data")) flags |= msf_compile_module_data;
	if (opt.Has("-list-unreferenced-scripts")) flags |= msf_list_unreferenced_scripts;
	if (opt.Has("-no-warnings")) flags |= msf_disable_warnings;
	if (opt.Has("-rusmod_rebalanser")) flags |= msf_rusmod_rebalanser;

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

	std::vector<std::string> leftover = opt.Leftover();

	for (auto it = leftover.begin(); it != leftover.end(); ++it) std::cout << "Unrecognized option: " << *it << std::endl;

	if (leftover.size()) return EXIT_FAILURE;

	ModuleSystem ms(in_path, out_path);

	ms.Compile(flags);
	return EXIT_SUCCESS;
}
