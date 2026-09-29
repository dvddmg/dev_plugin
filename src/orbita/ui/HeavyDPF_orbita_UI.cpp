// ─────────────────────────────────────────────────────────────────────────────
//  Interfaccia di ORBITA
//  La parte comune (knob, pulsanti, zoom, palette, doppio clic) è in common/ui/PluginUIBase.hpp:
//  qui c'è solo ciò che è specifico di questo plugin.
// ─────────────────────────────────────────────────────────────────────────────
#include "PluginUIBase.hpp"

START_NAMESPACE_DISTRHO

// Nome mostrato in alto a sinistra
static const char* const kTitle    = "ORBITA";
static const char* const kSubtitle = "spat · 8 ch";

// Misure specifiche di questo layout (pixel logici, da moltiplicare per fZ)
static constexpr float kCircleR = 170.0f;  // raggio del cerchio degli altoparlanti
static constexpr float kCurveH  = 120.0f;  // altezza del grafico della curva di spread

// Indici dei parametri degli 8 altoparlanti, in ordine
static const uint32_t kSpkParams[8] = {
    parampos_spk_1,
    parampos_spk_2,
    parampos_spk_3,
    parampos_spk_4,
    parampos_spk_5,
    parampos_spk_6,
    parampos_spk_7,
    parampos_spk_8,
};

// riporta un angolo in gradi(0-360) tra -180 e 180
static float wrap180(float deg)
{
    deg = std::fmod(deg + 180.0f, 360.0f);
    if (deg < 0.0f)
        deg += 360.0f;
    return deg - 180.0f;
}

// clip angolo tra -180 e 180
static float clamp180(float deg)
{
    return std::max(-180.0f, std::min(180.0f, deg));
}

// calcolo funzione di spread x disegno (stessa formula di spat.module~)
static float spreadGain(float x, float spread, float offset, float curve)
{
    const float p = 101.0f - spread;
    const float b = 1.0f - 1.0f / (std::sqrt(p) + 0.0001f);

    const float narrow = std::pow(x, p);
    const float wide = std::pow(x, b * curve * 0.25f);

    return (narrow + offset * wide) / (offset + 1.0f);
}

class OrbitaUI : public PluginUIBase
{
protected:
    // ------------------------------------------------------------------
    // Layout della finestra
    //
    //  ┌───────────────────────────┬──────────────────────┐
    //  │ ORBITA  spat · 8 ch        │ MODE [circ][rand][man]│
    //  │                            │  controlli modalità   │
    //  │          cerchio           │ FIELD  rot. compress  │
    //  │                            │ SPEAKERS  1 .. 8      │
    //  ├───────────────────────────┼──────────────────────┤
    //  │      curva di spread       │ SPREAD spr off crv    │
    //  └───────────────────────────┴──────────────────────┘
    void drawContent() override
    {
        const float pad    = kPad * fZ;
        const float gap    = kGap * fZ;
        const float leftW  = 2 * kCircleR * fZ + 40 * fZ;   // colonna sinistra: titolo, cerchio, curva
        const float rightW = 4 * kCell * fZ;                // colonna destra: 4 knob per riga
        const float rightX = pad + leftW + gap;

        // 1. Colonna sinistra: titolo e cerchio
        ImGui::SetCursorPos(ImVec2(pad, pad));
        drawTitle(kTitle, kSubtitle);
        ImGui::SetCursorPosX(pad);
        drawCircle();
        const float circleBottom = ImGui::GetCursorPosY();

        // 2. Colonna destra: modalità, campo e altoparlanti
        ImGui::SetCursorPos(ImVec2(rightX, pad));
        drawModeSection(rightW);
        ImGui::SetCursorPos(ImVec2(rightX, ImGui::GetCursorPosY() + gap * 0.5f));
        drawFieldSection(rightW);
        const float fieldBottom = ImGui::GetCursorPosY();

        // 3. Curva di spread sotto il cerchio, i suoi knob a destra
        const float rowY = std::max(circleBottom, fieldBottom) + gap * 0.5f;
        ImGui::SetCursorPos(ImVec2(pad, rowY));
        drawSpreadCurve(leftW, kCurveH * fZ);
        ImGui::SetCursorPos(ImVec2(rightX, rowY));
        drawSpreadSection(rightW);
    }

    // ------------------------------------------------------------------
    // Sezioni

    // Colonna destra, in alto: selezione della modalità e, sotto, solo i controlli che servono
    void drawModeSection(float width)
    {
        static const char* const modeItems[]      = { "circle", "random", "manual" };
        static const char* const directionItems[] = { "<", ">" };

        const float x0 = ImGui::GetCursorPosX();

        sectionHeader("MODE", width);

        // Tre pulsanti che riempiono la larghezza della colonna
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float bw = (width - 2.0f * spacing) / 3.0f;

        for (int m = 0; m < 3; ++m)
        {
            if (m > 0)
                ImGui::SameLine();
            choiceButton(modeItems[m], parammode, m, bw);
        }

        ImGui::SetCursorPosX(x0);
        const int mode = static_cast<int>(fParams[parammode]);

        if (mode == 0 || mode == 1)     // circolare / random
        {
            // Colonna di pulsanti, allineata al knob (sotto la riga dell'etichetta)
            ImGui::BeginGroup();
            ImGui::Dummy(ImVec2(0, ImGui::GetTextLineHeightWithSpacing()));
            toggleButton("power", parampower, kButtonW * fZ);
            if (mode == 0)
                cycleButton("direction", paramdirection, directionItems, 2, kButtonW * fZ);
            ImGui::EndGroup();

            ImGui::SameLine(0, 4.0f * fZ);
            knobParam("speed", paramspeed, 0.0f, 40.0f, "%.2f");

            if (mode == 0)
            {
                ImGui::SameLine(0, 0);
                knobParam("reset", paramreset, -180.0f, 180.0f, "%.0f°");
            }
        }
        else                            // manuale
        {
            knobParam("posizione", parammanual_pos, -180.0f, 180.0f, "%.0f°");
        }
    }

    // Il cerchio con gli altoparlanti (trascinabili) e la sorgente
    void drawCircle()
    {
        ImDrawList *dl = ImGui::GetWindowDrawList();
        const ImVec2 o = ImGui::GetCursorScreenPos();
        const float r = kCircleR * fZ;
        const ImVec2 c(o.x + r + 20 * fZ, o.y + r + 20 * fZ);

        // 0° = fronte (in alto), angoli positivi in senso orario
        auto toXY = [&](float deg, float rad)
        {
            const float a = deg * kPi / 180.0f;
            return ImVec2(c.x + rad * std::sin(a), c.y - rad * std::cos(a));
        };

        // Cerchio, quadranti e anelli
        dl->AddCircle(c, r, kWarmCream, 96, 2.0f * fZ);
        dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), kGrid);
        dl->AddLine(ImVec2(c.x, c.y - r), ImVec2(c.x, c.y + r), kGrid);

        const float rings[3] = {0.25f, 0.50f, 0.75f};
        for (int i = 0; i < 3; ++i)
            dl->AddCircle(c, r * rings[i], kGrid, 96);

        // Altoparlanti
        const float compress = fParams[paramcompress];
        const float rotation = fParams[paramrotazione];
        const float dotR = 9.0f * fZ;
        const float grabR = 13.0f * fZ;

        for (int i = 0; i < 8; ++i)
        {
            const uint32_t index = kSpkParams[i];
            const ImVec2 p = toXY(compress * fParams[index] + rotation, r);

            // Area cliccabile sul pallino
            ImGui::SetCursorScreenPos(ImVec2(p.x - grabR, p.y - grabR));
            ImGui::PushID(i);
            ImGui::InvisibleButton("spk", ImVec2(2 * grabR, 2 * grabR));
            ImGui::PopID();

            bool changed = false;

            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0) && std::fabs(compress) > 0.01f)
            {
                const ImVec2 m = ImGui::GetIO().MousePos;
                const float mouseDeg = std::atan2(m.x - c.x, c.y - m.y) * 180.0f / kPi;

                fParams[index] = clamp180(wrap180(mouseDeg - rotation) / compress);
                changed = true;
            }

            handleGesture(index, changed);

            const bool hot = ImGui::IsItemHovered() || ImGui::IsItemActive();
            const ImVec2 q = toXY(compress * fParams[index] + rotation, r);

            // Pallino Mocha Mousse; bordo panna quando è sotto il mouse o trascinato
            dl->AddCircleFilled(q, dotR, kMocha);
            if (hot)
                dl->AddCircle(q, dotR + 2.0f * fZ, kWarmCream, 0, 2.0f * fZ);

            const char label[2] = {char('1' + i), '\0'};
            const ImVec2 ts = ImGui::CalcTextSize(label);
            dl->AddText(ImVec2(q.x - ts.x * 0.5f, q.y - ts.y * 0.5f), kCloud, label);
        }

        // Sorgente: in modalità manuale si trascina con il mouse
        const bool manual = static_cast<int>(fParams[parammode]) == 2;
        const float srcDeg = manual ? fParams[parammanual_pos] : fParams[parampos_source_out];
        ImVec2 s = toXY(srcDeg, r * 0.85f);

        if (manual)
        {
            ImGui::SetCursorScreenPos(ImVec2(s.x - grabR, s.y - grabR));
            ImGui::InvisibleButton("src", ImVec2(2 * grabR, 2 * grabR));

            bool changed = false;
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))
            {
                const ImVec2 m = ImGui::GetIO().MousePos;
                fParams[parammanual_pos] = std::atan2(m.x - c.x, c.y - m.y) * 180.0f / kPi;
                changed = true;
            }
            handleGesture(parammanual_pos, changed);   // gesto, automazione e doppio clic

            s = toXY(fParams[parammanual_pos], r * 0.85f);
        }

        // Sorgente: pieno Cloud Dancer con anello Mocha, per distinguerla dagli altoparlanti
        dl->AddCircleFilled(s, 10.0f * fZ, kCloud);
        dl->AddCircle(s, 10.0f * fZ, kMocha, 0, 2.0f * fZ);

        ImGui::SetCursorScreenPos(o);
        ImGui::Dummy(ImVec2(2 * r + 40 * fZ, 2 * r + 40 * fZ));
    }

    // Colonna destra, sotto la modalità: rotazione e compress, poi gli 8 altoparlanti
    void drawFieldSection(float width)
    {
        const float x0 = ImGui::GetCursorPosX();

        sectionHeader("FIELD", width);
        knobParam("rotation", paramrotazione, -180.0f, 180.0f, "%.0f°");
        ImGui::SameLine(0, 0);
        knobParam("compress", paramcompress, -1.0f, 1.0f, "%.2f");

        ImGui::SetCursorPosX(x0);
        sectionHeader("SPEAKERS", width);

        char label[16];
        for (int i = 0; i < 8; ++i)
        {
            if (i % 4 == 0)
                ImGui::SetCursorPosX(x0);   // inizio riga
            else
                ImGui::SameLine(0, 0);      // stessa riga, celle attaccate

            std::snprintf(label, sizeof(label), "spk %d", i + 1);
            knobParam(label, kSpkParams[i], -180.0f, 180.0f, "%.1f°");
        }
    }

    // Sotto il cerchio: la curva di trasferimento dello spread
    void drawSpreadCurve(float w, float h)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 o = ImGui::GetCursorScreenPos();   // angolo in alto a sinistra del grafico

        const float spread = fParams[paramspread];
        const float offset = fParams[paramoffset];
        const float curve  = fParams[paramcurve];

        // Sfondo, cornice e griglia: verticali a -90°, 0°, +90°, orizzontale a 0.5
        dl->AddRectFilled(o, ImVec2(o.x + w, o.y + h), kGray426, 4.0f * fZ);
        dl->AddRect(o, ImVec2(o.x + w, o.y + h), kCoolGray11, 4.0f * fZ);
        for (int k = 1; k < 4; ++k)
        {
            const float gx = o.x + w * k / 4.0f;
            dl->AddLine(ImVec2(gx, o.y), ImVec2(gx, o.y + h), kGrid);
        }
        dl->AddLine(ImVec2(o.x, o.y + h * 0.5f), ImVec2(o.x + w, o.y + h * 0.5f), kGrid);

        // Curva: un punto per ogni grado, da -180° a +180°
        constexpr int kPoints = 361;
        ImVec2 pts[kPoints];

        for (int i = 0; i < kPoints; ++i)
        {
            const float deg = -180.0f + 360.0f * i / (kPoints - 1);        // distanza angolare
            const float x   = 0.5f * (1.0f + std::cos(deg * kPi / 180.0f)); // come [cos~] [*~ 0.5] [+~ 0.5]
            const float g   = spreadGain(x, spread, offset, curve);

            pts[i] = ImVec2(o.x + w * i / (kPoints - 1),   // da sinistra a destra
                            o.y + h * (1.0f - g));         // g = 1 in alto, g = 0 in basso
        }
        dl->AddPolyline(pts, kPoints, kCloud, 0, 2.0f * fZ);

        // Etichette degli assi
        const char* right = "180";
        const float ly = o.y + h + 2.0f * fZ;
        dl->AddText(ImVec2(o.x, ly), kCoolGray11, "-180");
        dl->AddText(ImVec2(o.x + w * 0.5f - ImGui::CalcTextSize("0").x * 0.5f, ly), kCoolGray11, "0");
        dl->AddText(ImVec2(o.x + w - ImGui::CalcTextSize(right).x, ly), kCoolGray11, right);
        dl->AddText(ImVec2(o.x + 4.0f * fZ, o.y + 2.0f * fZ), kCoolGray11, "1");

        // Riserva lo spazio: grafico + riga delle etichette
        ImGui::Dummy(ImVec2(w, h + ImGui::GetTextLineHeightWithSpacing()));
    }

    // A destra della curva: i tre parametri che la modellano
    void drawSpreadSection(float width)
    {
        sectionHeader("SPREAD", width);
        knobParam("spread", paramspread, 0.0f, 100.0f, "%.1f");
        ImGui::SameLine(0, 0);
        knobParam("offset", paramoffset, 0.0f, 100.0f, "%.1f");
        ImGui::SameLine(0, 0);
        knobParam("curve",  paramcurve,  0.0f, 100.0f, "%.1f");
    }
};

UI* createUI()
{
    return new OrbitaUI();
}

END_NAMESPACE_DISTRHO
