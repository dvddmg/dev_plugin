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
static const ImU32 kWarmCream  = IM_COL32(0xD6, 0xD2, 0xC4, 255);   // Pantone 7527 C: accento panna
static const ImU32 kCloud      = IM_COL32(0xF0, 0xEE, 0xE9, 255);   // Pantone 11-4201 Cloud Dancer: testo, evidenze
static const ImU32 kMocha      = IM_COL32(0xA4, 0x78, 0x64, 255);   // Pantone 17-1230 Mocha Mousse: accento caldo

static const ImU32 kGrid = IM_COL32(0x53, 0x56, 0x5A, 140);         // Cool Gray 11 semitrasparente

END_NAMESPACE_DISTRHO
