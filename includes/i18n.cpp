#include <SimpleIni.h>

namespace i18n {
    std::string pluginName;

    static void to_lower_ascii(std::string& s) {
        for (char& c : s) {
            if (c >= 'A' && c <= 'Z') {
                c = c - 'A' + 'a';
            }
        }
    }

    static std::string GetGameLanguage() {
        static std::string language;
        if (!language.empty()) return language;

        language = "english";
        auto ini = INISettingCollection::GetSingleton();
        if (ini) {
            auto setting = ini->GetSetting("sLanguage:General");
            if (setting && setting->GetType() == RE::Setting::Type::kString) {
                language = setting->GetString();
                to_lower_ascii(language);
            }
        }
        return language;
    }

    static std::string GetTranslation(const std::string& a_section, const std::string& a_key,
                               const std::string& a_default = "") {
        std::string result = "";
        CSimpleIniA ini;
        ini.SetUnicode();
        std::string filePath = fmt::format("Data/SKSE/Plugins/{}/Translation/{}.ini", pluginName, GetGameLanguage());
        if (ini.LoadFile(filePath.c_str()) == SI_OK) {
            result = ini.GetValue(a_section.c_str(), a_key.c_str(), a_default.c_str());
        }
        return result;
    }
}