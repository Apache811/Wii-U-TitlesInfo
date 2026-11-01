#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <string>
#include <cstdint>
#include <unordered_map>
#include "TitleRepository.hpp"
#include "DebugConfig.hpp"

enum class TitleListFilter
{
    All,
    Usb,
    Nand,
#if __DEBUG__
    None
#endif
};

class UserInterface
{
public:
    bool Initialize(std::string& errorMessage);
    void Shutdown();
    void Draw(TitleRepository& repository, int selectedIndex, TitleListFilter filter, const std::string& searchText);
    void DrawRefreshing(uint32_t elapsedMilliseconds);
    int FilteredTitleCount(const TitleRepository& repository, TitleListFilter filter, const std::string& searchText) const;

private:
    struct CachedText
    {
        SDL_Texture* texture = nullptr;
        int width = 0;
        int height = 0;
    };

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* zlButtonTexture_ = nullptr;
    SDL_Texture* zrButtonTexture_ = nullptr;
    SDL_Texture* flagAllTexture_ = nullptr;
    SDL_Texture* flagEurTexture_ = nullptr;
    SDL_Texture* flagUsaTexture_ = nullptr;
    SDL_Texture* flagJapTexture_ = nullptr;
    SDL_Texture* flagEurUsaTexture_ = nullptr;
    SDL_Texture* flagEurJapTexture_ = nullptr;
    SDL_Texture* flagJapUsaTexture_ = nullptr;
    SDL_Texture* unknownStatusTexture_ = nullptr;
    SDL_Texture* updateStatusTexture_ = nullptr;
    SDL_Texture* updatedStatusTexture_ = nullptr;
    void* iconFontData_ = nullptr;
    uint32_t iconFontSize_ = 0;
    int firstVisibleIndex_ = 0;
    std::unordered_map<int, TTF_Font*> fonts_;
    std::unordered_map<int, TTF_Font*> iconFonts_;
    std::unordered_map<std::string, CachedText> textCache_;
    std::unordered_map<std::string, CachedText> glyphCache_;

    TTF_Font* GetFont(int size);
    TTF_Font* GetIconFont(int size);
    int DrawGlyph(const char* glyph, int x, int y, int size, SDL_Color color);
    void DrawText(const std::string& text, int x, int y, int size, SDL_Color color);
    void DrawTitleRow(InstalledTitle& title, int rowIndex, int y, bool selected);
    SDL_Texture* GetRegionFlag(TitleRegion region) const;
    SDL_Texture* LoadEmbeddedPng(const unsigned char* data, unsigned int size);
    void DrawTopBar(TitleListFilter filter);
    void DrawBottomBar();
    void DrawFilterButton(const char* label, int x, int y, int width, int height, bool selected);
};
