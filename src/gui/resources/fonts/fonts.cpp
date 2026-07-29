#include "pch.h"
#include "fonts.h"
#include "misc/imgui_freetype.h"
#include "font_fredoka_one.h"
#include "font_tahoma_bold.h"
#include "font_proggyclean.h"
#include "font_visitor.h"

#include <Windows.h>

namespace fonts {

    ImFont* fredoka_one = nullptr;
    ImFont* imgui = nullptr;
    ImFont* tahoma_bold = nullptr;
    ImFont* proggy_clean = nullptr;
    ImFont* visitor = nullptr;
    ImFont* verdana = nullptr;

    ImFont* tahoma = nullptr;
    ImFont* esp = nullptr;
    ImFont* esp_bold = nullptr;

    void load(ImGuiIO& io) {
        io.Fonts->SetFontLoader(ImGuiFreeType::GetFontLoader());
        io.Fonts->FontLoaderFlags = 0;

        const unsigned int mono_flags =
            ImGuiFreeTypeLoaderFlags_MonoHinting |
            ImGuiFreeTypeLoaderFlags_Monochrome;

		// дефолт гуи — Fredoka One (OFL)
        ImFontConfig fk{};
        fk.PixelSnapH = true;
        fk.OversampleH = 2;
        fk.OversampleV = 1;
        fk.RasterizerMultiply = 1.0f;
        fk.FontDataOwnedByAtlas = false;
        fk.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;
        fk.SizePixels = 15.0f;
        fredoka_one = io.Fonts->AddFontFromMemoryTTF(
            FredokaOne, sizeof(FredokaOne), 15.0f, &fk,
            io.Fonts->GetGlyphRangesDefault());

        ImFontConfig mono{};
        mono.PixelSnapH = true;
        mono.OversampleH = 1;
        mono.OversampleV = 1;
        mono.RasterizerMultiply = 1.0f;
        mono.FontLoaderFlags = mono_flags;
        mono.SizePixels = 14.0f;
        imgui = io.Fonts->AddFontDefault(&mono);

        ImFontConfig bold{};
        bold.PixelSnapH = true;
        bold.OversampleH = 2;
        bold.OversampleV = 1;
        bold.RasterizerMultiply = 1.0f;
        bold.FontDataOwnedByAtlas = false;
        bold.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;
        bold.SizePixels = 14.0f;
        tahoma_bold = io.Fonts->AddFontFromMemoryTTF(
            TahomaBold, sizeof(TahomaBold), 14.0f, &bold);

        ImFontConfig pixel{};
        pixel.PixelSnapH = true;
        pixel.OversampleH = 1;
        pixel.OversampleV = 1;
        pixel.RasterizerMultiply = 1.0f;
        pixel.FontDataOwnedByAtlas = false;
        pixel.FontLoaderFlags = mono_flags;
        pixel.SizePixels = 14.0f;
        proggy_clean = io.Fonts->AddFontFromMemoryTTF(
            ProggyClean, sizeof(ProggyClean), 14.0f, &pixel);

        ImFontConfig vis_cfg = pixel;
        vis_cfg.SizePixels = 11.0f;
        visitor = io.Fonts->AddFontFromMemoryTTF(
            Visitor, sizeof(Visitor), 11.0f, &vis_cfg);

        // системный verdana, если винды найдутся
        ImFontConfig verdana_cfg{};
        verdana_cfg.PixelSnapH = true;
        verdana_cfg.OversampleH = 2;
        verdana_cfg.OversampleV = 1;
        verdana_cfg.RasterizerMultiply = 1.0f;
        verdana_cfg.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;
        verdana_cfg.SizePixels = 14.0f;

        char verdana_path[MAX_PATH]{};
        if (GetWindowsDirectoryA(verdana_path, MAX_PATH) > 0) {
            strcat_s(verdana_path, "\\Fonts\\verdana.ttf");
            verdana = io.Fonts->AddFontFromFileTTF(
                verdana_path, 14.0f, &verdana_cfg, io.Fonts->GetGlyphRangesCyrillic());
        }

        tahoma = fredoka_one ? fredoka_one : imgui;
        esp = fredoka_one ? fredoka_one : imgui;
        esp_bold = tahoma_bold ? tahoma_bold : tahoma;

        io.FontDefault = fredoka_one ? fredoka_one : imgui;
        if (!io.FontDefault)
            io.FontDefault = io.Fonts->AddFontDefault();
    }

}
