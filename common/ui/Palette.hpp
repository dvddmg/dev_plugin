// ─────────────────────────────────────────────────────────────────────────────
//  Palette.hpp — colori comuni a tutti i plugin
//  Valori sRGB dei colori Pantone indicati (per lo schermo sono un'approssimazione).
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "DistrhoUI.hpp"   // porta con sé ImGui (tramite DearImGui.hpp)

START_NAMESPACE_DISTRHO

static const ImU32 kBlack6     = IM_COL32(0x10, 0x18, 0x20, 255);   // Pantone Black 6 C: sfondo
static const ImU32 kGray426    = IM_COL32(0x25, 0x28, 0x2A, 255);   // Pantone 426 C: pannelli, pulsanti
static const ImU32 kCoolGray11 = IM_COL32(0x53, 0x56, 0x5A, 255);   // Pantone Cool Gray 11 C: griglie, testo secondario
static const ImU32 kWarmCream  = IM_COL32(0xEF, 0xDB, 0xB2, 255);   // vicino a Pantone 7506 C: accento panna
static const ImU32 kCloud      = IM_COL32(0xF0, 0xEE, 0xE9, 255);   // Pantone 11-4201 Cloud Dancer: testo, evidenze
static const ImU32 kMocha      = IM_COL32(0xC3, 0x7C, 0x54, 255);   // vicino a Pantone 16-1439 Caramel: accento caldo

static const ImU32 kGrid = IM_COL32(0x53, 0x56, 0x5A, 140);         // Cool Gray 11 semitrasparente

// Lo stesso colore con un'altra opacità (alpha 0..255)
static inline ImU32 withAlpha(ImU32 color, int alpha)
{
    return (color & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT);
}

// Colore intermedio tra a (t = 0) e b (t = 1)
static inline ImU32 mixColor(ImU32 a, ImU32 b, float t)
{
    const ImVec4 ca = ImGui::ColorConvertU32ToFloat4(a);
    const ImVec4 cb = ImGui::ColorConvertU32ToFloat4(b);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(ca.x + (cb.x - ca.x) * t,
                                                 ca.y + (cb.y - ca.y) * t,
                                                 ca.z + (cb.z - ca.z) * t,
                                                 ca.w + (cb.w - ca.w) * t));
}

END_NAMESPACE_DISTRHO
