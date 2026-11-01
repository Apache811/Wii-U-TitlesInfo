#include <whb/proc.h>
#include <whb/log_udp.h>
#include <vpad/input.h>
#include <mocha/mocha.h>
#include <nn/acp.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "DebugConfig.hpp"
#include "Log.hpp"
#include "InputHandler.hpp"
#include "TitleRepository.hpp"
#include "TitleDatabase.hpp"
#include "UserInterface.hpp"
#include "SearchKeyboard.hpp"

namespace
{
void LogMessage(const char* message)
{
    TitleInfoLog("Main", message);
}

void CleanupStorage()
{
    LogMessage("Cleanup: unmounting USB title volume.");
    Mocha_UnmountFS("storage_usb01");
    LogMessage("Cleanup: unmounting NAND title volume.");
    Mocha_UnmountFS("storage_mlc01");
    LogMessage("Cleanup: storage volumes unmounted.");
}

#if __DEBUG__
// Synthetic rows exercise every version-status result without depending on the
// titles physically installed on the console. Database metadata is applied to
// this separate collection so debug rows cannot affect installed GAME/UPDATE
// filtering or remove real rows.
void AppendDebugTitles(TitleDatabase& database, std::vector<InstalledTitle>& installedTitles)
{
    std::vector<InstalledTitle> samples;
    samples.reserve(6);

    auto add = [&samples](uint64_t titleId, const char* name, const char* productCode,
                          uint16_t installedVersion, StorageLocation storage)
    {
        InstalledTitle title;
        title.titleId = titleId;
        title.name = name;
        title.productCode = productCode;
        title.version = installedVersion;
        title.storage = storage;
        title.kind = (titleId >> 32) == 0x0005000EULL ? TitleKind::Update : TitleKind::Game;
        title.region = TitleRegion::All;
        title.iconPath.clear();
        samples.push_back(title);
    };

    // Alternating NAND/USB storage is deliberate, including titles absent from
    // the actual console, so both storage filters can be verified in debug.
    add(0x00050000101C9500ULL, "Zelda: Breath of the Wild [DBG OK]",
        "WUP-P-ALZP", 208, StorageLocation::Nand);
    add(0x000500001019E600ULL, "Zelda: Twilight Princess HD [DBG UPDATE]",
        "WUP-P-AZAP", 64, StorageLocation::Usb);
    add(0x0005000010143600ULL, "Zelda: Wind Waker HD [DBG N/A]",
        "WUP-P-BCZP", 0, StorageLocation::Nand);
    add(0x00050000101C9500ULL, "Zelda: Breath of the Wild [DBG STALE]",
        "WUP-P-ALZP", 224, StorageLocation::Usb);
    add(0x000500001019E600ULL, "Zelda: Twilight Princess HD [DBG ERROR]",
        "WUP-P-AZAP", 64, StorageLocation::Nand);
    add(0x0005000E101C9500ULL, "Zelda: BOTW Update [DBG N/A]",
        "WUP-P-ALZP", 192, StorageLocation::Usb);

    // Apply GAME samples separately from the UPDATE sample: the normal Apply
    // workflow intentionally hides a GAME when its UPDATE is installed.
    InstalledTitle updateSample = samples.back();
    samples.pop_back();
    database.Apply(samples);
    std::vector<InstalledTitle> updateOnly;
    updateOnly.push_back(updateSample);
    database.Apply(updateOnly);
    samples.push_back(updateOnly[0]);

    // Make the test cases deterministic even if the bundled database is updated.
    samples[0].version = 208;
    samples[0].latestVersion = 208;
    samples[0].hasLatestVersion = true;
    samples[0].latestVersionDataError = false;

    samples[1].version = 64;
    samples[1].latestVersion = 81;
    samples[1].hasLatestVersion = true;
    samples[1].latestVersionDataError = false;

    samples[2].hasLatestVersion = false;
    samples[2].latestVersionDataError = false;

    samples[3].version = 224;
    samples[3].latestVersion = 208;
    samples[3].hasLatestVersion = true;
    samples[3].latestVersionDataError = true;

    samples[4].version = 64;
    samples[4].latestVersion = 81;
    samples[4].hasLatestVersion = true;
    samples[4].latestVersionDataError = true;

    samples[5].kind = TitleKind::Update;
    samples[5].hasLatestVersion = false;
    samples[5].latestVersionDataError = true;

    for (InstalledTitle& sample : samples)
        installedTitles.push_back(sample);

    char message[96];
    snprintf(message, sizeof(message), "Debug: appended %u synthetic title cards.",
             static_cast<unsigned>(samples.size()));
    LogMessage(message);
}
#endif
}

int main(int, char const**)
{
    WHBProcInit();
    WHBLogUdpInit();
    LogMessage("Startup: ProcUI and UDP logging initialized.");

    LogMessage("Startup: initializing VPAD.");
    VPADInit();
    LogMessage("Startup: VPAD initialized.");

    LogMessage("Startup: initializing Mocha.");
    MochaUtilsStatus mochaResult = Mocha_InitLibrary();
    if (mochaResult != MOCHA_RESULT_SUCCESS)
    {
        LogMessage("Mocha_InitLibrary failed.");
        VPADShutdown();
        WHBProcShutdown();
        WHBLogUdpDeinit();
        return -1;
    }
    LogMessage("Startup: Mocha initialized.");

    LogMessage("Startup: mounting USB title volume.");
    if (Mocha_MountFS("storage_usb01", nullptr, "/vol/storage_usb01") != MOCHA_RESULT_SUCCESS)
    {
        LogMessage("USB title volume mount failed; USB metadata/icons may be unavailable.");
    }

    LogMessage("Startup: mounting NAND title volume.");
    if (Mocha_MountFS("storage_mlc01", nullptr, "/vol/storage_mlc01") != MOCHA_RESULT_SUCCESS)
    {
        LogMessage("NAND title volume mount failed; NAND metadata/icons may be unavailable.");
    }

    LogMessage("Startup: initializing ACP.");
    ACPInitialize();
    LogMessage("Startup: ACPInitialize returned.");

    UserInterface ui;
    std::string errorMessage;
    LogMessage("Startup: initializing user interface.");
    if (!ui.Initialize(errorMessage))
    {
        LogMessage(errorMessage.c_str());
        ACPFinalize();
        CleanupStorage();
        Mocha_DeInitLibrary();
        VPADShutdown();
        WHBProcShutdown();
        WHBLogUdpDeinit();
        return -1;
    }

    LogMessage("Startup: user interface initialized.");
    SearchKeyboard searchKeyboard;
    if (!searchKeyboard.Initialize())
        LogMessage("Search: failed to initialize SDL native keyboard handling.");
    else
        LogMessage("Search: SDL native keyboard handling initialized.");

    TitleRepository repository;
    InputHandler input;
    int selectedIndex = 0;
    TitleListFilter filter = TitleListFilter::All;
    std::string searchText;
    std::string searchTextBeforeKeyboard;

    // Present the empty UI first, then show the refresh panel before any MCP
    // calls. BeginLoad can take time while querying the installed title list.
    LogMessage("Startup: presenting empty title list before initial load.");
    ui.Draw(repository, selectedIndex, filter, searchText);
    ui.DrawRefreshing(0);

    TitleDatabase titleDatabase;
    LogMessage("Startup: loading local title database.");
    if (!titleDatabase.Load(errorMessage))
        LogMessage(errorMessage.c_str());
    else
    {
        char databaseMessage[96];
        snprintf(databaseMessage, sizeof(databaseMessage), "Startup: local title database loaded (%u records).",
                 static_cast<unsigned>(titleDatabase.Size()));
        LogMessage(databaseMessage);
    }

    LogMessage("Startup: starting incremental installed-title load.");
    const uint32_t initialLoadStart = SDL_GetTicks();
    bool initialLoadSucceeded = repository.BeginLoad(errorMessage);
    while (initialLoadSucceeded && repository.IsLoading() && WHBProcIsRunning())
    {
        const uint32_t elapsed = SDL_GetTicks() - initialLoadStart;
        ui.DrawRefreshing(elapsed);

        // Consume and discard controller input while the refreshing screen is
        // active. No search, filter, refresh, or list navigation is allowed
        // during the initial title load.
        (void)input.Poll();
        initialLoadSucceeded = repository.LoadStep(errorMessage);
    }
    // Discard any trigger at the transition back to the interactive UI.
    (void)input.Poll();

    if (!initialLoadSucceeded)
    {
        LogMessage(errorMessage.c_str());
    }
    else if (!WHBProcIsRunning())
    {
        LogMessage("Startup: initial title load interrupted by application shutdown.");
    }
    else
    {
        const size_t loadedTitleCount = repository.Titles().size();
        char message[96];
        snprintf(message, sizeof(message), "Startup: title repository loaded (%u entries).", static_cast<unsigned>(loadedTitleCount));
        LogMessage(message);
    }

    if (titleDatabase.Size() > 0)
    {
        titleDatabase.Apply(repository.Titles());
        LogMessage("Startup: local metadata applied to installed titles.");
    }
#if __DEBUG__
    AppendDebugTitles(titleDatabase, repository.Titles());
#endif
#if __DEBUG_HEARTBEAT__
    unsigned long long frameCount = 0;
#endif
    LogMessage("Main loop: entering.");

    while (WHBProcIsRunning())
    {
#if __DEBUG_HEARTBEAT__
        ++frameCount;
        if (frameCount == 1)
        {
            LogMessage("Main loop: first iteration started.");
        }
        else if ((frameCount % 60) == 0)
        {
            char message[96];
            snprintf(message, sizeof(message), "Main loop heartbeat: frame %llu.", frameCount);
            LogMessage(message);
        }
#endif

        searchKeyboard.Update();
        InputState state;
        if (!searchKeyboard.IsActive())
            state = input.Poll();

        bool keyboardConfirmed = false;
        std::string keyboardResult;
        if (searchKeyboard.ConsumeResult(keyboardConfirmed, keyboardResult))
        {
            if (keyboardConfirmed)
            {
                searchText = keyboardResult;
                LogMessage("Search: query accepted.");
            }
            else
            {
                searchText = searchTextBeforeKeyboard;
                LogMessage("Search: query cancelled.");
            }
            selectedIndex = 0;
        }

        if (state.search && !searchKeyboard.IsActive())
        {
            searchTextBeforeKeyboard = searchText;
            if (!searchKeyboard.Request(searchText, "Search by game name or Title ID"))
                LogMessage("Search: failed to open the Wii U native software keyboard.");
            else
                LogMessage("Search: Wii U native software keyboard requested.");
        }

        if (!searchKeyboard.IsActive() && (state.filterPrevious || state.filterNext))
        {
#if __DEBUG__
            const int filterCount = 4;
#else
            const int filterCount = 3;
#endif
            int filterIndex = static_cast<int>(filter);
            filterIndex = (filterIndex + (state.filterNext ? 1 : filterCount - 1)) % filterCount;
            filter = static_cast<TitleListFilter>(filterIndex);
            selectedIndex = 0;
            const char* filterName = filter == TitleListFilter::All ? "ALL" :
                                     filter == TitleListFilter::Usb ? "USB" :
                                     filter == TitleListFilter::Nand ? "NAND" : "NONE";
            char message[96];
            snprintf(message, sizeof(message), "Input: storage filter changed to %s.", filterName);
            LogMessage(message);
        }

        if (!searchKeyboard.IsActive() && state.refresh)
        {
            LogMessage("Input: refresh requested; releasing title graphics resources.");
            repository.ReleaseGraphicsResources();
            LogMessage("Input: starting incremental title-list refresh.");

            // Present the refresh screen before BeginLoad performs MCP queries.
            ui.Draw(repository, selectedIndex, filter, searchText);
            ui.DrawRefreshing(0);
            const uint32_t refreshStart = SDL_GetTicks();
            bool refreshSucceeded = repository.BeginLoad(errorMessage);
            while (refreshSucceeded && repository.IsLoading() && WHBProcIsRunning())
            {
                const uint32_t elapsed = SDL_GetTicks() - refreshStart;
                ui.DrawRefreshing(elapsed);

                // Keep consuming controller input while refreshing. Inputs are
                // intentionally ignored so they cannot trigger search, another
                // refresh, storage-filter changes, or list scrolling.
                (void)input.Poll();
                refreshSucceeded = repository.LoadStep(errorMessage);
            }
            // Clear any trigger at the transition back to the interactive UI.
            (void)input.Poll();

            if (!refreshSucceeded)
            {
                LogMessage(errorMessage.c_str());
            }
            else if (!WHBProcIsRunning())
            {
                LogMessage("Input: refresh interrupted by application shutdown.");
            }
            else
            {
                char message[96];
                snprintf(message, sizeof(message), "Input: refresh complete (%u entries).", static_cast<unsigned>(repository.Titles().size()));
                LogMessage(message);
                titleDatabase.Apply(repository.Titles());
                LogMessage("Input: local metadata reapplied after refresh.");
#if __DEBUG__
                AppendDebugTitles(titleDatabase, repository.Titles());
#endif
            }
            selectedIndex = 0;
        }

        const int visibleRows = 3;
        if (!searchKeyboard.IsActive() && state.up)
        {
            selectedIndex = std::max(0, selectedIndex - 1);
        }
        if (!searchKeyboard.IsActive() && state.down)
        {
            const int maxSelectedIndex = std::max(0, ui.FilteredTitleCount(repository, filter, searchText) - 1);
            selectedIndex = std::min(maxSelectedIndex, selectedIndex + 1);
        }
        if (!searchKeyboard.IsActive() && state.pageUp)
        {
            selectedIndex = std::max(0, selectedIndex - visibleRows);
        }
        if (!searchKeyboard.IsActive() && state.pageDown)
        {
            const int maxSelectedIndex = std::max(0, ui.FilteredTitleCount(repository, filter, searchText) - 1);
            selectedIndex = std::min(maxSelectedIndex, selectedIndex + visibleRows);
        }

        ui.Draw(repository, selectedIndex, filter, searchText);
    }

    LogMessage("Main loop: exited; beginning shutdown.");
    searchKeyboard.Shutdown();

    LogMessage("Shutdown: releasing title graphics resources.");
    repository.ReleaseGraphicsResources();
    LogMessage("Shutdown: shutting down user interface.");
    ui.Shutdown();
    LogMessage("Shutdown: finalizing ACP.");
    ACPFinalize();
    CleanupStorage();
    LogMessage("Shutdown: deinitializing Mocha.");
    Mocha_DeInitLibrary();
    LogMessage("Shutdown: shutting down VPAD.");
    VPADShutdown();

    LogMessage("Shutdown: shutting down ProcUI.");
    WHBProcShutdown();
    LogMessage("Shutdown: deinitializing UDP logging.");
    WHBLogUdpDeinit();
    return 0;
}
