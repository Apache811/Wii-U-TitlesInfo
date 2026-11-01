#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Creates a deterministic, case-insensitive sort key from a UTF-8 title name.
// Common Latin diacritics are folded to their base letter so, for example,
// accented Spanish/French names sort alongside the corresponding A-Z letter.
// Scripts without a simple Latin-style collation are ordered by Unicode code
// point; true dictionary collation would require locale-specific collation data.
inline std::vector<uint32_t> MakeTitleSortKey(const std::string& text)
{
    std::vector<uint32_t> key;
    key.reserve(text.size());

    for (size_t i = 0; i < text.size();)
    {
        const unsigned char first = static_cast<unsigned char>(text[i]);
        uint32_t cp = first;
        size_t length = 1;

        if ((first & 0xE0) == 0xC0 && i + 1 < text.size())
        {
            cp = ((first & 0x1F) << 6) |
                 (static_cast<unsigned char>(text[i + 1]) & 0x3F);
            length = 2;
        }
        else if ((first & 0xF0) == 0xE0 && i + 2 < text.size())
        {
            cp = ((first & 0x0F) << 12) |
                 ((static_cast<unsigned char>(text[i + 1]) & 0x3F) << 6) |
                 (static_cast<unsigned char>(text[i + 2]) & 0x3F);
            length = 3;
        }
        else if ((first & 0xF8) == 0xF0 && i + 3 < text.size())
        {
            cp = ((first & 0x07) << 18) |
                 ((static_cast<unsigned char>(text[i + 1]) & 0x3F) << 12) |
                 ((static_cast<unsigned char>(text[i + 2]) & 0x3F) << 6) |
                 (static_cast<unsigned char>(text[i + 3]) & 0x3F);
            length = 4;
        }

        i += length;

        // ASCII lowercase.
        if (cp >= 'A' && cp <= 'Z')
            cp += 'a' - 'A';
        // Latin-1 uppercase letters and common accented characters.
        else if (cp >= 0x00C0 && cp <= 0x00C5) cp = 'a';
        else if (cp == 0x00C7) cp = 'c';
        else if (cp >= 0x00C8 && cp <= 0x00CB) cp = 'e';
        else if (cp >= 0x00CC && cp <= 0x00CF) cp = 'i';
        else if (cp == 0x00D1) cp = 'n';
        else if (cp >= 0x00D2 && cp <= 0x00D6) cp = 'o';
        else if (cp >= 0x00D9 && cp <= 0x00DC) cp = 'u';
        else if (cp == 0x00DD || cp == 0x0178) cp = 'y';
        else if (cp >= 0x00E0 && cp <= 0x00E5) cp = 'a';
        else if (cp == 0x00E7) cp = 'c';
        else if (cp >= 0x00E8 && cp <= 0x00EB) cp = 'e';
        else if (cp >= 0x00EC && cp <= 0x00EF) cp = 'i';
        else if (cp == 0x00F1) cp = 'n';
        else if (cp >= 0x00F2 && cp <= 0x00F6) cp = 'o';
        else if (cp >= 0x00F9 && cp <= 0x00FC) cp = 'u';
        else if (cp == 0x00FD || cp == 0x00FF) cp = 'y';
        else if (cp == 0x00C6 || cp == 0x00E6) { key.push_back('a'); cp = 'e'; }
        else if (cp == 0x00DF) { key.push_back('s'); cp = 's'; }
        // Greek uppercase -> lowercase.
        else if (cp >= 0x0391 && cp <= 0x03A1) cp += 0x20;
        else if (cp >= 0x03A3 && cp <= 0x03AB) cp += 0x20;
        // Cyrillic uppercase -> lowercase (basic Cyrillic block).
        else if (cp >= 0x0410 && cp <= 0x042F) cp += 0x20;
        else if (cp >= 0x0400 && cp <= 0x040F) cp += 0x50;

        key.push_back(cp);
    }
    return key;
}

inline bool TitleNameLess(const std::string& left, const std::string& right)
{
    return MakeTitleSortKey(left) < MakeTitleSortKey(right);
}
