// NorseDistributor - SPID-like distribution for NORSE: Oath of Blood (UE4SS v4.0.0 C++ mod).
//
// Rules add (or remove) entries in the list fields of every loaded data asset of a class that passes the rule's filters
// - abilities, passives, traits or utility items onto unit types, by name, folder, field content (archetype, faction)
// and chance. Nothing in the paks is edited; the game builds each unit from its (now extended) unit asset.
//
// Rule files: <this mod>\distributions\*.ini, read in file-name order. Optional [ClassName] sections pick the asset
// class (default CombatUnit). One rule per line, SPID-style, every filter optional:
//   <List field> = <value> | <names> | <folders> | <field filters> | <chance %>
//   ActiveAbilities = /Game/Norse/Core/Data/Abilitites/Alert.Alert | Basic_*,-Basic_Warrior | EnemyTypes | Archetype:Basic | 100
//   ActiveAbilities = -/Game/Norse/Core/Data/Abilitites/Alert.Alert | Basic_Archer        (a leading - removes)
// names: the asset's name, * wildcards, -name excludes. folders: text in the asset path, -text excludes.
// field filters: Field:text - that field's value (as Unreal writes it) contains text; -Field:text excludes.
// chance: a fixed per-unit-type roll (the same every start), 100 = always.
// Applied at every map load and by the console commands nd.reload / nd.apply; nd.dump <object path> lists fields.
#include "NorseReflect.hpp"
#include "NorseManifest.hpp"
#include "NorseFilters.hpp"

#include <map>
#include <set>

namespace fs = std::filesystem;
using namespace nrn;
using namespace nrn::ue;
using namespace norse;

namespace
{
    struct Config
    {
        int LogLevel = 2;
        bool ApplyOnMapLoad = true;
        std::wstring LogDir;
    } g_cfg;

    fs::path g_mod, g_out;

    fs::path ModDir()
    {
        HMODULE self = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&ModDir), &self);
        wchar_t buf[MAX_PATH * 2]{};
        GetModuleFileNameW(self, buf, static_cast<DWORD>(std::size(buf)));
        return fs::path(buf).parent_path().parent_path();
    }

    void LoadConfig(const fs::path& ini)
    {
        g_cfg = Config{};
        for (const auto& raw : ReadLines(ini))
        {
            std::wstring l = Trim(StripComment(raw));
            if (l.empty() || l[0] == L'[') continue;
            size_t eq = l.find(L'=');
            if (eq == std::wstring::npos) continue;
            std::wstring k = Trim(l.substr(0, eq)), v = Trim(l.substr(eq + 1));
            if (k == L"uLogLevel") g_cfg.LogLevel = _wtoi(v.c_str());
            else if (k == L"bApplyOnMapLoad") g_cfg.ApplyOnMapLoad = _wtoi(v.c_str()) != 0;
            else if (k == L"sLogDir") g_cfg.LogDir = v;
        }
    }

    // ------------------------------------------------------------------ rules
    struct Rule
    {
        std::wstring cls, field, value;
        std::wstring when; // [Unit] sections: a NorseConditions condition about the live unit (refinement 2026-11-18)
        bool remove = false;
        Filters f; // NorseFilters.hpp
        std::string where;
    };
    std::vector<Rule> g_rules;

    void LoadRules()
    {
        g_rules.clear();
        fs::path dir = g_mod / "distributions";
        std::vector<fs::path> files;
        std::error_code ec;
        if (fs::is_directory(dir, ec))
            for (const auto& f : fs::directory_iterator(dir, ec))
                if (f.is_regular_file() && Lower(f.path().extension().wstring()) == L".ini") files.push_back(f.path());
        // NorseManifest: files of switched-off packs are skipped; the rest are read in the packs' load order, then by
        // file name; a file no pack claims is read last (ue4ss\NorsePacks\*.ini, include/NorseManifest.hpp)
        {
            nm::Manifest man;
            man.Resolve(g_mod.parent_path().parent_path());
            files = man.Arrange(files, [](const std::string& s) { NRN_INFO("load order: %s", s.c_str()); });
        }
        for (const auto& f : files)
        {
            std::wstring cls = L"CombatUnit", when;
            int n = 0;
            for (const auto& raw : ReadLines(f))
            {
                ++n;
                std::wstring l = Trim(StripComment(raw));
                if (l.empty()) continue;
                std::string where = Narrow(f.filename().wstring()) + ":" + std::to_string(n);
                if (l.front() == L'[' && l.back() == L']')
                {
                    cls = Trim(l.substr(1, l.size() - 2));
                    when.clear();
                    continue;
                }
                if (_wcsicmp(cls.c_str(), L"Unit") == 0 && _wcsnicmp(l.c_str(), L"When", 4) == 0 && l.find(L'=') != std::wstring::npos)
                {
                    when = Trim(l.substr(l.find(L'=') + 1));
                    continue;
                }
                size_t eq = l.find(L'=');
                if (eq == std::wstring::npos || eq == 0)
                {
                    NRN_WARN("%s: not a [Class] or List = value | names | folders | fields | chance line - skipped", where.c_str());
                    continue;
                }
                Rule r;
                r.cls = cls;
                r.when = when;
                r.where = where;
                r.field = Trim(l.substr(0, eq));
                auto parts = SplitList(l.substr(eq + 1), L'|');
                r.value = parts[0];
                if (!r.value.empty() && r.value[0] == L'-')
                {
                    r.remove = true;
                    r.value = Trim(r.value.substr(1));
                }
                if (r.value.empty())
                {
                    NRN_WARN("%s: no value to distribute - skipped", where.c_str());
                    continue;
                }
                if (parts.size() > 1) r.f.names = ParseFilter(parts[1]);
                if (parts.size() > 2) r.f.folders = ParseFilter(parts[2]);
                if (parts.size() > 3) r.f.fields = ParseFields(parts[3], where);
                if (parts.size() > 4) r.f.chance = ParseChance(parts[4]);
                g_rules.push_back(std::move(r));
            }
        }
        NRN_INFO("rules: %zu file(s) in %s, %zu rule(s)", files.size(), Narrow(dir.wstring()).c_str(), g_rules.size());
    }

    // Top-level elements of an exported array "(a,b,(c,d))".
    std::vector<std::wstring> Elements(const std::wstring& arr)
    {
        std::vector<std::wstring> out;
        std::wstring s = Trim(arr);
        if (s.size() < 2 || s.front() != L'(' || s.back() != L')') return out;
        s = s.substr(1, s.size() - 2);
        int depth = 0;
        bool q = false;
        std::wstring cur;
        for (wchar_t c : s)
        {
            if (c == L'"') q = !q;
            if (!q && c == L'(') ++depth;
            if (!q && c == L')') --depth;
            if (!q && depth == 0 && c == L',')
            {
                out.push_back(Trim(cur));
                cur.clear();
                continue;
            }
            cur += c;
        }
        if (!Trim(cur).empty()) out.push_back(Trim(cur));
        return out;
    }

    // An element compared as a path: quotes and a Class'...' wrapper dropped, lower-case.
    std::wstring Norm(std::wstring e)
    {
        e = Trim(e);
        size_t a = e.find(L'\''), b = e.rfind(L'\'');
        if (a != std::wstring::npos && b > a) e = e.substr(a + 1, b - a - 1);
        e.erase(std::remove(e.begin(), e.end(), L'"'), e.end());
        return Lower(Trim(e));
    }

    struct Tally
    {
        int changed = 0, already = 0, filtered = 0, failed = 0;
    };

    void ApplyRule(const Rule& r, const std::vector<UObject*>& objs, Tally& t)
    {
        std::wstring want = Norm(r.value);
        for (UObject* obj : objs)
        {
            std::wstring full = obj->GetFullName(nullptr);
            std::wstring path = full.substr(full.find(L' ') + 1);
            std::wstring name = ShortName(path);
            if (name.rfind(L"Default__", 0) == 0) continue;
            auto* cls = reinterpret_cast<UStruct*>(Reflect::Get().ClassOf(obj));
            if (!cls) continue;
            if (!Rejects(r.f, obj, cls, name, path, name + L"|" + r.value).empty())
            {
                ++t.filtered;
                continue;
            }
            FProperty* p = FindProp(cls, r.field);
            if (!p || CppType(p).rfind(L"TArray", 0) != 0)
            {
                NRN_WARN("%s: %s has no list field '%s' (nd.dump %s lists them)", r.where.c_str(), Narrow(name).c_str(), Narrow(r.field).c_str(),
                         Narrow(path).c_str());
                ++t.failed;
                return; // the same class for every object: one warning
            }
            uint8_t* ptr = reinterpret_cast<uint8_t*>(obj) + p->GetOffset_Internal();
            std::wstring before = Export(p, ptr, obj);
            auto els = Elements(before);
            auto it = std::find_if(els.begin(), els.end(), [&](const std::wstring& e) { return Norm(e) == want; });
            if (r.remove ? it == els.end() : it != els.end())
            {
                ++t.already;
                continue;
            }
            if (r.remove) els.erase(it);
            else els.push_back(r.value.front() == L'(' || r.value.front() == L'"' ? r.value : L"\"" + r.value + L"\"");
            std::wstring text = L"(";
            for (size_t i = 0; i < els.size(); ++i) text += (i ? L"," : L"") + els[i];
            text += L")";
            bool faulted = false;
            const wchar_t* ok = SehImport(p, text.c_str(), ptr, obj, &faulted);
            size_t now = Elements(Export(p, ptr, obj)).size();
            if (!ok || faulted || now != els.size())
            {
                NRN_WARN("%s: %s.%s - Unreal did not accept the list (%zu entries expected, %zu now)%s", r.where.c_str(), Narrow(name).c_str(),
                         Narrow(r.field).c_str(), els.size(), now, faulted ? " (fault caught)" : "");
                ++t.failed;
                continue;
            }
            NRN_INFO("%s: %s.%s %s %s (%zu entries)", r.where.c_str(), Narrow(name).c_str(), Narrow(r.field).c_str(), r.remove ? "-" : "+",
                     Narrow(ShortName(want)).c_str(), now);
            RecordWrite(obj, p, ptr, text, name + L"." + r.field); // re-imported at a map load if its asset was freed (refinement)
            ++t.changed;
        }
    }

    // ---- [Unit]: live units, the player's party too (refinement 2026-11-18). The party is not built from the CombatUnit
    // assets the other rules change, so a [Unit] rule edits each live unit's UnitComponent lists (ActiveAbilities,
    // PassiveAbilities, ...) and then has the game arm them (InitializePassives, UpdateActiveAbilities). A When line
    // (NorseConditions, about the unit) picks the units. Run 2 s after each map load and by nd.apply.
    using NcCompileFn = int (*)(const char*, char*, int);
    using NcEvalFn = int (*)(int, void*, void*);
    template <typename F>
    F ModExport(const wchar_t* mod, const char* name)
    {
        HMODULE m = GetModuleHandleW((g_mod.parent_path() / mod / L"dlls" / L"main.dll").c_str());
        return m ? reinterpret_cast<F>(GetProcAddress(m, name)) : nullptr;
    }
    std::map<std::wstring, int> g_conds; // condition text -> NorseConditions handle (0 = did not compile)

    std::vector<UObject*> LiveUnitComponents(const std::wstring& when, int& unitsSeen)
    {
        std::vector<UObject*> out;
        static NcCompileFn compile = nullptr;
        static NcEvalFn eval = nullptr;
        if (!compile) compile = ModExport<NcCompileFn>(L"NorseConditions", "NC_Compile");
        if (!eval) eval = ModExport<NcEvalFn>(L"NorseConditions", "NC_Eval");
        int cond = 0;
        if (!when.empty())
        {
            auto it = g_conds.find(when);
            if (it == g_conds.end())
            {
                char err[256]{};
                cond = compile ? compile(Narrow(when).c_str(), err, sizeof(err)) : 0;
                if (!cond) NRN_ERROR("[Unit] When = %s: %s - those rules are off", Narrow(when).c_str(), compile ? err : "NorseConditions is not loaded");
                g_conds[when] = cond;
            }
            else cond = it->second;
            if (!cond) return out;
        }
        std::vector<UObject*> units;
        try
        {
            RC::Unreal::UObjectGlobals::FindAllOf(L"NorseCharacter_Unit", units);
        }
        catch (...)
        {
        }
        for (UObject* u : units)
        {
            if (!Alive(u, IndexOf(u)) || ShortName(u->GetFullName(nullptr)).rfind(L"Default__", 0) == 0) continue;
            ++unitsSeen;
            if (cond && !(eval && eval(cond, u, nullptr))) continue;
            auto** uc = Reflect::Get().Prop<UObject*>(u, L"UnitComponent");
            if (uc && *uc) out.push_back(*uc);
        }
        return out;
    }

    void ApplyUnits(const char* why)
    {
        std::set<UObject*> touched;
        int total = 0, rules = 0;
        for (const Rule& r : g_rules)
        {
            if (_wcsicmp(r.cls.c_str(), L"Unit") != 0) continue;
            ++rules;
            int seen = 0;
            std::vector<UObject*> comps = LiveUnitComponents(r.when, seen);
            Tally t;
            try
            {
                ApplyRule(r, comps, t);
            }
            catch (...)
            {
                NRN_ERROR("%s threw - skipped", r.where.c_str());
            }
            if (t.changed)
                for (UObject* c : comps) touched.insert(c);
            NRN_INFO("%s: [Unit] %s %s on %zu of %d live unit(s) (When %s) -> %d changed, %d already so", r.where.c_str(), r.remove ? "remove" : "add",
                     Narrow(ShortName(r.value)).c_str(), comps.size(), seen, r.when.empty() ? "-" : Narrow(r.when).c_str(), t.changed, t.already);
            total += t.changed;
        }
        for (UObject* c : touched)
        {
            Buf b;
            Reflect::Get().Call(c, L"InitializePassives", b.b, 0, sizeof(b.b));
            Buf b2;
            Reflect::Get().Call(c, L"UpdateActiveAbilities", b2.b, 0, sizeof(b2.b));
        }
        if (rules) NRN_INFO("live units (%s): %d change(s) from %d [Unit] rule(s), %zu unit(s) re-armed", why, total, rules, touched.size());
    }
    ULONGLONG g_unitsAt = 0;

    void ApplyAll(const char* why)
    {
        // Tag: filters are answered by NorseTagger - have it tag this map's assets first (its own map-load pass may run
        // after this one)
        if (std::any_of(g_rules.begin(), g_rules.end(), [](const Rule& r) { return r.f.UsesTags(); }))
        {
            if (const TaggerApi* t = Tagger()) t->Apply();
            else NRN_WARN("Tag: filters used but NorseTagger is not loaded - those filters match nothing");
        }
        std::map<std::wstring, std::vector<UObject*>> byClass;
        int total = 0;
        for (const Rule& r : g_rules)
        {
            if (_wcsicmp(r.cls.c_str(), L"Unit") == 0) continue; // the live layer, ApplyUnits
            auto& objs = byClass[r.cls];
            if (objs.empty())
            {
                try
                {
                    RC::Unreal::UObjectGlobals::FindAllOf(r.cls.c_str(), objs);
                }
                catch (...)
                {
                }
                NRN_INFO("%s: %zu loaded %s asset(s)", why, objs.size(), Narrow(r.cls).c_str());
            }
            Tally t;
            try
            {
                ApplyRule(r, objs, t);
            }
            catch (...)
            {
                NRN_ERROR("%s threw - skipped", r.where.c_str());
            }
            NRN_INFO("%s: %s %s -> %d changed, %d already so, %d filtered out%s", r.where.c_str(), r.remove ? "remove" : "add",
                     Narrow(ShortName(r.value)).c_str(), t.changed, t.already, t.filtered, t.failed ? ", FAILED on some" : "");
            total += t.changed;
        }
        NRN_INFO("distributed (%s): %d change(s) from %zu rule(s)", why, total, g_rules.size());
    }

    bool RunCommand(const std::wstring& c)
    {
        std::wstring cmd = Trim(c), arg;
        if (size_t sp = cmd.find(L' '); sp != std::wstring::npos)
        {
            arg = Trim(cmd.substr(sp + 1));
            cmd = cmd.substr(0, sp);
        }
        cmd = Lower(cmd);
        NRN_INFO("command: %s %s", Narrow(cmd).c_str(), Narrow(arg).c_str());
        if (cmd == L"nd.reload")
        {
            LoadRules();
            ApplyAll("nd.reload");
        }
        else if (cmd == L"nd.apply")
        {
            ApplyAll("nd.apply");
            ApplyUnits("nd.apply");
        }
        else if (cmd == L"nd.dump" && !arg.empty()) Dump(arg, g_out);
        else NRN_WARN("unknown command - nd.reload | nd.apply | nd.dump <object path>");
        return true;
    }
} // namespace

class NorseDistributor : public RC::CppUserModBase
{
  public:
    NorseDistributor()
    {
        ModName = L"NorseDistributor";
        ModVersion = L"" ND_VERSION;
        ModDescription = L"SPID-like distribution: rules add abilities, passives, traits and items to unit types by name, folder, field and chance.";
        ModAuthors = L"ApocryphaRealm";
        for (auto& s : g_nopVtbl) s = reinterpret_cast<void*>(&NopFn);
        g_errOut.vtbl = g_nopVtbl;
        g_mod = ModDir();
        LoadConfig(g_mod / "NorseDistributor.ini");
        g_out = g_cfg.LogDir.empty() ? g_mod : fs::path(g_cfg.LogDir);
        Log::Get().Open(g_out / "NorseDistributor.log", g_cfg.LogLevel);
        Log::Get().Write(Level::Error, "NorseDistributor v%s - log level %d (2 = info: every change is logged)", ND_VERSION, g_cfg.LogLevel);
        LoadRules();
    }

    // Callbacks only from on_unreal_init: registered in the constructor they are dropped silently (NorsePatcher 0.1.0).
    auto on_unreal_init() -> void override
    {
        NRN_INFO("on_unreal_init: registering InitGameState and console callbacks");
        try
        {
            RC::Unreal::Hook::RegisterEngineTickPreCallback([](RC::Unreal::UEngine*, float) {
                if (!g_unitsAt || GetTickCount64() < g_unitsAt) return;
                g_unitsAt = 0;
                try
                {
                    ApplyUnits("map load");
                }
                catch (...)
                {
                    NRN_ERROR("live-unit distribution threw");
                }
            });
            RC::Unreal::Hook::RegisterInitGameStatePostCallback([](RC::Unreal::AGameModeBase*) {
                try
                {
                    RecheckWrites("map load"); // before the pass, which exports first
                    if (g_cfg.ApplyOnMapLoad) ApplyAll("map load");
                    g_unitsAt = GetTickCount64() + 2000; // the live layer once the units exist
                    g_conds.clear();
                }
                catch (...)
                {
                    NRN_ERROR("map-load distribution threw");
                }
            });
            RC::Unreal::Hook::RegisterProcessConsoleExecCallback(
                    [](UObject*, const wchar_t* cmd, RC::Unreal::FOutputDevice&, UObject*) -> bool {
                        if (!cmd || _wcsnicmp(cmd, L"nd.", 3) != 0) return false;
                        static ULONGLONG last = 0;
                        static std::wstring lastCmd;
                        ULONGLONG now = GetTickCount64();
                        if (lastCmd == cmd && now - last < 250) return true;
                        last = now;
                        lastCmd = cmd;
                        try
                        {
                            return RunCommand(cmd);
                        }
                        catch (...)
                        {
                            NRN_ERROR("command threw");
                            return true;
                        }
                    });
            NRN_INFO("callbacks registered (distribute at map load: %d)", g_cfg.ApplyOnMapLoad);
        }
        catch (const std::exception& e)
        {
            NRN_ERROR("callback registration failed: %s", e.what());
        }
    }
};

extern "C" __declspec(dllexport) RC::CppUserModBase* start_mod()
{
    return new NorseDistributor();
}

extern "C" __declspec(dllexport) void uninstall_mod(RC::CppUserModBase* mod)
{
    delete mod;
}
