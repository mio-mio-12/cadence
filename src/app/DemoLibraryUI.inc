// Included after openRecordedTake. Filesystem work is performed only on
// actions, never as a per-frame recursive scan.
std::optional<std::filesystem::path> choosePackZip(bool save) {
#ifdef _WIN32
  wchar_t path[32768]{};
  OPENFILENAMEW d{};
  d.lStructSize = sizeof(d);
  d.lpstrFilter = L"ZIP packs (*.zip)\0*.zip\0";
  d.lpstrFile = path;
  d.nMaxFile = 32768;
  d.lpstrDefExt = L"zip";
  d.Flags = OFN_PATHMUSTEXIST | (save ? 0 : OFN_FILEMUSTEXIST);
  if (save ? GetSaveFileNameW(&d) : GetOpenFileNameW(&d))
    return std::filesystem::path(path);
#endif
  return {};
}
void refreshDemoLibrary(AppState &app) {
  auto &l = app.demoLibrary;
  const auto home = cadence::portable::executableDirectory();
  l.sets = cadence::content::listSets(home);
  if (std::find(l.sets.begin(), l.sets.end(), l.set) == l.sets.end()) {
    l.set = l.sets.empty() ? std::filesystem::path{} : l.sets.front();
    l.plan.reset();
  }
  l.demos = cadence::content::listDemos(l.set);
  l.demoInfo.clear();
  for (const auto &demo : l.demos)
    try {
      l.demoInfo[demo] = cadence::content::readMetadata(demo);
    } catch (const std::exception &e) {
      app.status = e.what();
    }
  l.installed = cadence::content::packs(home);
  l.initialized = true;
}
template <class F> void demoAction(AppState &app, F &&action) {
  try {
    action();
  } catch (const std::exception &e) {
    app.status = e.what();
  }
}
template <class F> void demoJob(AppState &app, F &&action) {
  auto &l = app.demoLibrary;
  if (l.job)
    return;
  l.progress = std::make_shared<cadence::content::Progress>();
  auto progress = l.progress;
  l.job =
      std::async(std::launch::async,
                 [progress, action = std::forward<F>(action)]() mutable {
                   try {
                     return action(*progress);
                   } catch (const std::exception &e) {
                     return cadence::content::JobResult{e.what(), {}, false};
                   }
                 });
}
void processDemoLibrary(AppState &app) {
  auto &l = app.demoLibrary;
  if(l.pendingDemo.empty())app.pendingPreparedTakeOpen.reset();
  if (l.job &&
      l.job->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    auto r = l.job->get();
    l.job.reset();
    app.status = r.message;
    if (r.success && r.requiresRestart)
      l.restart = true;
    if (r.plan)
      l.plan = std::move(r.plan);
    demoAction(app, [&] { refreshDemoLibrary(app); });
  }
  if (!l.pendingDemo.empty() && !app.pendingMapFuture && !replayOpenBlockedReason(app)) {
    auto demo = std::exchange(l.pendingDemo, {});
    auto prepared=std::move(app.pendingPreparedTakeOpen);
    const bool matchingMap=app.loadedMap && app.mapGpuReady &&
        cadence::content::key(app.loadedMap->sourcePath) ==
            cadence::content::key(l.pendingMap) &&
        app.loadedMap->scaleMultiplier == l.pendingMapScale;
    l.pendingMap.clear();
    if(matchingMap){
      if(prepared&&cadence::content::key(prepared->path)==cadence::content::key(demo))
        publishRecordedTakeOpen(app,*prepared);
      else if(!prepared)openRecordedTake(app,demo); // Legacy/manual queue callers.
      else app.status="Pending demo changed; prepared replay was discarded";
    }else
      app.status = "Map load failed; demo was not opened";
  }
}
void openSetDemo(AppState &app, const std::filesystem::path &path) {
  if (const auto* blocked=replayOpenBlockedReason(app))
    throw std::runtime_error(blocked);
  auto prepared=prepareRecordedTakeOpen(app,path);
  if(!prepared)throw std::runtime_error(app.status);
  app.pendingPreparedTakeOpen.reset();
  app.demoLibrary.pendingDemo.clear();app.demoLibrary.pendingMap.clear();
  const auto& metadata=prepared->metadata;
  cadence::content::ResolutionScope resolution(metadata.pack);
  if (!metadata.map.empty()) {
    const auto map =
        cadence::content::resolve(std::filesystem::u8path(metadata.map));
    if (!std::filesystem::is_regular_file(map))
      throw std::runtime_error("Missing map: " + metadata.map);
    if (!app.loadedMap || !app.mapGpuReady ||
        cadence::content::key(app.loadedMap->sourcePath) !=
            cadence::content::key(map) ||
        app.loadedMap->scaleMultiplier != metadata.mapScale) {
      app.mapImportScale = metadata.mapScale;
      if (loadMapFile(app, map)) {
        app.pendingPreparedTakeOpen=std::move(prepared);
        app.demoLibrary.pendingDemo = path;
        app.demoLibrary.pendingMap = map;
        app.demoLibrary.pendingMapScale = metadata.mapScale;
      }
      return;
    }
  }
  publishRecordedTakeOpen(app,*prepared);
}
void drawDemoProgress(AppState &app) {
  auto &l = app.demoLibrary;
  if (!l.job)
    return;
  auto total = l.progress->total.load(), done = l.progress->done.load();
  ImGui::ProgressBar(total ? float(done) / float(total) : 0, {180, 0});
  ImGui::SameLine();
  if (ImGui::Button("Cancel##pack"))
    l.progress->cancel = true;
}
void drawDemoSets(AppState &app) {
  auto &l = app.demoLibrary;
  if (!l.initialized)
    demoAction(app, [&] { refreshDemoLibrary(app); });
  drawDemoProgress(app);
  ImGui::BeginDisabled(l.job.has_value() || app.takeRecording || app.liveRecorder.active() ||
                       app.exportActive || app.pendingMapFuture.has_value() ||
                       app.pendingClassFuture.has_value());
  bool changedSet = false;
  if (ImGui::BeginCombo("Set", l.set.empty()
                                   ? "Choose set"
                                   : l.set.filename().string().c_str())) {
    for (const auto &set : l.sets)
      if (ImGui::Selectable(set.filename().string().c_str(), set == l.set)) {
        l.set = set;
        changedSet = true;
        l.plan.reset();
      }
    ImGui::EndCombo();
  }
  if (changedSet)
    demoAction(app, [&] { refreshDemoLibrary(app); });
  ImGui::InputText("Set name", l.setName.data(), l.setName.size());
  if (ImGui::Button("New set"))
    demoAction(app, [&] {
      l.set = cadence::content::createSet(
          cadence::portable::executableDirectory(), l.setName.data());
      l.plan.reset();
      refreshDemoLibrary(app);
    });
  ImGui::SameLine();
  if (ImGui::Button("Refresh##sets"))
    demoAction(app, [&] { refreshDemoLibrary(app); });
  ImGui::SameLine();
  if (ImGui::Button("Import ZIP"))
    if (auto path = choosePackZip(false)) {
      const auto home = cadence::portable::executableDirectory();
      demoJob(app, [home, path = *path](auto &p) {
        const auto id = cadence::content::importPack(home, path, p);
        return cadence::content::JobResult{
            "Imported " + id + "; restart to activate assets", {}, true, true};
      });
    }
  ImGui::BeginDisabled(l.set.empty());
  ImGui::InputText("Demo name", l.demoName.data(), l.demoName.size());
  ImGui::BeginDisabled(app.recordedTake.samples.empty());
  if (ImGui::Button("Save demo"))
    demoAction(app, [&] {
      cadence::content::saveDemo(app.recordedTake, l.recording, l.set,
                                 l.demoName.data());
      l.plan.reset();
      refreshDemoLibrary(app);
      app.status = "Saved demo in " + l.set.filename().string();
    });
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button("Import demo"))
    if (auto path = chooseTakeFile(false)) {
      const auto set = l.set;
      demoJob(app, [set, path = *path](auto &p) {
        cadence::content::importDemo(path, set);
        return cadence::content::JobResult{"Imported demo", {}, true};
      });
      l.plan.reset();
    }
  if (ImGui::BeginChild("##set_demos", {0, 180}, true)) {
    for (const auto &demo : l.demos) {
      ImGui::PushID(demo.string().c_str());
      ImGui::TextUnformatted(demo.filename().string().c_str());
      ImGui::SameLine();
      if (ImGui::SmallButton("Open"))
        demoAction(app, [&] { openSetDemo(app, demo); });
      const auto info = l.demoInfo.find(demo);
      ImGui::Text("Map: %s",
                  info == l.demoInfo.end() || info->second.map.empty()
                      ? "Not recorded"
                      : std::filesystem::u8path(info->second.map)
                            .filename()
                            .string()
                            .c_str());
      if (ImGui::SmallButton("Use current map"))
        demoAction(app, [&] {
          if (!app.loadedMap)
            throw std::runtime_error("Load a map first");
          auto m = cadence::content::readMetadata(demo);
          m.map = cadence::content::utf8(app.loadedMap->sourcePath);
          m.mapScale = app.loadedMap->scaleMultiplier;
          for (const auto &mesh : app.loadedMap->scene.meshes)
            for (const auto &p :
                 {mesh.albedoPath, mesh.normalPath, mesh.specularPath,
                  mesh.metalnessPath, mesh.roughnessPath, mesh.emissivePath})
              if (!p.empty())
                m.construction.push_back(cadence::content::utf8(p));
          cadence::content::writeMetadata(demo, m);
          l.demoInfo[demo] = m;
          l.plan.reset();
          app.status = "Assigned map to " + demo.filename().string();
        });
      ImGui::Separator();
      ImGui::PopID();
    }
  }
  ImGui::EndChild();
  if (ImGui::Button("Review assets")) {
    l.plan.reset();
    const auto set = l.set;
    demoJob(app, [set](auto &p) {
      auto plan = cadence::content::planSet(set, p);
      return cadence::content::JobResult{"Asset review complete",
                                         std::move(plan), true};
    });
  }
  if (l.plan) {
    ImGui::Text("%zu files / %.2f GB / %zu missing", l.plan->files.size(),
                double(l.plan->bytes) / (1024 * 1024 * 1024),
                l.plan->missing.size());
    if (!l.plan->missing.empty() && ImGui::TreeNode("Missing assets")) {
      for (const auto &p : l.plan->missing)
        ImGui::TextWrapped("%s", p.c_str());
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Included files")) {
      if (ImGui::BeginChild("##included", {0, 160}, true)) {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(l.plan->files.size()));
        while (clipper.Step())
          for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            ImGui::TextUnformatted(
                l.plan->files[static_cast<size_t>(i)].destination.c_str());
      }
      ImGui::EndChild();
      ImGui::TreePop();
    }
    if (ImGui::Button("Pack set"))
      if (auto path = choosePackZip(true)) {
        auto plan = *l.plan;
        demoJob(app, [plan = std::move(plan), path = *path](auto &p) {
          cadence::content::packSet(plan, path, false, p);
          return cadence::content::JobResult{
              "Packed set: " + path.string(), {}, true};
        });
      }
    ImGui::SameLine();
    ImGui::BeginDisabled(!l.plan->missing.empty());
    if (ImGui::Button("Pack set with assets"))
      if (auto path = choosePackZip(true)) {
        auto plan = *l.plan;
        demoJob(app, [plan = std::move(plan), path = *path](auto &p) {
          cadence::content::packSet(plan, path, true, p);
          return cadence::content::JobResult{
              "Packed set with assets: " + path.string(), {}, true};
        });
      }
    ImGui::EndDisabled();
  }
  ImGui::EndDisabled();
  ImGui::EndDisabled();
}
void drawDemoLibraryWindow(AppState &app) {
  if (!app.demoLibrary.open)
    return;
  ImGui::SetNextWindowSize({580, 640}, ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Demo sets", &app.demoLibrary.open))
    drawDemoSets(app);
  ImGui::End();
}
void drawAssetPacks(AppState &app) {
  if (!ImGui::CollapsingHeader("Packs"))
    return;
  auto &l = app.demoLibrary;
  if (!l.initialized)
    demoAction(app, [&] { refreshDemoLibrary(app); });
  drawDemoProgress(app);
  ImGui::BeginDisabled(l.job.has_value() || app.takeRecording ||
                       app.exportActive);
  if (ImGui::Button("Import ZIP"))
    if (auto path = choosePackZip(false)) {
      const auto home = cadence::portable::executableDirectory();
      demoJob(app, [home, path = *path](auto &p) {
        const auto id = cadence::content::importPack(home, path, p);
        return cadence::content::JobResult{
            "Imported " + id + "; restart to activate", {}, true, true};
      });
    }
  ImGui::SameLine();
  if (ImGui::Button("Refresh##packs"))
    demoAction(app, [&] { refreshDemoLibrary(app); });
  bool packsChanged = false;
  for (auto &pack : l.installed) {
    ImGui::PushID(pack.id.c_str());
    bool enabled = pack.enabled;
    if (ImGui::Checkbox(pack.id.c_str(), &enabled))
      demoAction(app, [&] {
        cadence::content::setEnabled(pack, enabled);
        pack.enabled = enabled;
        l.restart = true;
        packsChanged = true;
      });
    if (!pack.error.empty())
      ImGui::TextWrapped("%s", pack.error.c_str());
    if (pack.conflicts && ImGui::TreeNode("Overlapping assets", "%zu overlapping assets", pack.conflicts)) {
      auto &selected = l.overlapSelection[pack.id];
      if (selected >= pack.overlaps.size()) selected = 0;
      if (ImGui::BeginChild("##overlap_paths", {0, 140}, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(pack.overlaps.size()));
        while (clipper.Step())
          for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
            ImGui::PushID(index);
            if (ImGui::Selectable(pack.overlaps[static_cast<size_t>(index)].path.c_str(), selected == static_cast<size_t>(index)))
              selected = static_cast<size_t>(index);
            ImGui::PopID();
          }
      }
      ImGui::EndChild();
      if (selected < pack.overlaps.size()) {
        const auto &overlap = pack.overlaps[selected];
        ImGui::TextWrapped("%s", overlap.path.c_str());
        ImGui::Text("%s: %s", l.restart ? "Fallback after restart" : "Default fallback", overlap.defaultPack.c_str());
        for (const auto &owner : overlap.packs)
          ImGui::BulletText("%s", owner.c_str());
      }
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Contents")) {
      ImGui::Text("%zu files", pack.files);
#ifdef _WIN32
      if (ImGui::Button("Open folder"))
        ShellExecuteW(nullptr, L"open", pack.directory.c_str(), nullptr,
                      nullptr, SW_SHOWNORMAL);
#endif
      ImGui::BeginDisabled(!cadence::content::packActive(pack.id) ||
                           app.pendingMapFuture.has_value());
      for (const auto &map : pack.maps) {
        ImGui::PushID(map.string().c_str());
        if (ImGui::Selectable(map.filename().string().c_str()))
          loadMapFile(app, map);
        ImGui::PopID();
      }
      ImGui::EndDisabled();
      ImGui::TreePop();
    }
    ImGui::PopID();
  }
  if (packsChanged)
    demoAction(app, [&] { refreshDemoLibrary(app); });
  if (l.restart)
    ImGui::TextUnformatted("Restart to apply pack changes");
  ImGui::EndDisabled();
}
