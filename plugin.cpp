#include "SkyPrompt/API.hpp";

using Flag = RE::OBJ_BOOK::Flag;

SkyPromptAPI::Prompt readPrompt{"Read Me now", 0, 0, SkyPromptAPI::PromptType::kHold};
std::array<SkyPromptAPI::Prompt, 1> prompts = {readPrompt};
SkyPromptAPI::ClientID clientID;
std::pair<RE::INPUT_DEVICE, uint32_t> key{INPUT_DEVICE::kKeyboard, 45};
PlayerCharacter* player;
FormID playerID;

TESObjectBOOK *lastBook;

void SetCantTakeFlag() {
    lastBook->data.flags |= Flag::kCantTake;
}

void ClearCantTakeFlag() {
    lastBook->data.flags &= static_cast<Flag>(~static_cast<std::underlying_type_t<Flag>>(Flag::kCantTake));
}

void RegisterForBookClose();
void UnregisterForBookClose();

class MyPromptSink : public SkyPromptAPI::PromptSink {
public:
    std::span<const SkyPromptAPI::Prompt> GetPrompts() const override { return prompts; };

    void ProcessEvent(SkyPromptAPI::PromptEvent event) const override {
        if (event.type == SkyPromptAPI::PromptEventType::kAccepted) {
            TESObjectREFRPtr spawn = player->PlaceObjectAtMe(lastBook, false);
            if (spawn) {
                TESObjectREFR* tempRef = spawn.get();
                // temporarily mark the spawned book as CantTake so the player
                // cannot pick up the spawned ref while reading
                SetCantTakeFlag();
                tempRef->ActivateRef(PlayerCharacter::GetSingleton(), 0, lastBook, 1, false);
                tempRef->Disable();
                tempRef->SetDelete(true);
                RegisterForBookClose();
            }
        }
    };
};

static MyPromptSink g_PromptSink;

class EventHandler : public BSTEventSink<TESContainerChangedEvent>, public BSTEventSink<MenuOpenCloseEvent> {
    BSEventNotifyControl ProcessEvent(const TESContainerChangedEvent *event,
                                      BSTEventSource<TESContainerChangedEvent> *) {
        if (!event || event->newContainer != playerID) return BSEventNotifyControl::kContinue;
        TESObjectBOOK *book = TESForm::LookupByID<TESObjectBOOK>(event->baseObj);
        if (!book || book->IsRead()) return BSEventNotifyControl::kContinue;
        lastBook = book;
        SkyPromptAPI::RemovePrompt(&g_PromptSink, clientID);
        prompts[0].button_key = std::span{&key, 1};
        SkyPromptAPI::SendPrompt(&g_PromptSink, clientID);
        return BSEventNotifyControl::kContinue;
    }

    BSEventNotifyControl ProcessEvent(const MenuOpenCloseEvent *event, BSTEventSource<MenuOpenCloseEvent> *) {
        if (lastBook && !event->opening && event->menuName == BookMenu::MENU_NAME) {
            ClearCantTakeFlag();
            UnregisterForBookClose();
        }
        return BSEventNotifyControl::kContinue;
    }
};

static EventHandler g_sink;

void RegisterForBookClose() { UI::GetSingleton()->AddEventSink<MenuOpenCloseEvent>(&g_sink); };
void UnregisterForBookClose() { UI::GetSingleton()->RemoveEventSink<MenuOpenCloseEvent>(&g_sink); }

void Setup() {
    player = PlayerCharacter::GetSingleton();
    playerID = player->GetFormID();
    clientID = SkyPromptAPI::RequestClientID();
    ScriptEventSourceHolder::GetSingleton()->AddEventSink<TESContainerChangedEvent>(&g_sink);
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SKSE::Init(skse);

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message *message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) Setup();
    });

    return true;
}