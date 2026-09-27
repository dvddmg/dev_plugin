#include "DistrhoUI.hpp"
#include "ResizeHandle.hpp"
#include "HeavyParams.hpp" // generato da build.sh: indici sempre allineati al plugin

#include <cmath>
#include <cfloat>
#include <cstdio>
#include <algorithm>

START_NAMESPACE_DISTRHO

// pi greco
static constexpr float kPi = 3.14159265f;

// Nome mostrato in alto a sinistra
static const char* const kTitle    = "ORBITA";
static const char* const kSubtitle = "spat · 8 ch";

// Misure del layout, in pixel "logici" alla dimensione di partenza della finestra.
// Vengono moltiplicate per fZ (scala dello schermo × zoom della finestra).
static constexpr float kPad     = 14.0f;   // margine esterno della finestra
static constexpr float kGap     = 16.0f;   // spazio tra i pannelli
static constexpr float kCircleR = 170.0f;  // raggio del cerchio degli altoparlanti
static constexpr float kKnob    = 44.0f;   // diametro di un knob
static constexpr float kCell    = 68.0f;   // larghezza della cella di un knob (knob + etichetta)
static constexpr float kCurveH  = 120.0f;  // altezza del grafico della curva di spread
static constexpr float kButtonW = 100.0f;  // larghezza dei pulsanti power / direction

// Palette (valori sRGB dei colori Pantone indicati)
static const ImU32 kBlack6     = IM_COL32(0x10, 0x18, 0x20, 255);   // Pantone Black 6 C: sfondo
static const ImU32 kGray426    = IM_COL32(0x25, 0x28, 0x2A, 255);   // Pantone 426 C: pannelli, pulsanti
static const ImU32 kCoolGray11 = IM_COL32(0x53, 0x56, 0x5A, 255);   // Pantone Cool Gray 11 C: griglie, testo secondario
static const ImU32 kWarmCream  = IM_COL32(0xD6, 0xD2, 0xC4, 255);   // Pantone 7527 C: accento panna
static const ImU32 kCloud      = IM_COL32(0xF0, 0xEE, 0xE9, 255);   // Pantone 11-4201 Cloud Dancer: testo, sorgente, curva
static const ImU32 kMocha      = IM_COL32(0xA4, 0x78, 0x64, 255);   // Pantone 17-1230 Mocha Mousse: altoparlanti

static const ImU32 kGrid = IM_COL32(0x53, 0x56, 0x5A, 140);         // Cool Gray 11 semitrasparente

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

// calcolo funzione di spread x disegno
static float spreadGain(float x, float spread, float offset, float curve)
{
    const float p = 101.0f - spread;
    const float b = 1.0f - 1.0f / (std::sqrt(p) + 0.0001f);

    const float narrow = std::pow(x, p);
    const float wide = std::pow(x, b * curve * 0.25f);

    return (narrow + offset * wide) / (offset + 1.0f);
}

// Testo centrato in una colonna che parte da x0 (coordinate della finestra) larga width
static void centeredText(const char* text, float x0, float width)
{
    const float w = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX(x0 + std::max(0.0f, (width - w) * 0.5f));
    ImGui::TextUnformatted(text);
}

class SpatUI : public UI
{
    // Valore di ogni parametro, indicizzato con i nomi di HeavyParams.hpp
    float fParams[HV_DPF_NUM_PARAMETER] = {};
    int fPendingReset = -1;

    // Scala di disegno del frame corrente: scala dello schermo × zoom della finestra
    float fZ = 1.0f;

    // Stile di partenza (colori e misure a zoom 1): a ogni frame lo ricopiamo e lo scaliamo
    ImGuiStyle fBaseStyle;

    ResizeHandle fResizeHandle;

public:
    SpatUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT),
          fResizeHandle(this)
    {
        // Dimensione minima = quella di partenza; il rapporto larghezza/altezza resta fisso
        setGeometryConstraints(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT, true);

        // Se l'host ridimensiona da sé la finestra, la maniglia non serve
        if (isResizable())
            fResizeHandle.hide();

        // Tema: le misure sono già scalate da DPF, qui cambiamo colori e arrotondamenti
        ImGuiStyle& style = ImGui::GetStyle();
        style.FrameRounding = 4.0f * getScaleFactor();
        style.Colors[ImGuiCol_WindowBg]      = ImGui::ColorConvertU32ToFloat4(kBlack6);
        style.Colors[ImGuiCol_Text]          = ImGui::ColorConvertU32ToFloat4(kCloud);
        style.Colors[ImGuiCol_FrameBg]       = ImGui::ColorConvertU32ToFloat4(kGray426);
        style.Colors[ImGuiCol_Button]        = ImGui::ColorConvertU32ToFloat4(kGray426);
        style.Colors[ImGuiCol_ButtonHovered] = ImGui::ColorConvertU32ToFloat4(kCoolGray11);
        style.Colors[ImGuiCol_ButtonActive]  = ImGui::ColorConvertU32ToFloat4(kMocha);

        fBaseStyle = style;
    }

protected:
    // ------------------------------------------------------------------
    // Il plugin (o l'automazione di Reaper) ha cambiato un parametro
    void parameterChanged(uint32_t index, float value) override
    {
        if (index >= HV_DPF_NUM_PARAMETER)
            return;

        fParams[index] = value;
        repaint();
    }

    // ------------------------------------------------------------------
    // Controlli collegati ai parametri

    // Da chiamare subito dopo un controllo continuo (knob o pallino):
    // apre/chiude il gesto di automazione, invia il valore, gestisce il doppio clic
    void handleGesture(uint32_t index, bool changed)
    {
        if (ImGui::IsItemActivated())
            editParameter(index, true);
        if (changed)
            setParameterValue(index, fParams[index]);

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            fPendingReset = static_cast<int>(index);

        if (ImGui::IsItemDeactivated())
        {
            if (fPendingReset == static_cast<int>(index))
            {
                fParams[index] = kParamDefaults[index];
                setParameterValue(index, fParams[index]);
                fPendingReset = -1;
            }
            editParameter(index, false);
        }
    }

    // Imposta un parametro con un clic (pulsanti): gesto aperto e chiuso subito
    void setParamFromClick(uint32_t index, float value)
    {
        fParams[index] = value;
        editParameter(index, true);
        setParameterValue(index, value);
        editParameter(index, false);
    }

    // Knob in una cella larga kCell: etichetta sopra, valore sotto
    void knobParam(const char* label, uint32_t index, float min, float max, const char* format)
    {
        const float cell = kCell * fZ;
        const float size = kKnob * fZ;

        ImGui::BeginGroup();
        const float x0 = ImGui::GetCursorPosX();

        centeredText(label, x0, cell);

        // I knob prendono i colori da ButtonHovered/ButtonActive: qui li mettiamo color panna
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kWarmCream);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kCloud);
        ImGui::SetCursorPosX(x0 + (cell - size) * 0.5f);
        const bool changed = ImGuiKnobs::Knob(label, &fParams[index], min, max, 0.0f, format,
                                              ImGuiKnobVariant_WiperDot, size,
                                              ImGuiKnobFlags_NoTitle | ImGuiKnobFlags_NoInput);
        ImGui::PopStyleColor(2);
        handleGesture(index, changed);   // subito dopo il knob: IsItem* si riferiscono a lui

        char value[32];
        std::snprintf(value, sizeof(value), format, fParams[index]);
        centeredText(value, x0, cell);

        ImGui::EndGroup();
    }

    // Pulsante evidenziato (sfondo panna, testo scuro) oppure normale
    bool styledButton(const char* label, bool highlighted, float width)
    {
        if (highlighted)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, kWarmCream);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kCloud);
            ImGui::PushStyleColor(ImGuiCol_Text, kBlack6);
        }
        const bool clicked = ImGui::Button(label, ImVec2(width, 0));
        if (highlighted)
            ImGui::PopStyleColor(3);
        return clicked;
    }

    // Pulsante che seleziona un valore (es. mode = 2); quello attivo è evidenziato
    void choiceButton(const char* label, uint32_t index, int value, float width)
    {
        const bool selected = static_cast<int>(fParams[index]) == value;

        if (styledButton(label, selected, width) && !selected)
            setParamFromClick(index, static_cast<float>(value));
    }

    // Pulsante on/off (parametri bool): evidenziato quando è acceso
    void toggleButton(const char* label, uint32_t index, float width)
    {
        const bool on = fParams[index] > 0.5f;

        if (styledButton(label, on, width))
            setParamFromClick(index, on ? 0.0f : 1.0f);
    }

    // Pulsante che mostra la voce attuale e passa alla successiva a ogni clic
    void cycleButton(const char* id, uint32_t index, const char* const items[], int count, float width)
    {
        const int current = std::max(0, std::min(count - 1, static_cast<int>(fParams[index])));

        char label[48];
        std::snprintf(label, sizeof(label), "%s###%s", items[current], id);   // "###" = ID fisso, nascosto

        if (styledButton(label, false, width))
            setParamFromClick(index, static_cast<float>((current + 1) % count));
    }

    // Titolo di sezione color panna, con una linea che arriva fino a width
    void sectionHeader(const char* title, float width)
    {
        const float x0 = ImGui::GetCursorPosX();

        ImGui::PushStyleColor(ImGuiCol_Text, kWarmCream);
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();

        const ImVec2 a = ImGui::GetItemRectMin();
        const ImVec2 b = ImGui::GetItemRectMax();
        const float  y = (a.y + b.y) * 0.5f;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(b.x + 6.0f * fZ, y), ImVec2(a.x + width, y), kGrid);

        ImGui::SetCursorPosX(x0);
    }

    // ------------------------------------------------------------------
    // Sezioni dell'interfaccia

    // In alto a sinistra: nome del plugin
    void drawTitle()
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        const ImVec2 o = ImGui::GetCursorScreenPos();

        const float small = ImGui::GetFontSize();
        const float big   = small * 2.0f;

        const ImVec2 ts = font->CalcTextSizeA(big, FLT_MAX, 0.0f, kTitle);
        dl->AddText(font, big, o, kCloud, kTitle);
        dl->AddText(font, small, ImVec2(o.x + ts.x + 10.0f * fZ, o.y + big - small - 2.0f * fZ),
                    kCoolGray11, kSubtitle);

        ImGui::Dummy(ImVec2(ts.x, big));
    }

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

    // ------------------------------------------------------------------
    // Layout della finestra
    //
    //  ┌───────────────────────────┬──────────────────────┐
    //  │ ORBITA  spat · 8 ch        │ mode [circ][rand][man]│
    //  │                            │  controlli modalità   │
    //  │          cerchio           │ CAMPO  rot. compress  │
    //  │                            │ ALTOPARLANTI  1 .. 8  │
    //  ├───────────────────────────┼──────────────────────┤
    //  │      curva di spread       │ SPREAD spr off crv    │
    //  └───────────────────────────┴──────────────────────┘
    void onImGuiDisplay() override
    {
        // Zoom: quanto la finestra è più grande della dimensione di partenza
        const float S = getScaleFactor();
        const float zoom = std::min(getWidth()  / (DISTRHO_UI_DEFAULT_WIDTH  * S),
                                    getHeight() / (DISTRHO_UI_DEFAULT_HEIGHT * S));
        fZ = S * zoom;

        // Stile ricopiato da quello di partenza e scalato: spaziature, bordi, arrotondamenti
        ImGuiStyle& style = ImGui::GetStyle();
        style = fBaseStyle;
        style.ScaleAllSizes(zoom);

        // Una sola finestra ImGui che occupa tutto il plugin, senza barra del titolo
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(getWidth(), getHeight()));

        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                                     | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse
                                     | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        if (ImGui::Begin("spat 8", nullptr, flags))
        {
            ImGui::SetWindowFontScale(zoom);   // anche il testo segue lo zoom

            const float pad    = kPad * fZ;
            const float gap    = kGap * fZ;
            const float leftW  = 2 * kCircleR * fZ + 40 * fZ;   // colonna sinistra: titolo, cerchio, curva
            const float rightW = 4 * kCell * fZ;                // colonna destra: 4 knob per riga
            const float rightX = pad + leftW + gap;

            // 1. Colonna sinistra: titolo e cerchio
            ImGui::SetCursorPos(ImVec2(pad, pad));
            drawTitle();
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
        ImGui::End();
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpatUI)
};

UI *createUI()
{
    return new SpatUI();
}

END_NAMESPACE_DISTRHO
