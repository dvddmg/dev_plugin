// ─────────────────────────────────────────────────────────────────────────────
//  Interfaccia di test-basic
//
//  Parti da qui. La parte comune (knob, pulsanti, zoom, palette, doppio clic)
//  è in common/ui/PluginUIBase.hpp: in questo file va solo ciò che è specifico
//  del plugin. I nomi dei parametri (paramgain, …) arrivano da HeavyParams.hpp,
//  generato da build.sh a partire dai [r nome @hv_param] della patch.
// ─────────────────────────────────────────────────────────────────────────────
#include "PluginUIBase.hpp"

START_NAMESPACE_DISTRHO

class PluginUI : public PluginUIBase
{
protected:
    void drawContent() override
    {
        const float pad   = kPad * fZ;
        const float width = getWidth() - 2.0f * pad;

        ImGui::SetCursorPos(ImVec2(pad, pad));
        drawTitle("test-basic", "test e info");

        ImGui::SetCursorPosX(pad);
        sectionHeader("PARAMETERS", width);

        // Un controllo per ogni parametro della patch, disposti a righe.
        // Quando vuoi un layout tuo, sostituisci questa riga con le tue chiamate, ad esempio:
        //   knobParam("gain", paramgain, 0.0f, 1.0f, "%.2f");
        //   toggleButton("bypass", parambypass, kButtonW * fZ);
        
        ImGui::SetCursorPosX(pad);
        static const char* const modeItems[] = { "0", "1723", "848" };

        ImGui::SetCursorPosX(pad);
        comboParam("mode", parammode, modeItems, 3, 140.0f * fZ);

        ImGui::SetCursorPosX(pad);
        knobParam("gain", paramgain, 0.0f, 1.0f, "%.2f");
    }
};

UI* createUI()
{
    return new PluginUI();
}

END_NAMESPACE_DISTRHO
