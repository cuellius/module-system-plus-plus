#pragma once

#include "CPyObject.h"
#if defined _WIN32
#include <Windows.h>
#else
#include <sys/time.h>
#endif
#include <ostream>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "StringUtils.h"

#if defined _WIN32
#define COLOR_RED (FOREGROUND_RED | FOREGROUND_INTENSITY)
#define COLOR_GREEN (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define COLOR_BLUE (FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define COLOR_MAGENTA (FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define COLOR_YELLOW (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define PATH_SEPARATOR '\\'
#else
#define COLOR_RED 31
#define COLOR_GREEN 32
#define COLOR_BLUE 34
#define COLOR_MAGENTA 35
#define COLOR_YELLOW 33
#define PATH_SEPARATOR '/'
#endif

struct ScriptErrorContext
{
	int line;
	int statement;
	std::string context;
	std::string error;
};

struct Variable
{
	int index;
	int usages;
	int assignments;
	bool compat;
};

struct QuickString
{
	int index;
	std::string value;
};

class CompileException
{
public:
	explicit CompileException(const std::string &error) : m_error(error) { }

	const std::string &GetText() const
	{
		return m_error;
	}

private:
	std::string m_error;
};

#define OPCODE(obj) (((unsigned long long)obj) & 0xFFFFFFF)
#define MAX_NUM_OPCODES 8192
#define OPTYPE_LHS 0x1
#define OPTYPE_GHS 0x2
#define OPTYPE_CF  0x4
#define OPMASK_REGISTER        (1ULL  << 56)
#define OPMASK_GLOBAL_VARIABLE (2ULL  << 56)
#define OPMASK_LOCAL_VARIABLE  (17ULL << 56)
#define OPMASK_QUICK_STRING    (22ULL << 56)

#define RES_MESH      0
#define RES_MATERIAL  1
#define RES_SKELETON  2
#define RES_BODY      3
#define RES_ANIMATION 4

#define MSF_STRICT                  0x1
#define MSF_OBFUSCATE_GLOBAL_VARS   0x2
#define MSF_OBFUSCATE_DIALOG_STATES 0x4
#define MSF_OBFUSCATE_SCRIPTS       0x8
#define MSF_OBFUSCATE_TAGS          0x10
#define MSF_SKIP_ID_FILES           0x20
#define MSF_LIST_RESOURCES          0x40
#define MSF_COMPILE_MODULE_DATA     0x80
#define MSF_LIST_OBFUSCATED_SCRIPTS 0x100
#define MSF_LIST_UNREFERENCED_SCRIPTS    0x200
#define MSF_DISABLE_WARNINGS    0x400
#define MSF_RUSMOD_REBALANSER    0x800

#define WL_WARNING  0
#define WL_ERROR    1
#define WL_CRITICAL 2

class ModuleSystem
{
public:
	ModuleSystem(const std::string &in_path, const std::string &out_path);
	~ModuleSystem();
	void Compile(unsigned long long flags = 0);

private:
	void LoadPythonInterpreter();
	void UnloadPythonInterpreter();
	void SetConsoleColor(int color);
	void ResetConsoleColor();
	void DoCompile();
	CPyList AddModule(const std::string &module_name, const std::string &list_name, const std::string &prefix, const std::string &id_name, const std::string &id_prefix, int tag = -1);
	CPyList AddModule(const std::string &module_name, const std::string &list_name, const std::string &prefix, int tag = -1);
	CPyList AddModule(const std::string &module_name, const std::string &prefix, int tag = -1);
	int GetId(const std::string &type, const CPyObject &obj, const std::string &context);
	unsigned long long GetOperandId(const CPyObject &obj, const std::string &context);
	std::string GetResource(const CPyObject &obj, int resource_type, const std::string &context);
	long long ParseOperand(const CPyObject &statement, int pos);
	static void PrepareModule(const std::string &name);
	void Warning(int level, const std::string &text, const std::string &context = "");
	void WriteAnimations();
	void WriteDialogs();
	void WriteFactions();
	void WriteFloraKinds();
	void WriteGlobalVars();
	void WriteGroundSpecs();
	void WriteInfoPages();
	void WriteItems();
	void WriteMapIcons();
	void WriteMenus();
	void WriteMeshes();
	void WriteMissionTemplates();
	void WriteMusic();
	void WritePresentations();
	void WriteQuests();
	void WriteQuickStrings();
	void WriteParticleSystems();
	void WriteParties();
	void WritePartyTemplates();
	void WritePostEffects();
	void WriteSceneProps();
	void WriteScenes();
	void WriteScripts();
	void WriteSimpleTriggers();
	void WriteSkills();
	void WriteSkins();
	void WriteSkyboxes();
	void WriteSounds();
	void WriteStrings();
	void WriteTableaus();
	void WriteTriggers();
	void WriteTroops();
	void WriteSimpleTriggerBlock(const CPyObject &simple_trigger_block, std::ostream &stream, const std::string &context);
	void WriteSimpleTrigger(const CPyObject &simple_trigger, std::ostream &stream, const std::string &context);
	void WriteTriggerBlock(const CPyObject &trigger_block, std::ostream &stream, const std::string &context);
	void WriteTrigger(const CPyObject &trigger, std::ostream &stream, const std::string &context);
	bool WriteStatementBlock(const CPyObject &statement_block, std::ostream &stream, const std::string &context);
	void WriteStatement(const CPyObject &statement, std::ostream &stream, int &depth, bool &fails_at_zero);

	int m_pass;
	std::string m_input_path;
	std::string m_output_path;
	unsigned long long m_flags;
	std::map<std::string, unsigned long long> m_tags;
	std::map<std::string, std::map<std::string, int>> m_ids;
	std::map<std::string, std::map<std::string, int>> m_uses;
	CPyList m_animations;
	CPyList m_dialogs;
	CPyList m_factions;
	CPyList m_flora_kinds;
	CPyList m_game_menus;
	CPyList m_ground_specs;
	CPyList m_info_pages;
	CPyList m_items;
	CPyList m_map_icons;
	CPyList m_meshes;
	CPyList m_music;
	CPyList m_mission_templates;
	CPyList m_particle_systems;
	CPyList m_parties;
	CPyList m_party_templates;
	CPyList m_postfx;
	CPyList m_presentations;
	CPyList m_quests;
	CPyList m_scene_props;
	CPyList m_scenes;
	CPyList m_scripts;
	CPyList m_simple_triggers;
	CPyList m_skills;
	CPyList m_skins;
	CPyList m_skyboxes;
	CPyList m_sounds;
	CPyList m_strings;
	CPyList m_tableau_materials;
	CPyList m_triggers;
	CPyList m_troops;
	unsigned int m_operations[MAX_NUM_OPCODES];
	int m_operation_depths[MAX_NUM_OPCODES];
	std::map<std::string, Variable> m_global_vars;
	std::map<std::string, Variable> m_local_vars;
	std::map<std::string, QuickString> m_quick_strings;
	std::map<int, std::map<std::string, int>> m_resources;
	std::map<std::string, bool> m_referencedScripts;
	std::string m_cur_context;
	int m_cur_statement;
#if defined _WIN32
	CONSOLE_SCREEN_BUFFER_INFO m_console_info;
	HANDLE m_console_handle;
#endif
};
