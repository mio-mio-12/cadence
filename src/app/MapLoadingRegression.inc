int mapLoadingRegression(const std::filesystem::path& path){
    if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(960,540,"Map loading regression",nullptr,nullptr);if(!window)return 2;
    glfwMakeContextCurrent(window);glfwSwapInterval(0);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
    ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 330");
    auto state=std::make_unique<AppState>();auto& app=*state;app.window=window;std::string error;
    if(!app.renderer.initialize(error)){std::cerr<<error;return 3;}
    if(!loadMapFile(app,path))return 4;
#ifdef _WIN32
    std::atomic<bool> probeDone{};std::atomic<unsigned> probes{},timeouts{};
    const auto handle=glfwGetWin32Window(window);
    std::jthread probe([&]{while(!probeDone){
        // Only this hidden test window receives synthetic clicks/messages.
        PostMessageW(handle,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(100,100));
        PostMessageW(handle,WM_LBUTTONUP,0,MAKELPARAM(100,100));
        DWORD_PTR response{};if(!SendMessageTimeoutW(handle,WM_NULL,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,1000,&response))++timeouts;
        ++probes;std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }});
#endif
    const double begin=glfwGetTime();std::size_t frames{};
    while(app.loadingActive&&glfwGetTime()-begin<600){
        glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();
        processPendingMapLoad(app);drawLoadingConsoleModal(app);ImGui::Render();
        glapi::BindFramebuffer(glapi::Framebuffer,0);glViewport(0,0,960,540);glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());glfwSwapBuffers(window);++frames;
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    bool ok=app.mapGpuReady&&!app.loadingActive&&app.rendererError.empty()&&glGetError()==GL_NO_ERROR&&MapLoadingHeartbeat::renderedFrames>1;
#ifdef _WIN32
    probeDone=true;while(probe.joinable()){
        // Drain a last synchronous probe before joining its thread.
        glfwPollEvents();std::this_thread::sleep_for(std::chrono::milliseconds(110));glfwPollEvents();probe.join();
    }
    ok=ok&&probes>0&&timeouts==0;
    std::cout<<"Window message probes "<<probes<<" timeouts "<<timeouts<<'\n';
#endif
    std::cout<<"Map loading UI frames "<<frames<<" upload redraws "<<MapLoadingHeartbeat::renderedFrames<<" result "<<(ok?"PASS":"FAIL")<<" "<<app.status<<'\n';
    app.renderer.shutdown();ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();return ok?0:1;
}
