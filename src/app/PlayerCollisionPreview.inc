if(app.playerClipPreview&&app.actorMode&&!app.takePreview&&!app.exportActive){
    // Horizontal offset and ground sample are independent of camera position/pitch.
    auto center=gameplay::collision::playerHeightPreviewCenter(app.actorPosition,app.actorYaw,app.actorPosition.z);
    const float ground=actorGroundHeight(app,center.x,center.y,app.actorPosition.z+scene::course::kStepHeight-45.f);
    if(std::isfinite(ground)&&ground>-1e8f){
        center.z=ground;
        const float height=playerCurrentCollisionHeight(app,app.gameplayStance);
        const float radius=(app.movementAlgorithm==3||app.movementAlgorithm==4)?gameplay::iw::worldUnits(16):scene::course::kPlayerRadius;
        const auto halfExtents=actorCollisionHalfExtents(app,radius);
        const float halfWidth=halfExtents.x,halfDepth=halfExtents.y;
        std::array<scene::Vec3,8> corners;
        for(int i=0;i<8;++i)corners[i]=center+scene::Vec3{(i&1)?halfWidth:-halfWidth,(i&2)?halfDepth:-halfDepth,(i&4)?height:0.f};
        feedbackDraw->PushClipRect(viewportImageMin,viewportImageMax,true);
        for(int i=0;i<8;++i)for(int bit:{1,2,4})if(!(i&bit)){const auto a=projectPoint(corners[i]),b=projectPoint(corners[i|bit]);if(a&&b)feedbackDraw->AddLine(*a,*b,IM_COL32(80,220,255,235),2.f);}
        if(const auto at=projectPoint(center+scene::Vec3{0,0,height})){char label[96]{};std::snprintf(label,sizeof(label),"Clip %.1f x %.1f x %.1f cm",2.f*halfWidth,2.f*halfDepth,height);feedbackDraw->AddText(*at,IM_COL32(80,220,255,255),label);}
        feedbackDraw->PopClipRect();
    }
}
