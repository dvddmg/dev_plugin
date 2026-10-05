// ─────────────────────────────────────────────────────────────────────────────
//  Interfaccia di DIFFUSER
//  La parte comune (knob, pulsanti, zoom, palette, doppio clic) è in
//  common/ui/PluginUIBase.hpp: qui c'è solo ciò che è specifico del plugin.
// ─────────────────────────────────────────────────────────────────────────────
#include "PluginUIBase.hpp"

START_NAMESPACE_DISTRHO

// Nome mostrato in alto a sinistra
static const char* const kTitle    = "DIFFUSER";
static const char* const kSubtitle = "stereo · 4-8 ch";

// Misure specifiche di questo layout (pixel logici, da moltiplicare per fZ)
static constexpr float kStageW = 280.0f;   // riquadro della sala
static constexpr float kStageH = 320.0f;
static constexpr float kStageM = 34.0f;    // margine sopra e sotto (scritte FRONT / BACK)
static constexpr float kSideM  = 34.0f;    // distanza degli altoparlanti dai lati

class DiffuserUI : public PluginUIBase
{
protected:
    // Numero di coppie attive: speakers 0/1/2 → 4/6/8 altoparlanti → 2/3/4 coppie
    int pairCount() const
    {
        return 2 + std::max(0, std::min(2, static_cast<int>(fParams[paramspeakers] + 0.5f)));
    }

    // Distanza in metri della coppia k (su pairs coppie) dalla sorgente: come db.pairdist
    float pairDistance(int k, int pairs) const
    {
        const float pos = static_cast<float>(k) / (pairs - 1);
        return std::min(50.0f, std::max(1.0f, std::fabs(fParams[paramdistance] - pos) * fParams[paramdepth]));
    }

    // ------------------------------------------------------------------
    // Layout: titolo e sala a sinistra, controlli a destra
    void drawContent() override
    {
        const float pad    = kPad * fZ;
        const float gap    = kGap * fZ;
        const float stageW = kStageW * fZ;
        const float stageH = kStageH * fZ;
        const float rightW = 2 * kCell * fZ;
        const float rightX = pad + stageW + gap;

        ImGui::SetCursorPos(ImVec2(pad, pad));
        drawTitle(kTitle, kSubtitle);
        const float topY = ImGui::GetCursorPosY() + 4.0f * fZ;

        ImGui::SetCursorPos(ImVec2(pad, topY));
        drawStage(stageW, stageH);

        ImGui::SetCursorPos(ImVec2(rightX, topY));
        drawControls(rightW);
    }

    // ------------------------------------------------------------------
    // La sala vista dall'alto: coppie di altoparlanti e sorgente trascinabile
    void drawStage(float w, float h)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 o = ImGui::GetCursorScreenPos();

        const float top    = o.y + kStageM * fZ;       // riga della prima coppia (fronte)
        const float bottom = o.y + h - kStageM * fZ;   // riga dell'ultima coppia (retro)
        const float xL     = o.x + kSideM * fZ;        // colonna degli altoparlanti sinistri
        const float xR     = o.x + w - kSideM * fZ;    // colonna degli altoparlanti destri
        const float cx     = (xL + xR) * 0.5f;         // centro
        const float half   = (xR - xL) * 0.5f;

        // 1. Interazione: trascinare nella sala muove la sorgente
        //    verticale → position (distance), orizzontale → crossing (panning)
        ImGui::SetCursorScreenPos(o);
        ImGui::InvisibleButton("stage", ImVec2(w, h));

        if (ImGui::IsItemActivated())
        {
            editParameter(paramdistance, true);
            editParameter(parampanning, true);
        }
        if (ImGui::IsItemActive())
        {
            const ImVec2 m = ImGui::GetIO().MousePos;
            const float pos   = std::max(0.0f, std::min(1.0f, (m.y - top) / (bottom - top)));
            const float cross = std::max(-1.0f, std::min(1.0f, (m.x - cx) / half));

            fParams[paramdistance] = pos;
            fParams[parampanning]  = (cross + 1.0f) * 0.5f;
            setParameterValue(paramdistance, fParams[paramdistance]);
            setParameterValue(parampanning, fParams[parampanning]);
        }
        if (ImGui::IsItemDeactivated())
        {
            editParameter(paramdistance, false);
            editParameter(parampanning, false);
        }

        // 2. Sfondo, cornice, asse centrale, FRONT / BACK
        dl->AddRectFilled(o, ImVec2(o.x + w, o.y + h), kGray426, 6.0f * fZ);
        dl->AddRect(o, ImVec2(o.x + w, o.y + h), kCoolGray11, 6.0f * fZ);
        dl->AddLine(ImVec2(cx, top), ImVec2(cx, bottom), kGrid);

        const ImVec2 fs = ImGui::CalcTextSize("FRONT");
        const ImVec2 bs = ImGui::CalcTextSize("BACK");
        dl->AddText(ImVec2(cx - fs.x * 0.5f, o.y + 4.0f * fZ), kCoolGray11, "FRONT");
        dl->AddText(ImVec2(cx - bs.x * 0.5f, o.y + h - bs.y - 4.0f * fZ), kCoolGray11, "BACK");

        // 3. Coppie di altoparlanti
        const int   pairs  = pairCount();
        const float depth  = fParams[paramdepth];
        const float trasp  = fParams[paramtrasp];
        const float source = fParams[paramdistance];
        const float dotR   = 9.0f * fZ;

        for (int k = 0; k < pairs; ++k)
        {
            const float pos = static_cast<float>(k) / (pairs - 1);   // 0 = fronte, 1 = retro
            const float y   = top + pos * (bottom - top);

            const float dist = pairDistance(k, pairs);
            const float g    = std::min(1.0f, 1.0f / std::pow(dist, trasp));
            const ImU32 col  = withAlpha(kMocha, static_cast<int>(70.0f + 185.0f * g));

            dl->AddLine(ImVec2(xL, y), ImVec2(xR, y), kGrid);

            // I due altoparlanti della coppia, con il numero del canale
            char label[4];
            for (int side = 0; side < 2; ++side)
            {
                const ImVec2 p(side == 0 ? xL : xR, y);
                dl->AddCircleFilled(p, dotR, col);
                std::snprintf(label, sizeof(label), "%d", 2 * k + 1 + side);
                const ImVec2 ts = ImGui::CalcTextSize(label);
                dl->AddText(ImVec2(p.x - ts.x * 0.5f, p.y - ts.y * 0.5f), kCloud, label);
            }

            // Distanza della coppia dal fronte, in metri
            char meters[16];
            std::snprintf(meters, sizeof(meters), "%.1f m", pos * depth);
            const ImVec2 ms = ImGui::CalcTextSize(meters);
            dl->AddText(ImVec2(cx - ms.x * 0.5f, y - ms.y - 3.0f * fZ), kCoolGray11, meters);
        }

        // 4. Sorgente: linea alla sua posizione, con i marcatori degli ingressi L e R
        const float ys    = top + source * (bottom - top);
        const float cross = fParams[parampanning] * 2.0f - 1.0f;   // -1 = RL, 0 = mono, +1 = LR

        dl->AddLine(ImVec2(xL, ys), ImVec2(xR, ys), kWarmCream, 1.5f * fZ);

        if (std::fabs(cross) < 0.05f)
        {
            sourceMarker(dl, ImVec2(cx, ys), "LR");   // mono: i due ingressi coincidono
        }
        else
        {
            sourceMarker(dl, ImVec2(cx - cross * half, ys), "L");
            sourceMarker(dl, ImVec2(cx + cross * half, ys), "R");
        }
    }

    // Marcatore di un ingresso: cerchio color Cloud Dancer con la lettera scura
    void sourceMarker(ImDrawList* dl, ImVec2 p, const char* text)
    {
        const float r = 11.0f * fZ;
        dl->AddCircleFilled(p, r, kCloud);
        dl->AddCircle(p, r, kMocha, 0, 2.0f * fZ);
        const ImVec2 ts = ImGui::CalcTextSize(text);
        dl->AddText(ImVec2(p.x - ts.x * 0.5f, p.y - ts.y * 0.5f), kBlack6, text);
    }

    // ------------------------------------------------------------------
    // Colonna destra: numero di altoparlanti, sorgente, spazio
    void drawControls(float width)
    {
        static const char* const countLabels[] = { "4", "6", "8" };

        const float x0      = ImGui::GetCursorPosX();
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float bw      = (width - 2.0f * spacing) / 3.0f;

        sectionHeader("SPEAKERS", width);
        for (int k = 0; k < 3; ++k)
        {
            if (k > 0)
                ImGui::SameLine();
            choiceButton(countLabels[k], paramspeakers, k, bw);   // valore 0/1/2 → 4/6/8
        }

        ImGui::SetCursorPosX(x0);
        sectionHeader("SOURCE", width);
        knobParam("position", paramdistance, 0.0f, 1.0f, "%.2f");
        ImGui::SameLine(0, 0);
        knobParam("crossing", parampanning, 0.0f, 1.0f, "%.2f");

        ImGui::SetCursorPosX(x0);
        sectionHeader("SPACE", width);
        knobParam("depth", paramdepth, 1.0f, 50.0f, "%.1f m");
        ImGui::SameLine(0, 0);
        knobParam("transparency", paramtrasp, 0.0f, 10.0f, "%.2f");

        // Livello in funzione della distanza, con un punto per ogni coppia attiva
        const int pairs = pairCount();
        float dists[4];
        for (int k = 0; k < pairs; ++k)
            dists[k] = pairDistance(k, pairs);
        ImGui::SetCursorPosX(x0);
        drawDistanceCurve(width, 90.0f * fZ, fParams[paramtrasp], 50.0f, dists, pairs);
    }
};

UI* createUI()
{
    return new DiffuserUI();
}

END_NAMESPACE_DISTRHO