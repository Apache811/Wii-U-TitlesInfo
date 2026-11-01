#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "TitleRepository.hpp"

class TitleDatabase
{
public:
    bool Load(std::string& errorMessage);
    void Apply(std::vector<InstalledTitle>& titles) const;
    size_t Size() const { return entries_.size(); }

public:
    struct Entry
    {
        std::string titleId;
        std::string notes;
        std::string region;
        uint16_t latestVersion = 0;
        bool hasLatestVersion = false;
        bool hasRegion = false;
    };

private:
    std::vector<Entry> entries_;
};
