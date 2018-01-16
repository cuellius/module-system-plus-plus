#include "ModuleSystem.h"

std::string encode_str(const std::string &str)
{
	std::string text = str;
	return replace(replace(text, ' ', '_'), '\t', '_');
}

std::string encode_res(const std::string &str)
{
	std::string text = str;
	return replace(replace(trim(text), ' ', '_'), '\t', '_');
}

std::string encode_full(const std::string &str)
{
	std::string text = encode_str(str);
	std::string result;

	for (size_t i = 0; i < text.size(); ++i)
	{
		char c = text[i];
		switch (c)
		{
		case ',':
		case '|':
			break;
		case '\'':
		case '`':
		case '(':
		case ')':
		case '-':
			result.push_back('_');
			break;
		default:
			result.push_back(c);
			break;
		}
	}

	return result;
}

std::string encode_strip(const std::string &str)
{
	std::string text = str;
	trim(text);
	return encode_full(text);
}

std::string encode_id(const std::string &str)
{
	std::string text = encode_full(str);
	std::transform(text.begin(), text.end(), text.begin(), ::tolower);
	return text;
}

ModuleSystem::ModuleSystem(const std::string &in_path, const std::string &out_path) : m_input_path(in_path), m_output_path(out_path)
{
#if defined _WIN32
	m_console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleScreenBufferInfo(m_console_handle, &m_console_info);
#endif
	LoadPythonInterpreter();
}

ModuleSystem::~ModuleSystem()
{
	//UnloadPythonInterpreter();
}

void ModuleSystem::LoadPythonInterpreter()
{
	Py_Initialize();

	PyObject *obj = PySys_GetObject("path");
	Py_XINCREF(obj);

	CPyList sys_path(obj);
	CPyString path(m_input_path);

	sys_path.Append(path);
}

void ModuleSystem::UnloadPythonInterpreter()
{
	Py_Finalize();
}

void ModuleSystem::SetConsoleColor(int color)
{
#if defined _WIN32
	SetConsoleTextAttribute(m_console_handle, (color & 0xF) | (m_console_info.wAttributes & 0xF0) );
#else
	std::cout << "\033[0;" << color << "m";
#endif
}

void ModuleSystem::ResetConsoleColor()
{
#if defined _WIN32
	SetConsoleTextAttribute(m_console_handle, m_console_info.wAttributes);
#else
	std::cout << "\033[0m";
#endif
}

void ModuleSystem::Compile(unsigned long long flags)
{
	m_flags = flags;

	if (m_input_path.length() && m_input_path[m_input_path.length() - 1] != PATH_SEPARATOR) m_input_path.push_back(PATH_SEPARATOR);

#if defined _WIN32
	LARGE_INTEGER frequency, t1, t2;

	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&t1);
#else
	timeval t1, t2;

	gettimeofday(&t1, NULL);
#endif

	try
	{
		if (!(m_flags & msf_skip_id_files))
		{
			m_pass = 1;
			DoCompile();
			UnloadPythonInterpreter();
			LoadPythonInterpreter();
		}

		m_pass = 2;
		DoCompile();
	}
	catch (CPyException &e)
	{
		SetConsoleColor(COLOR_GREEN);
		std::cout << "PYTHON ERROR: ";
		ResetConsoleColor();
		std::cout << e.GetText() << std::endl;
		SetConsoleColor(COLOR_MAGENTA);
		std::cout << "COMPILATION ABORTED" << std::endl;
		ResetConsoleColor();
		return;
	}
	catch (CompileException &e)
	{
		SetConsoleColor(COLOR_RED);
		std::cout << "ERROR: ";
		ResetConsoleColor();
		std::cout << e.GetText() << std::endl;
		SetConsoleColor(COLOR_MAGENTA);
		std::cout << "COMPILATION ABORTED" << std::endl;
		ResetConsoleColor();
		return;
	}

#if defined _WIN32
	QueryPerformanceCounter(&t2);
#else
	gettimeofday(&t2, NULL);
#endif

	if (m_flags & msf_list_resources)
	{
		std::ofstream res_stream(m_output_path + "resource_usage.txt");

		for (auto res_type_it = m_resources.begin(); res_type_it != m_resources.end(); ++res_type_it)
		{
			std::string res_type;

			switch (res_type_it->first)
			{
			case res_mesh:
				res_type = "Meshes";
				break;
			case res_material:
				res_type = "Materials";
				break;
			case res_skeleton:
				res_type = "Skeleton Models";
				break;
			case res_body:
				res_type = "Bodies";
				break;
			case res_animation:
				res_type = "Skeleton Animations";
				break;
			}

			res_stream << "== " << res_type << " ==" << std::endl;

			for (auto res_it = res_type_it->second.begin(); res_it != res_type_it->second.end(); ++res_it) 
				res_stream << res_it->first << ' ' << res_it->second << std::endl;

			res_stream << std::endl;
		}
	}

#if defined _WIN32
	double time = (t2.QuadPart - t1.QuadPart) * 1000.0 / frequency.QuadPart;
#else
	double time = ((t2.tv_sec - t1.tv_sec) * 1000.0) + (t2.tv_usec - t1.tv_usec) / 1000.0;
#endif

	std::cout << std::endl << "Compile time: " << time << "ms" << std::endl;

#ifdef _WIN32
	SetConsoleTitle("MS++ -- Finished");
#endif
}

void ModuleSystem::DoCompile()
{
	if (m_output_path.empty()) m_output_path = CPyModule("module_info").GetAttr("export_dir").Str();

	trim(m_output_path);

	if (m_output_path.length() && m_output_path[m_output_path.length() - 1] != PATH_SEPARATOR) m_output_path.push_back(PATH_SEPARATOR);

	if (m_pass == 2)
	{
		std::cout << "Initializing compiler..." << std::endl;

		memset(m_operations, 0, max_num_opcodes * sizeof(unsigned int));
		memset(m_operation_depths, 0, max_num_opcodes * sizeof(int));

		m_operation_depths[3] = -1; // try_end
		m_operation_depths[4] = 1; // try_begin
		m_operation_depths[6] = 1; // try_for_range
		m_operation_depths[7] = 1; // try_for_range_backwards
		m_operation_depths[11] = 1; // try_for_parties
		m_operation_depths[12] = 1; // try_for_agents
		m_operation_depths[15] = 1; // try_for_attached_parties (WSE)
		m_operation_depths[16] = 1; // try_for_active_players (WSE)
		m_operation_depths[17] = 1; // try_for_prop_instances (WSE)
		m_operation_depths[18] = 1; // try_for_dict_keys (WSE)

		CPyModule header_operations("header_operations");
		CPyIter lhs_operations_iter = header_operations.GetAttr("lhs_operations").GetIter();
		CPyIter ghs_operations_iter = header_operations.GetAttr("global_lhs_operations").GetIter();
		CPyIter cf_operations_iter = header_operations.GetAttr("can_fail_operations").GetIter();

		while (lhs_operations_iter.HasNext()) m_operations[OPCODE(lhs_operations_iter.Next().AsLong())] |= optype_lhs;
		while (ghs_operations_iter.HasNext()) m_operations[OPCODE(ghs_operations_iter.Next().AsLong())] |= optype_ghs;
		while (cf_operations_iter.HasNext()) m_operations[OPCODE(cf_operations_iter.Next().AsLong())] |= optype_cf;

		if (!(m_flags * msf_obfuscate_global_vars))
		{
			std::ifstream global_var_stream("variables.txt");

			if (!global_var_stream.is_open()) global_var_stream.open(m_output_path + "variables.txt");

			if (global_var_stream.is_open())
			{
				int i = 0;

				while (global_var_stream)
				{
					std::string global_var;

					global_var_stream >> global_var;

					if (!global_var.empty() && m_global_vars.find(global_var) == m_global_vars.end())
					{
						m_global_vars[global_var].index = i++;
						m_global_vars[global_var].compat = true;
					}
				}
			}
		}

		std::cout << "Loading modules..." << std::endl;
	}
	else
		std::cout << "Generating id files..." << std::endl;

	m_animations = AddModule("animations", "animations", "anim", 25);
	m_dialogs = AddModule("dialogs", "dialogs", "");
	m_factions = AddModule("factions", "fac", 6);
	m_game_menus = AddModule("game_menus", "game_menus", "mnu", "menus", "menu", 12);
	m_info_pages = AddModule("info_pages", "ip");
	m_items = AddModule("items", "itm", 4);
	m_map_icons = AddModule("map_icons", "icon", 18);
	m_meshes = AddModule("meshes", "mesh", 20);
	m_music = AddModule("music", "tracks", "track", 23);
	m_mission_templates = AddModule("mission_templates", "mission_templates", "mt", "mission_templates", "mst", 11);
	m_particle_systems = AddModule("particle_systems", "psys", 14);
	m_parties = AddModule("parties", "p", 9);
	m_party_templates = AddModule("party_templates", "pt", 8);
	m_postfx = AddModule("postfx", "postfx_params", "pfx", "postfx_params", "pfx");
	m_presentations = AddModule("presentations", "prsnt", 21);
	m_quests = AddModule("quests", "qst", 7);
	m_scene_props = AddModule("scene_props", "spr", 15);
	m_scenes = AddModule("scenes", "scn", 10);
	m_scripts = AddModule("scripts", "script", 13);
	m_simple_triggers = AddModule("simple_triggers", "");
	m_skills = AddModule("skills", "skl", 19);
	m_skins = AddModule("skins", "");
	m_sounds = AddModule("sounds", "snd", 16);
	m_strings = AddModule("strings", "str", 3);
	m_tableau_materials = AddModule("tableau_materials", "tableaus", "tableau", 24);
	m_triggers = AddModule("triggers", "");
	m_troops = AddModule("troops", "trp", 5);

	if (m_flags & msf_compile_module_data)
	{
		m_flora_kinds = AddModule("flora_kinds", "fauna_kinds", "");
		m_skyboxes = AddModule("skyboxes", "skyboxes", "");
		m_ground_specs = AddModule("ground_specs", "ground_specs", "");
	}

	if (m_pass == 2)
	{
		WriteStrings();
		WriteSkills();
		WriteMusic();
		WriteAnimations();
		WriteMeshes();
		WriteSounds();
		WriteSkins();
		WriteMapIcons();
		WriteFactions();
		WriteItems();
		WriteScenes();
		WriteTroops();
		WriteParticleSystems();
		WriteSceneProps();
		WriteTableaus();
		WritePresentations();
		WritePartyTemplates();
		WriteParties();
		WriteQuests();
		WriteInfoPages();
		WriteScripts();
		WriteMissionTemplates();
		WriteMenus();
		WriteSimpleTriggers();
		WriteTriggers();
		WriteDialogs();
		WritePostEffects();

		if (m_flags & msf_compile_module_data)
		{
			WriteFloraKinds();
			WriteSkyboxes();
			WriteGroundSpecs();
		}

		WriteQuickStrings();
		WriteGlobalVars();

		for (auto it = m_global_vars.begin(); it != m_global_vars.end(); ++it)
		{
			if (!it->second.compat && it->second.assignments == 0) Warning(wl_warning, "usage of unassigned global variable $" + it->first);
			if (!it->second.compat && it->second.usages == 0) Warning(wl_warning, "unused global variable $" + it->first);
		}

		if (m_flags & msf_list_unreferenced_scripts)
		{
			std::map<std::string, int> &uses_map = m_uses["script"];

			for (auto it = uses_map.begin(); it != uses_map.end(); ++it)
			{
				if (!it->second && it->first.find("game_") && it->first.find("wse_")) Warning(wl_warning, "unreferenced script " + it->first);
			}
		}
	}
};

CPyList ModuleSystem::AddModule(const std::string &module_name, const std::string &list_name, const std::string &prefix, const std::string &id_name, const std::string &id_prefix, int tag)
{
	std::string module_name_full = "module_" + module_name;
	CPyModule module(module_name_full);
	CPyList list = module.GetAttr(list_name);

	if (!prefix.empty())
	{
		int num_entries = (int)list.Size();
		std::vector<std::string> ids(num_entries);

		for (int i = 0; i < num_entries; ++i)
		{
			CPyObject item = list[i];
			std::string name;

			if (item.IsTuple() || item.IsList()) 
				name = item[0].AsString();
			else 
				Warning(wl_critical, "unrecognized list format for " + list_name, module_name_full);

			std::transform(name.begin(), name.end(), name.begin(), ::tolower);

			if (m_pass == 2)
			{
				if (m_ids[prefix].find(name) == m_ids[prefix].end())
					m_ids[prefix][name] = i;
				else
					Warning(wl_warning, "duplicate entry " + prefix + "_" + name, module_name_full);

				m_uses[prefix][name] = 0;
			}

			ids[i] = name;
		}

		if (m_pass == 1 && !(m_flags & msf_skip_id_files))
		{
			std::ofstream stream(m_input_path + "ID_" + id_name + ".py");

			for (int i = 0; i < num_entries; ++i) stream << id_prefix << "_" << ids[i] << " = " << i << std::endl;
		}

		if (m_pass == 2 && tag > 0 && ((!(m_flags & msf_obfuscate_tags)) || prefix == "str"))
			m_tags[prefix] = (unsigned long long)tag << 56;
	}

	return list;
};

CPyList ModuleSystem::AddModule(const std::string &module_name, const std::string &list_name, const std::string &prefix, int tag)
{
	return AddModule(module_name, list_name, prefix, module_name, prefix, tag);
};

CPyList ModuleSystem::AddModule(const std::string &module_name, const std::string &prefix, int tag)
{
	return AddModule(module_name, module_name, prefix, tag);
};

int ModuleSystem::GetId(const std::string &type, const CPyObject &obj, const std::string &context)
{
	if (obj.IsString())
	{
		std::string prefix = type;
		std::string str = obj.AsString();

		std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::tolower);
		std::transform(str.begin(), str.end(), str.begin(), ::tolower);

		std::string value = str.substr(0, prefix.length()) != prefix ? str : str.substr(prefix.length() + 1);

		if (m_ids.find(prefix) == m_ids.end()) Warning(wl_error, "unrecognized identifier prefix " + prefix, context);
		if (m_ids[prefix].find(value) == m_ids[prefix].end()) Warning(wl_error, "unrecognized identifier " + str, context);

		return m_ids[prefix][value];
	}
	else if (obj.IsLong())
		return (long)obj.AsLong();

	Warning(wl_critical, "unrecognized identifier type " + (std::string)obj.Type().Str() + " for " + (std::string)obj.Str(), context);
	return -1;
}

unsigned long long ModuleSystem::GetOperandId(const CPyObject &obj, const std::string &context)
{
	if (obj.IsString())
	{
		std::string str = obj.AsString();
		ssize_t underscore_pos = str.find('_');

		if (underscore_pos < 0) Warning(wl_error, "invalid identifier " + str, context);

		std::string prefix = str.substr(0, underscore_pos);
		std::string value = str.substr(underscore_pos + 1);

		std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::tolower);
		std::transform(value.begin(), value.end(), value.begin(), ::tolower);

		if (m_ids.find(prefix) == m_ids.end()) Warning(wl_error, "unrecognized identifier prefix " + prefix, context);
		if (m_ids[prefix].find(value) == m_ids[prefix].end()) Warning(wl_error, "unrecognized identifier " + str, context);

		m_uses[prefix][value]++;
		return m_ids[prefix][value] | m_tags[prefix];
	}
	else if (obj.IsLong())
		return obj.AsLong();

	Warning(wl_critical, "unrecognized identifier type " + (std::string)obj.Type().Str() + " for " + (std::string)obj.Str(), context);
	return -1;
}

std::string ModuleSystem::GetResource(const CPyObject &obj, int resource_type, const std::string &context)
{
	if (obj.IsString())
	{
		std::string resource_name = encode_res(obj.AsString());

		if (resource_name != "0" && resource_name != "none")
		{
			if (m_resources[resource_type].find(resource_name) == m_resources[resource_type].end()) m_resources[resource_type][resource_name] = 0;

			m_resources[resource_type][resource_name]++;
		}

		return resource_name;
	}
	else if (obj.IsLong())
		return obj.Str();

	Warning(wl_critical, "unrecognized resource type " + (std::string)obj.Type().Str() + " for " + (std::string)obj.Str(), context);
	return "0";
}

long long ModuleSystem::ParseOperand(const CPyObject &statement, int pos)
{
	CPyObject operand = statement[pos];

	if ((operand.IsTuple() || operand.IsList()) && operand.Len() == 1) operand = operand[0];

	if (operand.IsString())
	{
		std::string str = operand.AsString();

		if (str[0] == ':')
		{
			std::string value = str.substr(1);
			int index;

			if (m_local_vars.find(value) == m_local_vars.end())
			{
				index = (int)m_local_vars.size();
				m_local_vars[value].index = index;
				m_local_vars[value].assignments = 1;
				m_local_vars[value].usages = 0;

				if (pos != 1 || !(m_operations[OPCODE(statement[0].AsLong())] & optype_lhs))
				{
					Warning(wl_error, "usage of unassigned local variable :" + value, m_cur_context + ", statement " + itostr(m_cur_statement));
					m_local_vars[value].usages = 1;
				}
			}
			else
			{
				if (pos == 1 && m_operations[OPCODE(statement[0].AsLong())] & optype_lhs)
					m_local_vars[value].assignments++;
				else
					m_local_vars[value].usages++;

				index = m_local_vars[value].index;
			}

			if (m_local_vars.size() > 128) Warning(wl_error, "maximum amount of local variables (128) exceeded", m_cur_context + ", statement " + itostr(m_cur_statement));

			return index | opmask_local_variable;
		}
		else if (str[0] == '$')
		{
			std::string value = str.substr(1);
			int index;

			if (m_global_vars.find(value) == m_global_vars.end())
			{
				index = (int)m_global_vars.size();
				m_global_vars[value].index = index;

				if (pos == 1 && m_operations[OPCODE(statement[0].AsLong())] & (optype_lhs | optype_ghs))
					m_global_vars[value].assignments = 1;
				else
					m_global_vars[value].usages = 1;
			}
			else
			{
				if (pos == 1 && m_operations[OPCODE(statement[0].AsLong())] & (optype_lhs | optype_ghs))
					m_global_vars[value].assignments++;
				else
					m_global_vars[value].usages++;

				m_global_vars[value].compat = false;
				index = m_global_vars[value].index;
			}

			return index | opmask_global_variable;
		}
		else if (str[0] == '@')
		{
			std::string id = encode_full(str.substr(1));
			std::string text = encode_str(str.substr(1));
			size_t auto_id_len = std::min<size_t>(20, text.length());
			std::string auto_id;

			do
			{
				auto_id = "qstr_" + id.substr(0, auto_id_len++);
			}
			while (auto_id_len <= id.length() && (m_quick_strings.find(auto_id) != m_quick_strings.end() && m_quick_strings[auto_id].value != text));

			if (auto_id_len > id.length())
			{
				std::string new_auto_id = auto_id;
				int i = 1;

				while (m_quick_strings.find(new_auto_id) != m_quick_strings.end() && m_quick_strings[new_auto_id].value != text)
				{
					std::ostringstream oss;

					oss << auto_id << i++;
					new_auto_id = oss.str();
				}

				auto_id = std::move(new_auto_id);
			}

			if (m_quick_strings.find(auto_id) == m_quick_strings.end())
			{
				int size = (int)m_quick_strings.size();

				m_quick_strings[auto_id].index = size;
				m_quick_strings[auto_id].value = std::move(text);
			}

			return m_quick_strings[auto_id].index | opmask_quick_string;
		}
		else
		{
			return GetOperandId(operand, m_cur_context + ", statement " + itostr(m_cur_statement));
		}
	}
	else if (operand.IsLong())
		return operand.AsLong();
	else if (operand.IsFloat())
		return (long long)((double)operand.AsFloat());

	Warning(wl_critical, "unrecognized operand type " + (std::string)operand.Type().Str() + " for " + (std::string)operand.Str(), m_cur_context + ", statement " + itostr(m_cur_statement));
	return -1;
}

void ModuleSystem::PrepareModule(const std::string &name)
{
	std::cout << "Compiling " << name << "..." << std::endl;
}

void ModuleSystem::Warning(int level, const std::string &text, const std::string &context)
{
	std::string error = text;

	if (!context.empty()) error += " at " + context;

	if (level == wl_critical || (level == wl_error && m_flags & msf_strict)) throw CompileException(error);

	if(!(m_flags & msf_disable_warnings))
	{
		if (level == wl_warning)
		{
			SetConsoleColor(COLOR_YELLOW);
			std::cout << "WARNING: ";
			ResetConsoleColor();
			std::cout << error << std::endl;
		}
		else if (level == wl_error)
		{
			SetConsoleColor(COLOR_RED);
			std::cout << "ERROR: ";
			ResetConsoleColor();
			std::cout << error << std::endl;
		}
	}
}

void ModuleSystem::WriteAnimations()
{
	PrepareModule("animations");

	std::ofstream stream(m_output_path + "actions.txt");
	CPyIter iter = m_animations.GetIter();

	stream << m_animations.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject animation = iter.Next();
		std::string name = encode_str(animation[0].AsString());

		stream << ' ' << name << ' ';

		stream << animation[1] << ' ';
		stream << animation[2] << ' ';

		int num_sequences = (int)animation.Len() - 3;

		stream << num_sequences << ' ';

		for (int i = 0; i < num_sequences; ++i)
		{
			CPyObject sequence = animation[i + 3];

			stream << std::endl << "  ";
			stream << sequence[0] << ' ';
			stream << GetResource(sequence[1], res_animation, name) << ' ';
			stream << sequence[2] << ' ';
			stream << sequence[3] << ' ';
			stream << sequence[4] << ' ';

			if (sequence.Len() > 5)
				stream << sequence[5] << ' ';
			else
				stream << "0 ";

			if (sequence.Len() > 6)
			{
				stream << sequence[6][0] << ' ';
				stream << sequence[6][1] << ' ';
				stream << sequence[6][2] << ' ';
			}
			else
			{
				stream << "0.0 ";
				stream << "0.0 ";
				stream << "0.0 ";
			}

			if (sequence.Len() > 7)
				stream << sequence[7] << ' ';
			else
				stream << "0.0 ";
		}

		stream << std::endl;
	}
}

void ModuleSystem::WriteDialogs() // TODO: optimize
{
	PrepareModule("dialogs");
	std::ofstream states_stream(m_output_path + "dialog_states.txt");

	std::map<std::string, int> states;
	int num_states = 15;
	std::string default_states[] = {
		"start",
		"party_encounter",
		"prisoner_liberated",
		"enemy_defeated",
		"party_relieved",
		"event_triggered",
		"close_window",
		"trade",
		"exchange_members",
		"trade_prisoners",
		"buy_mercenaries",
		"view_char",
		"training",
		"member_chat",
		"prisoner_chat",
	};

	for (int i = 0; i < num_states; i++)
	{
		states[default_states[i]] = i;
		states_stream << default_states[i] << std::endl;
	}

	std::ofstream stream(m_output_path + "conversation.txt");
	CPyIter	iter = m_dialogs.GetIter();
	std::map<std::string, std::string> dialog_ids;

	stream << "dialogsfile version 2" << std::endl;
	stream << m_dialogs.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject sentence = iter.Next();
		std::string input_token = sentence[1].AsString();
		std::string output_token = sentence[4].AsString();

		if (states.find(input_token) == states.end())
		{
			states.insert({ input_token, num_states++ });

			if (m_flags & msf_obfuscate_dialog_states)
				states_stream << "state_" << states[input_token] << std::endl;
			else
				states_stream << input_token << std::endl;
		}

		if (states.find(output_token) == states.end())
		{
			states.insert({ output_token, num_states++ });

			if (m_flags & msf_obfuscate_dialog_states)
				states_stream << "state_" << states[output_token] << std::endl;
			else
				states_stream << output_token << std::endl;
		}

		std::string text = encode_str(sentence[3].AsString());

		std::string auto_id = "dlga_" + encode_id(input_token) + ":" + encode_id(output_token);
		std::string new_auto_id = auto_id;
		int i = 1;
		char buff[20] = { '.', '\0' };

		if (dialog_ids.find(new_auto_id) != dialog_ids.end() && dialog_ids[new_auto_id] != text)
		{
			while (dialog_ids.find(new_auto_id) != dialog_ids.end())
			{
				sprintf(buff + 1, "%d", i++);
				new_auto_id = auto_id + buff;
			}
		}

		auto_id = std::move(new_auto_id);
		dialog_ids[auto_id] = text;

		if (states.find(input_token) == states.end())
			Warning(wl_error, "input token not found: " + input_token, auto_id);

		stream << auto_id << ' ';
		stream << sentence[0] << ' ';
		stream << states[input_token] << ' ';
		WriteStatementBlock(sentence[2], stream, auto_id);

		if (text.empty())
			text = "NO_TEXT";

		stream << text << ' ';
		stream << states[output_token] << ' ';
		WriteStatementBlock(sentence[5], stream, auto_id);

		if (sentence.Len() > 6)
			stream << encode_str(sentence[6].AsString()) << ' ';
		else
			stream << "NO_VOICEOVER ";

		stream << std::endl;
	}
}

void ModuleSystem::WriteFactions()
{
	PrepareModule("factions");

	std::ofstream stream(m_output_path + "factions.txt");
	int num_factions = (int)m_factions.Len();
	double **relations = new double*[num_factions];
	for (int i = 0; i < num_factions; i++)
	{
		relations[i] = new double[num_factions];
		memset(relations[i], 0, num_factions * sizeof(double));
	}
	//std::map<int, std::map<int, double>> relations;

	for (int i = 0; i < num_factions; ++i)
	{
		CPyObject faction = m_factions[i];
		CPyIter relation_iter = faction[4].GetIter();

		relations[i][i] = faction[3].AsFloat();

		while (relation_iter.HasNext())
		{
			CPyObject relation = relation_iter.Next();
			int other_id = GetId("fac", relation[0], (std::string)faction[0].AsString() + " relations");
			double value = relation[1].AsFloat();

			relations[i][other_id] = value;

			//if (relations[other_id][i] == 0.0)
			if (std::abs(relations[other_id][i]) < 1e-8)
				relations[other_id][i] = value;
		}
	}

	stream << "factionsfile version 1" << std::endl;
	stream << num_factions << std::endl;

	for (int i = 0; i < num_factions; ++i)
	{
		CPyObject faction = m_factions[i];

		stream << "fac_" << encode_id(faction[0].AsString()) << ' ';
		stream << encode_str(faction[1].AsString()) << ' ';
		stream << faction[2] << ' ';

		if (faction.Len() > 6)
			stream << faction[6] << ' ';
		else
			stream << 0xAAAAAA << ' ';

		for (ssize_t j = 0; j < m_factions.Len(); ++j) stream << relations[i][j] << ' ';

		if (faction.Len() > 5)
		{
			CPyObject ranks = faction[5];
			CPyIter rank_iter = ranks.GetIter();

			stream << ranks.Len() << ' ';

			while (rank_iter.HasNext()) stream << encode_str(rank_iter.Next().AsString()) << ' ';
		}

		stream << std::endl;
	}

	for (int i = 0; i < num_factions; i++) delete[] relations[i];
	delete[] relations;
}

void ModuleSystem::WriteFloraKinds()
{
	PrepareModule("flora kinds");

	std::ofstream stream(m_output_path + "Data" + PATH_SEPARATOR + "flora_kinds.txt");
	CPyIter	iter = m_flora_kinds.GetIter();

	stream << m_flora_kinds.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject flora_kind = iter.Next();
		std::string name = encode_strip(flora_kind[0].AsString());
		unsigned long long flags = flora_kind[1].AsNumber();

		stream << name << ' ';
		stream << flags << ' ';

		CPyObject meshes = flora_kind[2];
		CPyIter	meshes_iter = meshes.GetIter();

		stream << meshes.Len() << ' ';

		while (meshes_iter.HasNext())
		{
			CPyObject mesh = meshes_iter.Next();

			stream << GetResource(mesh[0], res_mesh, name) << ' ';

			stream << (mesh.Len() > 1 ? GetResource(mesh[1], res_body, name) : "0") << ' ';

			if (flags & 0x02400000)
			{
				stream << GetResource(mesh[2][0], res_mesh, name) << ' ';
				stream << GetResource(mesh[2][1], res_body, name) << ' ';
			}
		}

		if (flags & 0x04000000)
		{
			stream << flora_kind[3] << ' ';
			stream << flora_kind[4] << ' ';
		}

		stream << std::endl;
	}
}

void ModuleSystem::WriteGlobalVars()
{
	PrepareModule("global variables");

	std::ofstream stream(m_output_path + "variables.txt");
	std::vector<std::string> global_vars(m_global_vars.size());

	for (auto it = m_global_vars.begin(); it != m_global_vars.end(); ++it) global_vars[it->second.index] = it->first;

	for (size_t i = 0; i < global_vars.size(); ++i)
	{
		if (m_flags & msf_obfuscate_global_vars)
			stream << "global_var_" << i << std::endl;
		else
			stream << global_vars[i] << std::endl;
	}
}

void ModuleSystem::WriteGroundSpecs()
{
	PrepareModule("ground specs");

	std::ofstream stream(m_output_path + "Data" + PATH_SEPARATOR + "ground_specs.txt");
	CPyIter	iter = m_ground_specs.GetIter();

	while (iter.HasNext())
	{
		CPyObject ground_spec = iter.Next();
		std::string name = encode_id(ground_spec[0].AsString());
		unsigned int flags = (unsigned int)ground_spec[1].AsNumber();

		stream << name << ' ';
		stream << flags << ' ';
		stream << GetResource(ground_spec[2], res_material, name) << ' ';
		stream << ground_spec[3] << ' ';
		stream << GetResource(ground_spec[4], res_material, name) << ' ';

		if (flags & 0x4)
		{
			stream << ground_spec[5][0] << ' ';
			stream << ground_spec[5][1] << ' ';
			stream << ground_spec[5][2] << ' ';
		}

		stream << std::endl;
	}
}

void ModuleSystem::WriteInfoPages()
{
	PrepareModule("info pages");

	std::ofstream stream(m_output_path + "info_pages.txt");
	CPyIter iter = m_info_pages.GetIter();

	stream << "infopagesfile version 1" << std::endl;
	stream << m_info_pages.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject info_page = iter.Next();

		stream << "ip_" << encode_id(info_page[0].AsString()) << ' ';
		stream << encode_str(info_page[1].AsString()) << ' ';
		stream << encode_str(info_page[2].AsString()) << ' ';
		stream << std::endl;
	}
}

void ModuleSystem::WriteItems()
{
	PrepareModule("items");

	std::ofstream stream(m_output_path + "item_kinds1.txt");
	CPyIter iter = m_items.GetIter();

	stream << "itemsfile version 3" << std::endl;
	stream << m_items.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject item = iter.Next();
		std::string name = "itm_" + encode_id(item[0].AsString());

		stream << name << ' ';
		stream << encode_str(item[1].AsString()) << ' ';
		stream << encode_str(item[1].AsString()) << ' ';

		CPyObject variations = item[2];
		int num_variations = (int)variations.Len();

		if (num_variations > 16)
		{
			Warning(wl_warning, "item variation count exceeds 16", name);
			num_variations = 16;
		}

		stream << num_variations << ' ';

		for (int i = 0; i < num_variations; ++i)
		{
			CPyObject variation = variations[i];

			stream << GetResource(variation[0], res_mesh, name) << ' ';
			stream << variation[1] << ' ';
		}

		stream << item[3] << ' ';
		stream << item[4] << ' ';
		stream << item[5] << ' ';
		stream << item[7] << ' ';

		CPyNumber item_stats = item[6].AsNumber();
		double weight = 0.25 * ((item_stats >> 24) & 0xFF);
		int head_armor = (item_stats >> 0) & 0xFF;
		int body_armor = (item_stats >> 8) & 0xFF;
		int leg_armor = (item_stats >> 16) & 0xFF;
		int difficulty = (item_stats >> 32) & 0xFF;
		int hit_points = (item_stats >> 40) & 0xFFFF;
		int speed_rating = (item_stats >> 80) & 0xFF;
		int missile_speed = (item_stats >> 90) & 0x3FF;
		int weapon_length = (item_stats >> 70) & 0x3FF;
		int max_ammo = (item_stats >> 100) & 0xFF;
		int thrust_damage = (item_stats >> 60) & 0x3FF;
		int swing_damage = (item_stats >> 50) & 0x3FF;
		int abundance = (item_stats >> 110) & 0xFF;

		if (abundance == 0) abundance = 100;

		if (m_flags & msf_rusmod_rebalanser)
		{
			const unsigned short itp_type_head_armor = 0xc;
			const unsigned short itp_type_body_armor = 0xd;

			unsigned short type = (item[3].AsNumber()) & 0xF;
			
			if (type == itp_type_head_armor)
			{
				double l = 4 * weight + 4;
				if (l >= 14)
					difficulty = int(l + 0.5);
			}

			if (type == itp_type_body_armor)
			{
				double l = 0.9375 * weight - 4.125;
				if (l >= 7)
					difficulty = int(l + 0.5);
			}
		}

		stream << weight << ' ';
		stream << abundance << ' ';
		stream << head_armor << ' ';
		stream << body_armor << ' ';
		stream << leg_armor << ' ';
		stream << difficulty << ' ';
		stream << hit_points << ' ';
		stream << speed_rating << ' ';
		stream << missile_speed << ' ';
		stream << weapon_length << ' ';
		stream << max_ammo << ' ';
		stream << thrust_damage << ' ';
		stream << swing_damage << ' ';

		if (item.Len() > 9)
		{
			CPyObject factions = item[9];
			int num_factions = (int)factions.Len();

			if (num_factions > 16)
			{
				Warning(wl_warning, "item faction count exceeds 16", name);
				num_factions = 16;
			}

			stream << num_factions << ' ';

			for (int i = 0; i < num_factions; ++i) stream << factions[i] << ' ';
		}
		else
			stream << "0 ";

		if (item.Len() > 8)
			WriteSimpleTriggerBlock(item[8], stream, name);
		else
			stream << "0" << std::endl;
	}
}

void ModuleSystem::WriteMapIcons()
{
	PrepareModule("map icons");

	std::ofstream stream(m_output_path + "map_icons.txt");
	CPyIter iter = m_map_icons.GetIter();

	stream << "map_icons_file version 1" << std::endl;
	stream << m_map_icons.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject map_icon = iter.Next();
		std::string name = encode_id(map_icon[0].AsString());

		stream << name << ' ';
		stream << map_icon[1] << ' ';
		stream << GetResource(map_icon[2], res_mesh, name) << ' ';
		stream << map_icon[3] << ' ';
		stream << GetId("snd", map_icon[4], name) << ' ';

		int trigger_pos;

		if (map_icon.Len() > 7)
		{
			stream << map_icon[5] << ' ';
			stream << map_icon[6] << ' ';
			stream << map_icon[7] << ' ';
			trigger_pos = 8;
		}
		else
		{
			stream << "0.0 ";
			stream << "0.0 ";
			stream << "0.0 ";
			trigger_pos = 5;
		}

		if (map_icon.Len() > trigger_pos)
			WriteSimpleTriggerBlock(map_icon[trigger_pos], stream, name);
		else
			stream << "0 " << std::endl;
	}
}

void ModuleSystem::WriteMenus()
{
	PrepareModule("game menus");

	std::ofstream stream(m_output_path + "menus.txt");
	CPyIter iter = m_game_menus.GetIter();

	stream << "menusfile version 1" << std::endl;
	stream << m_game_menus.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject menu = iter.Next();
		std::string name = "menu_" + encode_id(menu[0].AsString());

		stream << name << ' ';
		stream << menu[1] << ' ';
		stream << encode_str(menu[2].AsString()) << ' ';
		stream << GetResource(menu[3], res_mesh, name) << ' ';
		WriteStatementBlock(menu[4], stream, name);

		CPyObject items = menu[5];
		CPyIter item_iter = items.GetIter();

		stream << items.Len() << ' ';

		while (item_iter.HasNext())
		{
			CPyObject item = item_iter.Next();
			std::string item_name = "mno_" + encode_id(item[0].AsString());

			stream << std::endl;
			stream << item_name << ' ';
			WriteStatementBlock(item[1], stream, name + ", " + item_name + ", conditions");
			stream << encode_str(item[2].AsString()) << ' ';
			WriteStatementBlock(item[3], stream, name + ", " + item_name + ", consequences");

			if (item.Len() > 4)
				stream << encode_str(item[4].AsString()) << ' ';
			else
				stream << ". ";
		}

		stream << std::endl;
	}
}

void ModuleSystem::WriteMeshes()
{
	PrepareModule("meshes");

	std::ofstream stream(m_output_path + "meshes.txt");
	CPyIter iter = m_meshes.GetIter();

	stream << m_meshes.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject mesh = iter.Next();
		std::string name = "mesh_" + encode_id(mesh[0].AsString());

		stream << name << ' ';
		stream << mesh[1] << ' ';
		stream << GetResource(mesh[2], res_mesh, name) << ' ';
		stream << mesh[3] << ' ';
		stream << mesh[4] << ' ';
		stream << mesh[5] << ' ';
		stream << mesh[6] << ' ';
		stream << mesh[7] << ' ';
		stream << mesh[8] << ' ';
		stream << mesh[9] << ' ';
		stream << mesh[10] << ' ';
		stream << mesh[11] << ' ';
		stream << std::endl;
	}
}

void ModuleSystem::WriteMissionTemplates()
{
	PrepareModule("mission templates");

	std::ofstream stream(m_output_path + "mission_templates.txt");
	CPyIter iter = m_mission_templates.GetIter();

	stream << "missionsfile version 1" << std::endl;
	stream << m_mission_templates.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject mission_template = iter.Next();
		std::string name = "mst_" + encode_id(mission_template[0].AsString());

		stream << name << ' ';
		stream << encode_id(mission_template[0].AsString()) << ' ';
		stream << mission_template[1] << ' ';
		stream << mission_template[2] << ' ';
		stream << encode_str(mission_template[3].AsString()) << ' ';

		CPyObject groups = mission_template[4];
		int num_groups = (int)groups.Len();

		stream << num_groups << ' ';

		for (int j = 0; j < num_groups; ++j)
		{
			CPyObject group = groups[j];

			stream << group[0] << ' ';
			stream << group[1] << ' ';
			stream << group[2] << ' ';
			stream << group[3] << ' ';
			stream << group[4] << ' ';

			if (group.Len() > 5)
			{
				CPyObject overrides = group[5];
				int num_overrides = (int)overrides.Len();

				if (num_overrides > 8)
				{
					Warning(wl_warning, "item override count exceeds 8", name + ", group " + itostr(j));
					num_overrides = 8;
				}

				stream << num_overrides << ' ';

				for (int i = 0; i < num_overrides; ++i) stream << overrides[i] << ' ';
			}
			else
				stream << "0 ";
		}

		WriteTriggerBlock(mission_template[5], stream, name);
	}
}

void ModuleSystem::WriteMusic()
{
	PrepareModule("music tracks");

	std::ofstream stream(m_output_path + "music.txt");
	CPyIter iter = m_music.GetIter();

	stream << m_music.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject track = iter.Next();
		unsigned long long flags = track[2].AsLong();
		unsigned long long continue_flags = track[3].AsLong();

		stream << encode_str(track[1].AsString()) << ' ';
		stream << flags << ' ';
		stream << (flags | continue_flags) << ' ';
		stream << std::endl;
	}
}

void ModuleSystem::WriteParticleSystems()
{
	PrepareModule("particle systems");

	std::ofstream stream(m_output_path + "particle_systems.txt");
	CPyIter iter = m_particle_systems.GetIter();

	stream << "particle_systemsfile version 1" << std::endl;
	stream << m_particle_systems.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject particle_system = iter.Next();
		std::string name = "psys_" + encode_id(particle_system[0].AsString());

		stream << name << ' ';
		stream << particle_system[1] << ' ';
		stream << GetResource(particle_system[2], res_mesh, name) << ' ';
		stream << particle_system[3] << ' ';
		stream << particle_system[4] << ' ';
		stream << particle_system[5] << ' ';
		stream << particle_system[6] << ' ';
		stream << particle_system[7] << ' ';
		stream << particle_system[8] << ' ';

		for (int i = 0; i < 10; i += 2)
		{
			CPyObject key1 = particle_system[i + 9];
			CPyObject key2 = particle_system[i + 10];

			stream << key1[0] << ' ';
			stream << key1[1] << ' ';
			stream << key2[0] << ' ';
			stream << key2[1] << ' ';
		}

		CPyObject emit_box_size = particle_system[19];

		stream << emit_box_size[0] << ' ';
		stream << emit_box_size[1] << ' ';
		stream << emit_box_size[2] << ' ';

		CPyObject emit_velocity = particle_system[20];

		stream << emit_velocity[0] << ' ';
		stream << emit_velocity[1] << ' ';
		stream << emit_velocity[2] << ' ';
		stream << particle_system[21] << ' ';

		if (particle_system.Len() > 22)
			stream << particle_system[22] << ' ';
		else
			stream << "0.0 ";

		if (particle_system.Len() > 23)
			stream << particle_system[23] << ' ';
		else
			stream << "0.0 ";

		stream << std::endl;
	}
}

void ModuleSystem::WriteParties()
{
	PrepareModule("parties");

	std::ofstream stream(m_output_path + "parties.txt");
	int num_parties = (int)m_parties.Size();

	stream << "partiesfile version 1" << std::endl;
	stream << num_parties << std::endl;
	stream << num_parties << std::endl;

	for (int i = 0; i < num_parties; ++i)
	{
		CPyObject party = m_parties[i];
		std::string name = "p_" + encode_id(party[0].AsString());

		stream << 1 << ' ' << i << ' ' << i << ' ';
		stream << name << ' ';
		stream << encode_str(party[1].AsString()) << ' ';
		stream << party[2] << ' ';
		stream << GetId("menu", party[3], name) << ' ';
		stream << GetId("pt", party[4], name) << ' ';
		stream << GetId("fac", party[5], name) << ' ';
		stream << party[6] << ' ';
		stream << party[6] << ' ';
		stream << party[7] << ' ';

		int target_party_id = GetId("p", party[8], name);

		stream << target_party_id << ' ';
		stream << target_party_id << ' ';

		CPyObject position = party[9];

		stream << position[0] << ' ';
		stream << position[1] << ' ';
		stream << position[0] << ' ';
		stream << position[1] << ' ';
		stream << position[0] << ' ';
		stream << position[1] << ' ';
		stream << "0.0 ";

		CPyObject members = party[10];
		int num_members = (int)members.Len();

		stream << num_members << ' ';

		for (int j = 0; j < num_members; ++j)
		{
			CPyObject member = members[j];

			stream << GetId("trp", member[0], name + ", member " + itostr(j)) << ' ';
			stream << member[1] << ' ';
			stream << "0 ";
			stream << member[2] << ' ';
		}

		if (party.Len() > 11)
			stream << std::setprecision(7) << (3.1415926 / 180.0) * party[11].AsFloat() << ' ';
		else
			stream << "0.0 ";

		stream << std::endl;
	}
}

void ModuleSystem::WritePartyTemplates()
{
	PrepareModule("party templates");

	std::ofstream stream(m_output_path + "party_templates.txt");
	CPyIter iter = m_party_templates.GetIter();

	stream << "partytemplatesfile version 1" << std::endl;
	stream << m_party_templates.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject party_template = iter.Next();
		std::string name = "pt_" + encode_id(party_template[0].AsString());

		stream << name << ' ';
		stream << encode_str(party_template[1].AsString()) << ' ';
		stream << party_template[2] << ' ';
		stream << GetId("menu", party_template[3], name) << ' ';
		stream << GetId("fac", party_template[4], name) << ' ';
		stream << party_template[5] << ' ';

		CPyObject members = party_template[6];
		int num_members = (int)members.Len();

		if (num_members > 6)
		{
			Warning(wl_warning, "party template member count exceeds 6", name);
			num_members = 6;
		}

		for (int i = 0; i < num_members; ++i)
		{
			CPyObject member = members[i];

			stream << GetId("trp", member[0], name + ", member " + itostr(i)) << ' ';
			stream << member[1] << ' ';
			stream << member[2] << ' ';

			if (member.Len() > 3)
				stream << member[3] << ' ';
			else
				stream << "0 ";
		}

		for (int i = num_members; i < 6; ++i) stream << "-1 "; 

		stream << std::endl;
	}
}

void ModuleSystem::WritePostEffects()
{
	PrepareModule("post effects");

	std::ofstream stream(m_output_path + "postfx.txt");
	CPyIter iter = m_postfx.GetIter();

	stream << "postfx_paramsfile version 1" << std::endl;
	stream << m_postfx.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject effect = iter.Next();

		stream << "pfx_" << encode_id(effect[0].AsString()) << ' ';
		stream << effect[1] << ' ';
		stream << effect[2] << ' ';

		for (int i = 0; i < 3; ++i)
		{
			CPyObject params = effect[i + 3];

			stream << params[0] << ' ';
			stream << params[1] << ' ';
			stream << params[2] << ' ';
			stream << params[3] << ' ';
		}

		stream << std::endl;
	}
}

void ModuleSystem::WritePresentations()
{
	PrepareModule("presentations");

	std::ofstream stream(m_output_path + "presentations.txt");
	CPyIter iter = m_presentations.GetIter();

	stream << "presentationsfile version 1" << std::endl;
	stream << m_presentations.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject presentation = iter.Next();
		std::string name = "prsnt_" + encode_id(presentation[0].AsString());

		stream << name << ' ';
		stream << presentation[1] << ' ';
		stream << GetId("mesh", presentation[2], name) << ' ';
		WriteSimpleTriggerBlock(presentation[3], stream, name);
	}
}

void ModuleSystem::WriteQuests()
{
	PrepareModule("quests");

	std::ofstream stream(m_output_path + "quests.txt");
	CPyIter iter = m_quests.GetIter();

	stream << "questsfile version 1" << std::endl;
	stream << m_quests.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject quest = iter.Next();

		stream << "qst_" << encode_id(quest[0].AsString()) << ' ';
		stream << encode_str(quest[1].AsString()) << ' ';
		stream << quest[2] << ' ';
		stream << encode_str(quest[3].AsString()) << ' ';
		stream << std::endl;
	}
}

void ModuleSystem::WriteQuickStrings()
{
	PrepareModule("quick strings");

	std::ofstream stream(m_output_path + "quick_strings.txt");
	std::vector<std::string> quick_strings(m_quick_strings.size());

	for (auto it = m_quick_strings.begin(); it != m_quick_strings.end(); ++it) quick_strings[it->second.index] = it->first;

	stream << quick_strings.size() << std::endl;

	for (size_t i = 0; i < quick_strings.size(); ++i)
	{
		stream << quick_strings[i] << ' ';
		stream << m_quick_strings[quick_strings[i]].value << std::endl;
	}
}

void ModuleSystem::WriteSceneProps()
{
	PrepareModule("scene props");

	std::ofstream stream(m_output_path + "scene_props.txt");
	CPyIter iter = m_scene_props.GetIter();

	stream << "scene_propsfile version 1" << std::endl;
	stream << m_scene_props.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject scene_prop = iter.Next();
		std::string name = "spr_" + encode_strip(scene_prop[0].AsString());

		stream << name << ' ';
		stream << scene_prop[1] << ' ';
		stream << (((unsigned long long)scene_prop[1].AsLong() >> 20) & 0xFF) << ' ';
		stream << GetResource(scene_prop[2], res_mesh, name) << ' ';
		stream << GetResource(scene_prop[3], res_body, name) << ' ';
		WriteSimpleTriggerBlock(scene_prop[4], stream, name);
	}
}

void ModuleSystem::WriteScenes()
{
	PrepareModule("scenes");

	std::ofstream stream(m_output_path + "scenes.txt");
	CPyIter iter = m_scenes.GetIter();

	stream << "scenesfile version 1" << std::endl;
	stream << m_scenes.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject scene = iter.Next();
		std::string name = "scn_" + encode_id(scene[0].AsString());

		stream << name << ' ';
		stream << encode_str(scene[0].AsString()) << ' ';
		stream << scene[1] << ' ';
		stream << GetResource(scene[2], res_mesh, name) << ' ';
		stream << GetResource(scene[3], res_body, name) << ' ';
		stream << scene[4][0] << ' ';
		stream << scene[4][1] << ' ';
		stream << scene[5][0] << ' ';
		stream << scene[5][1] << ' ';
		stream << scene[6] << ' ';
		stream << scene[7] << ' ';

		CPyObject passages = scene[8];
		CPyIter passage_iter = passages.GetIter();

		stream << passages.Len() << ' ';

		while (passage_iter.HasNext())
		{
			CPyObject passage = passage_iter.Next();
			int scene_id;

			if (passage.IsLong())
				scene_id = (long)passage.AsLong();
			else
			{
				std::string name = passage.AsString();

				if (name.empty())
					scene_id = 0;
				else if (name == "exit")
					scene_id = 100000;
				else
					scene_id = GetId("scn", passage, name);
			}

			stream << scene_id << ' ';
		}

		CPyObject chests = scene[9];
		CPyIter chest_iter = chests.GetIter();

		stream << chests.Len() << ' ';

		while (chest_iter.HasNext()) stream << GetId("trp", chest_iter.Next(), name) << ' ';

		if (scene.Len() > 10)
			stream << scene[10] << ' ';
		else
			stream << "0 ";

		stream << std::endl;
	}
}

void ModuleSystem::WriteScripts()
{
	PrepareModule("scripts");
	std::ofstream table_stream;

	if (m_flags & msf_obfuscate_scripts && m_flags & msf_list_obfuscated_scripts) table_stream.open(m_output_path + "obfuscated_scripts.txt");

	std::ofstream stream(m_output_path + "scripts.txt");
	CPyIter iter = m_scripts.GetIter();
	int num_scripts = (int)m_scripts.Size();

	stream << "scriptsfile version 1" << std::endl;
	stream << num_scripts << std::endl;

	for (int i = 0; i < num_scripts; ++i)
	{
		CPyObject script = m_scripts[i];
		std::string name = encode_id(script[0].AsString());

		if ((m_flags & msf_obfuscate_scripts) && name.substr(0, 5) != "game_" && name.substr(0, 4) != "wse_")
		{
			if (m_flags & msf_list_obfuscated_scripts) table_stream << "script_" << i << "=" << name << std::endl;

			stream << "script_" << i << ' ';
		}
		else
			stream << name << ' ';

		CPyObject obj = script[1];
		bool fails_at_zero;

		if (obj.IsTuple() || obj.IsList())
		{
			stream << "-1 ";
			fails_at_zero = WriteStatementBlock(obj, stream, name);
		}
		else
		{
			stream << obj << ' ';
			fails_at_zero = WriteStatementBlock(script[2], stream, name);
		}

		if (fails_at_zero && name.substr(0, 3) != "cf_") Warning(wl_warning, "non cf_ script can fail", name);

		stream << std::endl;
	}

	if (m_flags & msf_list_obfuscated_scripts)
		table_stream.close();
}

void ModuleSystem::WriteSimpleTriggers()
{
	PrepareModule("simple triggers");

	std::ofstream stream(m_output_path + "simple_triggers.txt");

	stream << "simple_triggers_file version 1" << std::endl;
	WriteSimpleTriggerBlock(m_simple_triggers, stream, "simple game triggers");
}

void ModuleSystem::WriteSkills()
{
	PrepareModule("skills");

	std::ofstream stream(m_output_path + "skills.txt");
	CPyIter iter = m_skills.GetIter();

	stream << m_skills.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject skill = iter.Next();

		stream << "skl_" << encode_id(skill[0].AsString()) << ' ';
		stream << encode_str(skill[1].AsString()) << ' ';
		stream << skill[2] << ' ';
		stream << skill[3] << ' ';
		stream << encode_str(skill[4].AsString()) << ' ';
		stream << std::endl;
	}
}

void ModuleSystem::WriteSkins()
{
	PrepareModule("skins");

	std::ofstream stream(m_output_path + "skins.txt");
	int num_skins = (int)m_skins.Size();

	if (num_skins > 16)
	{
		Warning(wl_warning, "skin count exceeds 6");
		num_skins = 16;
	}

	stream << "skins_file version 1" << std::endl;
	stream << num_skins << std::endl;

	for (int i = 0; i < num_skins; ++i)
	{
		CPyObject skin = m_skins[i];
		std::string name = encode_id(skin[0].AsString());

		stream << name << ' ';
		stream << skin[1] << ' ';
		stream << GetResource(skin[2], res_mesh, name) << ' ';
		stream << GetResource(skin[3], res_mesh, name) << ' ';
		stream << GetResource(skin[4], res_mesh, name) << ' ';
		stream << GetResource(skin[5], res_mesh, name) << ' ';

		CPyObject face_keys = skin[6];
		CPyIter face_key_iter = face_keys.GetIter();

		stream << face_keys.Len() << ' ';

		while (face_key_iter.HasNext())
		{
			CPyObject face_key = face_key_iter.Next();

			stream << "skinkey_" << encode_id(face_key[4].AsString()) << ' ';
			stream << face_key[0] << ' ';
			stream << face_key[1] << ' ';
			stream << face_key[2] << ' ';
			stream << face_key[3] << ' ';
			stream << encode_str(face_key[4].AsString()) << ' ';
		}

		CPyObject hair_meshes = skin[7];
		CPyIter hair_mesh_iter = hair_meshes.GetIter();

		stream << hair_meshes.Len() << ' ';

		while (hair_mesh_iter.HasNext()) stream << GetResource(hair_mesh_iter.Next(), res_mesh, name) << ' ';

		CPyObject beard_meshes = skin[8];
		CPyIter beard_mesh_iter = beard_meshes.GetIter();

		stream << beard_meshes.Len() << ' ';

		while (beard_mesh_iter.HasNext()) stream << GetResource(beard_mesh_iter.Next(), res_mesh, name) << ' ';

		CPyObject hair_materials = skin[9];
		CPyIter hair_material_iter = hair_materials.GetIter();

		stream << hair_materials.Len() << ' ';

		while (hair_material_iter.HasNext()) stream << GetResource(hair_material_iter.Next(), res_material, name) << ' ';

		CPyObject beard_materials = skin[10];
		CPyIter beard_material_iter = beard_materials.GetIter();

		stream << beard_materials.Len() << ' ';

		while (beard_material_iter.HasNext()) stream << GetResource(beard_material_iter.Next(), res_material, name) << ' ';

		CPyObject face_textures = skin[11];
		CPyIter face_texture_iter = face_textures.GetIter();

		stream << face_textures.Len() << ' ';

		while (face_texture_iter.HasNext())
		{
			CPyObject face_texture = face_texture_iter.Next();

			stream << GetResource(face_texture[0], res_material, name) << ' ';
			stream << face_texture[1] << ' ';

			CPyObject hair_materials;
			CPyObject hair_colors;
			int num_hair_materials = 0;
			int num_hair_colors = 0;

			if (face_texture.Len() > 2)
			{
				hair_materials = face_texture[2];
				num_hair_materials = (int)hair_materials.Len();

				if (face_texture.Len() > 3)
				{
					hair_colors = face_texture[3];
					num_hair_colors = (int)hair_colors.Len();
				}
			}

			stream << num_hair_materials << ' ';
			stream << num_hair_colors << ' ';

			for (int i = 0; i < num_hair_materials; ++i) stream << GetResource(hair_materials[i], res_material, name) << ' ';

			for (int i = 0; i < num_hair_colors; ++i) stream << hair_colors[i] << ' ';
		}

		CPyObject voices = skin[12];
		CPyIter voice_iter = voices.GetIter();

		stream << voices.Len() << ' ';

		while (voice_iter.HasNext())
		{
			CPyObject voice = voice_iter.Next();

			stream << voice[0] << ' ';
			stream << encode_id(voice[1].Str()) << ' ';
		}

		stream << GetResource(skin[13], res_skeleton, name) << ' ';
		stream << skin[14] << ' ';

		if (skin.Len() > 15)
			stream << GetId("psys", skin[15], name) << ' ';
		else
			stream << "0 ";

		if (skin.Len() > 16)
			stream << GetId("psys", skin[16], name) << ' ';
		else
			stream << "0 ";

		if (skin.Len() > 17)
		{
			CPyObject constraints = skin[17];
			CPyIter constraint_iter = constraints.GetIter();

			stream << constraints.Len() << ' ';

			while (constraint_iter.HasNext())
			{
				CPyObject constraint = constraint_iter.Next();

				stream << constraint[0] << ' ';
				stream << constraint[1] << ' ';

				int num_pairs = (int)constraint.Len() - 2;

				stream << num_pairs << ' ';

				for (int i = 0; i < num_pairs; ++i)
				{
					CPyObject pair = constraint[i + 2];

					stream << pair[0] << ' ';
					stream << pair[1] << ' ';
				}
			}
		}
		else
		{
			stream << "0 ";
		}

		stream << std::endl;
	}
}

void ModuleSystem::WriteSkyboxes()
{
	PrepareModule("skyboxes");

	std::ofstream stream(m_output_path + "Data" + PATH_SEPARATOR + "skyboxes.txt");
	CPyIter	iter = m_skyboxes.GetIter();

	stream << m_skyboxes.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject skybox = iter.Next();
		std::string name = encode_res(skybox[0].AsString());

		stream << GetResource(skybox[0], res_mesh, name) << ' ';
		stream << skybox[1] << ' ';
		stream << skybox[2] << ' ';
		stream << skybox[3] << ' ';
		stream << skybox[4] << ' ';
		stream << encode_id(skybox[5].AsString()) << ' ';
		stream << skybox[6][0] << ' ';
		stream << skybox[6][1] << ' ';
		stream << skybox[6][2] << ' ';
		stream << skybox[7][0] << ' ';
		stream << skybox[7][1] << ' ';
		stream << skybox[7][2] << ' ';
		stream << skybox[8][0] << ' ';
		stream << skybox[8][1] << ' ';
		stream << skybox[8][2] << ' ';
		stream << skybox[9][0] << ' ';
		stream << skybox[9][1] << ' ';
		stream << std::endl;
	}
}

void ModuleSystem::WriteSounds()
{
	PrepareModule("sounds");

	std::ofstream stream(m_output_path + "sounds.txt");
	CPyIter iter = m_sounds.GetIter();
	std::map<std::string, int> samples;
	std::vector<std::string> samples_vec;
	std::vector<unsigned long> sample_flags;

	while (iter.HasNext())
	{
		CPyObject sound = iter.Next();
		CPyObject sound_files = sound[2];
		CPyIter sound_file_iter = sound_files.GetIter();

		while (sound_file_iter.HasNext())
		{
			CPyObject sound_file = sound_file_iter.Next();
			std::string file = sound_file.IsTuple() || sound_file.IsList() ? sound_file[0].AsString() : sound_file.AsString();

			if (samples.find(file) == samples.end())
			{
				samples[file] = (int)samples_vec.size();
				samples_vec.push_back(file);
				sample_flags.push_back(sound[1].AsLong());
			}
		}
	}

	stream << "soundsfile version 3" << std::endl;
	stream << samples_vec.size() << std::endl;

	for (size_t i = 0; i < samples_vec.size(); ++i)
	{
		stream << samples_vec[i] << ' ';
		stream << sample_flags[i] << ' ';
		stream << std::endl;
	}

	iter = m_sounds.GetIter();
	stream << std::endl;
	stream << m_sounds.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject sound = iter.Next();
		std::string name = "snd_" + encode_id(sound[0].AsString());

		stream << name << ' ';
		stream << sound[1] << ' ';

		CPyObject sound_files = sound[2];
		int num_samples = (int)sound_files.Len();

		stream << num_samples << ' ';

		if (num_samples > 32)
		{
			Warning(wl_warning, "sound sample count exceeds 32", name);
			num_samples = 32;
		}

		for (int i = 0; i < num_samples; ++i)
		{
			CPyObject sound_file = sound_files[i];
			std::string file;
			unsigned long flags;

			if (sound_file.IsTuple() || sound_file.IsList())
			{
				file = sound_file[0].AsString();
				flags = sound_file[1].AsLong();
			}
			else
			{
				file = sound_file.AsString();
				flags = 0;
			}

			stream << samples[file] << ' ';
			stream << flags << ' ';
		}
		stream << std::endl;
	}
}

void ModuleSystem::WriteStrings()
{
	PrepareModule("strings");

	std::ofstream stream(m_output_path + "strings.txt");
	CPyIter iter = m_strings.GetIter();

	stream << "stringsfile version 1" << std::endl;
	stream << m_strings.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject string = iter.Next();

		stream << "str_" << encode_id(string[0].AsString()) << ' ';
		stream << encode_str(string[1].AsString()) << ' ';
		stream << std::endl;
	}
}

void ModuleSystem::WriteTableaus()
{
	PrepareModule("tableau materials");

	std::ofstream stream(m_output_path + "tableau_materials.txt");
	CPyIter iter = m_tableau_materials.GetIter();

	stream << m_tableau_materials.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject tableau = iter.Next();
		std::string name = "tab_" + encode_id(tableau[0].AsString());

		stream << name << ' ';
		stream << tableau[1] << ' ';
		stream << GetResource(tableau[2].AsString(), res_material, name) << ' ';
		stream << tableau[3] << ' ';
		stream << tableau[4] << ' ';
		stream << tableau[5] << ' ';
		stream << tableau[6] << ' ';
		stream << tableau[7] << ' ';
		stream << tableau[8] << ' ';
		WriteStatementBlock(tableau[9], stream, name);
		stream << std::endl;
	}
}

void ModuleSystem::WriteTriggers()
{
	PrepareModule("triggers");

	std::ofstream stream(m_output_path + "triggers.txt");

	stream << "triggersfile version 1" << std::endl;
	WriteTriggerBlock(m_triggers, stream, "game triggers");
}

void ModuleSystem::WriteTroops()
{
	PrepareModule("troops");

	std::ofstream stream(m_output_path + "troops.txt");
	CPyIter iter = m_troops.GetIter();

	stream << "troopsfile version 2" << std::endl;
	stream << m_troops.Size() << std::endl;

	while (iter.HasNext())
	{
		CPyObject troop = iter.Next();
		std::string name = "trp_" + encode_id(troop[0].AsString());

		stream << name << ' ';
		stream << encode_str(troop[1].AsString()) << ' ';
		stream << encode_str(troop[2].AsString()) << ' ';

		if (troop.Len() > 13)
			stream << GetResource(troop[13].Str(), res_mesh, name) << ' ';
		else
			stream << "0 ";

		stream << troop[3] << ' ';
		stream << troop[4] << ' ';
		stream << troop[5] << ' ';
		stream << GetId("fac", troop[6], name) << ' ';

		if (troop.Len() > 14)
			stream << troop[14] << ' ';
		else
			stream << "0 ";

		if (troop.Len() > 15)
			stream << troop[15] << ' ';
		else
			stream << "0 ";

		CPyObject items = troop[7];
		int num_items = (int)items.Len();

		for (int i = 0; i < num_items; ++i) stream << GetId("itm", items[i], name) << " 0 ";

		for (int i = num_items; i < 64; ++i) stream << "-1 0 ";

		CPyNumber attribs = troop[8];

		stream << ((attribs >> 0) & 0xFF) << ' ';
		stream << ((attribs >> 8) & 0xFF) << ' ';
		stream << ((attribs >> 16) & 0xFF) << ' ';
		stream << ((attribs >> 24) & 0xFF) << ' ';
		stream << ((attribs >> 32) & 0xFF) << ' ';

		CPyNumber proficiencies = troop[9].AsNumber();

		for (int i = 0; i < 7; ++i)
		{
			stream << (proficiencies & 0x3FF) << ' ';
			proficiencies = proficiencies >> 10;
		}

		CPyNumber skills = troop[10].AsNumber();

		for (int i = 0; i < 6; ++i) stream << ((skills >> (i * 32)) & 0xFFFFFFFF) << ' ';

		for (int i = 0; i < 2; ++i)
		{
			CPyNumber face_key;

			if (troop.Len() > i + 11) face_key = troop[i + 11].AsNumber();

			for (int j = 0; j < 4; ++j) stream << ((face_key >> ((3 - j) * 64)) & 0xFFFFFFFFFFFFFFFF) << ' ';
		}

		stream << std::endl;
	}
}

void ModuleSystem::WriteSimpleTriggerBlock(const CPyObject &simple_trigger_block, std::ostream &stream, const std::string &context)
{
	int num_simple_triggers = (int)simple_trigger_block.Len();

	stream << num_simple_triggers << std::endl;

	for (int i = 0; i < num_simple_triggers; ++i)
	{
		WriteSimpleTrigger(simple_trigger_block[i], stream, context + ", simple trigger " + itostr(i));
		stream << std::endl;
	}
}

void ModuleSystem::WriteSimpleTrigger(const CPyObject &simple_trigger, std::ostream &stream, const std::string &context)
{
	stream << simple_trigger[0] << ' ';
	WriteStatementBlock(simple_trigger[1], stream, context);
}

void ModuleSystem::WriteTriggerBlock(const CPyObject &trigger_block, std::ostream &stream, const std::string &context)
{
	int num_triggers = (int)trigger_block.Len();

	stream << num_triggers << std::endl;

	for (int i = 0; i < num_triggers; ++i)
	{
		WriteTrigger(trigger_block[i], stream, context + ", trigger " + itostr(i));
		stream << std::endl;
	}
}

void ModuleSystem::WriteTrigger(const CPyObject &trigger, std::ostream &stream, const std::string &context)
{
	stream << trigger[0] << ' ';
	stream << trigger[1] << ' ';
	stream << trigger[2] << ' ';
	WriteStatementBlock(trigger[3], stream, context + ", conditions");
	WriteStatementBlock(trigger[4], stream, context + ", consequences");
}

bool ModuleSystem::WriteStatementBlock(const CPyObject &statement_block, std::ostream &stream, const std::string &context)
{
	int depth = 0;
	bool fails_at_zero = false;
	int num_statements = (int)statement_block.Len();

	m_local_vars.clear();
	stream << num_statements << ' ';
	m_cur_context = context;

	for (m_cur_statement = 0; m_cur_statement < num_statements; ++m_cur_statement) WriteStatement(statement_block[m_cur_statement], stream, depth, fails_at_zero);

	if (depth != 0) Warning(wl_error, "unexpected try block depth " + itostr(depth), context);

	for (auto it = m_local_vars.begin(); it != m_local_vars.end(); ++it)
	{
		if (it->second.usages == 0 && it->first.substr(0, 6) != "unused")
			Warning(wl_warning, "unused local variable :" + it->first, context);
	}

	return fails_at_zero;
}

void ModuleSystem::WriteStatement(const CPyObject &statement, std::ostream &stream, int &depth, bool &fails_at_zero)
{
	long long opcode = -1;

	if (statement.IsTuple() || statement.IsList())
	{
		int num_operands = (int)statement.Len() - 1;

		opcode = statement[0].AsLong();
		stream << opcode << ' ';

		if (num_operands > 16)
		{
			Warning(wl_warning, "operand count exceeds 16", m_cur_context + ", statement " + itostr(m_cur_statement));
			num_operands = 16;
		}

		stream << num_operands << ' ';

		for (int i = 0; i < num_operands; ++i) stream << ParseOperand(statement, i + 1) << ' ';
	}
	else if (statement.IsLong())
	{
		opcode = statement.AsLong();
		stream << opcode << " 0 ";
	}
	else
		Warning(wl_critical, "unrecognized statement type " + (std::string)statement.Type().Str(), m_cur_context + ", statement " + itostr(m_cur_statement));
	
	int operation = opcode & 0xFFFFFFF;

	depth += m_operation_depths[operation];

	if (depth == 0 && m_operations[operation] & optype_cf)
		fails_at_zero = true;
}
