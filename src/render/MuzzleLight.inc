void StageRenderer::renderMuzzleLight(scene::Vec3 position,scene::Vec4 color,const scene::Mat4& vp,scene::Vec3 camera,scene::Vec3 direction){
    if(!muzzleLight_.enabled||debugView_||muzzleLight_.radius<=0||muzzleLight_.intensity<=0)return;
    static constexpr const char* vertex=R"GLSL(#version 330 core
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0,1);}
)GLSL";
    static constexpr const char* fragment=R"GLSL(#version 330 core
uniform sampler2D uDepth;uniform vec2 uViewport,uRange;uniform bool uForeground;
uniform vec3 uPosition,uCamera,uColor,uDirection;uniform vec4 uLight;uniform float uCone,uAspect;uniform int uShape;
uniform vec3 uRayRight,uRayUp,uRayForward;out vec4 color;
vec3 reconstruct(vec2 uv,float d){
    float n=uRange.x,f=uRange.y;
    if(uForeground&&d<.0400001){n=.1;f=2000.;d/=.04;}
    float z=n*f/max(.00001,f-d*(f-n));
    return uCamera+(uRayForward+uRayRight*(uv.x*2.-1.)+uRayUp*(uv.y*2.-1.))*z;}
void main(){vec2 uv=gl_FragCoord.xy/uViewport;float depth=texture(uDepth,uv).r;if(depth>=1.)discard;
    vec3 p=reconstruct(uv,depth),delta=p-uPosition;float dist=length(delta);
    vec3 axis=normalize(uDirection);
    float along=dot(delta,axis);vec3 lateral=delta-axis*along;
    if(uShape==2)dist=length(vec2(length(lateral),along/max(.001,abs(uAspect))));
    float attenuation=pow(max(0.,1.-dist/max(.001,uLight.x)),max(.001,uLight.z));
    if(uShape==1){float c=cos(radians(clamp(uCone,.1,360.))* .5);attenuation*=smoothstep(c,min(1.,c+.15),dot(normalize(delta),axis));}
    // Reconstruct geometric normals without touching either depth attachment.
    vec3 n=normalize(cross(dFdx(p),dFdy(p)));if(dot(n,uCamera-p)<0.)n=-n;
    float diffuse=max(0.,dot(n,-delta/max(.001,length(delta))));
    float strength=attenuation*diffuse*uLight.y*uLight.w;
    if(strength<.00001)discard;color=vec4(uColor*strength,0);}
)GLSL";
    if(!muzzleLightProgram_){std::string error;muzzleLightProgram_=compileProgram(vertex,fragment,error);if(!muzzleLightProgram_){rainError_=error;return;}}
    if(!muzzleLightFramebuffer_)glapi::GenFramebuffers(1,&muzzleLightFramebuffer_);
    glapi::BindFramebuffer(glapi::Framebuffer,muzzleLightFramebuffer_);
    glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,colorTexture_,0);
    glViewport(0,0,width_,height_);glapi::UseProgram(muzzleLightProgram_);glapi::BindVertexArray(lineVao_);
    const auto l=[&](const char* n){return uniformLocation(muzzleLightProgram_,n);};
    glapi::Uniform2f(l("uViewport"),float(width_),float(height_));
    const scene::Vec3 right{vp.v[0],vp.v[4],vp.v[8]},up{vp.v[1],vp.v[5],vp.v[9]},forward{vp.v[3],vp.v[7],vp.v[11]};
    const auto rr=right/std::max(.00001f,scene::dot(right,right)),ru=up/std::max(.00001f,scene::dot(up,up)),rf=scene::normalize(forward);
    glapi::Uniform3f(l("uRayRight"),rr.x,rr.y,rr.z);glapi::Uniform3f(l("uRayUp"),ru.x,ru.y,ru.z);glapi::Uniform3f(l("uRayForward"),rf.x,rf.y,rf.z);
    glapi::Uniform2f(l("uRange"),cameraNear_,cameraFar_);glapi::Uniform1i(l("uForeground"),foregroundDrawn_&&firstPersonProjection_);
    glapi::Uniform3f(l("uPosition"),position.x,position.y,position.z);glapi::Uniform3f(l("uCamera"),camera.x,camera.y,camera.z);
    if(scene::length(direction)<.0001f)direction=scene::normalize(position-camera+scene::Vec3{0,0,.0001f});
    glapi::Uniform3f(l("uDirection"),direction.x,direction.y,direction.z);
    glapi::Uniform3f(l("uColor"),color.x,color.y,color.z);glapi::Uniform4f(l("uLight"),muzzleLight_.radius,muzzleLight_.intensity,muzzleLight_.falloff,color.w);
    glapi::Uniform1i(l("uShape"),muzzleLight_.shape);glapi::Uniform1f(l("uCone"),muzzleLight_.cone);glapi::Uniform1f(l("uAspect"),muzzleLight_.aspect);
    glapi::ActiveTexture(glapi::Texture0);glBindTexture(GL_TEXTURE_2D,depthBuffer_);glapi::Uniform1i(l("uDepth"),0);
    glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDepthMask(GL_FALSE);glEnable(GL_BLEND);
    glapi::BlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ONE);glapi::DrawArrays(GL_TRIANGLES,0,3);
    glDisable(GL_BLEND);glDepthMask(GL_TRUE);glEnable(GL_DEPTH_TEST);glapi::BindVertexArray(0);
    glapi::BindFramebuffer(glapi::Framebuffer,framebuffer_);
}
