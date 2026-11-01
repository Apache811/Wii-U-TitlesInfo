#include "UserInterface.hpp"
#include "TitleSort.hpp"

#include <SDL_image.h>
#include "Log.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <cstdint>
#include <coreinit/memory.h>
#include "DejaVuSans_ttf.h"
#include "ZL_png.h"
#include "ZR_png.h"
#include "flag_ALL_png.h"
#include "flag_EUR_png.h"
#include "flag_USA_png.h"
#include "flag_JAP_png.h"
#include "flag_EUR_USA_png.h"
#include "flag_EUR_JAP_png.h"
#include "flag_JAP_USA_png.h"
#include "unknown_png.h"
#include "update_png.h"
#include "updated_png.h"

namespace
{
void LogUI(const char* message)
{
    TitleInfoLog("UI", message);
}

constexpr int kScreenWidth = 1920;
constexpr int kScreenHeight = 1080;
constexpr int kTopBarHeight = 140;
constexpr int kBottomBarHeight = 90;
constexpr int kRowHeight = 270;
constexpr int kListTop = 160;
constexpr int kListBottom = kScreenHeight - kBottomBarHeight;
constexpr int kVisibleRows = (kListBottom - kListTop) / kRowHeight;

const SDL_Color kBackground = { 239, 244, 249, 255 };
const SDL_Color kWhite = { 13, 27, 57, 255 };
const SDL_Color kMuted = { 81, 105, 135, 255 };
const SDL_Color kUsb = { 91, 181, 255, 255 };
const SDL_Color kNand = { 255, 187, 54, 255 };

void DrawGradient(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color top, SDL_Color bottom)
{
    if (rect.h <= 0) return;
    for (int y = 0; y < rect.h; ++y)
    {
        const float t = rect.h > 1 ? static_cast<float>(y) / static_cast<float>(rect.h - 1) : 0.0f;
        SDL_SetRenderDrawColor(renderer,
            static_cast<Uint8>(top.r + (bottom.r - top.r) * t),
            static_cast<Uint8>(top.g + (bottom.g - top.g) * t),
            static_cast<Uint8>(top.b + (bottom.b - top.b) * t), 255);
        SDL_RenderDrawLine(renderer, rect.x, rect.y + y, rect.x + rect.w - 1, rect.y + y);
    }
}

void FillRoundedRect(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, SDL_Color color)
{
    radius = std::max(0, std::min(radius, std::min(rect.w, rect.h) / 2));
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int y = 0; y < rect.h; ++y)
    {
        const int dy = y < radius ? radius - y : (y >= rect.h - radius ? y - (rect.h - radius - 1) : 0);
        int inset = 0;
        if (dy > 0)
        {
            const float inside = static_cast<float>(radius * radius - dy * dy);
            inset = radius - static_cast<int>(std::sqrt(std::max(0.0f, inside)));
        }
        SDL_RenderDrawLine(renderer, rect.x + inset, rect.y + y, rect.x + rect.w - 1 - inset, rect.y + y);
    }
}

void DrawRoundedBorder(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, int thickness, SDL_Color color)
{
    FillRoundedRect(renderer, rect, radius, color);
    SDL_Rect inner = { rect.x + thickness, rect.y + thickness, rect.w - thickness * 2, rect.h - thickness * 2 };
    if (inner.w > 0 && inner.h > 0)
        FillRoundedRect(renderer, inner, std::max(0, radius - thickness), kBackground);
}

std::string FitUtf8Text(TTF_Font* font, const std::string& input, int maxWidth)
{
    if (!font || input.empty())
        return input;

    int width = 0;
    int height = 0;
    if (TTF_SizeUTF8(font, input.c_str(), &width, &height) == 0 && width <= maxWidth)
        return input;

    std::string fitted = input;
    while (!fitted.empty())
    {
        size_t start = fitted.size() - 1;
        while (start > 0 && (static_cast<unsigned char>(fitted[start]) & 0xC0) == 0x80)
            --start;
        fitted.resize(start);
        const std::string candidate = fitted + "...";
        if (TTF_SizeUTF8(font, candidate.c_str(), &width, &height) == 0 && width <= maxWidth)
            return candidate;
    }
    return "...";
}

}

bool UserInterface::Initialize(std::string& errorMessage)
{
    LogUI("SDL_Init starting.");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
    {
        errorMessage = std::string("SDL_Init failed: ") + SDL_GetError();
        return false;
    }
    LogUI("SDL_Init completed.");

    LogUI("TTF_Init starting.");
    if (TTF_Init() < 0)
    {
        errorMessage = std::string("TTF_Init failed: ") + TTF_GetError();
        SDL_Quit();
        return false;
    }
    LogUI("TTF_Init completed.");

    const int imageFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    LogUI("IMG_Init starting.");
    const int initializedImageFlags = IMG_Init(imageFlags);
    {
        char message[96];
        snprintf(message, sizeof(message), "IMG_Init completed (flags: 0x%X).",
                 static_cast<unsigned>(initializedImageFlags));
        LogUI(message);
    }

    LogUI("SDL_CreateWindow starting.");
    window_ = SDL_CreateWindow("Titles Info", 0, 0, kScreenWidth, kScreenHeight, 0);
    if (!window_)
    {
        errorMessage = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
        Shutdown();
        return false;
    }
    LogUI("SDL_CreateWindow completed.");

    LogUI("SDL_CreateRenderer starting.");
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_)
    {
        errorMessage = std::string("SDL_CreateRenderer failed: ") + SDL_GetError();
        Shutdown();
        return false;
    }
    LogUI("SDL_CreateRenderer completed.");

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    flagAllTexture_ = LoadEmbeddedPng(flag_ALL_png, flag_ALL_png_size);
    flagEurTexture_ = LoadEmbeddedPng(flag_EUR_png, flag_EUR_png_size);
    flagUsaTexture_ = LoadEmbeddedPng(flag_USA_png, flag_USA_png_size);
    flagJapTexture_ = LoadEmbeddedPng(flag_JAP_png, flag_JAP_png_size);
    flagEurUsaTexture_ = LoadEmbeddedPng(flag_EUR_USA_png, flag_EUR_USA_png_size);
    flagEurJapTexture_ = LoadEmbeddedPng(flag_EUR_JAP_png, flag_EUR_JAP_png_size);
    flagJapUsaTexture_ = LoadEmbeddedPng(flag_JAP_USA_png, flag_JAP_USA_png_size);
    unknownStatusTexture_ = LoadEmbeddedPng(unknown_png, unknown_png_size);
    updateStatusTexture_ = LoadEmbeddedPng(update_png, update_png_size);
    updatedStatusTexture_ = LoadEmbeddedPng(updated_png, updated_png_size);

    if (!unknownStatusTexture_ || !updateStatusTexture_ || !updatedStatusTexture_)
        LogUI("Warning: one or more version status icons could not be loaded.");

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    LogUI("Loading embedded ZL.png resource.");
    SDL_RWops* zlStream = SDL_RWFromConstMem(ZL_png, static_cast<int>(ZL_png_size));
    SDL_Surface* zlSurface = zlStream ? IMG_Load_RW(zlStream, 1) : nullptr;
    if (zlSurface)
    {
        zlButtonTexture_ = SDL_CreateTextureFromSurface(renderer_, zlSurface);
        SDL_FreeSurface(zlSurface);
    }
    else
    {
        LogUI("Failed to decode embedded ZL.png resource.");
    }

    LogUI("Loading embedded ZR.png resource.");
    SDL_RWops* zrStream = SDL_RWFromConstMem(ZR_png, static_cast<int>(ZR_png_size));
    SDL_Surface* zrSurface = zrStream ? IMG_Load_RW(zrStream, 1) : nullptr;
    if (zrSurface)
    {
        zrButtonTexture_ = SDL_CreateTextureFromSurface(renderer_, zrSurface);
        SDL_FreeSurface(zrSurface);
    }
    else
    {
        LogUI("Failed to decode embedded ZR.png resource.");
    }

    if (OSGetSharedData(OS_SHAREDDATATYPE_FONT_STANDARD, 0, &iconFontData_, &iconFontSize_))
        LogUI("Loaded Wii U system font for controller glyphs.");
    else
        LogUI("Wii U system font unavailable; controller glyphs may not render.");

    LogUI("UI initialization completed.");
    return true;
}

void UserInterface::Shutdown()
{
    LogUI("UI shutdown starting.");
    if (unknownStatusTexture_)
    {
        SDL_DestroyTexture(unknownStatusTexture_);
        unknownStatusTexture_ = nullptr;
    }
    if (updateStatusTexture_)
    {
        SDL_DestroyTexture(updateStatusTexture_);
        updateStatusTexture_ = nullptr;
    }
    if (updatedStatusTexture_)
    {
        SDL_DestroyTexture(updatedStatusTexture_);
        updatedStatusTexture_ = nullptr;
    }
    if (zlButtonTexture_)
    {
        SDL_DestroyTexture(zlButtonTexture_);
        zlButtonTexture_ = nullptr;
    }
    if (zrButtonTexture_)
    {
        SDL_DestroyTexture(zrButtonTexture_);
        zrButtonTexture_ = nullptr;
    }
    for (auto& entry : iconFonts_)
    {
        if (entry.second)
            TTF_CloseFont(entry.second);
    }
    iconFonts_.clear();

    for (auto& entry : textCache_)
    {
        if (entry.second.texture)
            SDL_DestroyTexture(entry.second.texture);
    }
    textCache_.clear();

    for (auto& entry : glyphCache_)
    {
        if (entry.second.texture)
            SDL_DestroyTexture(entry.second.texture);
    }
    glyphCache_.clear();

    for (auto& entry : fonts_)
    {
        if (entry.second)
            TTF_CloseFont(entry.second);
    }
    fonts_.clear();

    if (renderer_)
    {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_)
    {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    LogUI("UI shutdown completed.");
}

namespace
{
void AppendUtf8(std::string& output, uint32_t codepoint)
{
    if (codepoint <= 0x7F)
        output.push_back(static_cast<char>(codepoint));
    else if (codepoint <= 0x7FF)
    {
        output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    else if (codepoint <= 0xFFFF)
    {
        output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    else
    {
        output.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

std::string LowercaseSearchText(const std::string& value)
{
    std::string result;
    result.reserve(value.size());
    size_t index = 0;
    while (index < value.size())
    {
        const unsigned char first = static_cast<unsigned char>(value[index++]);
        uint32_t codepoint = first;
        if ((first & 0xE0) == 0xC0 && index < value.size())
            codepoint = ((first & 0x1F) << 6) | (static_cast<unsigned char>(value[index++]) & 0x3F);
        else if ((first & 0xF0) == 0xE0 && index + 1 < value.size())
        {
            const unsigned char second = static_cast<unsigned char>(value[index]);
            const unsigned char third = static_cast<unsigned char>(value[index + 1]);
            index += 2;
            codepoint = ((first & 0x0F) << 12) |
                        ((second & 0x3F) << 6) |
                        (third & 0x3F);
        }
        else if ((first & 0xF8) == 0xF0 && index + 2 < value.size())
        {
            const unsigned char second = static_cast<unsigned char>(value[index]);
            const unsigned char third = static_cast<unsigned char>(value[index + 1]);
            const unsigned char fourth = static_cast<unsigned char>(value[index + 2]);
            index += 3;
            codepoint = ((first & 0x07) << 18) |
                        ((second & 0x3F) << 12) |
                        ((third & 0x3F) << 6) |
                        (fourth & 0x3F);
        }

        if (codepoint >= 'A' && codepoint <= 'Z')
            codepoint += 0x20;
        else if ((codepoint >= 0x00C0 && codepoint <= 0x00D6) ||
                 (codepoint >= 0x00D8 && codepoint <= 0x00DE) ||
                 (codepoint >= 0x0391 && codepoint <= 0x03A1) ||
                 (codepoint >= 0x03A3 && codepoint <= 0x03AB) ||
                 (codepoint >= 0x0410 && codepoint <= 0x042F))
            codepoint += 0x20;
        else if (codepoint >= 0x0400 && codepoint <= 0x040F)
            codepoint += 0x50;
        else if (codepoint == 0x0178)
            codepoint = 0x00FF;

        AppendUtf8(result, codepoint);
    }
    return result;
}

bool MatchesTitleSearch(const InstalledTitle& title, const std::string& normalizedSearchText)
{
    if (normalizedSearchText.empty())
        return true;

    const std::string name = LowercaseSearchText(title.name);
    char titleIdText[32];
    snprintf(titleIdText, sizeof(titleIdText), "%016llX", static_cast<unsigned long long>(title.titleId));
    const std::string titleId = LowercaseSearchText(titleIdText);
    return name.find(normalizedSearchText) != std::string::npos || titleId.find(normalizedSearchText) != std::string::npos;
}

bool MatchesStorageFilter(const InstalledTitle& title, TitleListFilter filter)
{
    if (filter == TitleListFilter::All)
        return true;
    if (filter == TitleListFilter::Usb)
        return title.storage == StorageLocation::Usb;
    if (filter == TitleListFilter::Nand)
        return title.storage == StorageLocation::Nand;
#if __DEBUG__
    if (filter == TitleListFilter::None)
        return false;
#endif
    return false;
}
}

int UserInterface::FilteredTitleCount(const TitleRepository& repository, TitleListFilter filter, const std::string& searchText) const
{
    const std::string normalizedSearchText = LowercaseSearchText(searchText);
    int count = 0;
    for (const InstalledTitle& title : repository.Titles())
    {
        if (MatchesStorageFilter(title, filter) && MatchesTitleSearch(title, normalizedSearchText))
            ++count;
    }
    return count;
}

void UserInterface::Draw(TitleRepository& repository, int selectedIndex, TitleListFilter filter, const std::string& searchText)
{
    SDL_SetRenderDrawColor(renderer_, kBackground.r, kBackground.g, kBackground.b, kBackground.a);
    SDL_RenderClear(renderer_);

    DrawTopBar(filter);

    std::vector<InstalledTitle>& titles = repository.Titles();
    const std::string normalizedSearchText = LowercaseSearchText(searchText);
    std::vector<InstalledTitle*> filteredTitles;
    filteredTitles.reserve(titles.size());
    for (InstalledTitle& title : titles)
    {
        if (MatchesStorageFilter(title, filter) && MatchesTitleSearch(title, normalizedSearchText))
            filteredTitles.push_back(&title);
    }
    // Keep the visible list alphabetically ordered by the active localized
    // long name (or the fallback name), even after search/filtering.
    std::sort(filteredTitles.begin(), filteredTitles.end(),
        [](const InstalledTitle* left, const InstalledTitle* right)
        {
            if (left->name != right->name)
            {
                if (TitleNameLess(left->name, right->name)) return true;
                if (TitleNameLess(right->name, left->name)) return false;
            }
            return left->titleId < right->titleId;
        });
    const int total = static_cast<int>(filteredTitles.size());
    selectedIndex = std::min(std::max(0, selectedIndex), std::max(0, total - 1));

    // Keep the current viewport stable while the selection moves within it.
    // Scroll only when the selected item crosses a visible boundary.
    const int maxFirstVisible = std::max(0, total - kVisibleRows);
    firstVisibleIndex_ = std::min(std::max(0, firstVisibleIndex_), maxFirstVisible);
    if (selectedIndex < firstVisibleIndex_)
        firstVisibleIndex_ = selectedIndex;
    else if (selectedIndex >= firstVisibleIndex_ + kVisibleRows)
        firstVisibleIndex_ = selectedIndex - kVisibleRows + 1;

    firstVisibleIndex_ = std::min(std::max(0, firstVisibleIndex_), maxFirstVisible);
    const int first = firstVisibleIndex_;
    const int last = std::min(total, first + kVisibleRows);

    if (filteredTitles.empty())
    {
        const char* emptyMessage = !searchText.empty()
            ? "No titles match your search."
            : (filter == TitleListFilter::Usb ? "No titles found on USB."
                : filter == TitleListFilter::Nand ? "No titles found on NAND."
#if __DEBUG__
                : filter == TitleListFilter::None ? "Debug: empty list preview."
#endif
                : "No titles found on NAND or USB.");
        DrawText(emptyMessage, 64, 230, 36, kMuted);

        const int hintY = 290;
        const int xGlyphWidth = DrawGlyph("\xee\x80\x82", 64, hintY + 20, 38, kMuted);
        if (xGlyphWidth > 0)
            DrawText("to rescan the title list.", 64 + xGlyphWidth + 10, hintY + 3, 30, kMuted);
        else
            DrawText("Press X to rescan the title list.", 64, hintY, 30, kMuted);
    }
    else
    {
        for (int index = first; index < last; ++index)
            DrawTitleRow(*filteredTitles[static_cast<size_t>(index)], index, kListTop + (index - first) * kRowHeight, index == selectedIndex);
    }

    if (!searchText.empty())
    {
        SDL_Rect searchBadge = { 48, 104, 760, 48 };
        FillRoundedRect(renderer_, searchBadge, 12, SDL_Color{ 255, 255, 255, 235 });
        DrawText(std::string("Search: ") + searchText, 64, 108, 26, kWhite);
    }

    DrawBottomBar();
    SDL_RenderPresent(renderer_);
}

void UserInterface::DrawRefreshing(uint32_t elapsedMilliseconds)
{
    SDL_SetRenderDrawColor(renderer_, kBackground.r, kBackground.g, kBackground.b, kBackground.a);
    SDL_RenderClear(renderer_);
    DrawTopBar(TitleListFilter::All);

    // Keep the existing screen layout visible underneath a centered Wii U-style
    // refresh panel. The animation is driven by elapsed time, not frame count.
    const SDL_Rect panel = { 650, 345, 620, 390 };
    FillRoundedRect(renderer_, panel, 28, SDL_Color{ 255, 255, 255, 255 });
    SDL_SetRenderDrawColor(renderer_, 62, 174, 255, 255);
    for (int thickness = 0; thickness < 4; ++thickness)
    {
        SDL_Rect border = { panel.x + thickness, panel.y + thickness,
                            panel.w - thickness * 2, panel.h - thickness * 2 };
        SDL_RenderDrawRect(renderer_, &border);
    }

    const unsigned dotCount = (elapsedMilliseconds / 1000U) % 6U;
    std::string label = "Refreshing";
    label.append(dotCount, '.');
    int textWidth = 0;
    int textHeight = 0;
    TTF_Font* font = GetFont(42);
    if (font && TTF_SizeUTF8(font, label.c_str(), &textWidth, &textHeight) == 0)
        DrawText(label, (kScreenWidth - textWidth) / 2, 390, 42, kWhite);

    // Twelve soft blue dots form a rotating ring, matching the understated Wii U
    // loading indicator style without requiring external image resources.
    constexpr int dotTotal = 12;
    constexpr float twoPi = 6.28318530718f;
    const int centerX = kScreenWidth / 2;
    const int centerY = 565;
    const int ringRadius = 76;
    const int dotRadius = 10;
    const unsigned activeDot = (elapsedMilliseconds / 100U) % dotTotal;
    for (int i = 0; i < dotTotal; ++i)
    {
        const float angle = (static_cast<float>(i) / dotTotal) * twoPi - 1.57079632679f;
        const int x = centerX + static_cast<int>(std::cos(angle) * ringRadius);
        const int y = centerY + static_cast<int>(std::sin(angle) * ringRadius);
        const unsigned distance = (static_cast<unsigned>(i) + dotTotal - activeDot) % dotTotal;
        const Uint8 brightness = static_cast<Uint8>(90U + ((dotTotal - distance) * 165U) / dotTotal);
        SDL_SetRenderDrawColor(renderer_, brightness / 2, brightness, 224, 255);
        for (int dy = -dotRadius; dy <= dotRadius; ++dy)
        {
            const int halfWidth = static_cast<int>(std::sqrt(static_cast<float>(dotRadius * dotRadius - dy * dy)));
            SDL_RenderDrawLine(renderer_, x - halfWidth, y + dy, x + halfWidth, y + dy);
        }
    }

    DrawBottomBar();
    SDL_RenderPresent(renderer_);
}

void UserInterface::DrawTopBar(TitleListFilter filter)
{
    SDL_Rect bar = { 0, 0, kScreenWidth, kTopBarHeight };
    DrawGradient(renderer_, bar, SDL_Color{ 27, 139, 224, 255 }, SDL_Color{ 0, 91, 174, 255 });
    DrawText("Titles Info", 48, 27, 58, SDL_Color{ 255, 255, 255, 255 });

    const int buttonY = 30;
    const int buttonHeight = 78;
#if __DEBUG__
    if (zlButtonTexture_)
    {
        SDL_Rect icon = { 1072, 36, 104, 66 };
        SDL_RenderCopy(renderer_, zlButtonTexture_, nullptr, &icon);
    }
    else
    {
        DrawFilterButton("ZL", 1072, buttonY, 104, buttonHeight, false);
    }

    DrawFilterButton("ALL", 1212, buttonY, 124, buttonHeight, filter == TitleListFilter::All);
    DrawFilterButton("USB", 1344, buttonY, 124, buttonHeight, filter == TitleListFilter::Usb);
    DrawFilterButton("NAND", 1476, buttonY, 124, buttonHeight, filter == TitleListFilter::Nand);
    DrawFilterButton("NONE", 1608, buttonY, 124, buttonHeight, filter == TitleListFilter::None);
#else
    // Shift the non-debug control block 12 px right so the NAND-to-ZR gap
    // matches the ZL-to-ALL gap (54 px).
    if (zlButtonTexture_)
    {
        SDL_Rect icon = { 1084, 36, 104, 66 };
        SDL_RenderCopy(renderer_, zlButtonTexture_, nullptr, &icon);
    }
    else
    {
        DrawFilterButton("ZL", 1084, buttonY, 104, buttonHeight, false);
    }
    DrawFilterButton("ALL", 1242, buttonY, 142, buttonHeight, filter == TitleListFilter::All);
    DrawFilterButton("USB", 1400, buttonY, 142, buttonHeight, filter == TitleListFilter::Usb);
    DrawFilterButton("NAND", 1558, buttonY, 156, buttonHeight, filter == TitleListFilter::Nand);
#endif

#if __DEBUG__
    const int zrX = 1768; // Align the right margin with the left margin before the app title.
#else
    const int zrX = 1768;
#endif
    if (zrButtonTexture_)
    {
        SDL_Rect icon = { zrX, 36, 104, 66 };
        SDL_RenderCopy(renderer_, zrButtonTexture_, nullptr, &icon);
    }
    else
    {
        DrawFilterButton("ZR", zrX, buttonY, 104, buttonHeight, false);
    }
}

void UserInterface::DrawBottomBar()
{
    SDL_Rect bar = { 0, kScreenHeight - kBottomBarHeight, kScreenWidth, kBottomBarHeight };
    DrawGradient(renderer_, bar, SDL_Color{ 25, 137, 222, 255 }, SDL_Color{ 0, 89, 174, 255 });

    const int y = kScreenHeight - kBottomBarHeight + 45;
    const SDL_Color color = SDL_Color{ 255, 255, 255, 255 };
    // Enlarge only the bottom-bar content by 50%; the bar dimensions remain unchanged.
    const int iconSize = 51;
    const int labelSize = 36;
    const int gap = 12;

    // Keep the supported UP glyph only; the DOWN glyph is not implemented by the system font.
    int x = 28;
    int glyphWidth = DrawGlyph("\xee\x81\xbd", x, y, iconSize, color);
    x += glyphWidth + gap;
    DrawText("Next/Prev Title", x, y - 18, labelSize, color);

    // L/R are the glyphs immediately preceding ZL/ZR in the Wii U system-font set.
    x = 430;
    glyphWidth = DrawGlyph("\xee\x82\x83", x, y, iconSize, color);
    x += glyphWidth + 4;
    glyphWidth = DrawGlyph("\xee\x82\x84", x, y, iconSize, color);
    x += glyphWidth + gap;
    DrawText("Page Up/Down", x, y - 18, labelSize, color);

    // HOME/Exit is centered horizontally under the physical HOME button.
    const char* homeGlyph = "\xee\x81\x84";
    int homeGlyphWidth = 0;
    int exitLabelWidth = 0;
    TTF_Font* iconFont = GetIconFont(iconSize);
    TTF_Font* labelFont = GetFont(labelSize);
    if (iconFont) TTF_SizeUTF8(iconFont, homeGlyph, &homeGlyphWidth, nullptr);
    if (labelFont) TTF_SizeUTF8(labelFont, "Exit", &exitLabelWidth, nullptr);
    const int homeTotalWidth = homeGlyphWidth + gap + exitLabelWidth;
    const int homeX = (kScreenWidth - homeTotalWidth) / 2;
    glyphWidth = DrawGlyph(homeGlyph, homeX, y, iconSize, color);
    DrawText("Exit", homeX + glyphWidth + gap, y - 18, labelSize, color);

    x = 1200;
    glyphWidth = DrawGlyph("\xee\x80\x82", x, y, iconSize, color);
    DrawText("Refresh List", x + glyphWidth + gap, y - 18, labelSize, color);

    x = 1580;
    glyphWidth = DrawGlyph("\xee\x80\x83", x, y, iconSize, color);
    DrawText("Search", x + glyphWidth + gap, y - 18, labelSize, color);
}

void UserInterface::DrawFilterButton(const char* label, int x, int y, int width, int height, bool selected)
{
    SDL_Rect button = { x, y, width, height };
    const SDL_Color fill = selected
        ? SDL_Color{ 55, 223, 202, 255 }
        : SDL_Color{ 255, 255, 255, 255 };
    FillRoundedRect(renderer_, button, 34, fill);

    int textWidth = 0;
    int textHeight = 0;
    TTF_Font* font = GetFont(34);
    if (font && TTF_SizeUTF8(font, label, &textWidth, &textHeight) == 0)
    {
        const SDL_Color textColor = selected
            ? SDL_Color{ 0, 0, 0, 255 }
            : SDL_Color{ 0, 91, 174, 255 };
        DrawText(label, x + (width - textWidth) / 2, y + (height - textHeight) / 2, 34, textColor);
    }
}

void UserInterface::DrawTitleRow(InstalledTitle& title, int, int y, bool selected)
{
    SDL_Rect row = { 40, y, kScreenWidth - 108, kRowHeight - 16 };
    FillRoundedRect(renderer_, row, 30, SDL_Color{ 255, 255, 255, 255 });

    if (selected)
    {
        DrawRoundedBorder(renderer_, row, 30, 5, SDL_Color{ 74, 177, 255, 255 });
        SDL_Rect inner = { row.x + 5, row.y + 5, row.w - 10, row.h - 10 };
        FillRoundedRect(renderer_, inner, 25, SDL_Color{ 211, 234, 253, 255 });
    }

    if (!title.iconLoadAttempted)
    {
        title.iconLoadAttempted = true;
        SDL_Surface* surface = IMG_Load(title.iconPath.c_str());
        if (surface)
        {
            title.iconTexture = SDL_CreateTextureFromSurface(renderer_, surface);
            SDL_FreeSurface(surface);
        }
    }

    SDL_Rect iconRect = { 76, y + 18, 226, 226 };
    if (title.iconTexture)
        SDL_RenderCopy(renderer_, title.iconTexture, nullptr, &iconRect);
    else
    {
        FillRoundedRect(renderer_, iconRect, 18, SDL_Color{ 204, 219, 232, 255 });
        DrawText("?", iconRect.x + 88, iconRect.y + 66, 80, kMuted);
    }

    // Left metadata column: keep the existing row/card geometry, adding metadata
    // in the available vertical space under the title name.
    DrawText(title.name, 350, y + 18, 44, kWhite);

    const SDL_Color metadataLabelColor = { 0, 0, 0, 255 };
    const SDL_Color titleIdValueColor = { 36, 99, 166, 255 };
    const SDL_Color productCodeValueColor = { 0, 133, 119, 255 };

    char titleIdValue[24];
    snprintf(titleIdValue, sizeof(titleIdValue), "%016llX",
             static_cast<unsigned long long>(title.titleId));
    const char* titleIdLabel = "Title ID: ";
    DrawText(titleIdLabel, 352, y + 88, 34, metadataLabelColor);
    int titleIdLabelWidth = 0;
    int titleIdLabelHeight = 0;
    if (TTF_SizeUTF8(GetFont(34), titleIdLabel, &titleIdLabelWidth, &titleIdLabelHeight) == 0)
        DrawText(titleIdValue, 352 + titleIdLabelWidth, y + 88, 34, titleIdValueColor);

    const char* productCodeLabel = "Product Code: ";
    const std::string productCodeValue = title.productCode.empty() ? "N/A" : title.productCode;
    DrawText(productCodeLabel, 352, y + 129, 34, metadataLabelColor);
    int productCodeLabelWidth = 0;
    int productCodeLabelHeight = 0;
    if (TTF_SizeUTF8(GetFont(34), productCodeLabel, &productCodeLabelWidth, &productCodeLabelHeight) == 0)
        DrawText(productCodeValue, 352 + productCodeLabelWidth, y + 129, 34, productCodeValueColor);
    const std::string notesText = title.notes.empty()
        ? (title.kind == TitleKind::Wii ? "Wii Game" : "Wii U Game")
        : title.notes;
    DrawText(FitUtf8Text(GetFont(33), notesText, 1120), 352, y + 184, 33, kMuted);

    const char* kindLabel = "OTHER";
    switch (title.kind)
    {
        case TitleKind::Game: kindLabel = "GAME"; break;
        case TitleKind::VirtualConsole: kindLabel = "VIRTUAL CONSOLE"; break;
        case TitleKind::Update: kindLabel = "UPDATE"; break;
        case TitleKind::Dlc: kindLabel = "DLC"; break;
        case TitleKind::Wii: kindLabel = "WII"; break;
        default: kindLabel = "SYSTEM/OTHER"; break;
    }
    // The region flag, storage badge and title-kind label form one centered
    // vertical stack. The top-bar ALL/USB/NAND controls are intentionally untouched.
    SDL_Texture* flag = GetRegionFlag(title.region);
    if (flag)
    {
        SDL_Rect flagRect = { 1310, y + 14, 208, 104 };
        SDL_RenderCopy(renderer_, flag, nullptr, &flagRect);
    }

    const char* storageLabel = "UNKNOWN";
    SDL_Color storageColor = SDL_Color{ 130, 147, 165, 255 };
    if (title.storage == StorageLocation::Usb)
    {
        storageLabel = "USB";
        storageColor = kUsb;
    }
    else if (title.storage == StorageLocation::Nand)
    {
        storageLabel = "NAND";
        storageColor = kNand;
    }

    SDL_Rect storageBadge = { 1342, y + 125, 144, 36 };
    FillRoundedRect(renderer_, storageBadge, 18, storageColor);
    int labelWidth = 0;
    int labelHeight = 0;
    TTF_Font* badgeFont = GetFont(25);
    if (badgeFont && TTF_SizeUTF8(badgeFont, storageLabel, &labelWidth, &labelHeight) == 0)
        DrawText(storageLabel, storageBadge.x + (storageBadge.w - labelWidth) / 2,
                 storageBadge.y + (storageBadge.h - labelHeight) / 2, 25,
                 SDL_Color{ 8, 28, 55, 255 });

    int kindLabelWidth = 0;
    int kindLabelHeight = 0;
    TTF_Font* kindFont = GetFont(30);
    if (kindFont && TTF_SizeUTF8(kindFont, kindLabel, &kindLabelWidth, &kindLabelHeight) == 0)
        DrawText(kindLabel, 1310 + (208 - kindLabelWidth) / 2, y + 184, 30, kMuted);

    SDL_Rect divider = { 1544, y + 38, 3, 178 };
    SDL_SetRenderDrawColor(renderer_, 166, 192, 215, 255);
    SDL_RenderFillRect(renderer_, &divider);

    // Use the supplied circular status artwork; the version-state rules remain unchanged.
    const bool statusInstalledNewerThanDatabase =
        title.hasLatestVersion && title.version > title.latestVersion;
    const bool statusUncertain = !title.hasLatestVersion ||
                                 title.latestVersionDataError ||
                                 statusInstalledNewerThanDatabase;
    const bool statusUpdateAvailable = title.hasLatestVersion &&
                                      title.version < title.latestVersion;
    SDL_Texture* statusTexture = statusUncertain ? unknownStatusTexture_
        : (statusUpdateAvailable ? updateStatusTexture_ : updatedStatusTexture_);
    if (statusTexture)
    {
        const SDL_Rect statusRect = { 1669, y + 25, 64, 64 };
        SDL_RenderCopy(renderer_, statusTexture, nullptr, &statusRect);
    }

    // Keep the existing version capsules below the round status indicator.
    // Retain 50 px of horizontal padding and render each label/value as one string.
    char installedText[64];
    snprintf(installedText, sizeof(installedText), "Installed: v%u", static_cast<unsigned>(title.version));

    const bool updateAvailable = title.hasLatestVersion && title.version < title.latestVersion;
    const bool installedNewerThanDatabase = title.hasLatestVersion && title.version > title.latestVersion;
    const SDL_Color versionGoodBackground = SDL_Color{ 215, 241, 225, 255 };
    const SDL_Color versionGoodForeground = SDL_Color{ 27, 147, 74, 255 };
    const SDL_Color versionOldBackground = SDL_Color{ 251, 227, 227, 255 };
    const SDL_Color versionOldForeground = SDL_Color{ 198, 40, 40, 255 };
    const SDL_Color versionUnknownBackground = SDL_Color{ 230, 235, 241, 255 };
    const SDL_Color versionUnknownForeground = SDL_Color{ 81, 105, 135, 255 };
    // Reuse the existing amber Latest palette as the yellow warning palette for Installed.
    const SDL_Color versionWarningBackground = SDL_Color{ 255, 240, 214, 255 };
    const SDL_Color versionWarningForeground = SDL_Color{ 138, 75, 8, 255 };
    const bool comparisonUncertain = title.latestVersionDataError || installedNewerThanDatabase;

    const SDL_Rect installedCapsule = { 1561, y + 124, 280, 58 };
    const SDL_Color installedBackground = comparisonUncertain
        ? versionWarningBackground
        : (!title.hasLatestVersion ? versionUnknownBackground
           : (updateAvailable ? versionOldBackground : versionGoodBackground));
    const SDL_Color installedForeground = comparisonUncertain
        ? versionWarningForeground
        : (!title.hasLatestVersion ? versionUnknownForeground
           : (updateAvailable ? versionOldForeground : versionGoodForeground));
    FillRoundedRect(renderer_, installedCapsule, 20, installedBackground);

    char latestText[64];
    if (title.hasLatestVersion)
        snprintf(latestText, sizeof(latestText), "Latest: v%u", static_cast<unsigned>(title.latestVersion));
    else
        snprintf(latestText, sizeof(latestText), "Latest: N/A");
    const SDL_Rect latestCapsule = { 1561, y + 184, 280, 58 };
    const SDL_Color latestBackground = comparisonUncertain
        ? versionOldBackground
        : (title.hasLatestVersion ? versionWarningBackground : versionUnknownBackground);
    const SDL_Color latestForeground = comparisonUncertain
        ? versionOldForeground
        : (title.hasLatestVersion ? versionWarningForeground : versionUnknownForeground);
    FillRoundedRect(renderer_, latestCapsule, 20, latestBackground);

    TTF_Font* versionFont = GetFont(33);
    int installedTextWidth = 0;
    int installedTextHeight = 0;
    if (versionFont && TTF_SizeUTF8(versionFont, installedText, &installedTextWidth, &installedTextHeight) == 0)
        DrawText(installedText, installedCapsule.x + 25,
                 installedCapsule.y + (installedCapsule.h - installedTextHeight) / 2,
                 33, installedForeground);

    int latestTextWidth = 0;
    int latestTextHeight = 0;
    if (versionFont && TTF_SizeUTF8(versionFont, latestText, &latestTextWidth, &latestTextHeight) == 0)
        DrawText(latestText, latestCapsule.x + 25,
                 latestCapsule.y + (latestCapsule.h - latestTextHeight) / 2,
                 33, latestForeground);

}

SDL_Texture* UserInterface::GetRegionFlag(TitleRegion region) const
{
    switch (region)
    {
        case TitleRegion::Eur: return flagEurTexture_;
        case TitleRegion::Usa: return flagUsaTexture_;
        case TitleRegion::Jap: return flagJapTexture_;
        case TitleRegion::EurUsa: return flagEurUsaTexture_;
        case TitleRegion::EurJap: return flagEurJapTexture_;
        case TitleRegion::JapUsa: return flagJapUsaTexture_;
        case TitleRegion::All:
        default: return flagAllTexture_;
    }
}

SDL_Texture* UserInterface::LoadEmbeddedPng(const unsigned char* data, unsigned int size)
{
    if (!renderer_ || !data || size == 0)
        return nullptr;
    SDL_RWops* stream = SDL_RWFromConstMem(data, static_cast<int>(size));
    if (!stream)
        return nullptr;
    SDL_Surface* surface = IMG_Load_RW(stream, 1);
    if (!surface)
        return nullptr;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_FreeSurface(surface);
    return texture;
}

TTF_Font* UserInterface::GetFont(int size)
{
    const auto found = fonts_.find(size);
    if (found != fonts_.end())
        return found->second;

    TTF_Font* font = TTF_OpenFontRW(SDL_RWFromConstMem(DejaVuSans_ttf, DejaVuSans_ttf_size), 0, size);
    if (font)
        fonts_.emplace(size, font);
    return font;
}

TTF_Font* UserInterface::GetIconFont(int size)
{
    if (!iconFontData_ || iconFontSize_ == 0)
        return nullptr;
    const auto found = iconFonts_.find(size);
    if (found != iconFonts_.end())
        return found->second;
    SDL_RWops* stream = SDL_RWFromMem(iconFontData_, static_cast<int>(iconFontSize_));
    TTF_Font* font = stream ? TTF_OpenFontRW(stream, 1, size) : nullptr;
    if (font)
        iconFonts_.emplace(size, font);
    return font;
}

int UserInterface::DrawGlyph(const char* glyph, int x, int y, int size, SDL_Color color)
{
    if (!renderer_ || !glyph || !*glyph)
        return 0;

    char keyPrefix[64];
    snprintf(keyPrefix, sizeof(keyPrefix), "%d:%u:%u:%u:%u:",
             size, color.r, color.g, color.b, color.a);
    const std::string key = std::string(keyPrefix) + glyph;
    auto cached = glyphCache_.find(key);
    if (cached == glyphCache_.end())
    {
        TTF_Font* font = GetIconFont(size);
        if (!font)
            return 0;

        SDL_Surface* surface = TTF_RenderUTF8_Blended(font, glyph, color);
        if (!surface)
            return 0;

        CachedText entry;
        entry.texture = SDL_CreateTextureFromSurface(renderer_, surface);
        entry.width = surface->w;
        entry.height = surface->h;
        SDL_FreeSurface(surface);
        if (!entry.texture)
            return 0;

        cached = glyphCache_.emplace(key, entry).first;
    }

    const CachedText& entry = cached->second;
    SDL_Rect destination = { x, y - entry.height / 2, entry.width, entry.height };
    SDL_RenderCopy(renderer_, entry.texture, nullptr, &destination);
    return entry.width;
}

void UserInterface::DrawText(const std::string& text, int x, int y, int size, SDL_Color color)
{
    if (!renderer_ || text.empty())
        return;

    char colorPrefix[64];
    snprintf(colorPrefix, sizeof(colorPrefix), "%d:%u:%u:%u:%u:",
             size, color.r, color.g, color.b, color.a);
    const std::string key = std::string(colorPrefix) + text;

    auto cached = textCache_.find(key);
    if (cached == textCache_.end())
    {
        TTF_Font* font = GetFont(size);
        if (!font)
            return;

        SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
        if (!surface)
            return;

        CachedText entry;
        entry.texture = SDL_CreateTextureFromSurface(renderer_, surface);
        entry.width = surface->w;
        entry.height = surface->h;
        SDL_FreeSurface(surface);

        if (!entry.texture)
            return;

        if (textCache_.size() >= 512)
        {
            auto victim = textCache_.begin();
            if (victim->second.texture)
                SDL_DestroyTexture(victim->second.texture);
            textCache_.erase(victim);
        }

        cached = textCache_.emplace(key, entry).first;
    }

    SDL_Rect destination = { x, y, cached->second.width, cached->second.height };
    SDL_RenderCopy(renderer_, cached->second.texture, nullptr, &destination);
}
