// ─────────────────────────────────────────────────────────────────────────────
//  PluginUIBase.hpp — base comune per le interfacce ImGui dei plugin
//
//  build.sh copia common/ui/ in plugin/source/ di ogni plugin che ha una cartella ui/:
//  modifica i file qui, in common/ui/, non le copie generate.
//
//  Cosa offre alla classe che la eredita:
//   - fParams[]  valore di ogni parametro, indicizzato con i nomi di HeavyParams.hpp
//   - fZ         scala di disegno (schermo × zoom della finestra): moltiplica ogni misura per fZ
//   - finestra ridimensionabile con zoom, tema e palette comuni
//   - controlli già collegati ai parametri: knobParam, toggleButton, choiceButton, cycleButton
//   - doppio clic = valore di default, gesti di automazione corretti per l'host
//   - drawTitle, sectionHeader, drawGenericParameters (un controllo per ogni parametro)
//
//  Nel plugin basta derivare da PluginUIBase e implementare drawContent().
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "DistrhoUI.hpp"
#include "ResizeHandle.hpp"
#include "HeavyParams.hpp"   // generato da build.sh: indici, default e info dei parametri
#include "Palette.hpp"

#include <cmath>
#include <cfloat>
#include <cstdio>
#include <algorithm>
#include <cstdlib>

START_NAMESPACE_DISTRHO

static constexpr float kPi = 3.14159265f;

// Misure comuni, in pixel "logici" alla dimensione di partenza: moltiplicale sempre per fZ
static constexpr float kPad     = 14.0f;   // margine esterno della finestra
static constexpr float kGap     = 16.0f;   // spazio tra i pannelli
static constexpr float kKnob    = 44.0f;   // diametro di un knob
static constexpr float kCell    = 68.0f;   // larghezza della cella di un knob (knob + etichetta)
static constexpr float kButtonW = 100.0f;  // larghezza standard di un pulsante

class PluginUIBase : public UI
{
public:
    PluginUIBase()
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
    // Valore di ogni parametro, indicizzato con i nomi di HeavyParams.hpp
    float fParams[HV_DPF_NUM_PARAMETER] = {};

    // Scala di disegno del frame corrente: scala dello schermo × zoom della finestra
    float fZ = 1.0f;

    // ------------------------------------------------------------------
    // Da implementare nel plugin: disegna il contenuto della finestra.
    // Quando viene chiamata, la finestra ImGui è già aperta, scalata e con il tema applicato.
    virtual void drawContent() = 0;

    // ------------------------------------------------------------------
    // Il plugin (o l'automazione dell'host) ha cambiato un parametro
    void parameterChanged(uint32_t index, float value) override
    {
        if (index >= HV_DPF_NUM_PARAMETER)
            return;

        fParams[index] = value;
        repaint();
    }

    // Prepara zoom, stile e finestra, poi chiama drawContent()
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

        if (ImGui::Begin(DISTRHO_PLUGIN_NAME, nullptr, flags))
        {
            ImGui::SetWindowFontScale(zoom);   // anche il testo segue lo zoom
            drawContent();
        }
        ImGui::End();
    }

    // ------------------------------------------------------------------
    // Collegamento controlli ↔ parametri

    // Da chiamare subito dopo un controllo continuo (knob, pallino trascinabile…):
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

    // ------------------------------------------------------------------
    // Controlli

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

        valueField(index, min, max, format, x0, cell);

        ImGui::EndGroup();
    }

        
    // Valore di un parametro come testo. Un clic lo trasforma in un campo in cui digitare
    // il numero: Invio lo imposta (limitato a min..max), Esc o un clic altrove annulla.
    // x0/width: colonna in cui centrare il testo; width = 0 → nessuna centratura.
    void valueField(uint32_t index, float min, float max, const char* format, float x0, float width)
    {
        // Modalità campo: si sta digitando il valore di questo parametro
        if (fEditing == static_cast<int>(index))
        {
            if (width > 0.0f)
                ImGui::SetCursorPosX(x0);
            ImGui::SetNextItemWidth(width > 0.0f ? width : kCell * fZ);

            if (fEditFocus)
            {
                ImGui::SetKeyboardFocusHere();   // il campo riceve subito la tastiera
                fEditFocus = false;
            }

            ImGui::PushID(static_cast<int>(index));
            const bool enter = ImGui::InputText("##edit", fEditBuf, sizeof(fEditBuf),
                                                ImGuiInputTextFlags_EnterReturnsTrue
                                              | ImGuiInputTextFlags_CharsDecimal
                                              | ImGuiInputTextFlags_AutoSelectAll);
            ImGui::PopID();

            if (enter)
            {
                char* end = nullptr;
                const float v = std::strtof(fEditBuf, &end);
                if (end != fEditBuf)   // è stato scritto almeno un numero valido
                    setParamFromClick(index, std::max(min, std::min(max, v)));
                fEditing = -1;
            }
            else if (ImGui::IsItemDeactivated()
                  || (ImGui::IsMouseClicked(0) && !ImGui::IsItemHovered() && !ImGui::IsItemActive()))
            {
                fEditing = -1;   // Esc o clic altrove: annulla
            }
            return;
        }

        // Modalità normale: il valore come testo, cliccabile
        char value[32];
        std::snprintf(value, sizeof(value), format, fParams[index]);
        if (width > 0.0f)
            centeredText(value, x0, width);
        else
            ImGui::TextUnformatted(value);

        if (ImGui::IsItemClicked(0))
        {
            fEditing   = static_cast<int>(index);
            fEditFocus = true;
            std::snprintf(fEditBuf, sizeof(fEditBuf), "%g", fParams[index]);
            getWindow().focus();   // la finestra del plugin riceve la tastiera (altrimenti resta all'host)
        }
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

    // Menu a tendina per un parametro intero (0..count-1)
    void comboParam(const char* label, uint32_t index, const char* const items[], int count, float width)
    {
        int current = std::max(0, std::min(count - 1, static_cast<int>(fParams[index] + 0.5f)));

        ImGui::SetNextItemWidth(width);
        if (ImGui::Combo(label, &current, items, count))
            setParamFromClick(index, static_cast<float>(current));
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

        char label[64];
        std::snprintf(label, sizeof(label), "%s###%s", items[current], id);   // "###" = ID fisso, nascosto

        if (styledButton(label, false, width))
            setParamFromClick(index, static_cast<float>((current + 1) % count));
    }

    // Pulsante on/off in una cella larga kCell, con l'etichetta sopra (si allinea ai knob)
    void toggleCell(const char* label, uint32_t index)
    {
        const float cell = kCell * fZ;

        ImGui::BeginGroup();
        const float x0 = ImGui::GetCursorPosX();

        centeredText(label, x0, cell);

        ImGui::PushID(label);
        ImGui::SetCursorPosX(x0 + 4.0f * fZ);
        toggleButton(fParams[index] > 0.5f ? "on###toggle" : "off###toggle", index, cell - 8.0f * fZ);
        ImGui::PopID();

        ImGui::EndGroup();
    }

    // ------------------------------------------------------------------
    // Elementi di layout

    // Nome del plugin grande, con un sottotitolo piccolo accanto
    void drawTitle(const char* title, const char* subtitle)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        const ImVec2 o = ImGui::GetCursorScreenPos();

        const float small = ImGui::GetFontSize();
        const float big   = small * 2.0f;

        const ImVec2 ts = font->CalcTextSizeA(big, FLT_MAX, 0.0f, title);
        dl->AddText(font, big, o, kCloud, title);
        dl->AddText(font, small, ImVec2(o.x + ts.x + 10.0f * fZ, o.y + big - small - 2.0f * fZ),
                    kCoolGray11, subtitle);

        ImGui::Dummy(ImVec2(ts.x, big));
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

    // Un controllo per ogni parametro d'ingresso della patch, a righe larghe al massimo width:
    // knob per i numeri, pulsante per i bool. Punto di partenza per un plugin nuovo.
    void drawGenericParameters(float width)
    {
        const float x0 = ImGui::GetCursorPosX();
        const int perRow = std::max(1, static_cast<int>(width / (kCell * fZ)));
        int column = 0;

        for (uint32_t i = 0; i < HV_DPF_NUM_PARAMETER; ++i)
        {
            const ParamInfo& info = kParamInfo[i];
            if (info.flags & kParamOutput)
                continue;   // i parametri di uscita li scrive il DSP, non l'utente

            if (column == 0)
                ImGui::SetCursorPosX(x0);   // inizio riga
            else
                ImGui::SameLine(0, 0);      // stessa riga, celle attaccate

            if (info.flags & kParamBool)
                toggleCell(info.name, i);
            else
                knobParam(info.name, i, info.min, info.max, (info.flags & kParamInt) ? "%.0f" : "%.2f");

            column = (column + 1) % perRow;
        }
    }

        // Livello in funzione della distanza (da 1 a maxDist metri) per la legge 1/d^transparency,
    // con un punto per ogni distanza in points[0..count-1]. Stessa formula dei moduli "trasp".
    void drawDistanceCurve(float w, float h, float transparency, float maxDist,
                           const float* points, int count)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 o = ImGui::GetCursorScreenPos();
        const float rangeDb = 72.0f;   // il grafico va da 0 a -72 dB

        // Da distanza (m) a livello (dB), e da metri/dB a coordinate dello schermo
        auto dbOf = [&](float d)  { return -20.0f * transparency * std::log10(d); };
        auto xOf  = [&](float d)  { return o.x + w * (d - 1.0f) / (maxDist - 1.0f); };
        auto yOf  = [&](float db) { return o.y + h * std::min(1.0f, -db / rangeDb); };

        // Sfondo, cornice, griglia ogni 24 dB
        dl->AddRectFilled(o, ImVec2(o.x + w, o.y + h), kGray426, 4.0f * fZ);
        dl->AddRect(o, ImVec2(o.x + w, o.y + h), kCoolGray11, 4.0f * fZ);
        for (int k = 1; k < 3; ++k)
        {
            const float y = o.y + h * k / 3.0f;
            dl->AddLine(ImVec2(o.x, y), ImVec2(o.x + w, y), kGrid);
        }

        // Curva
        constexpr int kPoints = 100;
        ImVec2 pts[kPoints];
        for (int i = 0; i < kPoints; ++i)
        {
            const float d = 1.0f + (maxDist - 1.0f) * i / (kPoints - 1);
            pts[i] = ImVec2(xOf(d), yOf(dbOf(d)));
        }
        dl->AddPolyline(pts, kPoints, kCloud, 0, 2.0f * fZ);

        // Un punto per ogni distanza richiesta
        for (int i = 0; i < count; ++i)
            dl->AddCircleFilled(ImVec2(xOf(points[i]), yOf(dbOf(points[i]))), 4.0f * fZ, kMocha);

        // Etichette
        char right[16];
        std::snprintf(right, sizeof(right), "%.0f m", maxDist);
        const float  ly = o.y + h + 2.0f * fZ;
        const ImVec2 rs = ImGui::CalcTextSize(right);
        dl->AddText(ImVec2(o.x + 4.0f * fZ, o.y + 2.0f * fZ), kCoolGray11, "0 dB");
        dl->AddText(ImVec2(o.x, ly), kCoolGray11, "1 m");
        dl->AddText(ImVec2(o.x + w - rs.x, ly), kCoolGray11, right);

        // Riserva lo spazio: grafico + riga delle etichette
        ImGui::Dummy(ImVec2(w, h + ImGui::GetTextLineHeightWithSpacing()));
    }

    // Testo centrato in una colonna che parte da x0 (coordinate della finestra) larga width
    static void centeredText(const char* text, float x0, float width)
    {
        const float w = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPosX(x0 + std::max(0.0f, (width - w) * 0.5f));
        ImGui::TextUnformatted(text);
    }

private:
    int fPendingReset = -1;      // parametro da riportare al default al rilascio (-1 = nessuno)
    int  fEditing = -1;          // parametro il cui valore si sta digitando (-1 = nessuno)
    bool fEditFocus = false;     // al primo frame del campo, dargli il fuoco della tastiera
    char fEditBuf[32] = {};      // testo digitato

    ImGuiStyle fBaseStyle;       // stile a zoom 1: ogni frame viene ricopiato e scalato
    ResizeHandle fResizeHandle;  // maniglia di ridimensionamento in basso a destra

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginUIBase)
};

END_NAMESPACE_DISTRHO
