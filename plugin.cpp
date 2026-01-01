#include "SkyPrompt/API.hpp";
#include "includes/i18n.cpp";

using Flag = RE::OBJ_BOOK::Flag;

SkyPromptAPI::Prompt readPrompt{"", 0, 0, SkyPromptAPI::PromptType::kHold};
std::array<SkyPromptAPI::Prompt, 1> prompts = {readPrompt};
SkyPromptAPI::ClientID clientID;
std::array<std::pair<RE::INPUT_DEVICE, uint32_t>, 2> keys{std::pair{INPUT_DEVICE::kKeyboard, 0},
                                                          std::pair{INPUT_DEVICE::kGamepad, 0}};
PlayerCharacter* player;
FormID playerID;
std::string promptText;
TESQuest *readItNow;
BGSKeyword *exclusionKeyword;
std::string_view modName = "Read It Now.esp";
bool bSpellTomes = true;
bool bSkillBooks = true;

TESObjectBOOK *lastBook;
TESObjectREFR *tempRef;

void RegisterForBookClose();
void UnregisterForBookClose();

std::string str_replace(std::string_view haystack, std::string_view needle, std::string_view replacement) {
    if (needle.empty()) {
        return std::string(haystack);
    }

    std::string result;
    result.reserve(haystack.size());

    std::size_t pos = 0;
    while (true) {
        std::size_t found = haystack.find(needle, pos);
        if (found == std::string_view::npos) {
            result.append(haystack.substr(pos));
            break;
        }

        result.append(haystack.substr(pos, found - pos));
        result.append(replacement);
        pos = found + needle.size();
    }

    return result;
}

// this is handed via Papy so the RefAlias pointers don't need to be manually updated
void GiveBackTheBook(TESObjectREFR* a_ref) {
    auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) return;
    auto policy = vm->GetObjectHandlePolicy();
    if (!policy) return;
    RE::VMHandle handle = policy->GetHandleForObject(readItNow->GetFormType(), readItNow);
    if (handle == policy->EmptyHandle()) return;

    RE::BSFixedString scriptName = "ReadItNow_Script";
    RE::BSFixedString functionName = "GiveBackToPlayer";

    RE::BSTSmartPointer<RE::BSScript::Object> papyrusObject;
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> resultCallback;

    if (!vm->FindBoundObject(handle, scriptName.c_str(), papyrusObject)) {
        return;
    }

    auto args = RE::MakeFunctionArguments(std::move(a_ref));

    vm->DispatchMethodCall1(papyrusObject, functionName, args, resultCallback);
}

class MyPromptSink : public SkyPromptAPI::PromptSink {
public:
    std::span<const SkyPromptAPI::Prompt> GetPrompts() const override { return prompts; };

    void ProcessEvent(SkyPromptAPI::PromptEvent event) const override {
        if (event.type == SkyPromptAPI::PromptEventType::kAccepted) {
            if (lastBook->TeachesSpell()) {
                lastBook->Read(player);
            } else {
                auto refhandle = player->DropObject(lastBook, nullptr, 1);
                if (refhandle) {
                    tempRef = refhandle.get().get();
                    if (tempRef) {
                        tempRef->ActivateRef(player, 0, lastBook, 1, false);
                        tempRef->Disable();
                        RegisterForBookClose();
                    }
                }
            }
        }
    };
};

static MyPromptSink g_PromptSink;

class EventHandler : public BSTEventSink<TESContainerChangedEvent>, public BSTEventSink<MenuOpenCloseEvent> {
    BSEventNotifyControl ProcessEvent(const TESContainerChangedEvent *event,
                                      BSTEventSource<TESContainerChangedEvent> *) {
        if (!event || event->newContainer != playerID) return BSEventNotifyControl::kContinue;
        if (UI::GetSingleton()->IsMenuOpen(MainMenu::MENU_NAME) ||
            UI::GetSingleton()->IsMenuOpen(LoadingMenu::MENU_NAME))
            return BSEventNotifyControl::kContinue;
        TESObjectBOOK *book = TESForm::LookupByID<TESObjectBOOK>(event->baseObj);
        if (!book || book->IsRead() || book->HasKeyword(exclusionKeyword)) return BSEventNotifyControl::kContinue;
        if ((!bSkillBooks && book->TeachesSkill()) || (!bSpellTomes && book->TeachesSpell()))
            return BSEventNotifyControl::kContinue;

        lastBook = book;

        SkyPromptAPI::RemovePrompt(&g_PromptSink, clientID);
        prompts[0].text = str_replace(promptText, "<name>", lastBook->GetName());
        keys[0].second =
            ControlMap::GetSingleton()->GetMappedKey(UserEvents::GetSingleton()->readyWeapon, INPUT_DEVICE::kKeyboard);
        keys[1].second =
            ControlMap::GetSingleton()->GetMappedKey(UserEvents::GetSingleton()->readyWeapon, INPUT_DEVICE::kGamepad);
        prompts[0].button_key = std::span{keys};
        SkyPromptAPI::SendPrompt(&g_PromptSink, clientID);
        return BSEventNotifyControl::kContinue;
    }

    BSEventNotifyControl ProcessEvent(const MenuOpenCloseEvent *event, BSTEventSource<MenuOpenCloseEvent> *) {
        if (lastBook && !event->opening && event->menuName == BookMenu::MENU_NAME) {
            if (tempRef &&
                !tempRef->IsDeleted() // using the Take prompt in Book menu marks the ref as "deleted"
            ) {
                GiveBackTheBook(tempRef);
            }
            UnregisterForBookClose();
            lastBook = nullptr;
        }
        return BSEventNotifyControl::kContinue;
    }
};

static EventHandler g_sink;

void RegisterForBookClose() { UI::GetSingleton()->AddEventSink<MenuOpenCloseEvent>(&g_sink); };

void UnregisterForBookClose() { UI::GetSingleton()->RemoveEventSink<MenuOpenCloseEvent>(&g_sink); }

void LoadConfig() {
    CSimpleIniA ini;
    std::string filePath = "Data/SKSE/Plugins/ReadItNow.ini";
    if (ini.LoadFile(filePath.c_str()) == SI_OK) {
        bSpellTomes = ini.GetBoolValue("Main", "bShowForSpellTomes", true);
        bSkillBooks = ini.GetBoolValue("Main", "bShowForSkillBooks", true);
    }
}

void Setup() {
    readItNow = TESDataHandler::GetSingleton()->LookupForm<TESQuest>(0x800, modName);
    if (!readItNow) return;
    exclusionKeyword = TESDataHandler::GetSingleton()->LookupForm<BGSKeyword>(0x801, modName);
    if (!exclusionKeyword) return;
    player = PlayerCharacter::GetSingleton();
    playerID = player->GetFormID();
    clientID = SkyPromptAPI::RequestClientID();
    i18n::pluginName = "ReadItNow";
    promptText = i18n::GetTranslation("Main", "sReadPrompt");
    LoadConfig();
    ScriptEventSourceHolder::GetSingleton()->AddEventSink<TESContainerChangedEvent>(&g_sink);
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SKSE::Init(skse);

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message *message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) Setup();
    });

    return true;
}