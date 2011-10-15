#include "cbase.h"
#include "baseentity.h"
#include "asw_spawner.h"
//#include "asw_simpleai_senses.h"
#include "asw_marine.h"
#include "asw_gamerules.h"
#include "asw_marine_resource.h"
#include "asw_game_resource.h"
#include "entityapi.h"
#include "entityoutput.h"
#include "props.h"
#include "asw_alien.h"
#include "asw_buzzer.h"
#include "asw_director.h"
#include "asw_fail_advice.h"
#include "asw_spawn_manager.h"
//Ch1ckensCoop: Include entitylist.h
#include "entitylist.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_spawner, CASW_Spawner );

//ConVar asw_uber_drone_chance("asw_uber_drone_chance", "0.25f", FCVAR_CHEAT, "Chance of an uber drone spawning when playing in uber mode");
extern ConVar asw_debug_spawners;
extern ConVar asw_drone_health;
ConVar asw_spawning_enabled( "asw_spawning_enabled", "1", FCVAR_CHEAT, "If set to 0, asw_spawners won't spawn aliens" );

//Ch1ckenscoop: Reduce console spam by default
ConVar asw_carnage_debug("asw_carnage_debug", "0", FCVAR_NONE, "Print debug messages on each spawner that asw_carnage tries to change.", true, 0.0f, true, 1.0f);

//Ch1ckenscoop: Control over asw_carnage
ConVar asw_carnage_drone("asw_carnage_drone", "1", FCVAR_CHEAT, "Sets whether the asw_carnage command affects normal drones.", true, 0.0f, true, 1.0f);
ConVar asw_carnage_drone_jumper("asw_carnage_drone_jumper", "1", FCVAR_CHEAT, "Sets whether the asw_carnage command affects drone jumpers.", true, 0.0f, true, 1.0f);
ConVar asw_carnage_drone_uber("asw_carnage_drone_uber", "0", FCVAR_CHEAT, "Sets whether the asw_carnage command affects uber drones.", true, 0.0f, true, 1.0f);
ConVar asw_carnage_buzzer("asw_carnage_buzzer", "0", FCVAR_CHEAT, "Sets whether the asw_carnage command affects buzzers.", true, 0.0f, true, 1.0f);
ConVar asw_carnage_parasite("asw_carnage_parasite", "0", FCVAR_CHEAT, "Sets whether the asw_carnage command affects parasites.", true, 0.0f, true, 1.0f);
ConVar asw_carnage_shieldbug("asw_carnage_shieldbug", "0", FCVAR_CHEAT, "Sets whether the asw_carnage command affects shieldbugs.", true, 0.0f, true, 1.0f);
//Ch1ckenscoop: no asw_carnage control command for grubs; that'd be stupid
ConVar asw_carnage_harvester("asw_carnage_harvester", "0", FCVAR_CHEAT, "Sets whether the asw_carnage command affects harvesters.", true, 0.0f, true, 1.0f);
ConVar asw_carnage_parasite_defanged("asw_carnage_parasite_defanged", "0", FCVAR_CHEAT, "Sets whether the asw_carnage command affects defanged parasites.", true, 0.0f, true, 1.0f);
//Ch1ckenscoop: no asw_carnage control command for queens; there aren't spawners for them.
ConVar asw_carnage_boomer("asw_carnage_boomer", "0", FCVAR_CHEAT, "Sets whether the asw_carnage command affects boomers.", true, 0.0f, true, 1.0f);
ConVar asw_carnage_ranger("asw_carnage_ranger", "0", FCVAR_CHEAT, "Sets whether the asw_carnage command affects rangers.", true, 0.0f, true, 1.0f);
ConVar asw_carnage_mortar("asw_carnage_mortar", "0", FCVAR_CHEAT, "Sets whether the asw_carnage command affects mortars.", true, 0.0f, true, 1.0f);
//Ch1ckensCoop TODO: won't do anything at the moment; no spawners for shamen.
//ConVar asw_carnage_shamen("asw_carnage_shamen", "0", FCVAR_CHEAT, "Sets whether the asw_carnage command affects shamen.", true, 0.0f, true, 1.0f);




BEGIN_DATADESC( CASW_Spawner )
	DEFINE_KEYFIELD( m_nMaxLiveAliens,			FIELD_INTEGER,	"MaxLiveAliens" ),
	DEFINE_KEYFIELD( m_nNumAliens,				FIELD_INTEGER,	"NumAliens" ),	
	DEFINE_KEYFIELD( m_bInfiniteAliens,			FIELD_BOOLEAN,	"InfiniteAliens" ),
	DEFINE_KEYFIELD( m_flSpawnInterval,			FIELD_FLOAT,	"SpawnInterval" ),
	DEFINE_KEYFIELD( m_flSpawnIntervalJitter,	FIELD_FLOAT,	"SpawnIntervalJitter" ),
	DEFINE_KEYFIELD( m_AlienClassNum,			FIELD_INTEGER,	"AlienClass" ),
	DEFINE_KEYFIELD( m_SpawnerState,			FIELD_INTEGER,	"SpawnerState" ),

	DEFINE_INPUTFUNC( FIELD_VOID,	"SpawnOneAlien",	InputSpawnAlien ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"StartSpawning",	InputStartSpawning ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"StopSpawning",		InputStopSpawning ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"ToggleSpawning",	InputToggleSpawning ),

	DEFINE_OUTPUT( m_OnAllSpawned,		"OnAllSpawned" ),
	DEFINE_OUTPUT( m_OnAllSpawnedDead,	"OnAllSpawnedDead" ),

	DEFINE_FIELD( m_nCurrentLiveAliens, FIELD_INTEGER ),
	DEFINE_FIELD( m_AlienClassName,		FIELD_STRING ),

	DEFINE_THINKFUNC( SpawnerThink ),
END_DATADESC()

CASW_Spawner::CASW_Spawner()
{
	m_hAlienOrderTarget = NULL;
}

CASW_Spawner::~CASW_Spawner()
{

}

void CASW_Spawner::InitAlienClassName()
{
	if ( m_AlienClassNum < 0 || m_AlienClassNum >= ASWSpawnManager()->GetNumAlienClasses() )
	{
		m_AlienClassNum = 0;
	}

	m_AlienClassName = ASWSpawnManager()->GetAlienClass( m_AlienClassNum )->m_iszAlienClass;
}

void CASW_Spawner::Spawn()
{
	InitAlienClassName();

	BaseClass::Spawn();
	
	m_flSpawnIntervalJitter /= 100.0f;
	m_flSpawnIntervalJitter = clamp<float>(m_flSpawnIntervalJitter, 0, 100);

	SetSolid( SOLID_NONE );
	m_nCurrentLiveAliens = 0;

	// trigger any begin state stuff
	SetSpawnerState(m_SpawnerState);
}

void CASW_Spawner::Precache()
{
	BaseClass::Precache();

	InitAlienClassName();
	const char *pszNPCName = STRING( m_AlienClassName );
	if ( !pszNPCName || !pszNPCName[0] )
	{
		Warning("asw_spawner %s has no specified alien-to-spawn classname.\n", STRING(GetEntityName()) );
	}
	else
	{
		UTIL_PrecacheOther( pszNPCName );
	}
}

IASW_Spawnable_NPC* CASW_Spawner::SpawnAlien( const char *szAlienClassName, const Vector &vecHullMins, const Vector &vecHullMaxs )
{
	IASW_Spawnable_NPC *pSpawnable = BaseClass::SpawnAlien( szAlienClassName, vecHullMins, vecHullMaxs );
	if ( pSpawnable )
	{
		m_nCurrentLiveAliens++;

		if (!m_bInfiniteAliens)
		{
			m_nNumAliens--;
			if (m_nNumAliens <= 0)
			{
				SpawnedAllAliens();
			}
		}
		else
		{
			ASWFailAdvice()->OnAlienSpawnedInfinite();
		}
	}
	return pSpawnable;
}

bool CASW_Spawner::CanSpawn( const Vector &vecHullMins, const Vector &vecHullMaxs )
{
	if ( !asw_spawning_enabled.GetBool() )
		return false;

	//Ch1ckensCoop: over 2000 edicts?
	if (gEntList.NumberOfEdicts() > 2000)
		return false;

	// too many alive already?
	if (m_nMaxLiveAliens>0 && m_nCurrentLiveAliens>=m_nMaxLiveAliens)
		return false;

	// have we run out?
	if (!m_bInfiniteAliens && m_nNumAliens<=0)
		return false;

	return BaseClass::CanSpawn( vecHullMins, vecHullMaxs );
}

// called when we've spawned all the aliens we can,
//  spawner should go to sleep
void CASW_Spawner::SpawnedAllAliens()
{
	m_OnAllSpawned.FireOutput( this, this );

	SetSpawnerState(SST_Finished); // disables think functions and so on
}

void CASW_Spawner::AlienKilled( CBaseEntity *pVictim )
{
	BaseClass::AlienKilled( pVictim );

	m_nCurrentLiveAliens--;

	if (asw_debug_spawners.GetBool())
		Msg("%d AlienKilled NumLive = %d\n", entindex(), m_nCurrentLiveAliens );

	// If we're here, we're getting erroneous death messages from children we haven't created
	AssertMsg( m_nCurrentLiveAliens >= 0, "asw_spawner receiving child death notice but thinks has no children\n" );

	if ( m_nCurrentLiveAliens <= 0 )
	{
		// See if we've exhausted our supply of NPCs
		if (!m_bInfiniteAliens && m_nNumAliens <= 0 )
		{
			if (asw_debug_spawners.GetBool())
				Msg("%d OnAllSpawnedDead (%s)\n", entindex(), STRING(GetEntityName()));
			// Signal that all our children have been spawned and are now dead
			m_OnAllSpawnedDead.FireOutput( this, this );
		}
	}
}

// mission started
void CASW_Spawner::MissionStart()
{
	if (asw_debug_spawners.GetBool())
		Msg("Spawner mission start, always inf=%d infinitealiens=%d\n", HasSpawnFlags( ASW_SF_ALWAYS_INFINITE ), m_bInfiniteAliens );
	// remove infinite spawns on easy mode
	if ( !HasSpawnFlags( ASW_SF_ALWAYS_INFINITE ) && ASWGameRules() && ASWGameRules()->GetSkillLevel() == 1
			&& m_bInfiniteAliens)
	{
		m_bInfiniteAliens = false;
		if (m_nNumAliens < 8)
			m_nNumAliens = 8;

		if (asw_debug_spawners.GetBool())
			Msg("  removed infinite and set num aliens to %d\n", m_nNumAliens);
	}

	if (m_SpawnerState == SST_StartSpawningWhenMissionStart)
		SetSpawnerState(SST_Spawning);
}

void CASW_Spawner::SetSpawnerState(SpawnerState_t newState)
{
	m_SpawnerState = newState;

	// begin state stuff
	if (m_SpawnerState == SST_Spawning)
	{
		SetThink ( &CASW_Spawner::SpawnerThink );
		SetNextThink( gpGlobals->curtime );
	}
	else if (m_SpawnerState == SST_Finished)
	{
		// Disable this forever.  Don't kill it because it still gets death notices
		SetThink( NULL );
		SetUse( NULL );
	}
	else if (m_SpawnerState == SST_WaitForInputs)
	{
		SetThink( NULL );	// stop thinking
	}
}

void CASW_Spawner::SpawnerThink()
{	
	// calculate jitter
	float fInterval = random->RandomFloat(1.0f - m_flSpawnIntervalJitter, 1.0f + m_flSpawnIntervalJitter) * m_flSpawnInterval;
	SetNextThink( gpGlobals->curtime + fInterval );

	if ( ASWDirector() && ASWDirector()->CanSpawnAlien( this ) )
	{
		SpawnAlien( STRING( m_AlienClassName ), GetAlienMins(), GetAlienMaxs() );
	}
}

// =====================
// Inputs
// =====================

void CASW_Spawner::SpawnOneAlien()
{
	SpawnAlien( STRING( m_AlienClassName ), GetAlienMins(), GetAlienMaxs() );
}

void CASW_Spawner::InputSpawnAlien( inputdata_t &inputdata )
{
	if (m_SpawnerState == SST_WaitForInputs)
	{
		if ( ASWDirector() && ASWDirector()->CanSpawnAlien( this ) )
		{
			SpawnOneAlien();
		}
	}
}

void CASW_Spawner::InputStartSpawning( inputdata_t &inputdata )
{
	if (m_SpawnerState == SST_WaitForInputs)
		SetSpawnerState(SST_Spawning);
}

void CASW_Spawner::InputStopSpawning( inputdata_t &inputdata )
{
	if (m_SpawnerState == SST_Spawning)
		SetSpawnerState(SST_WaitForInputs);
}

void CASW_Spawner::InputToggleSpawning( inputdata_t &inputdata )
{
	if (m_SpawnerState == SST_Spawning)
		SetSpawnerState(SST_WaitForInputs);
	else if (m_SpawnerState == SST_WaitForInputs)
		SetSpawnerState(SST_Spawning);
}

const Vector& CASW_Spawner::GetAlienMins()
{
	return NAI_Hull::Mins( ASWSpawnManager()->GetAlienClass( m_AlienClassNum )->m_nHullType );
}

const Vector& CASW_Spawner::GetAlienMaxs()
{
	return NAI_Hull::Maxs( ASWSpawnManager()->GetAlienClass( m_AlienClassNum )->m_nHullType );
}

bool CASW_Spawner::ApplyCarnageMode( float fScaler, float fInvScaler )
{
	if (asw_carnage_drone.GetBool() && m_AlienClassNum == g_nDroneClassEntry)
	{
		if (asw_carnage_debug.GetBool())
		{
			Msg( "[%d] Found a spawner set to spawn Drones\n", entindex());
			Msg( "  previous numaliens is %d; max live is %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		}
		m_nNumAliens *= fScaler;
		m_nMaxLiveAliens *= fScaler;
		m_flSpawnInterval *= fInvScaler;
		if (asw_carnage_debug.GetBool())
			Msg( "  Set its numaliens to %d; max live to %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		return true;
	}
	if (asw_carnage_drone_jumper.GetBool() && m_AlienClassNum == g_nDroneJumperClassEntry)
	{
		if (asw_carnage_debug.GetBool())
		{
			Msg( "[%d] Found a spawner set to spawn Drone Jumpers\n", entindex());
			Msg( "  previous numaliens is %d; max live is %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		}
		m_nNumAliens *= fScaler;
		m_nMaxLiveAliens *= fScaler;
		m_flSpawnInterval *= fInvScaler;
		if (asw_carnage_debug.GetBool())
			Msg( "  Set its numaliens to %d; max live to %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		return true;
	}
	if (asw_carnage_drone_uber.GetBool() && m_AlienClassNum == g_nUberDroneClassEntry)
	{
		if (asw_carnage_debug.GetBool())
		{
			Msg( "[%d] Found a spawner set to spawn Drone Ubers\n", entindex());
			Msg( "  previous numaliens is %d; max live is %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		}
		m_nNumAliens *= fScaler;
		m_nMaxLiveAliens *= fScaler;
		m_flSpawnInterval *= fInvScaler;
		if (asw_carnage_debug.GetBool())
			Msg( "  Set its numaliens to %d; max live to %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		return true;
	}
	if (asw_carnage_buzzer.GetBool() && m_AlienClassNum == g_nBuzzerClassEntry)
	{
		if (asw_carnage_debug.GetBool())
		{
			Msg( "[%d] Found a spawner set to spawn Buzzers\n", entindex());
			Msg( "  previous numaliens is %d; max live is %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		}
		m_nNumAliens *= fScaler;
		m_nMaxLiveAliens *= fScaler;
		m_flSpawnInterval *= fInvScaler;
		if (asw_carnage_debug.GetBool())
			Msg( "  Set its numaliens to %d; max live to %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		return true;
	}
	if (asw_carnage_parasite.GetBool() && m_AlienClassNum == g_nParasiteClassEntry)
	{
		if (asw_carnage_debug.GetBool())
		{
			Msg( "[%d] Found a spawner set to spawn Parasites\n", entindex());
			Msg( "  previous numaliens is %d; max live is %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		}
		m_nNumAliens *= fScaler;
		m_nMaxLiveAliens *= fScaler;
		m_flSpawnInterval *= fInvScaler;
		if (asw_carnage_debug.GetBool())
			Msg( "  Set its numaliens to %d; max live to %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		return true;
	}
	if (asw_carnage_shieldbug.GetBool() && m_AlienClassNum == g_nShieldbugClassEntry)
	{
		if (asw_carnage_debug.GetBool())
		{
			Msg( "[%d] Found a spawner set to spawn Shieldbugs\n", entindex());
			Msg( "  previous numaliens is %d; max live is %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		}
		m_nNumAliens *= fScaler;
		m_nMaxLiveAliens *= fScaler;
		m_flSpawnInterval *= fInvScaler;
		if (asw_carnage_debug.GetBool())
			Msg( "  Set its numaliens to %d; max live to %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		return true;
	}
	if (asw_carnage_harvester.GetBool() && m_AlienClassNum == g_nHarvesterClassEntry)
	{
		if (asw_carnage_debug.GetBool())
		{
			Msg( "[%d] Found a spawner set to Harvesters\n", entindex());
			Msg( "  previous numaliens is %d; max live is %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		}
		m_nNumAliens *= fScaler;
		m_nMaxLiveAliens *= fScaler;
		m_flSpawnInterval *= fInvScaler;
		if (asw_carnage_debug.GetBool())
			Msg( "  Set its numaliens to %d; max live to %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		return true;
	}
	if (asw_carnage_parasite_defanged.GetBool() && m_AlienClassNum == g_nSafeParasiteClassEntry)
	{
		if (asw_carnage_debug.GetBool())
		{
			Msg( "[%d] Found a spawner set to spawn Defanged Parasites\n", entindex());
			Msg( "  previous numaliens is %d; max live is %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		}
		m_nNumAliens *= fScaler;
		m_nMaxLiveAliens *= fScaler;
		m_flSpawnInterval *= fInvScaler;
		if (asw_carnage_debug.GetBool())
			Msg( "  Set its numaliens to %d; max live to %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		return true;
	}
	if (asw_carnage_boomer.GetBool() && m_AlienClassNum == g_nBoomerClassEntry)
	{
		if (asw_carnage_debug.GetBool())
		{
			Msg( "[%d] Found a spawner set to spawn Boomers\n", entindex());
			Msg( "  previous numaliens is %d; max live is %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		}
		m_nNumAliens *= fScaler;
		m_nMaxLiveAliens *= fScaler;
		m_flSpawnInterval *= fInvScaler;
		if (asw_carnage_debug.GetBool())
			Msg( "  Set its numaliens to %d; max live to %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		return true;
	}
	if (asw_carnage_ranger.GetBool() && m_AlienClassNum == g_nRangerClassEntry)
	{
		if (asw_carnage_debug.GetBool())
		{
			Msg( "[%d] Found a spawner set to spawn Rangers\n", entindex());
			Msg( "  previous numaliens is %d; max live is %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		}
		m_nNumAliens *= fScaler;
		m_nMaxLiveAliens *= fScaler;
		m_flSpawnInterval *= fInvScaler;
		if (asw_carnage_debug.GetBool())
			Msg( "  Set its numaliens to %d; max live to %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		return true;
	}
	if (asw_carnage_mortar.GetBool() && m_AlienClassNum == g_nMortarClassEntry)
	{
		if (asw_carnage_debug.GetBool())
		{
			Msg( "[%d] Found a spawner set to Mortars\n", entindex());
			Msg( "  previous numaliens is %d; max live is %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		return true;
		}
		m_nNumAliens *= fScaler;
		m_nMaxLiveAliens *= fScaler;
		m_flSpawnInterval *= fInvScaler;
		if (asw_carnage_debug.GetBool())
			Msg( "  Set its numaliens to %d; max live to %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		return true;
	}
	/*if ( m_AlienClassNum == g_nDroneClassEntry ||  m_AlienClassNum == g_nDroneJumperClassEntry )
	{
		// Ch1ckensCoop: Lessened console spam on mapchange
		Msg( "[%d] Found a spawner set to spawn drones or drone jumpers\n", entindex());
		//Msg( "  previous numaliens is %d; max live is %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );
		m_nNumAliens *= fScaler;
		m_nMaxLiveAliens *= fScaler;
		m_flSpawnInterval *= fInvScaler;
		//Msg( "  Set its numaliens to %d; max live to %d; interval %f\n", m_nNumAliens, m_nMaxLiveAliens, m_flSpawnInterval );

		return true;
	}	*/
	if (asw_carnage_debug.GetBool())
		Msg( "[%d] Found a spawner (%d) but asw_carnage is not enabled for it\n", entindex(), m_AlienClassNum );
	return false;
}

bool CASW_Spawner::RandomizeUber(float percent)
{
	if ( m_AlienClassNum == 0 && random->RandomFloat() <= percent)
	{
		//m_AlienClassNum = g_nUberDroneClassEntry;
		//Ch1ckensCoop: Doesn't work ^^^^^
		m_AlienClassName = ASWSpawnManager()->GetAlienClass( g_nUberDroneClassEntry )->m_iszAlienClass;
		DevMsg("Set m_AlienClassNum to %i\n", m_AlienClassNum);
		return true;
	}
	return false;
}
int	CASW_Spawner::DrawDebugTextOverlays()
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT)
	{
		NDebugOverlay::EntityText( entindex(),text_offset,CFmtStr( "Num Live Aliens: %d", m_nCurrentLiveAliens ),0 );
		text_offset++;
		NDebugOverlay::EntityText( entindex(),text_offset,CFmtStr( "Max Live Aliens: %d", m_nMaxLiveAliens ),0 );
		text_offset++;
		NDebugOverlay::EntityText( entindex(),text_offset,CFmtStr( "Alien supply: %d", m_bInfiniteAliens ? -1 : m_nNumAliens ),0 );
		text_offset++;
	}
	return text_offset;
}

void ASW_ApplyCarnage_f(float fScaler)
{
	if ( fScaler <= 0 )
		fScaler = 1.0f;

	float fInvScaler = 1.0f / fScaler;

	int iNewHealth = fInvScaler * 80.0f;	// note: boosted health a bit here so this mode is harder than normal
	asw_drone_health.SetValue(iNewHealth);

	CBaseEntity* pEntity = NULL;
	int iSpawnersChanged = 0;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "asw_spawner" )) != NULL)
	{
		CASW_Spawner* pSpawner = dynamic_cast<CASW_Spawner*>(pEntity);			
		if (pSpawner)
		{
			if ( pSpawner->ApplyCarnageMode( fScaler, fInvScaler ) )
			{
				iSpawnersChanged++;
			}
		}
	}
	Msg("%i asw_spawners had their output changed by asw_carnage.\n", iSpawnersChanged);
}

void ASW_RandomUber_f(float percent)
{
	CBaseEntity* pEntity = NULL;
	int iSpawnersChanged = 0;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "asw_spawner" )) != NULL)
	{
		CASW_Spawner* pSpawner = dynamic_cast<CASW_Spawner*>(pEntity);			
		if (pSpawner)
		{
			if ( pSpawner->RandomizeUber(percent) )
			{
				iSpawnersChanged++;
			}
		}
	}
	Msg("%i asw_spawners had their output changed by asw_carnage_randomize_uber.\n", iSpawnersChanged);
}

void asw_carnage_f(const CCommand &args)
{
	if ( args.ArgC() < 2 )
	{
		Msg( "Please supply a scale\n" );
	}
	ASW_ApplyCarnage_f( atof( args[1] ) );
}

void asw_carnage_random_uber_f(const CCommand &args)
{
	if (args.ArgC() < 2)
	{
		Msg("Please supply a percentage of spawners to randomize by\n");
	}
	else if (atof(args[1]) < 0.0f || atof(args[1]) > 1.0f)
	{
		Msg("Value must be between 0 and 1.0\n");
	}
	else
	{
		ASW_RandomUber_f(atof(args[1]));
	}
}

ConCommand asw_carnage( "asw_carnage", asw_carnage_f, "Scales the number of aliens each spawner will put out", FCVAR_CHEAT );
//Ch1ckensCoop: randomize asw_drone spawners with asw_drone_uber spawners
ConCommand asw_carnage_randomize_uber( "asw_carnage_randomize_uber", asw_carnage_random_uber_f, "Randomizes asw_drone spawners with asw_drone_uber spawners.", FCVAR_CHEAT);
