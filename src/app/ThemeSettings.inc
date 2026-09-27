// Kept separate from gameplay and visual presets.
namespace themes {
struct Palette {
  const char *name;
  ImVec4 background, panel, text, accent;
};
constexpr Palette presets[] = {{"Default", {}, {}, {}, {}},
                               {"White / blue",
                                {.95f, .96f, .98f, 1},
                                {.84f, .88f, .94f, 1},
                                {.08f, .11f, .17f, 1},
                                {.08f, .35f, .72f, 1}},
                               {"Cream / brown",
                                {.96f, .92f, .83f, 1},
                                {.85f, .78f, .65f, 1},
                                {.20f, .13f, .08f, 1},
                                {.50f, .24f, .08f, 1}},
                               {"Black / white",
                                {.015f, .015f, .015f, 1},
                                {.13f, .13f, .13f, 1},
                                {1, 1, 1, 1},
                                {.78f, .78f, .78f, 1}},
                               {"Grey / orange",
                                {.15f, .15f, .15f, 1},
                                {.25f, .25f, .25f, 1},
                                {.96f, .96f, .94f, 1},
                                {1, .48f, .12f, 1}},
                               {"Navy / cyan",
                                {.035f, .065f, .12f, 1},
                                {.09f, .16f, .25f, 1},
                                {.85f, .94f, 1, 1},
                                {.05f, .78f, .92f, 1}},
                               {"Green / lime",
                                {.04f, .095f, .065f, 1},
                                {.11f, .21f, .14f, 1},
                                {.91f, .96f, .82f, 1},
                                {.66f, .86f, .22f, 1}},
                               {"Purple / pink",
                                {.095f, .04f, .14f, 1},
                                {.20f, .10f, .28f, 1},
                                {.97f, .89f, 1, 1},
                                {.94f, .43f, .72f, 1}},
                               {"Brown / gold",
                                {.10f, .065f, .04f, 1},
                                {.22f, .16f, .10f, 1},
                                {1, .93f, .80f, 1},
                                {.94f, .69f, .24f, 1}},
                               {"Pale blue / teal",
                                {.85f, .94f, .96f, 1},
                                {.70f, .84f, .87f, 1},
                                {.04f, .18f, .21f, 1},
                                {.02f, .43f, .45f, 1}}};
ImVec4 mix(ImVec4 a, ImVec4 b, float t) {
  return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
          a.w + (b.w - a.w) * t};
}
void colors(ImVec4 background, ImVec4 panel, ImVec4 text, ImVec4 accent) {
  auto &c = ImGui::GetStyle().Colors;
  // Start from a neutral palette so no unrelated default-blue states survive.
  for (int i = 0; i < ImGuiCol_COUNT; ++i)
    c[i] = panel;
  c[ImGuiCol_Text] = text;
  c[ImGuiCol_TextDisabled] = mix(text, background, .48f);
  c[ImGuiCol_WindowBg] = c[ImGuiCol_ChildBg] = background;
  c[ImGuiCol_PopupBg] = mix(background, panel, .35f);
  c[ImGuiCol_Border] = mix(panel, text, .22f);
  c[ImGuiCol_BorderShadow] = {0, 0, 0, 0};
  for (int i : {ImGuiCol_FrameBg, ImGuiCol_TitleBg, ImGuiCol_TitleBgCollapsed,
                ImGuiCol_MenuBarBg, ImGuiCol_ScrollbarBg, ImGuiCol_Button,
                ImGuiCol_Header, ImGuiCol_Tab})
    c[i] = panel;
  for (int i : {ImGuiCol_FrameBgHovered, ImGuiCol_ButtonHovered,
                ImGuiCol_HeaderHovered, ImGuiCol_TabHovered,
                ImGuiCol_ResizeGripHovered, ImGuiCol_ScrollbarGrabHovered})
    c[i] = mix(panel, accent, .38f);
  for (int i : {ImGuiCol_FrameBgActive, ImGuiCol_ButtonActive,
                ImGuiCol_HeaderActive, ImGuiCol_TitleBgActive,
                ImGuiCol_TabSelected, ImGuiCol_ResizeGripActive})
    c[i] = mix(panel, accent, .55f);
  for (int i :
       {ImGuiCol_CheckMark, ImGuiCol_SliderGrab, ImGuiCol_SliderGrabActive,
        ImGuiCol_SeparatorHovered, ImGuiCol_SeparatorActive,
        ImGuiCol_ScrollbarGrabActive, ImGuiCol_PlotLines,
        ImGuiCol_PlotHistogram, ImGuiCol_NavHighlight, ImGuiCol_DragDropTarget})
    c[i] = accent;
  c[ImGuiCol_ScrollbarGrab] = mix(panel, text, .35f);
  c[ImGuiCol_Separator] = c[ImGuiCol_Border];
  c[ImGuiCol_ResizeGrip] = mix(panel, accent, .2f);
  c[ImGuiCol_TextSelectedBg] = {accent.x, accent.y, accent.z, .35f};
  c[ImGuiCol_TableRowBg] = {0, 0, 0, 0};
  c[ImGuiCol_TableRowBgAlt] = {text.x, text.y, text.z, .035f};
  c[ImGuiCol_ModalWindowDimBg] = {0, 0, 0, .55f};
  c[ImGuiCol_NavWindowingDimBg] = {0, 0, 0, .3f};
}
} // namespace themes
void applyCadenceTheme(int theme) {
  if (theme <= 0 || theme >= int(std::size(themes::presets))) {
    ImGui::StyleColorsDark();
    return;
  }
  const auto &p = themes::presets[theme];
  themes::colors(p.background, p.panel, p.text, p.accent);
}
bool saveCadenceTheme(AppState &app, const std::filesystem::path &path) {
  return uiappearance::save(path, app.colorScheme, app.fontIndex,
                            app.interfaceScale);
}
bool loadCadenceTheme(AppState &app, const std::filesystem::path &path) {
  std::ifstream in(path);
  std::string header;
  int version{};
  if (!(in >> header >> version))
    return false;
  if (header == "CADENCE_APPEARANCE") {
    if (!uiappearance::load(path, app.colorScheme, app.fontIndex,
                            app.interfaceScale))
      return false;
    if (!app.interfaceFonts.empty()) {
      app.fontIndex =
          std::clamp(app.fontIndex, 0, int(app.interfaceFonts.size()) - 1);
      ImGui::GetIO().FontDefault = app.interfaceFonts[app.fontIndex].second;
    }
    return true;
  }
  if (header != "CADENCETHEME" || version != 1)
    return false;
  auto candidate = ImGui::GetStyle();
  for (auto &c : candidate.Colors) {
    if (!(in >> c.x >> c.y >> c.z >> c.w))
      return false;
    for (float v : {c.x, c.y, c.z, c.w})
      if (!std::isfinite(v) || v < 0 || v > 1)
        return false;
  }
  ImGui::GetStyle() = candidate;
  return true;
}
std::vector<std::filesystem::path> cadenceThemes() {
  std::vector<std::filesystem::path> out;
  std::error_code ec;
  for (const auto &item :
       std::filesystem::directory_iterator(colorSchemeDirectory(), ec))
    if (item.is_regular_file(ec) && item.path().extension() == ".cadencetheme")
      out.push_back(item.path());
  std::sort(out.begin(), out.end());
  return out;
}
void drawThemeSettings(AppState &app) {
  if (!ImGui::CollapsingHeader("Theme settings"))
    return;
  if (ImGui::BeginCombo("Preset", app.colorScheme >= 0 &&
                                          app.colorScheme <
                                              int(std::size(themes::presets))
                                      ? themes::presets[app.colorScheme].name
                                      : "Custom")) {
    for (int i = 0; i < int(std::size(themes::presets)); ++i)
      if (ImGui::Selectable(themes::presets[i].name, app.colorScheme == i)) {
        app.colorScheme = i;
        applyCadenceTheme(i);
      }
    ImGui::EndCombo();
  }
  static std::vector<std::filesystem::path> saved = cadenceThemes();
  static std::filesystem::path selected;
  if (ImGui::BeginCombo("Saved theme",
                        selected.empty() ? "Choose"
                                         : selected.stem().string().c_str())) {
    for (const auto &path : saved)
      if (ImGui::Selectable(path.stem().string().c_str(), path == selected)) {
        if (loadCadenceTheme(app, path)) {
          selected = path;
          const auto n = path.stem().string();
          std::snprintf(app.colorSchemeName.data(), app.colorSchemeName.size(),
                        "%s", n.c_str());
        } else
          app.status = "Could not load theme";
      }
    ImGui::EndCombo();
  }
  auto &style = ImGui::GetStyle();
  auto background = style.Colors[ImGuiCol_WindowBg],
       panel = style.Colors[ImGuiCol_FrameBg],
       text = style.Colors[ImGuiCol_Text],
       accent = style.Colors[ImGuiCol_CheckMark];
  bool changed = ImGui::ColorEdit3("Background", &background.x);
  changed |= ImGui::ColorEdit3("Controls", &panel.x);
  changed |= ImGui::ColorEdit3("Text", &text.x);
  changed |= ImGui::ColorEdit3("Accent", &accent.x);
  if (changed)
    themes::colors(background, panel, text, accent);
  if (!app.interfaceFonts.empty() &&
      ImGui::BeginCombo(
          "Font",
          app.interfaceFonts[std::clamp(app.fontIndex, 0,
                                        int(app.interfaceFonts.size()) - 1)]
              .first.c_str())) {
    for (int i = 0; i < int(app.interfaceFonts.size()); ++i)
      if (ImGui::Selectable(app.interfaceFonts[i].first.c_str(),
                            app.fontIndex == i)) {
        app.fontIndex = i;
        ImGui::GetIO().FontDefault = app.interfaceFonts[i].second;
      }
    ImGui::EndCombo();
  }
  if (ImGui::SliderFloat("Text size", &app.interfaceScale, .75f, 1.75f,
                         "%.2fx"))
    ImGui::GetIO().FontGlobalScale = app.interfaceScale;
  ImGui::SliderFloat("Rounded corners", &style.FrameRounding, 0, 12, "%.1f px");
  if (ImGui::TreeNode("Individual colors")) {
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
      std::string label = ImGui::GetStyleColorName(i);
      for (size_t n = 1; n < label.size(); ++n)
        if (std::isupper(static_cast<unsigned char>(label[n])) &&
            !std::isspace(static_cast<unsigned char>(label[n - 1]))) {
          label.insert(n, " ");
          ++n;
        }
      ImGui::PushID(i);
      ImGui::ColorEdit4(label.c_str(), &style.Colors[i].x);
      ImGui::PopID();
    }
    ImGui::TreePop();
  }
  ImGui::InputText("Theme name", app.colorSchemeName.data(),
                   app.colorSchemeName.size());
  static std::filesystem::path pendingSave;
  const auto save = [&](const auto &path) {
    app.status =
        saveCadenceTheme(app, path) ? "Saved theme" : "Could not save theme";
    selected = path;
    saved = cadenceThemes();
  };
  if (ImGui::Button("Save theme")) {
    const std::string name = app.colorSchemeName.data();
    if (!cadence::content::validName(name))
      app.status = "Enter a valid theme name";
    else {
      pendingSave = colorSchemeDirectory() / (name + ".cadencetheme");
      if (std::filesystem::exists(pendingSave))
        ImGui::OpenPopup("Replace saved theme?");
      else
        save(pendingSave);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Refresh##themes"))
    saved = cadenceThemes();
  ImGui::SameLine();
  if (ImGui::Button("Reset colors"))
    applyCadenceTheme(app.colorScheme);
  if (ImGui::BeginPopupModal("Replace saved theme?", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted(pendingSave.stem().string().c_str());
    if (ImGui::Button("Replace")) {
      save(pendingSave);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
}
