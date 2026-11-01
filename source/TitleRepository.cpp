#include "TitleRepository.hpp"
#include "TitleSort.hpp"

#include <coreinit/mcp.h>
#include "Log.hpp"
#include <nn/acp.h>
#include <malloc.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <inttypes.h>

namespace
{
void LogRepository(const char* message)
{
    TitleInfoLog("Repository", message);
}

constexpr uint64_t kBaseGameHigh = 0x00050000ULL;
constexpr uint64_t kVirtualConsoleHigh = 0x00050002ULL;
constexpr uint64_t kDlcHigh = 0x0005000CULL;
constexpr uint64_t kUpdateHigh = 0x0005000EULL;

uint64_t HighTitleId(uint64_t titleId)
{
    return titleId >> 32;
}

std::string NormalizeLanguage(const std::string& language)
{
    if (language == "0" || language == "ja" || language == "ja-JP") return "ja";
    if (language == "1" || language == "en" || language == "en-US") return "en";
    if (language == "2" || language == "fr" || language == "fr-FR") return "fr";
    if (language == "3" || language == "de" || language == "de-DE") return "de";
    if (language == "4" || language == "it" || language == "it-IT") return "it";
    if (language == "5" || language == "es" || language == "es-ES") return "es";
    if (language == "6" || language == "zh" || language == "zh-CN" || language == "zhs") return "zhs";
    if (language == "7" || language == "ko" || language == "ko-KR") return "ko";
    if (language == "8" || language == "nl" || language == "nl-NL") return "nl";
    if (language == "9" || language == "pt" || language == "pt-PT") return "pt";
    if (language == "10" || language == "ru" || language == "ru-RU") return "ru";
    if (language == "11" || language == "zh-TW" || language == "zht") return "zht";
    return "en";
}

void AppendUtf8(std::string& output, unsigned long codePoint)
{
    if (codePoint <= 0x7FUL)
        output.push_back(static_cast<char>(codePoint));
    else if (codePoint <= 0x7FFUL)
    {
        output.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
    else if (codePoint <= 0xFFFFUL && (codePoint < 0xD800UL || codePoint > 0xDFFFUL))
    {
        output.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
    else if (codePoint <= 0x10FFFFUL)
    {
        output.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

std::string DecodeXmlEntities(const std::string& input)
{
    std::string output;
    output.reserve(input.size());

    for (size_t i = 0; i < input.size();)
    {
        if (input[i] != '&')
        {
            output.push_back(input[i++]);
            continue;
        }

        const size_t semicolon = input.find(';', i + 1);
        if (semicolon == std::string::npos || semicolon - i > 12)
        {
            output.push_back(input[i++]);
            continue;
        }

        const std::string entity = input.substr(i + 1, semicolon - i - 1);
        if (entity == "amp") output.push_back('&');
        else if (entity == "apos") output.push_back('\'');
        else if (entity == "quot") output.push_back('"');
        else if (entity == "lt") output.push_back('<');
        else if (entity == "gt") output.push_back('>');
        else if (!entity.empty() && entity[0] == '#')
        {
            char* end = nullptr;
            const bool hex = entity.size() > 2 && (entity[1] == 'x' || entity[1] == 'X');
            const char* number = entity.c_str() + (hex ? 2 : 1);
            const unsigned long value = strtoul(number, &end, hex ? 16 : 10);
            if (end == number || *end != '\0' || value == 0 || value > 0x10FFFFUL ||
                (value >= 0xD800UL && value <= 0xDFFFUL))
            {
                output.append(input, i, semicolon - i + 1);
            }
            else
            {
                AppendUtf8(output, value);
            }
        }
        else
        {
            output.append(input, i, semicolon - i + 1);
        }

        i = semicolon + 1;
    }
    return output;
}

std::string LongNameFromMetadata(const ACPMetaXml& metadata, const std::string& language)
{
    const std::string lang = NormalizeLanguage(language);
    const char* localized = nullptr;
    if (lang == "ja") localized = metadata.longname_ja;
    else if (lang == "fr") localized = metadata.longname_fr;
    else if (lang == "de") localized = metadata.longname_de;
    else if (lang == "it") localized = metadata.longname_it;
    else if (lang == "es") localized = metadata.longname_es;
    else if (lang == "zhs") localized = metadata.longname_zhs;
    else if (lang == "ko") localized = metadata.longname_ko;
    else if (lang == "nl") localized = metadata.longname_nl;
    else if (lang == "pt") localized = metadata.longname_pt;
    else if (lang == "ru") localized = metadata.longname_ru;
    else if (lang == "zht") localized = metadata.longname_zht;
    else localized = metadata.longname_en;

    if (localized && localized[0] != '\0')
    {
        std::string value(localized);
        if (value.find_first_not_of(" \t\r\n") != std::string::npos)
            return DecodeXmlEntities(value);
    }
    if (metadata.longname_en[0] != '\0')
    {
        std::string value(metadata.longname_en);
        if (value.find_first_not_of(" \t\r\n") != std::string::npos)
            return DecodeXmlEntities(value);
    }
    if (metadata.shortname_en[0] != '\0')
    {
        std::string value(metadata.shortname_en);
        if (value.find_first_not_of(" \t\r\n") != std::string::npos)
            return DecodeXmlEntities(value);
    }
    return {};
}

std::string ReadXmlValue(const std::string& filePath, const char* tagName)
{
    FILE* file = fopen(filePath.c_str(), "rb");
    if (!file)
        return {};

    char line[1024];
    std::string result;
    const std::string tag(tagName);
    while (fgets(line, sizeof(line), file))
    {
        const char* startTag = strstr(line, tag.c_str());
        if (!startTag)
            continue;

        const char* valueStart = strchr(startTag, '>');
        if (!valueStart)
            continue;
        ++valueStart;

        const char* valueEnd = strchr(valueStart, '<');
        if (!valueEnd || valueEnd <= valueStart)
            continue;

        result.assign(valueStart, static_cast<size_t>(valueEnd - valueStart));
        const size_t firstNonWhitespace = result.find_first_not_of(" \t\r\n");
        if (firstNonWhitespace == std::string::npos)
        {
            result.clear();
            continue;
        }
        const size_t lastNonWhitespace = result.find_last_not_of(" \t\r\n");
        result = result.substr(firstNonWhitespace, lastNonWhitespace - firstNonWhitespace + 1);
        if (!result.empty())
            break;
    }

    fclose(file);
    return DecodeXmlEntities(result);
}
}

bool TitleRepository::BeginLoad(std::string& errorMessage)
{
    errorMessage.clear();
    LogRepository("Load started.");
    titles_.clear();
    pendingTitleList_.clear();
    pendingTitleCount_ = 0;
    pendingTitleIndex_ = 0;
    loading_ = false;

    LogRepository("Opening MCP title service.");
    const int32_t handle = MCP_Open();
    if (handle < 0)
    {
        errorMessage = "MCP_Open failed. The system title service is unavailable.";
        return false;
    }

    LogRepository("MCP opened; querying installed title count.");
    const MCPError countResult = MCP_TitleCount(handle);
    if (countResult < 0)
    {
        MCP_Close(handle);
        errorMessage = "MCP_TitleCount failed.";
        return false;
    }

    const uint32_t titleCount = static_cast<uint32_t>(countResult);
    {
        char message[96];
        snprintf(message, sizeof(message), "MCP reports %u installed titles.", titleCount);
        LogRepository(message);
    }
    if (titleCount == 0)
    {
        MCP_Close(handle);
        LogRepository("No installed titles reported by MCP.");
        return true;
    }

    LogRepository("Requesting installed title list from MCP.");
    pendingTitleList_.resize(titleCount);
    pendingTitleCount_ = titleCount;
    uint32_t fetchedCount = titleCount;
    const MCPError listResult = MCP_TitleList(
        handle,
        &fetchedCount,
        pendingTitleList_.data(),
        static_cast<uint32_t>(pendingTitleList_.size() * sizeof(MCPTitleListType))
    );
    MCP_Close(handle);

    if (listResult < 0)
    {
        pendingTitleList_.clear();
        pendingTitleCount_ = 0;
        errorMessage = "MCP_TitleList failed.";
        return false;
    }

    pendingTitleCount_ = fetchedCount;
    pendingTitleList_.resize(fetchedCount);
    {
        char message[96];
        snprintf(message, sizeof(message), "MCP returned %u title records.", fetchedCount);
        LogRepository(message);
    }

    titles_.reserve(fetchedCount);
    loading_ = fetchedCount > 0;
    if (!loading_)
    {
        pendingTitleList_.clear();
        LogRepository("Load completed: 0 relevant titles retained.");
    }
    return true;
}

bool TitleRepository::LoadStep(std::string&)
{
    if (!loading_)
        return true;

    // Process one record per frame to keep refresh rendering responsive.
    if (pendingTitleIndex_ < pendingTitleCount_)
    {
        const MCPTitleListType& source = pendingTitleList_[pendingTitleIndex_++];
        if (IsRelevantTitle(source.titleId))
        {
            InstalledTitle title;
            title.titleId = source.titleId;
            title.version = source.titleVersion;
            title.path = source.path;
            title.storage = GetStorageLocation(source.indexedDevice);
            title.appType = static_cast<uint32_t>(source.appType);

            const std::string filesystemPath = ToFilesystemPath(title.path);
            const std::string metaPath = filesystemPath + "/meta/meta.xml";
            const std::string contentPlatform = ReadXmlValue(metaPath, "content_platform");
            title.kind = GetTitleKind(source.titleId, title.appType, contentPlatform);
            static const std::string systemLanguage = GetSystemLanguage();
            title.name = ReadTitleName(source.titleId, filesystemPath, systemLanguage);
            title.productCode = ReadXmlValue(metaPath, "product_code");
            title.region = GetTitleRegion(ReadXmlValue(filesystemPath + "/meta/meta.xml", "region"));
            if (title.name.empty())
            {
                char fallback[32];
                snprintf(fallback, sizeof(fallback), "%016" PRIx64, title.titleId);
                title.name = fallback;
            }

            title.iconPath = filesystemPath + "/meta/iconTex.tga";
            titles_.push_back(std::move(title));
        }
    }

    if (pendingTitleIndex_ < pendingTitleCount_)
        return true;

    std::sort(titles_.begin(), titles_.end(),
        [](const InstalledTitle& left, const InstalledTitle& right)
        {
            if (left.name != right.name)
            {
                if (TitleNameLess(left.name, right.name)) return true;
                if (TitleNameLess(right.name, left.name)) return false;
            }
            return left.titleId < right.titleId;
        });

    char message[96];
    snprintf(message, sizeof(message), "Load completed: %u relevant titles retained.",
             static_cast<unsigned>(titles_.size()));
    LogRepository(message);
    pendingTitleList_.clear();
    pendingTitleCount_ = 0;
    pendingTitleIndex_ = 0;
    loading_ = false;
    return true;
}

bool TitleRepository::IsLoading() const
{
    return loading_;
}

void TitleRepository::ReleaseGraphicsResources()
{
    for (InstalledTitle& title : titles_)
    {
        if (title.iconTexture)
        {
            SDL_DestroyTexture(title.iconTexture);
            title.iconTexture = nullptr;
        }
    }
}

std::vector<InstalledTitle>& TitleRepository::Titles()
{
    return titles_;
}

const std::vector<InstalledTitle>& TitleRepository::Titles() const
{
    return titles_;
}

StorageLocation TitleRepository::GetStorageLocation(const char* indexedDevice)
{
    if (indexedDevice && strstr(indexedDevice, "usb"))
        return StorageLocation::Usb;
    if (indexedDevice && strstr(indexedDevice, "mlc"))
        return StorageLocation::Nand;
    return StorageLocation::Unknown;
}

TitleKind TitleRepository::GetTitleKind(uint64_t titleId, uint32_t appType, const std::string& contentPlatform)
{
    // MCP's native Wii title type takes precedence so Wii titles can be labelled
    // explicitly without discarding the existing high-level categories.
    if (appType == static_cast<uint32_t>(MCP_APP_TYPE_GAME_WII))
        return TitleKind::Wii;

    const uint64_t high = HighTitleId(titleId);
    if (high == kUpdateHigh) return TitleKind::Update;
    if (high == kDlcHigh) return TitleKind::Dlc;
    if (high == kVirtualConsoleHigh) return TitleKind::VirtualConsole;

    // Some Virtual Console titles share the regular 00050000 title namespace.
    // Use their metadata platform when it explicitly identifies a legacy system.
    std::string platform = contentPlatform;
    std::transform(platform.begin(), platform.end(), platform.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (platform.find("VIRTUAL CONSOLE") != std::string::npos ||
        platform == "NES" || platform == "FAMICOM" || platform == "SNES" ||
        platform == "SUPER FAMICOM" || platform == "N64" || platform == "NINTENDO 64" ||
        platform == "GBA" || platform == "GAME BOY ADVANCE" || platform == "GBC" ||
        platform == "GAME BOY COLOR" || platform == "GB" || platform == "GAME BOY" ||
        platform == "DS" || platform == "NINTENDO DS")
        return TitleKind::VirtualConsole;

    if (high == kBaseGameHigh) return TitleKind::Game;
    return TitleKind::SystemOrOther;
}


TitleRegion TitleRepository::GetTitleRegion(const std::string& metadataRegion)
{
    if (metadataRegion.empty())
        return TitleRegion::All;

    char* end = nullptr;
    const unsigned long value = strtoul(metadataRegion.c_str(), &end, 16);
    if (end == metadataRegion.c_str() || *end != '\0')
        return TitleRegion::All;

    // Wii U meta.xml region bit values: 1=Japan, 2=USA, 4=Europe.
    // Combined values are represented by the corresponding bit mask; FFFFFFFF
    // means region-free. Unknown values safely fall back to the ALL flag.
    switch (value)
    {
        case 0x00000001UL: return TitleRegion::Jap;
        case 0x00000002UL: return TitleRegion::Usa;
        case 0x00000004UL: return TitleRegion::Eur;
        case 0x00000006UL: return TitleRegion::EurUsa;
        case 0x00000005UL: return TitleRegion::EurJap;
        case 0x00000003UL: return TitleRegion::JapUsa;
        case 0xFFFFFFFFUL:
        case 0x00000007UL:
        default: return TitleRegion::All;
    }
}

bool TitleRepository::IsRelevantTitle(uint64_t titleId)
{
    const uint64_t high = HighTitleId(titleId);
    return high == kBaseGameHigh || high == kVirtualConsoleHigh ||
           high == kUpdateHigh || high == kDlcHigh;
}

std::string TitleRepository::ToFilesystemPath(const std::string& volumePath)
{
    if (volumePath.compare(0, 5, "/vol/") == 0)
    {
        const size_t slash = volumePath.find('/', 5);
        if (slash != std::string::npos)
        {
            const std::string device = volumePath.substr(5, slash - 5);
            const std::string rest = volumePath.substr(slash);
            return device + ":" + rest;
        }
    }
    return volumePath;
}

std::string TitleRepository::GetSystemLanguage()
{
    // Wii U system language values documented by Cafe SDK: 0=Japanese,
    // 1=English, 2=French, 3=German, 4=Italian, 5=Spanish, 6=Chinese,
    // 7=Korean, 8=Dutch, 9=Portuguese, 10=Russian, 11=Taiwanese.
    // system.xml is consulted at runtime; if it is inaccessible or malformed,
    // NormalizeLanguage() safely defaults to English.
    const std::string language = ReadXmlValue("/vol/system/config/system.xml", "language");
    if (!language.empty())
        return NormalizeLanguage(language);
    return "en";
}

std::string TitleRepository::ReadTitleName(uint64_t titleId, const std::string& titlePath, const std::string& language)
{
    const std::string metaPath = titlePath + "/meta/meta.xml";
    const std::string lang = NormalizeLanguage(language);
    const char* localizedTag = "longname_en";
    if (lang == "ja") localizedTag = "longname_ja";
    else if (lang == "fr") localizedTag = "longname_fr";
    else if (lang == "de") localizedTag = "longname_de";
    else if (lang == "it") localizedTag = "longname_it";
    else if (lang == "es") localizedTag = "longname_es";
    else if (lang == "zhs") localizedTag = "longname_zhs";
    else if (lang == "ko") localizedTag = "longname_ko";
    else if (lang == "nl") localizedTag = "longname_nl";
    else if (lang == "pt") localizedTag = "longname_pt";
    else if (lang == "ru") localizedTag = "longname_ru";
    else if (lang == "zht") localizedTag = "longname_zht";

    std::string name = ReadXmlValue(metaPath, localizedTag);
    if (name.find_first_not_of(" \t\r\n") == std::string::npos)
        name.clear();
    if (name.empty()) name = ReadXmlValue(metaPath, "longname_en");
    if (name.find_first_not_of(" \t\r\n") == std::string::npos)
        name.clear();
    if (name.empty()) name = ReadXmlValue(metaPath, "shortname_en");
    if (name.find_first_not_of(" \t\r\n") == std::string::npos)
        name.clear();
    if (!name.empty()) return name;

    ACPMetaXml* metadata = static_cast<ACPMetaXml*>(memalign(0x40, sizeof(ACPMetaXml)));
    if (metadata)
    {
        memset(metadata, 0, sizeof(ACPMetaXml));
        if (ACPGetTitleMetaXml(titleId, metadata) == 0)
            name = LongNameFromMetadata(*metadata, lang);
        free(metadata);
    }
    return name;
}
