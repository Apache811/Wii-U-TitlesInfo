#include "TitleDatabase.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "Log.hpp"
#include "title_database_json.h"

namespace
{
constexpr const char* kExternalDatabasePath = "sd:/wiiu/apps/TitlesInfo/title_database.json";
constexpr size_t kMaximumDatabaseBytes = 16 * 1024 * 1024;

void LogDatabase(const char* message)
{
    TitleInfoLog("TitleDatabase", message);
}

void AppendUtf8(std::string& out, unsigned long cp)
{
    if (cp <= 0x7FUL)
        out.push_back(static_cast<char>(cp));
    else if (cp <= 0x7FFUL)
    {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    else if (cp <= 0xFFFFUL && !(cp >= 0xD800UL && cp <= 0xDFFFUL))
    {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    else if (cp <= 0x10FFFFUL)
    {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

bool RegionFromString(const std::string& value, TitleRegion& region);

class JsonReader
{
public:
    JsonReader(const unsigned char* data, size_t size)
        : current_(reinterpret_cast<const char*>(data)), end_(current_ + size) {}

    bool Parse(std::vector<TitleDatabase::Entry>& entries, std::string& error)
    {
        SkipWhitespace();
        if (!Consume('{')) return Fail(error, "JSON root must be an object.");
        bool foundTitles = false;
        bool schemaValid = false;

        SkipWhitespace();
        while (current_ < end_ && *current_ != '}')
        {
            std::string key;
            if (!ParseString(key) || !ConsumeAfterWhitespace(':'))
                return Fail(error, "Invalid JSON object property.");

            if (key == "schemaVersion")
            {
                unsigned long value = 0;
                if (!ParseUnsigned(value)) return Fail(error, "schemaVersion must be an integer.");
                schemaValid = value == 1;
            }
            else if (key == "titles")
            {
                if (!ParseTitles(entries, error)) return false;
                foundTitles = true;
            }
            else if (!SkipValue())
                return Fail(error, "Invalid JSON value.");

            SkipWhitespace();
            if (current_ < end_ && *current_ == ',') { ++current_; SkipWhitespace(); continue; }
            break;
        }

        if (!ConsumeAfterWhitespace('}')) return Fail(error, "JSON root object is not closed.");
        SkipWhitespace();
        if (current_ != end_) return Fail(error, "Unexpected content after JSON root.");
        if (!schemaValid) return Fail(error, "Unsupported or missing schemaVersion.");
        if (!foundTitles || entries.empty()) return Fail(error, "Database has no title records.");

        std::sort(entries.begin(), entries.end(),
            [](const TitleDatabase::Entry& a, const TitleDatabase::Entry& b) { return a.titleId < b.titleId; });
        for (size_t i = 1; i < entries.size(); ++i)
        {
            if (entries[i - 1].titleId == entries[i].titleId)
                return Fail(error, "Database contains duplicate Title IDs.");
        }
        return true;
    }

private:
    const char* current_;
    const char* end_;

    bool Fail(std::string& error, const char* message)
    {
        error = message;
        return false;
    }

    void SkipWhitespace()
    {
        while (current_ < end_ && (*current_ == ' ' || *current_ == '\t' ||
               *current_ == '\r' || *current_ == '\n')) ++current_;
    }

    bool Consume(char expected)
    {
        if (current_ >= end_ || *current_ != expected) return false;
        ++current_;
        return true;
    }

    bool ConsumeAfterWhitespace(char expected)
    {
        SkipWhitespace();
        return Consume(expected);
    }

    bool ParseHex4(unsigned long& value)
    {
        if (end_ - current_ < 4) return false;
        value = 0;
        for (int i = 0; i < 4; ++i)
        {
            const char c = *current_++;
            value <<= 4;
            if (c >= '0' && c <= '9') value |= static_cast<unsigned long>(c - '0');
            else if (c >= 'a' && c <= 'f') value |= static_cast<unsigned long>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= static_cast<unsigned long>(c - 'A' + 10);
            else return false;
        }
        return true;
    }

    bool ParseString(std::string& out)
    {
        SkipWhitespace();
        if (!Consume('"')) return false;
        out.clear();
        while (current_ < end_)
        {
            const unsigned char c = static_cast<unsigned char>(*current_++);
            if (c == '"') return true;
            if (c < 0x20) return false;
            if (c != '\\')
            {
                out.push_back(static_cast<char>(c));
                continue;
            }
            if (current_ >= end_) return false;
            const char escaped = *current_++;
            switch (escaped)
            {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u':
                {
                    unsigned long cp = 0;
                    if (!ParseHex4(cp)) return false;
                    if (cp >= 0xD800UL && cp <= 0xDBFFUL)
                    {
                        if (end_ - current_ < 6 || current_[0] != '\\' || current_[1] != 'u') return false;
                        current_ += 2;
                        unsigned long low = 0;
                        if (!ParseHex4(low) || low < 0xDC00UL || low > 0xDFFFUL) return false;
                        cp = 0x10000UL + ((cp - 0xD800UL) << 10) + (low - 0xDC00UL);
                    }
                    else if (cp >= 0xDC00UL && cp <= 0xDFFFUL) return false;
                    AppendUtf8(out, cp);
                    break;
                }
                default: return false;
            }
        }
        return false;
    }

    bool ParseUnsigned(unsigned long& value)
    {
        SkipWhitespace();
        const char* start = current_;
        while (current_ < end_ && *current_ >= '0' && *current_ <= '9') ++current_;
        if (start == current_) return false;
        value = strtoul(start, nullptr, 10);
        return true;
    }

    bool SkipValue()
    {
        SkipWhitespace();
        if (current_ >= end_) return false;
        if (*current_ == '"')
        {
            std::string ignored;
            return ParseString(ignored);
        }
        if (*current_ == '{')
        {
            ++current_; SkipWhitespace();
            while (current_ < end_ && *current_ != '}')
            {
                std::string key;
                if (!ParseString(key) || !ConsumeAfterWhitespace(':') || !SkipValue()) return false;
                SkipWhitespace();
                if (current_ < end_ && *current_ == ',') { ++current_; SkipWhitespace(); continue; }
                break;
            }
            return ConsumeAfterWhitespace('}');
        }
        if (*current_ == '[')
        {
            ++current_; SkipWhitespace();
            while (current_ < end_ && *current_ != ']')
            {
                if (!SkipValue()) return false;
                SkipWhitespace();
                if (current_ < end_ && *current_ == ',') { ++current_; SkipWhitespace(); continue; }
                break;
            }
            return ConsumeAfterWhitespace(']');
        }
        const char* start = current_;
        while (current_ < end_ && *current_ != ',' && *current_ != ']' && *current_ != '}' &&
               *current_ != ' ' && *current_ != '\t' && *current_ != '\r' && *current_ != '\n') ++current_;
        if (start == current_) return false;
        const std::string literal(start, current_);
        return literal == "null" || literal == "true" || literal == "false" ||
               (literal[0] >= '0' && literal[0] <= '9') || literal[0] == '-';
    }

    bool ParseTitles(std::vector<TitleDatabase::Entry>& entries, std::string& error)
    {
        if (!ConsumeAfterWhitespace('[')) return Fail(error, "titles must be an array.");
        SkipWhitespace();
        while (current_ < end_ && *current_ != ']')
        {
            TitleDatabase::Entry entry;
            if (!ParseTitle(entry)) return Fail(error, "Invalid title record.");
            entries.push_back(entry);
            SkipWhitespace();
            if (current_ < end_ && *current_ == ',') { ++current_; SkipWhitespace(); continue; }
            break;
        }
        if (!ConsumeAfterWhitespace(']')) return Fail(error, "titles array is not closed.");
        return true;
    }

    bool ParseTitle(TitleDatabase::Entry& entry)
    {
        if (!ConsumeAfterWhitespace('{')) return false;
        bool hasId = false, hasNotes = false, hasRegion = false, hasVersion = false;
        SkipWhitespace();
        while (current_ < end_ && *current_ != '}')
        {
            std::string key;
            if (!ParseString(key) || !ConsumeAfterWhitespace(':')) return false;
            if (key == "titleId")
            {
                if (!ParseString(entry.titleId)) return false;
                hasId = true;
            }
            else if (key == "notes")
            {
                if (!ParseString(entry.notes)) return false;
                hasNotes = true;
            }
            else if (key == "region")
            {
                SkipWhitespace();
                if (end_ - current_ >= 4 && std::strncmp(current_, "null", 4) == 0)
                {
                    current_ += 4;
                    entry.hasRegion = false;
                }
                else
                {
                    if (!ParseString(entry.region)) return false;
                    entry.hasRegion = !entry.region.empty();
                }
                hasRegion = true;
            }
            else if (key == "latestVersion")
            {
                SkipWhitespace();
                if (end_ - current_ >= 4 && std::strncmp(current_, "null", 4) == 0)
                {
                    current_ += 4;
                    entry.hasLatestVersion = false;
                }
                else
                {
                    unsigned long version = 0;
                    if (!ParseUnsigned(version) || version > 65535UL) return false;
                    entry.latestVersion = static_cast<uint16_t>(version);
                    entry.hasLatestVersion = true;
                }
                hasVersion = true;
            }
            else if (!SkipValue()) return false;

            SkipWhitespace();
            if (current_ < end_ && *current_ == ',') { ++current_; SkipWhitespace(); continue; }
            break;
        }
        if (!ConsumeAfterWhitespace('}')) return false;
        if (!hasId || !hasNotes || !hasRegion || !hasVersion || entry.titleId.size() != 16) return false;
        for (char c : entry.titleId)
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return false;
        if (entry.hasRegion)
        {
            TitleRegion parsedRegion;
            if (!RegionFromString(entry.region, parsedRegion)) return false;
        }
        return true;
    }
};

bool ReadFile(const char* path, std::vector<unsigned char>& bytes)
{
    FILE* file = fopen(path, "rb");
    if (!file) return false;
    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return false;
    }
    const long length = ftell(file);
    if (length <= 0 || static_cast<unsigned long>(length) > kMaximumDatabaseBytes)
    {
        fclose(file);
        return false;
    }
    rewind(file);
    bytes.resize(static_cast<size_t>(length));
    const size_t readCount = fread(bytes.data(), 1, bytes.size(), file);
    fclose(file);
    return readCount == bytes.size();
}

bool RegionFromString(const std::string& value, TitleRegion& region)
{
    if (value == "ALL") region = TitleRegion::All;
    else if (value == "EUR") region = TitleRegion::Eur;
    else if (value == "USA") region = TitleRegion::Usa;
    else if (value == "JPN" || value == "JAP") region = TitleRegion::Jap;
    else if (value == "EUR/USA" || value == "USA/EUR") region = TitleRegion::EurUsa;
    else if (value == "EUR/JPN" || value == "JPN/EUR") region = TitleRegion::EurJap;
    else if (value == "JPN/USA" || value == "USA/JPN") region = TitleRegion::JapUsa;
    else return false;
    return true;
}
}

bool TitleDatabase::Load(std::string& errorMessage)
{
    entries_.clear();
    std::vector<unsigned char> externalBytes;
    if (ReadFile(kExternalDatabasePath, externalBytes))
    {
        JsonReader externalReader(externalBytes.data(), externalBytes.size());
        if (externalReader.Parse(entries_, errorMessage))
        {
            LogDatabase("Loaded external JSON database from SD.");
            errorMessage.clear();
            return true;
        }
        entries_.clear();
        LogDatabase("External JSON database is invalid; falling back to embedded database.");
    }

    JsonReader embeddedReader(title_database_json, static_cast<size_t>(title_database_json_size));
    if (!embeddedReader.Parse(entries_, errorMessage))
    {
        entries_.clear();
        LogDatabase("Embedded JSON database is invalid.");
        return false;
    }
    LogDatabase("Loaded embedded JSON database.");
    errorMessage.clear();
    return true;
}

void TitleDatabase::Apply(std::vector<InstalledTitle>& titles) const
{
    // Build an index of the database records so base-game/update associations
    // are based on Title ID namespaces, never on localized names.
    std::map<std::string, const Entry*> lookup;
    for (const Entry& entry : entries_)
        lookup[entry.titleId] = &entry;

    std::map<std::string, const InstalledTitle*> installedUpdates;
    for (const InstalledTitle& title : titles)
    {
        if (title.kind != TitleKind::Update)
            continue;

        char id[17];
        snprintf(id, sizeof(id), "%016llX", static_cast<unsigned long long>(title.titleId));
        installedUpdates[id] = &title;
    }

    for (InstalledTitle& title : titles)
    {
        char id[17];
        snprintf(id, sizeof(id), "%016llX", static_cast<unsigned long long>(title.titleId));
        const auto found = lookup.find(id);
        if (found != lookup.end())
        {
            const Entry& entry = *found->second;
            title.notes = entry.notes;
            title.latestVersion = entry.latestVersion;
            title.hasLatestVersion = entry.hasLatestVersion;
            title.latestVersionDataError = (title.kind == TitleKind::Update) && !entry.hasLatestVersion;

            TitleRegion databaseRegion;
            if (entry.hasRegion && RegionFromString(entry.region, databaseRegion))
            {
                title.region = databaseRegion;
                title.hasDatabaseRegion = true;
            }
        }

        // A GAME uses the corresponding UPDATE record as Latest when such a
        // record exists in the database. If that UPDATE is installed, its own
        // row is the authoritative row and the GAME row will be removed below.
        if (title.kind == TitleKind::Game)
        {
            std::string updateId = id;
            updateId.replace(0, 8, "0005000E");
            const auto updateRecord = lookup.find(updateId);
            if (updateRecord != lookup.end())
            {
                const Entry& entry = *updateRecord->second;
                title.latestVersion = entry.latestVersion;
                title.hasLatestVersion = entry.hasLatestVersion;
                title.latestVersionDataError = !entry.hasLatestVersion;
            }
        }

        // If the installed version exceeds the database's latest value, flag
        // the comparison as uncertain: the database may simply be stale.
        if (title.hasLatestVersion && title.version > title.latestVersion)
            title.latestVersionDataError = true;
    }

    // When an UPDATE is installed, show only that UPDATE, not both it and its
    // associated base GAME. Preserve all UPDATE metadata and installed version.
    titles.erase(std::remove_if(titles.begin(), titles.end(),
        [&installedUpdates](const InstalledTitle& title)
        {
            if (title.kind != TitleKind::Game)
                return false;

            char id[17];
            snprintf(id, sizeof(id), "%016llX", static_cast<unsigned long long>(title.titleId));
            std::string updateId(id);
            updateId.replace(0, 8, "0005000E");
            return installedUpdates.find(updateId) != installedUpdates.end();
        }), titles.end());
}

