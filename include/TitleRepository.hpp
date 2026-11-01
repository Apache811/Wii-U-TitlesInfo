#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <SDL.h>
#include <coreinit/mcp.h>

enum class StorageLocation : uint8_t
{
    Unknown,
    Nand,
    Usb
};

enum class TitleRegion : uint8_t
{
    All,
    Eur,
    Usa,
    Jap,
    EurUsa,
    EurJap,
    JapUsa
};

enum class TitleKind : uint8_t
{
    Game,
    VirtualConsole,
    Update,
    Dlc,
    Wii,
    SystemOrOther
};

struct InstalledTitle
{
    uint64_t titleId = 0;
    uint16_t version = 0;
    std::string name;
    std::string productCode;
    TitleRegion region = TitleRegion::All;
    std::string path;
    std::string iconPath;
    StorageLocation storage = StorageLocation::Unknown;
    TitleKind kind = TitleKind::SystemOrOther;
    std::string notes;
    uint16_t latestVersion = 0;
    bool hasLatestVersion = false;
    // True when the JSON contains the relevant version record but cannot provide a trustworthy latest version.
    bool latestVersionDataError = false;
    bool hasDatabaseRegion = false;
    uint32_t appType = 0;
    SDL_Texture* iconTexture = nullptr;
    bool iconLoadAttempted = false;
};

class TitleRepository
{
public:
    bool BeginLoad(std::string& errorMessage);
    bool LoadStep(std::string& errorMessage);
    bool IsLoading() const;
    void ReleaseGraphicsResources();
    std::vector<InstalledTitle>& Titles();
    const std::vector<InstalledTitle>& Titles() const;

private:
    std::vector<InstalledTitle> titles_;
    std::vector<MCPTitleListType> pendingTitleList_;
    uint32_t pendingTitleCount_ = 0;
    uint32_t pendingTitleIndex_ = 0;
    bool loading_ = false;

    static StorageLocation GetStorageLocation(const char* indexedDevice);
    static TitleKind GetTitleKind(uint64_t titleId, uint32_t appType, const std::string& contentPlatform);
    static TitleRegion GetTitleRegion(const std::string& metadataRegion);
    static bool IsRelevantTitle(uint64_t titleId);
    static std::string ToFilesystemPath(const std::string& volumePath);
    static std::string ReadTitleName(uint64_t titleId, const std::string& titlePath, const std::string& language);
    static std::string GetSystemLanguage();
};
