#pragma once
// Original GLSL sky/cloud implementation, not a port of IW3XO's unlicensed code.
// Values are artistic scattering approximations, not an atmospheric simulation.
#define CADENCE_DAY_NIGHT_SKY_GLSL R"GLSL(
uniform bool uDayNightSky;
uniform vec3 uDnSun,uDnSunset;
uniform vec4 uDnAtmosphere; // Rayleigh, Mie, asymmetry, intensity.
uniform vec4 uDnCloud; // Coverage, thickness, absorption, scale.
uniform vec4 uDnMotion; // Wind x/y offset, exposure, star intensity.
uniform int uDnSteps,uDnLightSteps;
uniform float uDnDisk;
uniform vec4 uDnStarTrail; // Arc radians, brightness, fade, sidereal angle.
uniform vec3 uDnStarPole;
uniform bool uNightPhoto;
uniform vec4 uNightLook; // Exposure EV, shadow floor, saturation, cloud light.
uniform vec3 uNightZenith;
uniform vec4 uNightHorizon; // RGB and amount.
uniform vec4 uNightStars; // Density, size, color variation, halo.
uniform vec4 uNightGalaxyNoise; // Algorithm, feature scale, detail, contrast.
uniform vec4 uNightGalaxy; // Amount, width radians, tilt, azimuth.
uniform sampler2D uNightPanorama;
uniform bool uNightPanoramaReady;
uniform vec3 uNightMoonDir;
uniform vec4 uNightMoon; // Intensity, angular radius, illuminated fraction, halo.
float dnHash(vec3 p){p=fract(p*.1031);p+=dot(p,p.yzx+33.33);return fract((p.x+p.y)*p.z);}
float dnNoise(vec3 p){vec3 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);return mix(mix(mix(dnHash(i),dnHash(i+vec3(1,0,0)),f.x),mix(dnHash(i+vec3(0,1,0)),dnHash(i+vec3(1,1,0)),f.x),f.y),mix(mix(dnHash(i+vec3(0,0,1)),dnHash(i+vec3(1,0,1)),f.x),mix(dnHash(i+vec3(0,1,1)),dnHash(i+vec3(1,1,1)),f.x),f.y),f.z);}
float dnFbm(vec3 p){float n=0.0,a=.57;for(int i=0;i<4;++i){n+=a*dnNoise(p);p=p*2.03+vec3(3.7,9.2,5.1);a*=.5;}return n;}
float dnDensity(vec3 p){float layer=smoothstep(1.0,1.12,p.z)*(1.0-smoothstep(1.0+uDnCloud.y*.7,1.0+uDnCloud.y,p.z));vec3 q=vec3(p.xy*uDnCloud.w+uDnMotion.xy,(p.z-1.0)*2.0);return smoothstep(1.0-uDnCloud.x,1.2-uDnCloud.x,dnFbm(q))*layer;}
// Analytic arcs instead of sparse temporal taps: no dashed trails or frame history.
vec3 dnStars(vec3 d){
    vec3 pole=normalize(uDnStarPole),axis=normalize(cross(abs(pole.z)>.99?vec3(0,1,0):vec3(0,0,1),pole)),other=cross(pole,axis);
    float latitude=acos(clamp(dot(d,pole),-1.0,1.0)),longitude=atan(dot(d,other),dot(d,axis))-uDnStarTrail.w;
    float row=floor(latitude/3.14159265*600.0),aa=max(fwidth(latitude)*.75,.00015);vec3 sum=vec3(0);
    for(int y=-1;y<=1;++y){float ring=row+float(y);if(ring<0.0||ring>=600.0)continue;
        for(int x=0;x<4;++x){vec3 seed=vec3(ring,float(x),17);float lat=(ring+dnHash(seed))/600.0*3.14159265,lon=dnHash(seed+13.0)*6.2831853;
            float delta=mod(lon-longitude+6.2831853,6.2831853),radial=abs(lat-latitude),size=mix(.0007,.0017,dnHash(seed+31.0));
            if(uNightPhoto){if(dnHash(seed+83.0)>uNightStars.x)continue;size*=uNightStars.y;}
            float angular=min(delta,6.2831853-delta)*sin(lat);float point=(1.0-smoothstep(max(0.0,size-aa),size+aa,sqrt(radial*radial+angular*angular)))*min(1.0,size/aa);
            float tail=0.0;if(uDnStarTrail.x>.0001&&delta<uDnStarTrail.x)tail=(1.0-smoothstep(max(0.0,size-aa),size+aa,radial))*min(1.0,size/aa)*pow(max(.00001,1.0-delta/uDnStarTrail.x),uDnStarTrail.z)*uDnStarTrail.y;
            float strength=mix(.3,1.0,dnHash(seed+47.0));vec3 tint=vec3(1);float glow=0.0;
            if(uNightPhoto){float magnitude=dnHash(seed+47.0);strength=mix(.08,1.4,pow(magnitude,3.0));tint=mix(vec3(1),mix(vec3(.65,.8,1),vec3(1,.77,.53),dnHash(seed+63.0)),uNightStars.z);float separation=sqrt(radial*radial+angular*angular);glow=exp(-separation/max(.0001,size*3.0))*uNightStars.w*.2;}
            sum+=tint*(max(point,tail)+glow)*strength;
        }
    }return sum;
}
vec3 dnPhotographicNight(vec3 d){
    float h=max(d.z,0.0);vec3 sky=uNightZenith+uNightHorizon.rgb*uNightHorizon.a*pow(1.0-h,4.0);
    if(uNightGalaxy.x>0.0){
        vec3 axis=normalize(uDnStarPole);float c=cos(uDnStarTrail.w),s=sin(uDnStarTrail.w);vec3 q=d*c+cross(axis,d)*s+axis*dot(axis,d)*(1.0-c);
        vec3 normal=vec3(cos(uNightGalaxy.w)*cos(uNightGalaxy.z),sin(uNightGalaxy.w)*cos(uNightGalaxy.z),sin(uNightGalaxy.z));
        if(int(uNightGalaxyNoise.x+.5)==4){
            // ESO/S. Brunier eso0932a: galactic plane on the image equator.
            // Decode sRGB through the texture format; composite in linear space.
            vec3 east=vec3(-sin(uNightGalaxy.w),cos(uNightGalaxy.w),0);
            vec3 center=normalize(cross(normal,east));
            float longitude=atan(dot(q,east),dot(q,center));
            float latitude=asin(clamp(dot(q,normal),-1.0,1.0));
            vec2 uv=vec2(longitude/6.2831853+.5,.5-latitude/3.14159265);
            if(uNightPanoramaReady){vec3 photo=texture(uNightPanorama,uv).rgb;
                photo=pow(max(photo,vec3(0)),vec3(uNightGalaxyNoise.w));
                sky+=photo*uNightGalaxy.x;
            }
        }else{
        float band=exp(-pow(dot(q,normal)/max(.02,uNightGalaxy.y),2.0));
        vec3 noisePosition=q/max(.2,uNightGalaxyNoise.y);int algorithm=int(uNightGalaxyNoise.x+.5);
        if(algorithm==2){float warpA=dnFbm(noisePosition*8.0),warpB=dnFbm(noisePosition*8.0+31.0);noisePosition+=vec3(warpA-.5,warpB-.5,warpA-warpB)*.18;}
        float clouds=dnFbm(noisePosition*19.0),dust=dnFbm(noisePosition*(43.0*uNightGalaxyNoise.z)+7.0);
        if(algorithm==1){clouds=pow(1.0-abs(clouds*2.0-1.0),2.0);dust=1.0-abs(dust*2.0-1.0);}
        if(algorithm==3){float strands=abs(sin(dot(noisePosition,vec3(19,31,11))+clouds*12.0));clouds=mix(clouds,pow(1.0-strands,2.0),.65);dust=mix(dust,pow(strands,.35),.5);}
        clouds=clamp((clouds-.5)*uNightGalaxyNoise.w+.5,0.0,1.0);dust=clamp((dust-.5)*uNightGalaxyNoise.w+.5,0.0,1.0);
        sky+=mix(vec3(.055,.062,.085),vec3(.11,.095,.072),clouds)*band*uNightGalaxy.x*(.4+clouds)*smoothstep(.18,.7,dust);
        }
    }
    if(uNightMoon.x>0.0&&uNightMoonDir.z>-.03){
        float angular=length(d-uNightMoonDir),radius=uNightMoon.y,aa=max(fwidth(angular),.0001);
        float disc=1.0-smoothstep(radius-aa,radius+aa,angular);
        vec3 side=normalize(cross(abs(uNightMoonDir.z)>.99?vec3(0,1,0):vec3(0,0,1),uNightMoonDir)),up=cross(uNightMoonDir,side);
        vec2 uv=vec2(dot(d,side),dot(d,up))/max(radius,.0001);float z=sqrt(max(0.0,1.0-dot(uv,uv))),phase=2.0*uNightMoon.z-1.0;
        float lit=dot(vec3(uv,z),vec3(sqrt(max(0.0,1.0-phase*phase)),0,phase));
        float textureValue=disc>0.0?mix(.6,1.0,dnFbm(vec3(uv*8.0,1.0))):0.0;
        sky+=vec3(.92,.95,1)*disc*smoothstep(-.03,.1,lit)*textureValue*uNightMoon.x*2.0;
        sky+=vec3(.06,.08,.12)*exp(-angular*16.0)*uNightMoon.w*uNightMoon.x*uNightMoon.z;
    }
    return sky;
}
vec3 dayNightSky(vec3 d){
    float daylight=smoothstep(-.15,.15,uDnSun.z),h=max(0.0,d.z),mu=dot(d,uDnSun);
    float air=1.0/(.12+h),g=uDnAtmosphere.z;
    vec3 beta=vec3(.0752,.13,.224)*uDnAtmosphere.x;
    vec3 extinction=exp(-beta*air);
    float phase=(1.0-g*g)/pow(max(.002,1.0+g*g-2.0*g*mu),1.5);
    float dusk=exp(-abs(uDnSun.z)*8.0);
    vec3 day=mix(vec3(.52,.69,.93),vec3(.07,.24,.52),pow(h,.35));
    day=mix(day,uDnSunset,dusk*pow(1.0-h,3.0)*(.35+.65*max(mu,0.0)));
    day*=mix(vec3(1),extinction,.3);
    day+=uDnSunset*phase*.018*uDnAtmosphere.y*(1.0-extinction);
    vec3 night=uNightPhoto&&daylight<.999?dnPhotographicNight(d):vec3(.003,.006,.016)+vec3(.006,.009,.018)*(1.0-h);
    vec3 rgb=mix(night,day,daylight);
    float disk=smoothstep(cos(.006),cos(.0045),mu);
    rgb+=vec3(1,.87,.65)*disk*uDnDisk*8.0*smoothstep(-.025,.015,uDnSun.z);
    float starVisibility=1.0-smoothstep(-.2,-.03,uDnSun.z);vec3 stars=starVisibility>.001?dnStars(d):vec3(0);
    rgb+=vec3(stars*uDnMotion.w*starVisibility*smoothstep(0.0,.2,h)*1.5);
    // A bounded sky-space slab: no map geometry, texture scans or per-frame allocation.
    if(d.z>.12&&uDnCloud.x>.001){float entry=1.0/d.z,exitDistance=(1.0+uDnCloud.y)/d.z;float stepLength=(exitDistance-entry)/float(uDnSteps);float transmittance=1.0;vec3 cloud=vec3(0);vec3 lightDir=uDnSun.z>=0.0?uDnSun:-uDnSun;
        for(int i=0;i<32;++i){if(i>=uDnSteps||transmittance<.015)break;vec3 p=d*(entry+(float(i)+.5)*stepLength);float density=dnDensity(p);if(density<.001)continue;float shadow=0.0;for(int j=0;j<4;++j){if(j>=uDnLightSteps)break;shadow+=dnDensity(p+lightDir*(float(j)+1.0)*.12)*.12;}
            float a=1.0-exp(-density*stepLength*uDnCloud.z*6.0);vec3 lit=mix(vec3(.035,.045,.075),mix(vec3(.85,.9,1),uDnSunset,dusk*.6),daylight);if(uNightPhoto)lit*=mix(uNightLook.w,1.0,daylight);lit*=.3+.7*exp(-shadow*uDnCloud.z*8.0);cloud+=transmittance*a*lit*uDnMotion.z;transmittance*=1.0-a;
        }rgb=mix(rgb,cloud+rgb*transmittance,smoothstep(.12,.35,d.z));
    }
    if(uNightPhoto){float nightWeight=1.0-smoothstep(-.18,.04,uDnSun.z);vec3 exposed=rgb*exp2(uNightLook.x);float l=dot(exposed,vec3(.2126,.7152,.0722));exposed=mix(vec3(l),exposed,uNightLook.z)+vec3(uNightLook.y);rgb=mix(rgb,exposed,nightWeight);}
    return max(rgb*uDnAtmosphere.w,vec3(0));
}
)GLSL"
