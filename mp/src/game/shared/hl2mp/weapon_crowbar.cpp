//========= Copyright Valve Corporation, All rights reserved. ============//
// Purpose:	Crowbar
// $NoKeywords: $
// FPC

#include "cbase.h"
#include "hl2mp/weapon_crowbar.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "gamerules.h"
#include "ammodef.h"
#include "mathlib/mathlib.h"
#include "in_buttons.h"
#include "vstdlib/random.h"
#include "npcevent.h"

#if defined( CLIENT_DLL )
	#include "c_hl2mp_player.h"
#else
	#include "hl2mp_player.h"
	#include "ai_basenpc.h"
#endif

// memdbgon must be the last include file in a .cpp file! #Vc
#include "tier0/memdbgon.h"

static const float CROWBAR_RANGE = 75.0f; // measured in inches #L
static const float CROWBAR_REFIRE = .4f; // Refire indicates minimum delay between attacks, i.e. maximum rate of fire #L
static const float CROWBAR_DAMAGE = 90.0f; // Original value: 25.0

//// CWeaponCrowbar Class #Vc

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCrowbar, DT_WeaponCrowbar )

BEGIN_NETWORK_TABLE( CWeaponCrowbar, DT_WeaponCrowbar )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponCrowbar )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_crowbar, CWeaponCrowbar );
PRECACHE_WEAPON_REGISTER( weapon_crowbar );

#ifndef CLIENT_DLL

acttable_t	CWeaponCrowbar::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1,				ACT_RANGE_ATTACK_SLAM, true },
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_MELEE,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_MELEE,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_MELEE,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_MELEE,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_MELEE,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_MELEE,			false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_MELEE,					false },
};

IMPLEMENT_ACTTABLE(CWeaponCrowbar);

#endif

//// Constructor using defaults #C Is this line really necessary when the functionality is automatic?
CWeaponCrowbar::CWeaponCrowbar( void ) {}

// Purpose: Get the damage amount for the animation we're doing
// Input  : hitActivity = currently played activity
// Output : Damage amount
float CWeaponCrowbar::GetDamageForActivity( Activity hitActivity ) { return CROWBAR_DAMAGE; }
/* ^ Why does this always return the same value? Probably because there is no variation in crowbar weapon damage.
	It is likely that this is a generalized function which happens to be useless for this particular weapon.
		This should come in handy when we are specifically writing different attacks. */

// Purpose: Add in a view kick for this weapon
void CWeaponCrowbar::AddViewKick( void ) {
	CBasePlayer *pPlayer  = ToBasePlayer( GetOwner() );
	
	if ( pPlayer == NULL )
		return;

	QAngle punchAng;

	punchAng.x = SharedRandomFloat( "crowbarpax", 1.0f, 2.0f );
	punchAng.y = SharedRandomFloat( "crowbarpay", -2.0f, -1.0f );
	punchAng.z = 0.0f;
	
	pPlayer->ViewPunch( punchAng ); 
}
/*? ^ By view kick, do they mean the rotation of the first-person camera? 
	For some reason, no kicking is taking place in Server Test or HL2DM, but is clear 
	in HL2 single-player*/


#ifndef CLIENT_DLL
//// Animation event handlers
void CWeaponCrowbar::HandleAnimEventMeleeHit( animevent_t *pEvent, CBaseCombatCharacter *pOperator ) {
	// Trace up or down based on where the enemy is...
	// But only if we're basically facing that direction #V
	Vector vecDirection;
	AngleVectors( GetAbsAngles(), &vecDirection );

	Vector vecEnd;
	VectorMA( pOperator->Weapon_ShootPosition(), 50, vecDirection, vecEnd );
	CBaseEntity *pHurt = pOperator->CheckTraceHullAttack( pOperator->Weapon_ShootPosition(), vecEnd, 
		Vector(-16,-16,-16), Vector(36,36,36), GetDamageForActivity( GetActivity() ), DMG_CLUB, 0.75 );
	
	// did I hit someone? #V
	if ( pHurt ) {
		// play sound #V
		WeaponSound( MELEE_HIT );

		// Fake a trace impact, so the effects work out like a player's crowbar #V
		trace_t traceHit;
		UTIL_TraceLine( pOperator->Weapon_ShootPosition(), pHurt->GetAbsOrigin(), MASK_SHOT_HULL, pOperator, COLLISION_GROUP_NONE, &traceHit );
		ImpactEffect( traceHit );
	}
	else
		WeaponSound( MELEE_MISS );
}


//// Animation event 
void CWeaponCrowbar::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator ) {
	switch( pEvent->event ) {
	case EVENT_WEAPON_MELEE_HIT:
		HandleAnimEventMeleeHit( pEvent, pOperator );
		break;

	default:
		BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
		break;
	}
}

//// Attempt to lead the target (needed because citizens can't hit manhacks with the crowbar!)
ConVar sk_crowbar_lead_time( "sk_crowbar_lead_time", "0.9" );

int CWeaponCrowbar::WeaponMeleeAttack1Condition( float flDot, float flDist ) {
	// Attempt to lead the target (needed because citizens can't hit manhacks with the crowbar!)
	CAI_BaseNPC *pNPC	= GetOwner()->MyNPCPointer();
	CBaseEntity *pEnemy = pNPC->GetEnemy();
	if (!pEnemy)
		return COND_NONE;

	Vector vecVelocity;
	vecVelocity = pEnemy->GetSmoothedVelocity( );

	// Project where the enemy will be in a little while
	float dt = sk_crowbar_lead_time.GetFloat();
	dt += SharedRandomFloat( "crowbarmelee1", -0.3f, 0.2f );
	if ( dt < 0.0f )
		dt = 0.0f;

	Vector vecExtrapolatedPos;
	VectorMA( pEnemy->WorldSpaceCenter(), dt, vecVelocity, vecExtrapolatedPos );

	Vector vecDelta;
	VectorSubtract( vecExtrapolatedPos, pNPC->WorldSpaceCenter(), vecDelta );

	if ( fabs( vecDelta.z ) > 70 )
		return COND_TOO_FAR_TO_ATTACK;

	Vector vecForward = pNPC->BodyDirection2D( );
	vecDelta.z = 0.0f;
	float flExtrapolatedDist = Vector2DNormalize( vecDelta.AsVector2D() );
	
	if ((flDist > 64) && (flExtrapolatedDist > 64))
		return COND_TOO_FAR_TO_ATTACK;

	float flExtrapolatedDot = DotProduct2D( vecDelta.AsVector2D(), vecForward.AsVector2D() );
	
	if ((flDot < 0.7) && (flExtrapolatedDot < 0.7))
		return COND_NOT_FACING_ATTACK;

	return COND_CAN_MELEE_ATTACK1;
}

#endif


//? Purpose: removes crowbar from inventory?
void CWeaponCrowbar::Drop( const Vector &vecVelocity ) {
#ifndef CLIENT_DLL
	UTIL_Remove( this );
#endif
}

float CWeaponCrowbar::GetRange( void ) { return	CROWBAR_RANGE; }

float CWeaponCrowbar::GetFireRate( void ) { return	CROWBAR_REFIRE;	}