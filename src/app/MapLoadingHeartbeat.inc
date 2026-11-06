// Called only before the ordinary UI is built, while a map is being finalized.
// No scene rendering, simulation, asset commands or nested load is permitted.
struct MapLoadingHeartbeat {
    inline static std::size_t renderedFrames{};
    AppState& app;double last{};
    explicit MapLoadingHeartbeat(AppState& value):app(value){
        app.renderer.setUploadHeartbeat([this]{pulse();});pulse();
    }
    ~MapLoadingHeartbeat(){app.renderer.setUploadHeartbeat({});}
    void pulse(){
        const double now=glfwGetTime();if(now-last<1./30.)return;last=now;
        glfwPollEvents();
        if(!app.window||!ImGui::GetCurrentContext()||!ImGui::GetCurrentContext()->WithinFrameScope||!ImGui::GetIO().BackendRendererUserData)return;
        int w{},h{};glfwGetFramebufferSize(app.window,&w,&h);if(w<=0||h<=0)return;
        GLint drawFbo{},readFbo{},viewport[4]{};GLfloat clear[4]{};
        glGetIntegerv(0x8CA6,&drawFbo);glGetIntegerv(0x8CAA,&readFbo);
        glGetIntegerv(GL_VIEWPORT,viewport);glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);
        const bool scissor=glIsEnabled(GL_SCISSOR_TEST)!=0;
        // End the empty outer frame; rebuild only the progress window. The
        // ordinary UI gets a fresh frame after the upload callback returns.
        ImGui::EndFrame();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();
        drawLoadingConsoleModal(app);ImGui::Render();
        glapi::BindFramebuffer(glapi::Framebuffer,0);glViewport(0,0,w,h);
        glDisable(GL_SCISSOR_TEST);glClearColor(.055f,.065f,.08f,1);glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());glfwSwapBuffers(app.window);
        ++renderedFrames;
        glapi::BindFramebuffer(glapi::DrawFramebuffer,drawFbo);glapi::BindFramebuffer(glapi::ReadFramebuffer,readFbo);
        glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glClearColor(clear[0],clear[1],clear[2],clear[3]);
        if(scissor)glEnable(GL_SCISSOR_TEST);else glDisable(GL_SCISSOR_TEST);
        ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::GetIO().DeltaTime=.000001f;ImGui::NewFrame();
    }
};
