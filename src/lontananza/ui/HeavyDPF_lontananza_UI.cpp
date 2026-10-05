// ─────────────────────────────────────────────────────────────────────────────
//  Interfaccia di LONTANANZA
//  La parte comune (knob, pulsanti, zoom, palette, doppio clic) è in
//  common/ui/PluginUIBase.hpp: qui c'è solo ciò che è specifico del plugin.
// ─────────────────────────────────────────────────────────────────────────────
#include "PluginUIBase.hpp"

START_NAMESPACE_DISTRHO

// Nome mostrato in alto a sinistra
static const char* const kTitle    = "LONTANANZA";
static const char* const kSubtitle = "distance · 4-8 ch";

// Canali: devono corrispondere a [r inputs @hv_param 4 8 8 int] nella patch
static constexpr int   kMinChannels = 4;
static constexpr int   kMaxChannels = 8;

// Distanza massima: deve corrispondere a [clip 1 50] in trasp_module~
static constexpr float kMaxDist     = 50.0f;

// Misure specifiche di questo layout (pixel logici, da moltiplicare per fZ)
static constexpr float kRowH        = 40.0f;   // altezza di una riga = un canale
static constexpr float kSmallKnob   = 32.0f;   // knob compatti delle righe
static constexpr float kNumW        = 24.0f;   // colonna con il numero del canale
static constexpr float kKnobColW    = 104.0f;  // knob compatto + valore
static constexpr float kLaneW       = 250.0f;  // corsia della distanza
static constexpr float kCurveH      = 90.0f;   // grafico dell'attenuazione

// Indici dei parametri di ogni canale, in ordine
static const uint32_t kDistParams[kMaxChannels] = {
    paramdist_1_mt, paramdist_2_mt, paramdist_3_mt, paramdist_4_mt,
    paramdist_5_mt, paramdist_6_mt, paramdist_7_mt, paramdist_8_mt,
};
static const uint32_t kGainParams[kMaxChannels] = {
    paramgain_1_dB, paramgain_2_dB, paramgain_3_dB, paramgain_4_dB,
    paramgain_5_dB, paramgain_6_dB, paramgain_7_dB, paramgain_8_dB,
};

class LontananzaUI : public PluginUIBase
{
protected:
    // ------------------------------------------------------------------
    // Calcoli: le stesse formule della patch

    // Numero di canali attivi, dal parametro "inputs"
    int channelCount() const
    {
        return std::max(kMinChannels, std::min(kMaxChannels, static_cast<int>(fParams[paraminputs] + 0.5f)));
    }

    // Distanza effettiva del canale i: distanza × master, limitata a 1..50 m ([*] e [clip 1 50])
    float effectiveDistance(int i) const
    {
        return std::max(1.0f, std::min(kMaxDist, fParams[kDistParams[i]] * fParams[parammaster_dist]));
    }

    // Attenuazione dovuta alla distanza, in dB: 1 / d^trasparenza ([expr 1 / max(pow(...))])
    float distanceDb(float d) const
    {
        return -20.0f * fParams[paramtrasparenza] * std::log10(d);
    }

    // ------------------------------------------------------------------
    // Layout: numero | distanza (knob + corsia) | gain | master
    void drawContent() override
    {
        const float pad     = kPad * fZ;
        const float gap     = kGap * fZ;
        const float lineH   = ImGui::GetTextLineHeightWithSpacing();
        const float numW    = kNumW * fZ;
        const float knobW   = kKnobColW * fZ;
        const float laneW   = kLaneW * fZ;
        const float rowH    = kRowH * fZ;
        const float masterW = 2 * kCell * fZ;

        const float xNum    = pad;
        const float xDist   = xNum + numW;
        const float xLane   = xDist + knobW;
        const float xGain   = xLane + laneW + 12.0f * fZ;
        const float xMaster = xGain + knobW + gap;

        // Titolo
        ImGui::SetCursorPos(ImVec2(pad, pad));
        drawTitle(kTitle, kSubtitle);
        const float headerY = ImGui::GetCursorPosY();

        // Intestazioni delle colonne
        ImGui::SetCursorPos(ImVec2(xDist, headerY));
        sectionHeader("DISTANCE", knobW + laneW - 8.0f * fZ);
        ImGui::SetCursorPos(ImVec2(xGain, headerY));
        sectionHeader("GAIN", knobW - 8.0f * fZ);

        // Righello in metri sopra le corsie
        const float rulerY = headerY + lineH;
        ImGui::SetCursorPos(ImVec2(xLane, rulerY));
        drawRuler(laneW);

        // Una riga per ogni canale attivo
        const float rowsY = rulerY + lineH + 2.0f * fZ;
        const int n = channelCount();
        char id[16];

        for (int i = 0; i < n; ++i)
        {
            const float y = rowsY + i * rowH;

            // Numero del canale, centrato in verticale rispetto al knob
            char num[4];
            std::snprintf(num, sizeof(num), "%d", i + 1);
            ImGui::SetCursorPos(ImVec2(xNum, y + (kSmallKnob * fZ - ImGui::GetTextLineHeight()) * 0.5f));
            ImGui::PushStyleColor(ImGuiCol_Text, kCoolGray11);
            ImGui::TextUnformatted(num);
            ImGui::PopStyleColor();

            ImGui::SetCursorPos(ImVec2(xDist, y));
            std::snprintf(id, sizeof(id), "##dist%d", i);
            knobInline(id, kDistParams[i], 1.0f, kMaxDist, "%.1f m");

            ImGui::SetCursorPos(ImVec2(xLane, y));
            drawLane(i, laneW);

            ImGui::SetCursorPos(ImVec2(xGain, y));
            std::snprintf(id, sizeof(id), "##gain%d", i);
            knobInline(id, kGainParams[i], -90.0f, 12.0f, "%.1f dB");
        }

        // Colonna destra: parte dalla stessa altezza delle intestazioni
        ImGui::SetCursorPos(ImVec2(xMaster, headerY));
        drawRightColumn(masterW, n);
    }

    // ------------------------------------------------------------------
    // Elementi

    // Knob compatto su una riga: knob a sinistra, valore a destra
    void knobInline(const char* id, uint32_t index, float min, float max, const char* format)
    {
        const float size = kSmallKnob * fZ;

        ImGui::BeginGroup();

        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kWarmCream);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kCloud);
        const bool changed = ImGuiKnobs::Knob(id, &fParams[index], min, max, 0.0f, format,
                                              ImGuiKnobVariant_WiperDot, size,
                                              ImGuiKnobFlags_NoTitle | ImGuiKnobFlags_NoInput);
        ImGui::PopStyleColor(2);
        handleGesture(index, changed);   // subito dopo il knob: IsItem* si riferiscono a lui

        // Valore accanto al knob, centrato in verticale (cliccabile per digitarlo)
        ImGui::SameLine(0, 8.0f * fZ);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (size - ImGui::GetTextLineHeight()) * 0.5f);
        valueField(index, min, max, format, 0.0f, 0.0f);

        ImGui::EndGroup();
    }

    // Righello: 0, 10, 20, 30, 40, 50 m sopra le corsie
    void drawRuler(float w)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 o = ImGui::GetCursorScreenPos();
        char text[8];

        for (int m = 0; m <= 50; m += 10)
        {
            if (m == 50)
                std::snprintf(text, sizeof(text), "%d m", m);
            else
                std::snprintf(text, sizeof(text), "%d", m);

            const float x  = o.x + w * m / kMaxDist;
            const ImVec2 ts = ImGui::CalcTextSize(text);
            const float tx = std::max(o.x, std::min(o.x + w - ts.x, x - ts.x * 0.5f));   // resta dentro la corsia
            dl->AddText(ImVec2(tx, o.y), kCoolGray11, text);
        }
    }

    // Corsia del canale i: altoparlante alla distanza effettiva (trascinabile), ritardo e livello
    void drawLane(int i, float w)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 o = ImGui::GetCursorScreenPos();
        const float h  = kSmallKnob * fZ;
        const float cy = o.y + h * 0.5f;
        const uint32_t index = kDistParams[i];

        // 1. Interazione: trascinando si sceglie la distanza effettiva;
        //    il parametro del canale è quella distanza divisa per il master
        ImGui::PushID(i);
        ImGui::InvisibleButton("lane", ImVec2(w, h));
        ImGui::PopID();

        bool changed = false;
        if (ImGui::IsItemActive())
        {
            const float d = (ImGui::GetIO().MousePos.x - o.x) / w * kMaxDist;
            fParams[index] = std::max(1.0f, std::min(kMaxDist, d / fParams[parammaster_dist]));
            changed = true;
        }
        handleGesture(index, changed);   // gesto di automazione e doppio clic, come per i knob
        const bool hot = ImGui::IsItemHovered() || ImGui::IsItemActive();

        // 2. Corsia con una tacca ogni 10 m
        dl->AddLine(ImVec2(o.x, cy), ImVec2(o.x + w, cy), kGrid, 2.0f * fZ);
        for (int m = 10; m < 50; m += 10)
        {
            const float x = o.x + w * m / kMaxDist;
            dl->AddLine(ImVec2(x, cy - 3.0f * fZ), ImVec2(x, cy + 3.0f * fZ), kGrid);
        }

        // 3. Altoparlante alla distanza effettiva: più lontano = più scuro (filtro "aria")
        const float d = effectiveDistance(i);
        const ImVec2 p(o.x + w * d / kMaxDist, cy);
        const float r = 7.0f * fZ;
        dl->AddCircleFilled(p, r, mixColor(kWarmCream, kMocha, d / kMaxDist));
        if (hot)
            dl->AddCircle(p, r + 2.0f * fZ, kCloud, 0, 1.5f * fZ);

        // 4. Ritardo e livello risultanti, sotto la corsia a destra
        const float delayMs = d / 343.0f * 1000.0f;                 // [expr ($f1 / 343) * 1000]
        const bool  silent  = fParams[kGainParams[i]] <= -89.9f || fParams[parammaster_gain] <= -89.9f;
        const float level   = fParams[kGainParams[i]] + fParams[parammaster_gain] + distanceDb(d);

        char info[32];
        if (silent)
            std::snprintf(info, sizeof(info), "%.0f ms  -inf dB", delayMs);
        else
            std::snprintf(info, sizeof(info), "%.0f ms  %.1f dB", delayMs, level);

        const ImVec2 ts = ImGui::CalcTextSize(info);
        dl->AddText(ImVec2(o.x + w - ts.x, cy + 6.0f * fZ), kCoolGray11, info);
    }

    // Colonna destra: altoparlanti, spazio (con il grafico) e master
    void drawRightColumn(float width, int n)
    {
        static const char* const labels[] = { "4", "5", "6", "7", "8" };

        const float x0      = ImGui::GetCursorPosX();
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float bw      = (width - 4.0f * spacing) / 5.0f;

        sectionHeader("SPEAKERS", width);
        for (int k = 0; k < 5; ++k)
        {
            if (k > 0)
                ImGui::SameLine();
            choiceButton(labels[k], paraminputs, kMinChannels + k, bw);
        }

        ImGui::SetCursorPosX(x0);
        sectionHeader("SPACE", width);
        knobParam("distance", parammaster_dist, 1.0f, 2.0f, "x %.2f");
        ImGui::SameLine(0, 0);
        knobParam("transparency", paramtrasparenza, 0.0f, 10.0f, "%.2f");

        float dists[kMaxChannels];
        for (int i = 0; i < n; ++i)
            dists[i] = effectiveDistance(i);
        ImGui::SetCursorPosX(x0);
        drawDistanceCurve(width, kCurveH * fZ, fParams[paramtrasparenza], kMaxDist, dists, n);

        ImGui::SetCursorPosX(x0);
        sectionHeader("MASTER", width);
        ImGui::SetCursorPosX(x0 + (width - kCell * fZ) * 0.5f);
        knobParam("gain", parammaster_gain, -90.0f, 12.0f, "%.1f dB");
    }
};

UI* createUI()
{
    return new LontananzaUI();
}

END_NAMESPACE_DISTRHO