#pragma once
// Unmodified function bodies from Valve Source SDK 2013 gamemovement.cpp,
// b8cfb12c0e083a2ef5b2f9f9b50f3902fa034474; test-only engine interface shim.
// Copyright (c) Valve Corporation. All rights reserved.
// See tools/licenses/source-sdk/LICENSE and thirdpartylegalnotices.txt.
#include <cmath>
namespace source_sdk_oracle {
struct Vector {
    float v[3]{};
    float& operator[](int i){return v[i];}
    float operator[](int i)const{return v[i];}
    float Dot(const Vector& b)const{return v[0]*b[0]+v[1]*b[1]+v[2]*b[2];}
    float Length()const{return std::sqrt(Dot(*this));}
    Vector operator*(float f)const{return {{v[0]*f,v[1]*f,v[2]*f}};}
    Vector& operator*=(float f){for(float& x:v)x*=f;return *this;}
    Vector& operator-=(const Vector& b){for(int i=0;i<3;++i)v[i]-=b[i];return *this;}
};
inline float DotProduct(const Vector& a,const Vector& b){return a.Dot(b);}
struct Player {struct {bool deadflag{};} pl;float m_flWaterJumpTime{},m_surfaceFriction{1};};
struct Move {Vector m_vecVelocity{},m_outWishVel{};};
struct Globals {float frametime{.015f};};
inline Globals globals;
inline Globals* gpGlobals=&globals;
struct CGameMovement {
    Player* player;Move* mv;
    bool CanAccelerate(){return !player->pl.deadflag&&!player->m_flWaterJumpTime;}
    float GetAirSpeedCap(){return 30.f;}
    void AirAccelerate(Vector&,float,float);
    void Accelerate(Vector&,float,float);
    int ClipVelocity(Vector&,Vector&,Vector&,float,float=0);
};
void CGameMovement::AirAccelerate( Vector& wishdir, float wishspeed, float accel )
{
	int i;
	float addspeed, accelspeed, currentspeed;
	float wishspd;

	wishspd = wishspeed;
	
	if (player->pl.deadflag)
		return;
	
	if (player->m_flWaterJumpTime)
		return;

	// Cap speed
	if ( wishspd > GetAirSpeedCap() )
		wishspd = GetAirSpeedCap();

	// Determine veer amount
	currentspeed = mv->m_vecVelocity.Dot(wishdir);

	// See how much to add
	addspeed = wishspd - currentspeed;

	// If not adding any, done.
	if (addspeed <= 0)
		return;

	// Determine acceleration speed after acceleration
	accelspeed = accel * wishspeed * gpGlobals->frametime * player->m_surfaceFriction;

	// Cap it
	if (accelspeed > addspeed)
		accelspeed = addspeed;
	
	// Adjust pmove vel.
	for (i=0 ; i<3 ; i++)
	{
		mv->m_vecVelocity[i] += accelspeed * wishdir[i];
		mv->m_outWishVel[i] += accelspeed * wishdir[i];
	}
}
void CGameMovement::Accelerate( Vector& wishdir, float wishspeed, float accel )
{
	int i;
	float addspeed, accelspeed, currentspeed;

	// This gets overridden because some games (CSPort) want to allow dead (observer) players
	// to be able to move around.
	if ( !CanAccelerate() )
		return;

	// See if we are changing direction a bit
	currentspeed = mv->m_vecVelocity.Dot(wishdir);

	// Reduce wishspeed by the amount of veer.
	addspeed = wishspeed - currentspeed;

	// If not going to add any speed, done.
	if (addspeed <= 0)
		return;

	// Determine amount of accleration.
	accelspeed = accel * gpGlobals->frametime * wishspeed * player->m_surfaceFriction;

	// Cap at addspeed
	if (accelspeed > addspeed)
		accelspeed = addspeed;
	
	// Adjust velocity.
	for (i=0 ; i<3 ; i++)
	{
		mv->m_vecVelocity[i] += accelspeed * wishdir[i];	
	}
}
int CGameMovement::ClipVelocity( Vector& in, Vector& normal, Vector& out, float overbounce, float flRedirectCoeff /* = 0.f */ )
{
	float	backoff;
	float	change;
	float angle;
	int		i, blocked;
	
	angle = normal[ 2 ];

	blocked = 0x00;         // Assume unblocked.
	if (angle > 0)			// If the plane that is blocking us has a positive z component, then assume it's a floor.
		blocked |= 0x01;	// 
	if (!angle)				// If the plane has no Z, it is vertical (wall/step)
		blocked |= 0x02;	// 
	

	// Determine how far along plane to slide based on incoming direction.
	float flBlocked = DotProduct (in, normal);
	backoff = flBlocked * overbounce;

	for (i=0 ; i<3 ; i++)
	{
		change = normal[i]*backoff;
		out[i] = in[i] - change; 
	}
	
	// iterate once to make sure we aren't still moving through the plane
	float adjust = DotProduct( out, normal );
	if( adjust < 0.0f )
	{
		out -= ( normal * adjust );
//		Msg( "Adjustment = %lf\n", adjust );
	}

	if ( flRedirectCoeff > 0.f )
	{
		// Redirect clipped velocity along angle of movement
		float flLen = out.Length();
		out *= ( -1.f * flBlocked * flRedirectCoeff + flLen ) / flLen;
	}

	// Return blocking flags.
	return blocked;
}

} // namespace source_sdk_oracle

