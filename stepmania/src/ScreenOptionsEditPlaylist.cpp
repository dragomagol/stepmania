#include "global.h"

#include "ScreenOptionsEditPlaylist.h"
#include "ScreenMiniMenu.h"
#include "SongUtil.h"
#include "SongManager.h"
#include "OptionRowHandler.h"
#include "Song.h"
#include "Workout.h"
#include "GameState.h"
#include "ScreenPrompt.h"
#include "LocalizedString.h"
#include "WorkoutManager.h"
#include "song.h"
#include "Style.h"
#include "Steps.h"


static void GetStepsForSong( Song *pSong, vector<Steps*> &vpStepsOut )
{
	SongUtil::GetSteps( pSong, vpStepsOut, GAMESTATE->GetCurrentStyle()->m_StepsType );
	StepsUtil::RemoveLockedSteps( pSong, vpStepsOut );
	StepsUtil::SortNotesArrayByDifficulty( vpStepsOut );
}

// XXX: very similar to OptionRowHandlerSteps
class OptionRowHandlerSteps : public OptionRowHandler
{
public:
	void Load( int iEntryIndex )
	{
		m_iEntryIndex = iEntryIndex;
	}
	virtual ReloadChanged Reload()
	{
		m_Def.m_vsChoices.clear();
		m_vpSteps.clear();

		Song *pSong = GAMESTATE->m_pCurSong;
		if( pSong ) // playing a song
		{
			GetStepsForSong( pSong, m_vpSteps );
			FOREACH_CONST( Steps*, m_vpSteps, steps )
			{
				RString s;
				if( (*steps)->GetDifficulty() == Difficulty_Edit )
					s = (*steps)->GetDescription();
				else
					s = GetLocalizedCustomDifficulty( (*steps)->m_StepsType, (*steps)->GetDifficulty() );
				s += ssprintf( " %d", (*steps)->GetMeter() );
				m_Def.m_vsChoices.push_back( s );
			}
			m_Def.m_vEnabledForPlayers.clear();
			m_Def.m_vEnabledForPlayers.insert( PLAYER_1 );
		}
		else
		{
			m_Def.m_vsChoices.push_back( "n/a" );
			m_vpSteps.push_back( NULL );
			m_Def.m_vEnabledForPlayers.clear();
		}


		return RELOAD_CHANGED_ALL;
	}
	virtual void ImportOption( OptionRow *pRow, const vector<PlayerNumber> &vpns, vector<bool> vbSelectedOut[NUM_PLAYERS] ) const 
	{
		Trail *pTrail = GAMESTATE->m_pCurTrail[PLAYER_1];
		Steps *pSteps;
		if( m_iEntryIndex < (int)pTrail->m_vEntries.size() )
			pSteps = pTrail->m_vEntries[ m_iEntryIndex ].pSteps;
		
		vector<Steps*>::const_iterator iter = find( m_vpSteps.begin(), m_vpSteps.end(), pSteps );
		if( iter == m_vpSteps.end() )
		{
			pRow->SetOneSharedSelection( 0 );
		}
		else
		{
			int index = iter - m_vpSteps.begin();
			pRow->SetOneSharedSelection( index );
		}

	}
	virtual int ExportOption( const vector<PlayerNumber> &vpns, const vector<bool> vbSelected[NUM_PLAYERS] ) const 
	{
		return 0;
	}
	Steps *GetSteps( int iStepsIndex ) const
	{
		return m_vpSteps[iStepsIndex];
	}

protected:
	int	m_iEntryIndex;
	vector<Steps*> m_vpSteps;
};



const int NUM_SONG_ROWS = 20;

REGISTER_SCREEN_CLASS( ScreenOptionsEditPlaylist );

enum WorkoutDetailsRow
{
	WorkoutDetailsRow_Minutes,
	NUM_WorkoutDetailsRow
};

enum RowType
{
	RowType_Song, 
	RowType_Steps, 
	NUM_RowType,
	RowType_Invalid, 
};
static int RowToEntryIndex( int iRow )
{
	if( iRow < NUM_WorkoutDetailsRow )
		return -1;

	return (iRow-NUM_WorkoutDetailsRow)/NUM_RowType;
}
static RowType RowToRowType( int iRow )
{
	if( iRow < NUM_WorkoutDetailsRow )
		return RowType_Invalid;
	return (RowType)((iRow-NUM_WorkoutDetailsRow) % NUM_RowType);
}
static int EntryIndexAndRowTypeToRow( int iEntryIndex, RowType rowType )
{
	return NUM_WorkoutDetailsRow + iEntryIndex*NUM_RowType + rowType;
}

void ScreenOptionsEditPlaylist::Init()
{
	ScreenOptions::Init();
	SONGMAN->GetPreferredSortSongs( m_vpSongs );
	SongUtil::SortSongPointerArrayByTitle( m_vpSongs );
}

const MenuRowDef g_MenuRows[] = 
{
	MenuRowDef( -1,	"Workout Minutes",	true, EditMode_Practice, true, false, 0, NULL ),
};

static LocalizedString EMPTY	("ScreenOptionsEditPlaylist","-Empty-");
static LocalizedString SONG	("ScreenOptionsEditPlaylist","Song");
static LocalizedString STEPS	("ScreenOptionsEditPlaylist","Steps");
static LocalizedString MINUTES	("ScreenOptionsEditPlaylist","minutes");

static RString MakeMinutesString( int mins )
{
	if( mins == 0 )
		return "No Cut-off";
	return ssprintf( "%d", mins ) + " " + MINUTES.GetValue();
}

void ScreenOptionsEditPlaylist::BeginScreen()
{
	vector<OptionRowHandler*> vHands;

	FOREACH_ENUM( WorkoutDetailsRow, rowIndex )
	{
		const MenuRowDef &mr = g_MenuRows[rowIndex];
		OptionRowHandler *pHand = OptionRowHandlerUtil::MakeSimple( mr );
	
		pHand->m_Def.m_layoutType = LAYOUT_SHOW_ONE_IN_ROW;
		pHand->m_Def.m_vsChoices.clear();
	
		switch( rowIndex )
		{
		DEFAULT_FAIL(rowIndex);
		case WorkoutDetailsRow_Minutes:
			pHand->m_Def.m_vsChoices.push_back( MakeMinutesString(0) );
			for( int i=MIN_WORKOUT_MINUTES; i<=20; i+=2 )
				pHand->m_Def.m_vsChoices.push_back( MakeMinutesString(i) );
			for( int i=20; i<=MAX_WORKOUT_MINUTES; i+=5 )
				pHand->m_Def.m_vsChoices.push_back( MakeMinutesString(i) );
			break;
		}

		pHand->m_Def.m_bExportOnChange = true;
		vHands.push_back( pHand );
	}



	for( int i=0; i<NUM_SONG_ROWS; i++ )
	{
		{
			MenuRowDef mrd = MenuRowDef( -1, "---", true, EditMode_Practice, true, false, 0, EMPTY.GetValue() );
			FOREACH_CONST( Song*, m_vpSongs, s )
				mrd.choices.push_back( (*s)->GetDisplayFullTitle() );
			mrd.sName = ssprintf(SONG.GetValue() + " %d",i+1);
			OptionRowHandler *pHand = OptionRowHandlerUtil::MakeSimple( mrd );
			pHand->m_Def.m_bAllowThemeTitle = false;	// already themed
			pHand->m_Def.m_sExplanationName = "Choose Song";
			pHand->m_Def.m_layoutType = LAYOUT_SHOW_ONE_IN_ROW;
			pHand->m_Def.m_bExportOnChange = true;
			vHands.push_back( pHand );
		}
		
		{
			OptionRowHandlerSteps *pHand = new OptionRowHandlerSteps;
			pHand->Load( i );
			pHand->m_Def.m_vsChoices.push_back( "n/a" );
			pHand->m_Def.m_sName = ssprintf(STEPS.GetValue() + " %d",i+1);
			pHand->m_Def.m_bAllowThemeTitle = false;	// already themed
			pHand->m_Def.m_bAllowThemeItems = false;	// already themed
			pHand->m_Def.m_sExplanationName = "Choose Steps";
			pHand->m_Def.m_bOneChoiceForAllPlayers = true;
			pHand->m_Def.m_layoutType = LAYOUT_SHOW_ONE_IN_ROW;
			pHand->m_Def.m_bExportOnChange = true;
			vHands.push_back( pHand );
		}

	}

	ScreenOptions::InitMenu( vHands );

	ScreenOptions::BeginScreen();


	for( int i=0; i<(int)m_pRows.size(); i++ )
	{
		OptionRow *pRow = m_pRows[i];
		m_iCurrentRow[PLAYER_1] = i;
		this->SetCurrentSong();
		pRow->Reload();
	}
	m_iCurrentRow[PLAYER_1] = 0;

	this->SetCurrentSong();

	//this->AfterChangeRow( PLAYER_1 );
}

ScreenOptionsEditPlaylist::~ScreenOptionsEditPlaylist()
{

}

void ScreenOptionsEditPlaylist::ImportOptions( int iRow, const vector<PlayerNumber> &vpns )
{
	OptionRow &row = *m_pRows[iRow];
	if( row.GetRowType() == OptionRow::RowType_Exit )
		return;

	switch( iRow )
	{
	case WorkoutDetailsRow_Minutes:
		row.SetOneSharedSelection( 0 );
		row.SetOneSharedSelectionIfPresent( MakeMinutesString(GAMESTATE->m_pCurCourse->m_fGoalSeconds/60) );
		break;
	default:
		{
			int iEntryIndex = RowToEntryIndex( iRow );
			RowType rowType = RowToRowType( iRow );

			switch( rowType )
			{
			DEFAULT_FAIL( rowType );
			case RowType_Song:
				{
					Song *pSong = NULL;
					if( iEntryIndex < (int)GAMESTATE->m_pCurCourse->m_vEntries.size() )
						pSong = GAMESTATE->m_pCurCourse->m_vEntries[iEntryIndex].songID.ToSong();

					vector<Song*>::iterator iter = find( m_vpSongs.begin(), m_vpSongs.end(), pSong );
					if( iter == m_vpSongs.end() )
						row.SetOneSharedSelection( 0 );
					else
						row.SetOneSharedSelection( 1 + iter - m_vpSongs.begin() );
				}
				break;
			case RowType_Steps:
				// the OptionRowHandler does its own importing
				break;
			}
		}
		break;
	}
}

void ScreenOptionsEditPlaylist::ExportOptions( int iRow, const vector<PlayerNumber> &vpns )
{
	FOREACH_ENUM( WorkoutDetailsRow, i )
	{
		OptionRow &row = *m_pRows[i];
		int iIndex = row.GetOneSharedSelection( true );
		RString sValue;
		if( iIndex >= 0 )
			sValue = row.GetRowDef().m_vsChoices[ iIndex ];

		switch( i )
		{
		DEFAULT_FAIL(i);
		case WorkoutDetailsRow_Minutes:
			GAMESTATE->m_pCurCourse->m_fGoalSeconds = 0;
			int mins;
			if( sscanf( sValue, "%d", &mins ) == 1 )
				GAMESTATE->m_pCurCourse->m_fGoalSeconds = mins * 60;
			break;
		}
	}

	GAMESTATE->m_pCurCourse->m_vEntries.clear();

	for( int i=NUM_WorkoutDetailsRow; i<(int)m_pRows.size(); i++ )
	{
		OptionRow &row = *m_pRows[i];
		if( row.GetRowType() == OptionRow::RowType_Exit )
			continue;

		RowType rowType = RowToRowType( i );
		int iEntryIndex = RowToEntryIndex( i );

		switch( rowType )
		{
		case RowType_Song:
			{
				Song *pSong = this->GetSongForEntry( iEntryIndex );
				if( pSong )
				{
					Steps *pSteps = this->GetStepsForEntry( iEntryIndex );
					ASSERT( pSteps );
					CourseEntry ce;
					ce.songID.FromSong( pSong );
					ce.stepsCriteria.m_difficulty = pSteps->GetDifficulty();
					GAMESTATE->m_pCurCourse->m_vEntries.push_back( ce );
				}
			}
			break;
		case RowType_Steps:
			// push each CourseEntry when we handle each RowType_Song above
			break;
		default:
			break;
		}
	}

	EditCourseUtil::UpdateAndSetTrail();
}

void ScreenOptionsEditPlaylist::GoToNextScreen()
{
}

void ScreenOptionsEditPlaylist::GoToPrevScreen()
{
}

void ScreenOptionsEditPlaylist::HandleScreenMessage( const ScreenMessage SM )
{
	if( SM == SM_ExportOptions )
	{
		//g_Workout.m_vpSongs.clear();
	}

	ScreenOptions::HandleScreenMessage( SM );
}

void ScreenOptionsEditPlaylist::SetCurrentSong()
{
	int iRow = m_iCurrentRow[PLAYER_1];
	OptionRow &row = *m_pRows[iRow];

	if( row.GetRowType() == OptionRow::RowType_Exit )
	{
		GAMESTATE->m_pCurSong.Set( NULL );
		GAMESTATE->m_pCurSteps[PLAYER_1].Set( NULL );
	}
	else
	{
		int iRow = m_iCurrentRow[PLAYER_1];
		int iEntryIndex = RowToEntryIndex( iRow );
		Song *pSong = NULL;
		if( iEntryIndex != -1 )
		{
			int iCurrentSongRow = EntryIndexAndRowTypeToRow(iEntryIndex,RowType_Song);
			OptionRow &row = *m_pRows[ iCurrentSongRow ];
			int index = row.GetOneSelection(PLAYER_1);
			if( index != 0 )
				pSong = m_vpSongs[ index - 1 ];
		}
		GAMESTATE->m_pCurSong.Set( pSong );
	}
}

void ScreenOptionsEditPlaylist::SetCurrentSteps()
{
	Song *pSong = GAMESTATE->m_pCurSong;
	if( pSong )
	{
		int iRow = m_iCurrentRow[PLAYER_1];
		int iEntryIndex = RowToEntryIndex( iRow );
		OptionRow &row = *m_pRows[ EntryIndexAndRowTypeToRow(iEntryIndex, RowType_Steps) ];
		int iStepsIndex = row.GetOneSharedSelection();
		const OptionRowHandlerSteps *pHand = dynamic_cast<const OptionRowHandlerSteps *>( row.GetHandler() );
		ASSERT( pHand );
		Steps *pSteps = pHand->GetSteps( iStepsIndex );
		GAMESTATE->m_pCurSteps[PLAYER_1].Set( pSteps );
	}
	else
	{
		GAMESTATE->m_pCurSteps[PLAYER_1].Set( NULL );
	}
}

Song *ScreenOptionsEditPlaylist::GetSongForEntry( int iEntryIndex )
{
	int iRow = EntryIndexAndRowTypeToRow( iEntryIndex, RowType_Song );
	OptionRow &row = *m_pRows[iRow];

	int index = row.GetOneSharedSelection();
	if( index == 0 )
		return NULL;
	return m_vpSongs[ index - 1 ];
}

Steps *ScreenOptionsEditPlaylist::GetStepsForEntry( int iEntryIndex )
{
	int iRow = EntryIndexAndRowTypeToRow( iEntryIndex, RowType_Steps );
	OptionRow &row = *m_pRows[iRow];
	int index = row.GetOneSharedSelection();
	Song *pSong = GetSongForEntry( iEntryIndex );
	vector<Steps*> vpSteps;
	GetStepsForSong( pSong, vpSteps );
	return vpSteps[index];
}

void ScreenOptionsEditPlaylist::AfterChangeRow( PlayerNumber pn )
{
	ScreenOptions::AfterChangeRow( pn );

	SetCurrentSong();
}

void ScreenOptionsEditPlaylist::AfterChangeValueInRow( int iRow, PlayerNumber pn )
{
	ScreenOptions::AfterChangeValueInRow( iRow, pn );

	int iEntryIndex = RowToEntryIndex( iRow );
	RowType rowType = RowToRowType( iRow );
	switch( rowType )
	{
	case RowType_Song:
		{
			SetCurrentSong();
			OptionRow &row = *m_pRows[ EntryIndexAndRowTypeToRow(iEntryIndex, RowType_Steps) ];
			row.Reload();
		}
		break;
	case RowType_Steps:
		SetCurrentSteps();
		break;
	default:
		break;
	}
}

const int MIN_ENABLED_SONGS = 2;

static LocalizedString MUST_ENABLE_AT_LEAST("ScreenOptionsEditPlaylist","You must enable at least %d songs.");
void ScreenOptionsEditPlaylist::ProcessMenuStart( const InputEventPlus &input )
{
	if( IsTransitioning() )
		return;

	int iRow = m_iCurrentRow[GAMESTATE->m_MasterPlayerNumber];

	unsigned iSongCount = GAMESTATE->m_pCurCourse->m_vEntries.size();

	if( m_pRows[iRow]->GetRowType() == OptionRow::RowType_Exit  &&  iSongCount < unsigned(MIN_ENABLED_SONGS) )
	{
		ScreenPrompt::Prompt( SM_None, ssprintf(MUST_ENABLE_AT_LEAST.GetValue(),MIN_ENABLED_SONGS) );
		return;
	}

	ScreenOptions::ProcessMenuStart( input );
}


/*
 * (c) 2003-2004 Chris Danford
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
