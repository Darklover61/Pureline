#include "stdafx.h"
#ifdef __FreeBSD__
	#include <md5.h>
#else
	#include "../../libthecore/include/xmd5.h"
#endif

#include "utils.h"
#include "config.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "char.h"
#include "char_manager.h"
#include "motion.h"
#include "packet.h"
#include "affect.h"
#include "pvp.h"
#include "start_position.h"
#include "party.h"
#include "guild_manager.h"
#include "p2p.h"
#include "dungeon.h"
#include "messenger_manager.h"
#include "war_map.h"
#include "questmanager.h"
#include "item_manager.h"
#include "monarch.h"
#include "mob_manager.h"
#include "dev_log.h"
#include "item.h"
#include "arena.h"
#include "buffer_manager.h"
#include "unique_item.h"
#include "threeway_war.h"
#include "log.h"
#include "../../common/VnumHelper.h"
#ifdef __AUCTION__
	#include "auction_manager.h"
#endif

extern int g_server_id;

extern int g_nPortalLimitTime;

ACMD (do_user_horse_ride)
{
	if (ch->IsObserverMode())
	{
		return;
	}

	if (ch->IsDead() || ch->IsStun())
	{
		return;
	}

	if (ch->IsHorseRiding() == false)
	{
		// ���� �ƴ� �ٸ�Ż���� Ÿ���ִ�.
		if (ch->GetMountVnum())
		{
			ch->ChatPacket (CHAT_TYPE_INFO, "[LS;1054]"/* "You're already riding. Get off first." */);
			return;
		}

		if (ch->GetHorse() == NULL)
		{
			ch->ChatPacket (CHAT_TYPE_INFO, "[LS;501]"/* "Please call your Horse first." */);
			return;
		}

		ch->StartRiding();
	}
	else
	{
		ch->StopRiding();
	}
}

ACMD (do_user_horse_back)
{
	if (ch->GetHorse() != NULL)
	{
		ch->HorseSummon (false);
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;502]"/* "You have sent your horse away." */);
	}
	else if (ch->IsHorseRiding() == true)
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;504]"/* "You have to get off your Horse." */);
	}
	else
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;501]"/* "Please call your Horse first." */);
	}
}

ACMD (do_user_horse_feed)
{
	// ���λ����� �� ���¿����� �� ���̸� �� �� ����.
	if (ch->GetMyShop())
	{
		return;
	}

	if (ch->GetHorse() == NULL)
	{
		if (ch->IsHorseRiding() == false)
		{
			ch->ChatPacket (CHAT_TYPE_INFO, "[LS;501]"/* "Please call your Horse first." */);
		}
		else
		{
			ch->ChatPacket (CHAT_TYPE_INFO, "[LS;505]"/* "You cannot feed your Horse whilst sitting on it." */);
		}
		return;
	}

	DWORD dwFood = ch->GetHorseGrade() + 50054 - 1;

	if (ch->CountSpecifyItem (dwFood) > 0)
	{
		ch->RemoveSpecifyItem (dwFood, 1);
		ch->FeedHorse();
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;506;%s;%s]"/* "You have fed the Horse with %s%s." */,
						ITEM_MANAGER::instance().GetTable (dwFood)->szName); /* [yosun_fixme] make this multilang and where is the second %s? */
	}
	else
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;507;%s]"/* "You need %s." */, ITEM_MANAGER::instance().GetTable (dwFood)->szName); /* [KaptanYosun TODO] make this multilang */
	}
}

#define MAX_REASON_LEN		128

EVENTINFO (TimedEventInfo)
{
	DynamicCharacterPtr ch;
	int		subcmd;
	int         	left_second;
	char		szReason[MAX_REASON_LEN];

	TimedEventInfo()
		: ch()
		, subcmd (0)
		, left_second (0)
	{
		::memset (szReason, 0, MAX_REASON_LEN);
	}
};

struct SendDisconnectFunc
{
	void operator() (LPDESC d)
	{
		if (d->GetCharacter())
		{
			if (d->GetCharacter()->GetGMLevel() == GM_PLAYER)
			{
				d->GetCharacter()->ChatPacket (CHAT_TYPE_COMMAND, "quit Shutdown(SendDisconnectFunc)");
			}
		}
	}
};

struct DisconnectFunc
{
	void operator() (LPDESC d)
	{
		if (d->GetType() == DESC_TYPE_CONNECTOR)
		{
			return;
		}

		if (d->IsPhase (PHASE_P2P))
		{
			return;
		}

		if (d->GetCharacter())
		{
			d->GetCharacter()->Disconnect ("Shutdown(DisconnectFunc)");
		}

		d->SetPhase (PHASE_CLOSE);
	}
};

EVENTINFO (shutdown_event_data)
{
	int seconds;

	shutdown_event_data()
		: seconds (0)
	{
	}
};

EVENTFUNC (shutdown_event)
{
	shutdown_event_data* info = dynamic_cast<shutdown_event_data*> (event->info);

	if (info == NULL)
	{
		sys_err ("shutdown_event> <Factor> Null pointer");
		return 0;
	}

	int* pSec = & (info->seconds);

	if (*pSec < 0)
	{
		sys_log (0, "shutdown_event sec %d", *pSec);

		if (--*pSec == -10)
		{
			const DESC_MANAGER::DESC_SET & c_set_desc = DESC_MANAGER::instance().GetClientSet();
			std::ranges::for_each (c_set_desc, DisconnectFunc());
			return passes_per_sec;
		}
		else if (*pSec < -10)
		{
			return 0;
		}

		return passes_per_sec;
	}
	else if (*pSec == 0)
	{
		const DESC_MANAGER::DESC_SET & c_set_desc = DESC_MANAGER::instance().GetClientSet();
		std::ranges::for_each (c_set_desc, SendDisconnectFunc());
		g_bNoMoreClient = true;
		--*pSec;
		return passes_per_sec;
	}
	else
	{
		char buf[64];
		snprintf (buf, sizeof (buf), "[LS;508;%d]"/* "%d seconds until Exit." */, *pSec);
		SendNotice (buf);

		--*pSec;
		return passes_per_sec;
	}
}

void Shutdown (int iSec)
{
	if (g_bNoMoreClient)
	{
		thecore_shutdown();
		return;
	}

	CWarMapManager::instance().OnShutdown();

	char buf[64];
	snprintf (buf, sizeof (buf), "[LS;509;%d]"/* "The game will be closed in %d seconds." */, iSec);

	SendNotice (buf);

	shutdown_event_data* info = AllocEventInfo<shutdown_event_data>();
	info->seconds = iSec;

	event_create (shutdown_event, info, 1);
}

ACMD (do_shutdown)
{
	if (nullptr == ch)
	{
		sys_err ("Accept shutdown command from %s.", ch->GetName());
	}
	TPacketGGShutdown p;
	p.bHeader = HEADER_GG_SHUTDOWN;
	P2P_MANAGER::instance().Send (&p, sizeof (TPacketGGShutdown));

	Shutdown (10);
}

EVENTFUNC (timed_event)
{
	TimedEventInfo * info = dynamic_cast<TimedEventInfo*> (event->info);

	if (info == nullptr)
	{
		sys_err ("timed_event> <Factor> Null pointer");
		return 0;
	}

	LPCHARACTER	ch = info->ch;
	if (ch == nullptr)   // <Factor>
	{
		return 0;
	}
	LPDESC d = ch->GetDesc();

	if (info->left_second <= 0)
	{
		ch->m_pkTimedEvent = nullptr;

		if (true == LC_IsEurope() || true == LC_IsYMIR() || true == LC_IsKorea())
		{
			switch (info->subcmd)
			{
				case SCMD_LOGOUT:
				case SCMD_QUIT:
				case SCMD_PHASE_SELECT:
				{
					TPacketNeedLoginLogInfo acc_info;
					acc_info.dwPlayerID = ch->GetDesc()->GetAccountTable().id;

					db_clientdesc->DBPacket (HEADER_GD_VALID_LOGOUT, 0, &acc_info, sizeof (acc_info));

					LogManager::instance().DetailLoginLog (false, ch);
				}
				break;
			}
		}

		switch (info->subcmd)
		{
			case SCMD_LOGOUT:
				if (d)
				{
					d->SetPhase (PHASE_CLOSE);
				}
				break;

			case SCMD_QUIT:
				ch->ChatPacket (CHAT_TYPE_COMMAND, "quit");
				break;

			case SCMD_PHASE_SELECT:
			{
				ch->Disconnect ("timed_event - SCMD_PHASE_SELECT");

				if (d)
				{
					d->SetPhase (PHASE_SELECT);
				}
			}
			break;
		}

		return 0;
	}
	else
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;510;%d]"/* "%d seconds until Exit." */, info->left_second);
		--info->left_second;
	}

	return PASSES_PER_SEC (1);
}

ACMD (do_cmd)
{
	/* RECALL_DELAY
	   if (ch->m_pkRecallEvent != NULL)
	   {
	   ch->ChatPacket(CHAT_TYPE_INFO, "[LS;511]") // "Your logout has been cancelled.";
	   event_cancel(&ch->m_pkRecallEvent);
	   return;
	   }
	// END_OF_RECALL_DELAY */

	if (ch->m_pkTimedEvent)
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;511]"/* "Your logout has been cancelled." */);
		event_cancel (&ch->m_pkTimedEvent);
		return;
	}

	switch (subcmd)
	{
		case SCMD_LOGOUT:
			ch->ChatPacket (CHAT_TYPE_INFO, "[LS;512]"/* "Back to login window. Please wait." */);
			break;

		case SCMD_QUIT:
			ch->ChatPacket (CHAT_TYPE_INFO, "[LS;513]"/* "You have been disconnected from the server. Please wait." */);
			break;

		case SCMD_PHASE_SELECT:
			ch->ChatPacket (CHAT_TYPE_INFO, "[LS;515]"/* "You are changing character. Please wait." */);
			break;
	}

	int nExitLimitTime = 10;

	if (ch->IsHack (false, true, nExitLimitTime) &&
	false == CThreeWayWar::instance().IsSungZiMapIndex (ch->GetMapIndex()) &&
	(!ch->GetWarMap() || ch->GetWarMap()->GetType() == GUILD_WAR_TYPE_FLAG))
	{
		return;
	}

	switch (subcmd)
	{
		case SCMD_LOGOUT:
		case SCMD_QUIT:
		case SCMD_PHASE_SELECT:
		{
			TimedEventInfo* info = AllocEventInfo<TimedEventInfo>();

			{
				if (ch->IsPosition (POS_FIGHTING))
				{
					info->left_second = 10;
				}
				else
				{
					info->left_second = 3;
				}
			}

			info->ch		= ch;
			info->subcmd		= subcmd;
			strlcpy (info->szReason, argument, sizeof (info->szReason));

			ch->m_pkTimedEvent	= event_create (timed_event, info, 1);
		}
		break;
	}
}

ACMD (do_mount)
{
	/*
	   char			arg1[256];
	   struct action_mount_param	param;

	// �̹� Ÿ�� ������
	if (ch->GetMountingChr())
	{
	char arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || !*arg2)
	return;

	param.x		= atoi(arg1);
	param.y		= atoi(arg2);
	param.vid	= ch->GetMountingChr()->GetVID();
	param.is_unmount = true;

	float distance = DISTANCE_SQRT(param.x - (DWORD) ch->GetX(), param.y - (DWORD) ch->GetY());

	if (distance > 600.0f)
	{
	ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("�� �� ������ ���� ��������."));
	return;
	}

	action_enqueue(ch, ACTION_TYPE_MOUNT, &param, 0.0f, true);
	return;
	}

	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	return;

	LPCHARACTER tch = CHARACTER_MANAGER::instance().Find(atoi(arg1));

	if (!tch->IsNPC() || !tch->IsMountable())
	{
	ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("�ű⿡�� Ż �� �����."));
	return;
	}

	float distance = DISTANCE_SQRT(tch->GetX() - ch->GetX(), tch->GetY() - ch->GetY());

	if (distance > 600.0f)
	{
	ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("�� �� ������ ���� Ÿ����."));
	return;
	}

	param.vid		= tch->GetVID();
	param.is_unmount	= false;

	action_enqueue(ch, ACTION_TYPE_MOUNT, &param, 0.0f, true);
	 */
}

ACMD (do_fishing)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	if (!*arg1)
	{
		return;
	}

	ch->SetRotation (atof (arg1));
	ch->fishing();
}

ACMD (do_console)
{
	ch->ChatPacket (CHAT_TYPE_COMMAND, "ConsoleEnable");
}

ACMD (do_restart)
{
	if (false == ch->IsDead())
	{
		ch->ChatPacket (CHAT_TYPE_COMMAND, "CloseRestartWindow");
		ch->StartRecoveryEvent();
		return;
	}

	if (nullptr == ch->m_pkDeadEvent)
	{
		return;
	}

	int iTimeToDead = (event_time (ch->m_pkDeadEvent) / passes_per_sec);

	if (subcmd != SCMD_RESTART_TOWN && (!ch->GetWarMap() || ch->GetWarMap()->GetType() == GUILD_WAR_TYPE_FLAG))
	{
		if (!test_server)
		{
			if (ch->IsHack())
			{
				//���� ���ϰ�쿡�� üũ ���� �ʴ´�.
				if (false == CThreeWayWar::instance().IsSungZiMapIndex (ch->GetMapIndex()))
				{
					ch->ChatPacket (CHAT_TYPE_INFO, "[LS;516;%d]"/* "A new start is not possible at the moment. Please wait %d seconds." */, iTimeToDead - (180 - g_nPortalLimitTime));
					return;
				}
			}

			if (iTimeToDead > 170)
			{
				ch->ChatPacket (CHAT_TYPE_INFO, "[LS;516;%d]"/* "A new start is not possible at the moment. Please wait %d seconds." */, iTimeToDead - 170);
				return;
			}
		}
	}

	//PREVENT_HACK
	//DESC : â��, ��ȯ â �� ��Ż�� ����ϴ� ���׿� �̿�ɼ� �־
	//		��Ÿ���� �߰�
	if (subcmd == SCMD_RESTART_TOWN)
	{
		if (ch->IsHack())
		{
			//����, �����ʿ����� üũ ���� �ʴ´�.
			if ((!ch->GetWarMap() || ch->GetWarMap()->GetType() == GUILD_WAR_TYPE_FLAG) ||
			false == CThreeWayWar::instance().IsSungZiMapIndex (ch->GetMapIndex()))
			{
				ch->ChatPacket (CHAT_TYPE_INFO, "[LS;516;%d]"/* "A new start is not possible at the moment. Please wait %d seconds." */, iTimeToDead - (180 - g_nPortalLimitTime));
				return;
			}
		}

		if (iTimeToDead > 173)
		{
			ch->ChatPacket (CHAT_TYPE_INFO, "[LS;519;%d]"/* "You cannot restart in the city yet. Wait another %d seconds." */, iTimeToDead - 173);
			return;
		}
	}
	//END_PREVENT_HACK

	ch->ChatPacket (CHAT_TYPE_COMMAND, "CloseRestartWindow");

	ch->GetDesc()->SetPhase (PHASE_GAME);
	ch->SetPosition (POS_STANDING);
	ch->StartRecoveryEvent();

	//FORKED_LOAD
	//DESC: ��Ÿ� ������ ��Ȱ�� �Ұ�� ���� �Ա��� �ƴ� ��Ÿ� ������ ������������ �̵��ϰ� �ȴ�.
	if (1 == quest::CQuestManager::instance().GetEventFlag ("threeway_war"))
	{
		if (subcmd == SCMD_RESTART_TOWN || subcmd == SCMD_RESTART_HERE)
		{
			if (true == CThreeWayWar::instance().IsThreeWayWarMapIndex (ch->GetMapIndex()) &&
			false == CThreeWayWar::instance().IsSungZiMapIndex (ch->GetMapIndex()))
			{
				ch->WarpSet (EMPIRE_START_X (ch->GetEmpire()), EMPIRE_START_Y (ch->GetEmpire()));

				ch->ReviveInvisible (5);
				ch->PointChange (POINT_HP, ch->GetMaxHP() - ch->GetHP());
				ch->PointChange (POINT_SP, ch->GetMaxSP() - ch->GetSP());

				return;
			}

			//����
			if (true == CThreeWayWar::instance().IsSungZiMapIndex (ch->GetMapIndex()))
			{
				if (CThreeWayWar::instance().GetReviveTokenForPlayer (ch->GetPlayerID()) <= 0)
				{
					ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("�������� ��Ȱ ��ȸ�� ��� �Ҿ����ϴ�! ������ �̵��մϴ�!"));
					ch->WarpSet (EMPIRE_START_X (ch->GetEmpire()), EMPIRE_START_Y (ch->GetEmpire()));
				}
				else
				{
					ch->Show (ch->GetMapIndex(), GetSungziStartX (ch->GetEmpire()), GetSungziStartY (ch->GetEmpire()));
				}

				ch->PointChange (POINT_HP, ch->GetMaxHP() - ch->GetHP());
				ch->PointChange (POINT_SP, ch->GetMaxSP() - ch->GetSP());
				ch->ReviveInvisible (5);

				return;
			}
		}
	}
	//END_FORKED_LOAD

	if (ch->GetDungeon())
	{
		ch->GetDungeon()->UseRevive (ch);
	}

	if (ch->GetWarMap() && !ch->IsObserverMode())
	{
		CWarMap * pMap = ch->GetWarMap();
		DWORD dwGuildOpponent = pMap ? pMap->GetGuildOpponent (ch) : 0;

		if (dwGuildOpponent)
		{
			switch (subcmd)
			{
				case SCMD_RESTART_TOWN:
					sys_log (0, "do_restart: restart town");
					PIXEL_POSITION pos;

					if (CWarMapManager::instance().GetStartPosition (ch->GetMapIndex(), ch->GetGuild()->GetID() < dwGuildOpponent ? 0 : 1, pos))
					{
						ch->Show (ch->GetMapIndex(), pos.x, pos.y);
					}
					else
					{
						ch->ExitToSavedLocation();
					}

					ch->PointChange (POINT_HP, ch->GetMaxHP() - ch->GetHP());
					ch->PointChange (POINT_SP, ch->GetMaxSP() - ch->GetSP());
					ch->ReviveInvisible (5);
					break;

				case SCMD_RESTART_HERE:
					sys_log (0, "do_restart: restart here");
					ch->RestartAtSamePos();
					//ch->Show(ch->GetMapIndex(), ch->GetX(), ch->GetY());
					ch->PointChange (POINT_HP, ch->GetMaxHP() - ch->GetHP());
					ch->PointChange (POINT_SP, ch->GetMaxSP() - ch->GetSP());
					ch->ReviveInvisible (5);
					break;
			}

			return;
		}
	}

	switch (subcmd)
	{
		case SCMD_RESTART_TOWN:
			sys_log (0, "do_restart: restart town");
			PIXEL_POSITION pos;

			if (SECTREE_MANAGER::instance().GetRecallPositionByEmpire (ch->GetMapIndex(), ch->GetEmpire(), pos))
			{
				ch->WarpSet (pos.x, pos.y);
			}
			else
			{
				ch->WarpSet (EMPIRE_START_X (ch->GetEmpire()), EMPIRE_START_Y (ch->GetEmpire()));
			}

			ch->PointChange (POINT_HP, 50 - ch->GetHP());
			ch->DeathPenalty (1);
			break;

		case SCMD_RESTART_HERE:
			sys_log (0, "do_restart: restart here");
			ch->RestartAtSamePos();
			//ch->Show(ch->GetMapIndex(), ch->GetX(), ch->GetY());
			ch->PointChange (POINT_HP, 50 - ch->GetHP());
			ch->DeathPenalty (0);
			ch->ReviveInvisible (5);
			break;
	}
}

#define MAX_STAT 90

ACMD (do_stat_reset)
{
	ch->PointChange (POINT_STAT_RESET_COUNT, 12 - ch->GetPoint (POINT_STAT_RESET_COUNT));
}

ACMD (do_stat_minus)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	if (!*arg1)
	{
		return;
	}

	if (ch->IsPolymorphed())
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;521]"/* "You cannot change your status while you are transformed." */);
		return;
	}

	if (ch->GetPoint (POINT_STAT_RESET_COUNT) <= 0)
	{
		return;
	}

	if (!strcmp (arg1, "st"))
	{
		if (ch->GetRealPoint (POINT_ST) <= JobInitialPoints[ch->GetJob()].st)
		{
			return;
		}

		ch->SetRealPoint (POINT_ST, ch->GetRealPoint (POINT_ST) - 1);
		ch->SetPoint (POINT_ST, ch->GetPoint (POINT_ST) - 1);
		ch->ComputePoints();
		ch->PointChange (POINT_ST, 0);
	}
	else if (!strcmp (arg1, "dx"))
	{
		if (ch->GetRealPoint (POINT_DX) <= JobInitialPoints[ch->GetJob()].dx)
		{
			return;
		}

		ch->SetRealPoint (POINT_DX, ch->GetRealPoint (POINT_DX) - 1);
		ch->SetPoint (POINT_DX, ch->GetPoint (POINT_DX) - 1);
		ch->ComputePoints();
		ch->PointChange (POINT_DX, 0);
	}
	else if (!strcmp (arg1, "ht"))
	{
		if (ch->GetRealPoint (POINT_HT) <= JobInitialPoints[ch->GetJob()].ht)
		{
			return;
		}

		ch->SetRealPoint (POINT_HT, ch->GetRealPoint (POINT_HT) - 1);
		ch->SetPoint (POINT_HT, ch->GetPoint (POINT_HT) - 1);
		ch->ComputePoints();
		ch->PointChange (POINT_HT, 0);
		ch->PointChange (POINT_MAX_HP, 0);
	}
	else if (!strcmp (arg1, "iq"))
	{
		if (ch->GetRealPoint (POINT_IQ) <= JobInitialPoints[ch->GetJob()].iq)
		{
			return;
		}

		ch->SetRealPoint (POINT_IQ, ch->GetRealPoint (POINT_IQ) - 1);
		ch->SetPoint (POINT_IQ, ch->GetPoint (POINT_IQ) - 1);
		ch->ComputePoints();
		ch->PointChange (POINT_IQ, 0);
		ch->PointChange (POINT_MAX_SP, 0);
	}
	else
	{
		return;
	}

	ch->PointChange (POINT_STAT, +1);
	ch->PointChange (POINT_STAT_RESET_COUNT, -1);
	ch->ComputePoints();
}

ACMD (do_stat)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	if (!*arg1)
	{
		return;
	}

	if (ch->IsPolymorphed())
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;521]"/* "You cannot change your status while you are transformed." */);
		return;
	}

	if (ch->GetPoint (POINT_STAT) <= 0)
	{
		return;
	}

	BYTE idx = 0;

	if (!strcmp (arg1, "st"))
	{
		idx = POINT_ST;
	}
	else if (!strcmp (arg1, "dx"))
	{
		idx = POINT_DX;
	}
	else if (!strcmp (arg1, "ht"))
	{
		idx = POINT_HT;
	}
	else if (!strcmp (arg1, "iq"))
	{
		idx = POINT_IQ;
	}
	else
	{
		return;
	}

	if (ch->GetRealPoint (idx) >= MAX_STAT)
	{
		return;
	}

	ch->SetRealPoint (idx, ch->GetRealPoint (idx) + 1);
	ch->SetPoint (idx, ch->GetPoint (idx) + 1);
	ch->ComputePoints();
	ch->PointChange (idx, 0);

	if (idx == POINT_IQ)
	{
		ch->PointChange (POINT_MAX_HP, 0);
	}
	else if (idx == POINT_HT)
	{
		ch->PointChange (POINT_MAX_SP, 0);
	}

	ch->PointChange (POINT_STAT, -1);
	ch->ComputePoints();
}

ACMD (do_pvp)
{
	if (ch->GetArena() != nullptr || CArenaManager::instance().IsArenaMap (ch->GetMapIndex()) == true)
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;403]"/* "You cannot use this in the duel arena." */);
		return;
	}

	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	DWORD vid = 0;
	str_to_number (vid, arg1);
	LPCHARACTER pkVictim = CHARACTER_MANAGER::instance().Find (vid);

	if (!pkVictim)
	{
		return;
	}

	if (pkVictim->IsNPC())
	{
		return;
	}

	if (pkVictim->GetArena() != nullptr)
	{
		pkVictim->ChatPacket (CHAT_TYPE_INFO, "[LS;522]"/* "This player is currently fighting." */);
		return;
	}

	CPVPManager::instance().Insert (ch, pkVictim);
}

ACMD (do_guildskillup)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	if (!*arg1)
	{
		return;
	}

	if (!ch->GetGuild())
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;523]"/* "[Guild] It does not belong to the guild." */);
		return;
	}

	CGuild* g = ch->GetGuild();
	TGuildMember* gm = g->GetMember (ch->GetPlayerID());
	if (gm->grade == GUILD_LEADER_GRADE)
	{
		DWORD vnum = 0;
		str_to_number (vnum, arg1);
		g->SkillLevelUp (vnum);
	}
	else
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;524]"/* "[Guild] You do not have the authority to change the level of the guild skills." */);
	}
}

ACMD (do_skillup)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	if (!*arg1)
	{
		return;
	}

	DWORD vnum = 0;
	str_to_number (vnum, arg1);

	if (true == ch->CanUseSkill (vnum))
	{
		ch->SkillLevelUp (vnum);
	}
	else
	{
		switch (vnum)
		{
			case SKILL_HORSE_WILDATTACK:
			case SKILL_HORSE_CHARGE:
			case SKILL_HORSE_ESCAPE:
			case SKILL_HORSE_WILDATTACK_RANGE:

			case SKILL_7_A_ANTI_TANHWAN:
			case SKILL_7_B_ANTI_AMSEOP:
			case SKILL_7_C_ANTI_SWAERYUNG:
			case SKILL_7_D_ANTI_YONGBI:

			case SKILL_8_A_ANTI_GIGONGCHAM:
			case SKILL_8_B_ANTI_YEONSA:
			case SKILL_8_C_ANTI_MAHWAN:
			case SKILL_8_D_ANTI_BYEURAK:

			case SKILL_ADD_HP:
			case SKILL_RESIST_PENETRATE:
				ch->SkillLevelUp (vnum);
				break;
		}
	}
}

//
// @version	05/06/20 Bang2ni - Ŀ�ǵ� ó�� Delegate to CHARACTER class
//
ACMD (do_safebox_close)
{
	ch->CloseSafebox();
}

//
// @version	05/06/20 Bang2ni - Ŀ�ǵ� ó�� Delegate to CHARACTER class
//
ACMD (do_safebox_password)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));
	ch->ReqSafeboxLoad (arg1);
}

ACMD (do_safebox_change_password)
{
	char arg1[256];
	char arg2[256];

	two_arguments (argument, arg1, sizeof (arg1), arg2, sizeof (arg2));

	if (!*arg1 || strlen (arg1) > 6)
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;526]"/* "[Storeroom] You have entered an incorrect password." */);
		return;
	}

	if (!*arg2 || strlen (arg2) > 6)
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;526]"/* "[Storeroom] You have entered an incorrect password." */);
		return;
	}

	if (LC_IsBrazil() == true)
	{
		for (int i = 0; i < 6; ++i)
		{
			if (arg2[i] == '\0')
			{
				break;
			}

			if (isalpha (arg2[i]) == false)
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("[Storeroom] Password must contain only English letters."));
				return;
			}
		}
	}

	TSafeboxChangePasswordPacket p;

	p.dwID = ch->GetDesc()->GetAccountTable().id;
	strlcpy (p.szOldPassword, arg1, sizeof (p.szOldPassword));
	strlcpy (p.szNewPassword, arg2, sizeof (p.szNewPassword));

	db_clientdesc->DBPacket (HEADER_GD_SAFEBOX_CHANGE_PASSWORD, ch->GetDesc()->GetHandle(), &p, sizeof (p));
}

ACMD (do_mall_password)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	if (!*arg1 || strlen (arg1) > 6)
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;526]"/* "[Storeroom] You have entered an incorrect password." */);
		return;
	}

	int iPulse = thecore_pulse();

	if (ch->GetMall())
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;527]"/* "[Storeroom] The Storeroom is already open." */);
		return;
	}

	if (iPulse - ch->GetMallLoadTime() < passes_per_sec * 10) // 10�ʿ� �ѹ��� ��û ����
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;528]"/* "[Storeroom] You have to wait 10 seconds before you can open the Storeroom again." */);
		return;
	}

	ch->SetMallLoadTime (iPulse);

	TSafeboxLoadPacket p;
	p.dwID = ch->GetDesc()->GetAccountTable().id;
	strlcpy (p.szLogin, ch->GetDesc()->GetAccountTable().login, sizeof (p.szLogin));
	strlcpy (p.szPassword, arg1, sizeof (p.szPassword));

	db_clientdesc->DBPacket (HEADER_GD_MALL_LOAD, ch->GetDesc()->GetHandle(), &p, sizeof (p));
}

ACMD (do_mall_close)
{
	if (ch->GetMall())
	{
		ch->SetMallLoadTime (thecore_pulse());
		ch->CloseMall();
		ch->Save();
	}
}

ACMD (do_ungroup)
{
	if (!ch->GetParty())
	{
		return;
	}

	if (!CPartyManager::instance().IsEnablePCParty())
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;530]"/* "[Group] The server cannot execute this group request." */);
		return;
	}

	if (ch->GetDungeon())
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;531]"/* "[Group] You cannot leave a group while you are in a dungeon." */);
		return;
	}

	LPPARTY pParty = ch->GetParty();

	if (pParty->GetMemberCount() == 2)
	{
		// party disband
		CPartyManager::instance().DeleteParty (pParty);
	}
	else
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;532]"/* "[Group] You have left the group." */);
		//pParty->SendPartyRemoveOneToAll(ch);
		pParty->Quit (ch->GetPlayerID());
		//pParty->SendPartyRemoveAllToOne(ch);
	}
}

ACMD (do_close_shop)
{
	if (ch->GetMyShop())
	{
		ch->CloseMyShop();
		return;
	}
}

ACMD (do_set_walk_mode)
{
	ch->SetNowWalking (true);
	ch->SetWalking (true);
}

ACMD (do_set_run_mode)
{
	ch->SetNowWalking (false);
	ch->SetWalking (false);
}

ACMD (do_war)
{
	//�� ��� ������ ������
	CGuild * g = ch->GetGuild();

	if (!g)
	{
		return;
	}

	//���������� üũ�ѹ�!
	if (g->UnderAnyWar())
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;533]"/* "[Guild] Your guild is already participating in another war." */);
		return;
	}

	//�Ķ���͸� �ι�� ������
	char arg1[256], arg2[256];
	int type = GUILD_WAR_TYPE_FIELD;
	two_arguments (argument, arg1, sizeof (arg1), arg2, sizeof (arg2));

	if (!*arg1)
	{
		return;
	}

	if (*arg2)
	{
		str_to_number (type, arg2);

		if (type >= GUILD_WAR_TYPE_MAX_NUM)
		{
			type = GUILD_WAR_TYPE_FIELD;
		}
	}

	//����� ������ ���̵� ���µ�
	DWORD gm_pid = g->GetMasterPID();

	//���������� üũ(������ ����常�� ����)
	if (gm_pid != ch->GetPlayerID())
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;534]"/* "[Guild] No one is entitled to a guild war." */);
		return;
	}

	//��� ��带 ������
	CGuild * opp_g = CGuildManager::instance().FindGuildByName (arg1);

	if (!opp_g)
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;535]"/* "[Guild] No guild with this name exists." */);
		return;
	}

	//�������� ���� üũ
	switch (g->GetGuildWarState (opp_g->GetID()))
	{
		case GUILD_WAR_NONE:
		{
			if (opp_g->UnderAnyWar())
			{
				ch->ChatPacket (CHAT_TYPE_INFO, "[LS;541]"/* "[Guild] This guild is already participating in another war." */);
				return;
			}

			int iWarPrice = KOR_aGuildWarInfo[type].iWarPrice;

			if (g->GetGuildMoney() < iWarPrice)
			{
				ch->ChatPacket (CHAT_TYPE_INFO, "[LS;538]"/* "[Guild] Not enough Yang to participate in a guild war." */);
				return;
			}

			if (opp_g->GetGuildMoney() < iWarPrice)
			{
				ch->ChatPacket (CHAT_TYPE_INFO, "[LS;539]"/* "[Guild] The guild does not have enough Yang to participate in a guild war." */);
				return;
			}
		}
		break;

		case GUILD_WAR_SEND_DECLARE:
		{
			ch->ChatPacket (CHAT_TYPE_INFO, "[LS;537]"/* "[Guild] This guild is already participating in a war." */);
			return;
		}
		break;

		case GUILD_WAR_RECV_DECLARE:
		{
			if (opp_g->UnderAnyWar())
			{
				ch->ChatPacket (CHAT_TYPE_INFO, "[LS;541]"/* "[Guild] This guild is already participating in another war." */);
				g->RequestRefuseWar (opp_g->GetID());
				return;
			}
		}
		break;

		case GUILD_WAR_RESERVE:
		{
			ch->ChatPacket (CHAT_TYPE_INFO, "[LS;540]"/* "[Guild] This Guild is already scheduled for another war." */);
			return;
		}
		break;

		case GUILD_WAR_END:
			return;

		default:
			ch->ChatPacket (CHAT_TYPE_INFO, "[LS;1050]"/* "[Guild] This guild is taking part in a battle at the moment." */);
			g->RequestRefuseWar (opp_g->GetID());
			return;
	}

	if (!g->CanStartWar (type))
	{
		// ������� �� �� �ִ� ������ ���������ʴ´�.
		if (g->GetLadderPoint() == 0)
		{
			ch->ChatPacket (CHAT_TYPE_INFO, "[LS;1308]"/* "[Guild] The guild level is too low." */);
			sys_log (0, "GuildWar.StartError.NEED_LADDER_POINT");
		}
		else if (g->GetMemberCount() < GUILD_WAR_MIN_MEMBER_COUNT)
		{
			ch->ChatPacket (CHAT_TYPE_INFO, "[LS;543;%d]"/* "[Guild] A minimum of %d players are needed to participate in a guild war." */, GUILD_WAR_MIN_MEMBER_COUNT);
			sys_log (0, "GuildWar.StartError.NEED_MINIMUM_MEMBER[%d]", GUILD_WAR_MIN_MEMBER_COUNT);
		}
		else
		{
			sys_log (0, "GuildWar.StartError.UNKNOWN_ERROR");
		}
		return;
	}

	// �ʵ��� üũ�� �ϰ� ������ üũ�� ������ �³��Ҷ� �Ѵ�.
	if (!opp_g->CanStartWar (GUILD_WAR_TYPE_FIELD))
	{
		if (opp_g->GetLadderPoint() == 0)
		{
			ch->ChatPacket (CHAT_TYPE_INFO, "[LS;544]"/* "[Guild] The guild does not have enough points to participate in a guild war." */);
		}
		else if (opp_g->GetMemberCount() < GUILD_WAR_MIN_MEMBER_COUNT)
		{
			ch->ChatPacket (CHAT_TYPE_INFO, "[LS;545]"/* "[Guild] The guild does not have enough members to participate in a guild war." */);
		}
		return;
	}

	do
	{
		if (g->GetMasterCharacter() != nullptr)
		{
			break;
		}

		CCI *pCCI = P2P_MANAGER::instance().FindByPID (g->GetMasterPID());

		if (pCCI != nullptr)
		{
			break;
		}

		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;1048]"/* "[Guild] The enemy's guild leader is offline." */);
		g->RequestRefuseWar (opp_g->GetID());
		return;

	}
	while (false);

	do
	{
		if (opp_g->GetMasterCharacter() != nullptr)
		{
			break;
		}

		CCI *pCCI = P2P_MANAGER::instance().FindByPID (opp_g->GetMasterPID());

		if (pCCI != nullptr)
		{
			break;
		}

		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;1048]"/* "[Guild] The enemy's guild leader is offline." */);
		g->RequestRefuseWar (opp_g->GetID());
		return;

	}
	while (false);

	g->RequestDeclareWar (opp_g->GetID(), type);
}

ACMD (do_nowar)
{
	CGuild* g = ch->GetGuild();
	if (!g)
	{
		return;
	}

	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	if (!*arg1)
	{
		return;
	}

	DWORD gm_pid = g->GetMasterPID();

	if (gm_pid != ch->GetPlayerID())
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;534]"/* "[Guild] No one is entitled to a guild war." */);
		return;
	}

	CGuild* opp_g = CGuildManager::instance().FindGuildByName (arg1);

	if (!opp_g)
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;535]"/* "[Guild] No guild with this name exists." */);
		return;
	}

	g->RequestRefuseWar (opp_g->GetID());
}

ACMD (do_detaillog)
{
	ch->DetailLog();
}

ACMD (do_monsterlog)
{
	ch->ToggleMonsterLog();
}

ACMD (do_pkmode)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	if (!*arg1)
	{
		return;
	}

	BYTE mode = 0;
	str_to_number (mode, arg1);

	if (mode == PK_MODE_PROTECT)
	{
		return;
	}

	if (ch->GetLevel() < PK_PROTECT_LEVEL && mode != 0)
	{
		return;
	}

	ch->SetPKMode (mode);
}

ACMD (do_messenger_auth)
{
	if (ch->GetArena())
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;403]"/* "You cannot use this in the duel arena." */);
		return;
	}

	char arg1[256], arg2[256];
	two_arguments (argument, arg1, sizeof (arg1), arg2, sizeof (arg2));

	if (!*arg1 || !*arg2)
	{
		return;
	}

	char answer = LOWER (*arg1);

	if (answer != 'y')
	{
		LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC (arg2);

		if (tch)
		{
			tch->ChatPacket (CHAT_TYPE_INFO, "[LS;548;%s]"/* "%s declined the invitation." */, ch->GetName());
		}
	}

	MessengerManager::instance().AuthToAdd (ch->GetName(), arg2, answer == 'y' ? false : true); // DENY
}

ACMD (do_setblockmode)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	if (*arg1)
	{
		BYTE flag = 0;
		str_to_number (flag, arg1);
		ch->SetBlockMode (flag);
	}
}

ACMD (do_unmount)
{
	if (true == ch->UnEquipSpecialRideUniqueItem())
	{
		ch->RemoveAffect (AFFECT_MOUNT);
		ch->RemoveAffect (AFFECT_MOUNT_BONUS);

		if (ch->IsHorseRiding())
		{
			ch->StopRiding();
		}
	}
	else
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("�κ��丮�� �� ���� ���� �� �����ϴ�."));
	}

}

ACMD (do_observer_exit)
{
	if (ch->IsObserverMode())
	{
		if (ch->GetWarMap())
		{
			ch->SetWarMap (nullptr);
		}

		if (ch->GetArena() != nullptr || ch->GetArenaObserverMode() == true)
		{
			ch->SetArenaObserverMode (false);

			if (ch->GetArena() != nullptr)
			{
				ch->GetArena()->RemoveObserver (ch->GetPlayerID());
			}

			ch->SetArena (nullptr);
			ch->WarpSet (ARENA_RETURN_POINT_X (ch->GetEmpire()), ARENA_RETURN_POINT_Y (ch->GetEmpire()));
		}
		else
		{
			ch->ExitToSavedLocation();
		}
		ch->SetObserverMode (false);
	}
}

ACMD (do_view_equip)
{
	if (ch->GetGMLevel() <= GM_PLAYER)
	{
		return;
	}

	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	if (*arg1)
	{
		DWORD vid = 0;
		str_to_number (vid, arg1);
		LPCHARACTER tch = CHARACTER_MANAGER::instance().Find (vid);

		if (!tch)
		{
			return;
		}

		if (!tch->IsPC())
		{
			return;
		}
		/*
		   int iSPCost = ch->GetMaxSP() / 3;

		   if (ch->GetSP() < iSPCost)
		   {
		   ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("���ŷ��� �����Ͽ� �ٸ� ����� ��� �� �� �����ϴ�."));
		   return;
		   }
		   ch->PointChange(POINT_SP, -iSPCost);
		 */
		tch->SendEquipment (ch);
	}
}

ACMD (do_party_request)
{
	if (ch->GetArena())
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;403]"/* "You cannot use this in the duel arena." */);
		return;
	}

	if (ch->GetParty())
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "[LS;549]"/* "You cannot accept the invitation because you are already in the group." */);
		return;
	}

	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	if (!*arg1)
	{
		return;
	}

	DWORD vid = 0;
	str_to_number (vid, arg1);
	LPCHARACTER tch = CHARACTER_MANAGER::instance().Find (vid);

	if (tch)
		if (!ch->RequestToParty (tch))
		{
			ch->ChatPacket (CHAT_TYPE_COMMAND, "PartyRequestDenied");
		}
}

ACMD (do_party_request_accept)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	if (!*arg1)
	{
		return;
	}

	DWORD vid = 0;
	str_to_number (vid, arg1);
	LPCHARACTER tch = CHARACTER_MANAGER::instance().Find (vid);

	if (tch)
	{
		ch->AcceptToParty (tch);
	}
}

ACMD (do_party_request_deny)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	if (!*arg1)
	{
		return;
	}

	DWORD vid = 0;
	str_to_number (vid, arg1);
	LPCHARACTER tch = CHARACTER_MANAGER::instance().Find (vid);

	if (tch)
	{
		ch->DenyToParty (tch);
	}
}

ACMD (do_monarch_warpto)
{
	if (true == LC_IsYMIR() || true == LC_IsKorea())
	{
		return;
	}

	if (!CMonarch::instance().IsMonarch (ch->GetPlayerID(), ch->GetEmpire()))
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("���ָ��� ��� ������ ����Դϴ�"));
		return;
	}

	//���� ��Ÿ�� �˻�
	if (!ch->IsMCOK (CHARACTER::MI_WARP))
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("%d �ʰ� ��Ÿ���� �������Դϴ�."), ch->GetMCLTime (CHARACTER::MI_WARP));
		return;
	}

	//���� �� ��ȯ ���
	const int WarpPrice = 10000;

	//���� ���� �˻�
	if (!CMonarch::instance().IsMoneyOk (WarpPrice, ch->GetEmpire()))
	{
		int NationMoney = CMonarch::instance().GetMoney (ch->GetEmpire());
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("���� ���� �����մϴ�. ���� : %u �ʿ�ݾ� : %u"), NationMoney, WarpPrice);
		return;
	}

	int x = 0, y = 0;
	char arg1[256];

	one_argument (argument, arg1, sizeof (arg1));

	if (!*arg1)
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("����: warpto <character name>"));
		return;
	}

	LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC (arg1);

	if (!tch)
	{
		CCI * pkCCI = P2P_MANAGER::instance().Find (arg1);

		if (pkCCI)
		{
			if (pkCCI->bEmpire != ch->GetEmpire())
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("Ÿ���� �������Դ� �̵��Ҽ� �����ϴ�"));
				return;
			}

			if (pkCCI->bChannel != g_bChannel)
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("�ش� ������ %d ä�ο� �ֽ��ϴ�. (���� ä�� %d)"), pkCCI->bChannel, g_bChannel);
				return;
			}
			if (!IsMonarchWarpZone (pkCCI->lMapIndex))
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("�ش� �������� �̵��� �� �����ϴ�."));
				return;
			}

			PIXEL_POSITION pos;

			if (!SECTREE_MANAGER::instance().GetCenterPositionOfMap (pkCCI->lMapIndex, pos))
			{
				ch->ChatPacket (CHAT_TYPE_INFO, "Cannot find map (index %d)", pkCCI->lMapIndex);
			}
			else
			{
				//ch->ChatPacket(CHAT_TYPE_INFO, "You warp to (%d, %d)", pos.x, pos.y);
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("%s ���Է� �̵��մϴ�"), arg1);
				ch->WarpSet (pos.x, pos.y);

				//���� �� �谨
				CMonarch::instance().SendtoDBDecMoney (WarpPrice, ch->GetEmpire(), ch);

				//��Ÿ�� �ʱ�ȭ
				ch->SetMC (CHARACTER::MI_WARP);
			}
		}
		else if (nullptr == CHARACTER_MANAGER::instance().FindPC (arg1))
		{
			ch->ChatPacket (CHAT_TYPE_INFO, "There is no one by that name");
		}

		return;
	}
	else
	{
		if (tch->GetEmpire() != ch->GetEmpire())
		{
			ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("Ÿ���� �������Դ� �̵��Ҽ� �����ϴ�"));
			return;
		}
		if (!IsMonarchWarpZone (tch->GetMapIndex()))
		{
			ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("�ش� �������� �̵��� �� �����ϴ�."));
			return;
		}
		x = tch->GetX();
		y = tch->GetY();
	}

	ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("%s ���Է� �̵��մϴ�"), arg1);
	ch->WarpSet (x, y);
	ch->Stop();

	//���� �� �谨
	CMonarch::instance().SendtoDBDecMoney (WarpPrice, ch->GetEmpire(), ch);

	//��Ÿ�� �ʱ�ȭ
	ch->SetMC (CHARACTER::MI_WARP);
}

ACMD (do_monarch_transfer)
{
	if (true == LC_IsYMIR() || true == LC_IsKorea())
	{
		return;
	}

	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	if (!*arg1)
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("����: transfer <name>"));
		return;
	}

	if (!CMonarch::instance().IsMonarch (ch->GetPlayerID(), ch->GetEmpire()))
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("���ָ��� ��� ������ ����Դϴ�"));
		return;
	}

	//���� ��Ÿ�� �˻�
	if (!ch->IsMCOK (CHARACTER::MI_TRANSFER))
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("%d �ʰ� ��Ÿ���� �������Դϴ�."), ch->GetMCLTime (CHARACTER::MI_TRANSFER));
		return;
	}

	//���� ���� ���
	const int WarpPrice = 10000;

	//���� ���� �˻�
	if (!CMonarch::instance().IsMoneyOk (WarpPrice, ch->GetEmpire()))
	{
		int NationMoney = CMonarch::instance().GetMoney (ch->GetEmpire());
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("���� ���� �����մϴ�. ���� : %u �ʿ�ݾ� : %u"), NationMoney, WarpPrice);
		return;
	}


	LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC (arg1);

	if (!tch)
	{
		if (CCI * pkCCI = P2P_MANAGER::instance().Find (arg1))
		{
			if (pkCCI->bEmpire != ch->GetEmpire())
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("�ٸ� ���� ������ ��ȯ�� �� �����ϴ�."));
				return;
			}
			if (pkCCI->bChannel != g_bChannel)
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("%s ���� %d ä�ο� ���� �� �Դϴ�. (���� ä��: %d)"), arg1, pkCCI->bChannel, g_bChannel);
				return;
			}
			if (!IsMonarchWarpZone (pkCCI->lMapIndex))
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("�ش� �������� �̵��� �� �����ϴ�."));
				return;
			}
			if (!IsMonarchWarpZone (ch->GetMapIndex()))
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("�ش� �������� ��ȯ�� �� �����ϴ�."));
				return;
			}

			TPacketGGTransfer pgg;

			pgg.bHeader = HEADER_GG_TRANSFER;
			strlcpy (pgg.szName, arg1, sizeof (pgg.szName));
			pgg.lX = ch->GetX();
			pgg.lY = ch->GetY();

			P2P_MANAGER::instance().Send (&pgg, sizeof (TPacketGGTransfer));
			ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("%s ���� ��ȯ�Ͽ����ϴ�."), arg1);

			//���� �� �谨
			CMonarch::instance().SendtoDBDecMoney (WarpPrice, ch->GetEmpire(), ch);
			//��Ÿ�� �ʱ�ȭ
			ch->SetMC (CHARACTER::MI_TRANSFER);
		}
		else
		{
			ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("�Է��Ͻ� �̸��� ���� ����ڰ� �����ϴ�."));
		}

		return;
	}


	if (ch == tch)
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("�ڽ��� ��ȯ�� �� �����ϴ�."));
		return;
	}

	if (tch->GetEmpire() != ch->GetEmpire())
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("�ٸ� ���� ������ ��ȯ�� �� �����ϴ�."));
		return;
	}
	if (!IsMonarchWarpZone (tch->GetMapIndex()))
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("�ش� �������� �̵��� �� �����ϴ�."));
		return;
	}
	if (!IsMonarchWarpZone (ch->GetMapIndex()))
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("�ش� �������� ��ȯ�� �� �����ϴ�."));
		return;
	}

	//tch->Show(ch->GetMapIndex(), ch->GetX(), ch->GetY(), ch->GetZ());
	tch->WarpSet (ch->GetX(), ch->GetY(), ch->GetMapIndex());

	//���� �� �谨
	CMonarch::instance().SendtoDBDecMoney (WarpPrice, ch->GetEmpire(), ch);
	//��Ÿ�� �ʱ�ȭ
	ch->SetMC (CHARACTER::MI_TRANSFER);
}

ACMD (do_monarch_info)
{
	if (CMonarch::instance().IsMonarch (ch->GetPlayerID(), ch->GetEmpire()))
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("���� ���� ����"));
		TMonarchInfo * p = CMonarch::instance().GetMonarch();
		for (int n = 1; n < 4; ++n)
		{
			if (n == ch->GetEmpire())
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("[%s����] : %s  �����ݾ� %lld "), EMPIRE_NAME (n), p->name[n], p->money[n]);
			}
			else
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("[%s����] : %s  "), EMPIRE_NAME (n), p->name[n]);
			}

		}
	}
	else
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("���� ����"));
		TMonarchInfo * p = CMonarch::instance().GetMonarch();
		for (int n = 1; n < 4; ++n)
		{
			ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("[%s����] : %s  "), EMPIRE_NAME (n), p->name[n]);

		}
	}

}

ACMD (do_elect)
{
	db_clientdesc->DBPacketHeader (HEADER_GD_COME_TO_VOTE, ch->GetDesc()->GetHandle(), 0);
}

// LUA_ADD_GOTO_INFO
struct GotoInfo
{
	std::string 	st_name;

	BYTE 	empire;
	int 	mapIndex;
	DWORD 	x, y;

	GotoInfo()
	{
		st_name 	= "";
		empire 		= 0;
		mapIndex 	= 0;

		x = 0;
		y = 0;
	}

	GotoInfo (const GotoInfo& c_src)
	{
		__copy__ (c_src);
	}

	void operator = (const GotoInfo& c_src)
	{
		__copy__ (c_src);
	}

	void __copy__ (const GotoInfo& c_src)
	{
		st_name 	= c_src.st_name;
		empire 		= c_src.empire;
		mapIndex 	= c_src.mapIndex;

		x = c_src.x;
		y = c_src.y;
	}
};

extern void BroadcastNotice (const char* c_pszBuf);

ACMD (do_monarch_tax)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	if (!*arg1)
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "Usage: monarch_tax <1-50>");
		return;
	}

	// ���� �˻�
	if (!ch->IsMonarch())
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("���ָ��� ����Ҽ� �ִ� ����Դϴ�"));
		return;
	}

	// ���ݼ���
	int tax = 0;
	str_to_number (tax,  arg1);

	if (tax < 1 || tax > 50)
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("1-50 ������ ��ġ�� �������ּ���"));
	}

	quest::CQuestManager::instance().SetEventFlag ("trade_tax", tax);

	// ���ֿ��� �޼��� �ϳ�
	ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("������ %d %�� �����Ǿ����ϴ�"));

	// ����
	char szMsg[1024];

	snprintf (szMsg, sizeof (szMsg), "������ ������ ������ %d %% �� ����Ǿ����ϴ�", tax);
	BroadcastNotice (szMsg);

	snprintf (szMsg, sizeof (szMsg), "�����δ� �ŷ� �ݾ��� %d %% �� ����� ���Ե˴ϴ�.", tax);
	BroadcastNotice (szMsg);

	// ��Ÿ�� �ʱ�ȭ
	ch->SetMC (CHARACTER::MI_TAX);
}

static const DWORD cs_dwMonarchMobVnums[] =
{
	191, //	��߽�
	192, //	����
	193, //	����
	194, //	ȣ��
	391, //	����
	392, //	����
	393, //	����
	394, //	����
	491, //	��ȯ
	492, //	����
	493, //	����
	494, //	����
	591, //	����ܴ���
	691, //	���� ����
	791, //	�б�����
	1304, // ��������
	1901, // ����ȣ
	2091, // ���հŹ�
	2191, // �Ŵ�縷�ź�
	2206, // ȭ����i
	0,
};

ACMD (do_monarch_mob)
{
	char arg1[256];
	LPCHARACTER	tch;

	one_argument (argument, arg1, sizeof (arg1));

	if (!ch->IsMonarch())
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("���ָ��� ����Ҽ� �ִ� ����Դϴ�"));
		return;
	}

	if (!*arg1)
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "Usage: mmob <mob name>");
		return;
	}

	BYTE pcEmpire = ch->GetEmpire();
	BYTE mapEmpire = SECTREE_MANAGER::instance().GetEmpireFromMapIndex (ch->GetMapIndex());

	if (LC_IsYMIR() == true || LC_IsKorea() == true)
	{
		if (mapEmpire != pcEmpire && mapEmpire != 0)
		{
			ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("�ڱ� ���信���� ����� �� �ִ� ����Դϴ�"));
			return;
		}
	}

	// ���� �� ��ȯ ���
	const int SummonPrice = 5000000;

	// ���� ��Ÿ�� �˻�
	if (!ch->IsMCOK (CHARACTER::MI_SUMMON))
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("%d �ʰ� ��Ÿ���� �������Դϴ�."), ch->GetMCLTime (CHARACTER::MI_SUMMON));
		return;
	}

	// ���� ���� �˻�
	if (!CMonarch::instance().IsMoneyOk (SummonPrice, ch->GetEmpire()))
	{
		int NationMoney = CMonarch::instance().GetMoney (ch->GetEmpire());
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("���� ���� �����մϴ�. ���� : %u �ʿ�ݾ� : %u"), NationMoney, SummonPrice);
		return;
	}

	const CMob * pkMob;
	DWORD vnum = 0;

	if (isdigit (*arg1))
	{
		str_to_number (vnum, arg1);

		if ((pkMob = CMobManager::instance().Get (vnum)) == NULL)
		{
			vnum = 0;
		}
	}
	else
	{
		pkMob = CMobManager::Instance().Get (arg1, true);

		if (pkMob)
		{
			vnum = pkMob->m_table.dwVnum;
		}
	}

	DWORD count;

	// ��ȯ ���� �� �˻�
	for (count = 0; cs_dwMonarchMobVnums[count] != 0; ++count)
		if (cs_dwMonarchMobVnums[count] == vnum)
		{
			break;
		}

	if (0 == cs_dwMonarchMobVnums[count])
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("��ȯ�Ҽ� ���� ���� �Դϴ�. ��ȯ������ ���ʹ� Ȩ�������� �����ϼ���"));
		return;
	}

	tch = CHARACTER_MANAGER::instance().SpawnMobRange (vnum,
													   ch->GetMapIndex(),
													   ch->GetX() - number (200, 750),
													   ch->GetY() - number (200, 750),
													   ch->GetX() + number (200, 750),
													   ch->GetY() + number (200, 750),
													   true,
													   pkMob->m_table.bType == CHAR_TYPE_STONE,
													   true);

	if (tch)
	{
		// ���� �� �谨
		CMonarch::instance().SendtoDBDecMoney (SummonPrice, ch->GetEmpire(), ch);

		// ��Ÿ�� �ʱ�ȭ
		ch->SetMC (CHARACTER::MI_SUMMON);
	}
}

static const char* FN_point_string (int apply_number)
{
	switch (apply_number)
	{
		case POINT_MAX_HP:
			return "[LS;568;%d]"/* "Hit Points +%d" */;
		case POINT_MAX_SP:
			return "[LS;569;%d]"/* "Spell Points +%d" */;
		case POINT_HT:
			return "[LS;571;%d]"/* "Endurance +%d" */;
		case POINT_IQ:
			return "[LS;572;%d]"/* "Intelligence +%d" */;
		case POINT_ST:
			return "[LS;573;%d]"/* "Strength +%d" */;
		case POINT_DX:
			return "[LS;574;%d]"/* "Dexterity +%d" */;
		case POINT_ATT_SPEED:
			return "[LS;575;%d]"/* "Attack Speed +%d" */;
		case POINT_MOV_SPEED:
			return "[LS;576;%d]"/* "Movement Speed %d" */;
		case POINT_CASTING_SPEED:
			return "[LS;577;%d]"/* "Cooldown Time -%d" */;
		case POINT_HP_REGEN:
			return "[LS;578;%d]"/* "Energy Recovery +%d" */;
		case POINT_SP_REGEN:
			return "[LS;579;%d]"/* "Spell Point Recovery +%d" */;
		case POINT_POISON_PCT:
			return "[LS;580;%d]"/* "Poison Attack %d" */;
		case POINT_STUN_PCT:
			return "[LS;582;%d]"/* "Star +%d" */;
		case POINT_SLOW_PCT:
			return "[LS;583;%d]"/* "Speed Reduction +%d" */;
		case POINT_CRITICAL_PCT:
			return "[LS;584;%d]"/* "Critical Attack with a chance of %d%%" */;
		case POINT_RESIST_CRITICAL:
			return LC_TEXT ("����� ġ��Ÿ Ȯ�� %d%% ����");
		case POINT_PENETRATE_PCT:
			return "[LS;585;%d]"/* "Chance of a Speared Attack of %d%%" */;
		case POINT_RESIST_PENETRATE:
			return LC_TEXT ("����� ���� ���� Ȯ�� %d%% ����");
		case POINT_ATTBONUS_HUMAN:
			return "[LS;586;%d]"/* "Player's Attack Power against Monsters +%d%%" */;
		case POINT_ATTBONUS_ANIMAL:
			return "[LS;587;%d]"/* "Horse's Attack Power against Monsters +%d%%" */;
		case POINT_ATTBONUS_ORC:
			return "[LS;588;%d]"/* "Attack Boost against Wonggui + %d%%" */;
		case POINT_ATTBONUS_MILGYO:
			return "[LS;589;%d]"/* "Attack Boost against Milgyo + %d%%" */;
		case POINT_ATTBONUS_UNDEAD:
			return "[LS;590;%d]"/* "Attack boost against zombies + %d%%" */;
		case POINT_ATTBONUS_DEVIL:
			return "[LS;591;%d]"/* "Attack boost against devils + %d%%" */;
		case POINT_STEAL_HP:
			return "[LS;593;%d]"/* "Absorbing of Energy %d%% while attacking." */;
		case POINT_STEAL_SP:
			return "[LS;594;%d]"/* "Absorption of Spell Points (SP) %d%% while attacking." */;
		case POINT_MANA_BURN_PCT:
			return "[LS;595;%d]"/* "With a chance of %d%% Spell Points (SP) will be taken from the enemy." */;
		case POINT_DAMAGE_SP_RECOVER:
			return "[LS;596;%d]"/* "Absorbing of Spell Points (SP) with a chance of %d%%" */;
		case POINT_BLOCK:
			return "[LS;597;%d]"/* "%d%% Chance of blocking a close-combat attack" */;
		case POINT_DODGE:
			return "[LS;598;%d]"/* "%d%% Chance of blocking a long range attack" */;
		case POINT_RESIST_SWORD:
			return "[LS;599;%d]"/* "One-Handed Sword defence %d%%" */;
		case POINT_RESIST_TWOHAND:
			return "[LS;600;%d]"/* "Two-Handed Sword Defence %d%%" */;
		case POINT_RESIST_DAGGER:
			return "[LS;601;%d]"/* "Two-Handed Sword Defence %d%%" */;
		case POINT_RESIST_BELL:
			return "[LS;602;%d]"/* "Bell Defence %d%%" */;
		case POINT_RESIST_FAN:
			return "[LS;604;%d]"/* "Fan Defence %d%%" */;
		case POINT_RESIST_BOW:
			return "[LS;605;%d]"/* "Distant Attack Resistance %d%%" */;
		case POINT_RESIST_FIRE:
			return "[LS;606;%d]"/* "Fire Resistance %d%%" */;
		case POINT_RESIST_ELEC:
			return "[LS;607;%d]"/* "Lightning Resistance %d%%" */;
		case POINT_RESIST_MAGIC:
			return "[LS;608;%d]"/* "Magic Resistance %d%%" */;
		case POINT_RESIST_WIND:
			return "[LS;609;%d]"/* "Wind Resistance %d%%" */;
		case POINT_RESIST_ICE:
			return LC_TEXT ("�ñ� ���� %d%%");
		case POINT_RESIST_EARTH:
			return LC_TEXT ("���� ���� %d%%");
		case POINT_RESIST_DARK:
			return LC_TEXT ("��� ���� %d%%");
		case POINT_REFLECT_MELEE:
			return "[LS;610;%d]"/* "Reflect Direct Hit: %d%%" */;
		case POINT_REFLECT_CURSE:
			return "[LS;611;%d]"/* "Reflect Curse: %d%%" */;
		case POINT_POISON_REDUCE:
			return "[LS;612;%d]"/* "Poison Resistance %d%%" */;
		case POINT_KILL_SP_RECOVER:
			return "[LS;613;%d]"/* "Spell Points (SP) will be increased by %d%% if you win." */;
		case POINT_EXP_DOUBLE_BONUS:
			return "[LS;615;%d]"/* "Experience increases by %d%% if you win against an opponent." */;
		case POINT_GOLD_DOUBLE_BONUS:
			return "[LS;616;%d]"/* "Increase of Yang up to %d%% if you win." */;
		case POINT_ITEM_DROP_BONUS:
			return "[LS;617;%d]"/* "Increase of captured Items up to %d%% if you win." */;
		case POINT_POTION_BONUS:
			return "[LS;618;%d]"/* "Power increase of up to %d%% after taking the potion." */;
		case POINT_KILL_HP_RECOVERY:
			return "[LS;619;%d]"/* "%d%% Chance of filling up Hit Points after a victory." */;
		case POINT_IMMUNE_STUN:
			return "[LS;620;%d]"/* "No Dizziness %d%%" */;
		case POINT_IMMUNE_SLOW:
			return "[LS;621;%d]"/* "No Slowing Down %d%%" */;
		case POINT_IMMUNE_FALL:
			return "[LS;622;%d]"/* "No falling down %d%%" */;
		//		case POINT_SKILL:	return LC_TEXT("");
		//		case POINT_BOW_DISTANCE:	return LC_TEXT("");
		case POINT_ATT_GRADE_BONUS:
			return "[LS;623;%d]"/* "Attack Power + %d" */;
		case POINT_DEF_GRADE_BONUS:
			return "[LS;624;%d]"/* "Armour + %d" */;
		case POINT_MAGIC_ATT_GRADE:
			return "[LS;626;%d]"/* "Magical Attack + %d" */;
		case POINT_MAGIC_DEF_GRADE:
			return "[LS;627;%d]"/* "Magical Defence + %d" */;
		//		case POINT_CURSE_PCT:	return LC_TEXT("");
		case POINT_MAX_STAMINA:
			return "[LS;628;%d]"/* "Maximum Endurance + %d" */;
		case POINT_ATTBONUS_WARRIOR:
			return "[LS;629;%d]"/* "Strong against Warriors + %d%%" */;
		case POINT_ATTBONUS_ASSASSIN:
			return "[LS;630;%d]"/* "Strong against Ninjas + %d%%" */;
		case POINT_ATTBONUS_SURA:
			return "[LS;631;%d]"/* "Strong against Sura + %d%%" */;
		case POINT_ATTBONUS_SHAMAN:
			return "[LS;632;%d]"/* "Strong against Shamans + %d%%" */;
		case POINT_ATTBONUS_MONSTER:
			return "[LS;633;%d]"/* "Strength against monsters + %d%%" */;
		case POINT_MALL_ATTBONUS:
			return "[LS;634;%d]"/* "Attack + %d%%" */;
		case POINT_MALL_DEFBONUS:
			return "[LS;635;%d]"/* "Defence + %d%%" */;
		case POINT_MALL_EXPBONUS:
			return "[LS;637;%d]"/* "Experience %d%%" */;
		case POINT_MALL_ITEMBONUS:
			return "[LS;638;%d]"/* "Chance to find an Item %. 1f" */;
		case POINT_MALL_GOLDBONUS:
			return "[LS;639;%d]"/* "Chance to find Yang %. 1f" */;
		case POINT_MAX_HP_PCT:
			return "[LS;640;%d]"/* "Maximum Energy +%d%%" */;
		case POINT_MAX_SP_PCT:
			return "[LS;641;%d]"/* "Maximum Energy +%d%%" */;
		case POINT_SKILL_DAMAGE_BONUS:
			return "[LS;642;%d]"/* "Skill Damage %d%%" */;
		case POINT_NORMAL_HIT_DAMAGE_BONUS:
			return "[LS;643;%d]"/* "Hit Damage %d%%" */;
		case POINT_SKILL_DEFEND_BONUS:
			return "[LS;644;%d]"/* "Resistance against Skill Damage %d%%" */;
		case POINT_NORMAL_HIT_DEFEND_BONUS:
			return "[LS;645;%d]"/* "Resistance against Hits %d%%" */;
		//		case POINT_EXTRACT_HP_PCT:	return LC_TEXT("");
		case POINT_RESIST_WARRIOR:
			return "[LS;646;%d]"/* "%d%% Resistance against Warrior Attacks" */;
		case POINT_RESIST_ASSASSIN:
			return "[LS;648;%d]"/* "%d%% Resistance against Ninja Attacks" */;
		case POINT_RESIST_SURA:
			return "[LS;649;%d]"/* "%d%% Resistance against Sura Attacks" */;
		case POINT_RESIST_SHAMAN:
			return "[LS;650;%d]"/* "%d%% Resistance against Shaman Attacks" */;
		default:
			return NULL;
	}
}

static bool FN_hair_affect_string (LPCHARACTER ch, char* buf, size_t bufsiz)
{
	if (NULL == ch || NULL == buf)
	{
		return false;
	}

	CAffect* aff = NULL;
	time_t expire = 0;
	struct tm ltm;
	int	year, mon, day;
	int	offset = 0;

	aff = ch->FindAffect (AFFECT_HAIR);

	if (NULL == aff)
	{
		return false;
	}

	expire = ch->GetQuestFlag ("hair.limit_time");

	if (expire < get_global_time())
	{
		return false;
	}

	// set apply string
	offset = snprintf (buf, bufsiz, FN_point_string (aff->bApplyOn), aff->lApplyValue);

	if (offset < 0 || offset >= (int) bufsiz)
	{
		offset = bufsiz - 1;
	}

	localtime_r (&expire, &ltm);

	year	= ltm.tm_year + 1900;
	mon		= ltm.tm_mon + 1;
	day		= ltm.tm_mday;

	snprintf (buf + offset, bufsiz - offset, "[LS;651;%d;%d;%d]"/* " (Procedure: %d y- %d m - %d d)" */, year, mon, day);

	return true;
}

ACMD (do_costume)
{
	char buf[512];
	const size_t bufferSize = sizeof (buf);

	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	CItem* pBody = ch->GetWear (WEAR_COSTUME_BODY);
	CItem* pHair = ch->GetWear (WEAR_COSTUME_HAIR);

	ch->ChatPacket (CHAT_TYPE_INFO, "COSTUME status:");

	if (pHair)
	{
		const char* itemName = pHair->GetName();
		ch->ChatPacket (CHAT_TYPE_INFO, "  HAIR : %s", itemName);

		for (int i = 0; i < pHair->GetAttributeCount(); ++i)
		{
			const TPlayerItemAttribute& attr = pHair->GetAttribute (i);
			if (0 < attr.bType)
			{
				snprintf (buf, bufferSize, FN_point_string (attr.bType), attr.sValue);
				ch->ChatPacket (CHAT_TYPE_INFO, "     %s", buf);
			}
		}

		if (pHair->IsEquipped() && arg1[0] == 'h')
		{
			ch->UnequipItem (pHair);
		}
	}

	if (pBody)
	{
		const char* itemName = pBody->GetName();
		ch->ChatPacket (CHAT_TYPE_INFO, "  BODY : %s", itemName);

		if (pBody->IsEquipped() && arg1[0] == 'b')
		{
			ch->UnequipItem (pBody);
		}
	}
}

ACMD (do_hair)
{
	char buf[256];

	if (false == FN_hair_affect_string (ch, buf, sizeof (buf)))
	{
		return;
	}

	ch->ChatPacket (CHAT_TYPE_INFO, buf);
}

ACMD (do_inventory)
{
	int	index = 0;
	int	count		= 1;

	char arg1[256];
	char arg2[256];

	LPITEM	item;

	two_arguments (argument, arg1, sizeof (arg1), arg2, sizeof (arg2));

	if (!*arg1)
	{
		ch->ChatPacket (CHAT_TYPE_INFO, "Usage: inventory <start_index> <count>");
		return;
	}

	if (!*arg2)
	{
		index = 0;
		str_to_number (count, arg1);
	}
	else
	{
		str_to_number (index, arg1);
		index = MIN (index, INVENTORY_MAX_NUM);
		str_to_number (count, arg2);
		count = MIN (count, INVENTORY_MAX_NUM);
	}

	for (int i = 0; i < count; ++i)
	{
		if (index >= INVENTORY_MAX_NUM)
		{
			break;
		}

		item = ch->GetInventoryItem (index);

		ch->ChatPacket (CHAT_TYPE_INFO, "inventory [%d] = %s",
						index, item ? item->GetName() : "<NONE>");
		++index;
	}
}

//gift notify quest command
ACMD (do_gift)
{
	ch->ChatPacket (CHAT_TYPE_COMMAND, "gift");
}

ACMD (do_cube)
{
	if (!ch->CanDoCube())
	{
		return;
	}

	dev_log (LOG_DEB0, "CUBE COMMAND <%s>: %s", ch->GetName(), argument);
	int cube_index = 0, inven_index = 0;
	const char* line;

	char arg1[256], arg2[256], arg3[256];

	line = two_arguments (argument, arg1, sizeof (arg1), arg2, sizeof (arg2));
	one_argument (line, arg3, sizeof (arg3));

	if (0 == arg1[0])
	{
		// print usage
		ch->ChatPacket (CHAT_TYPE_INFO, "Usage: cube open");
		ch->ChatPacket (CHAT_TYPE_INFO, "       cube close");
		ch->ChatPacket (CHAT_TYPE_INFO, "       cube add <inveltory_index>");
		ch->ChatPacket (CHAT_TYPE_INFO, "       cube delete <cube_index>");
		ch->ChatPacket (CHAT_TYPE_INFO, "       cube list");
		ch->ChatPacket (CHAT_TYPE_INFO, "       cube cancel");
		ch->ChatPacket (CHAT_TYPE_INFO, "       cube make [all]");
		return;
	}

	const std::string& strArg1 = std::string (arg1);

	// r_info (request information)
	// /cube r_info     ==> (Client -> Server) ���� NPC�� ���� �� �ִ� ������ ��û
	//					    (Server -> Client) /cube r_list npcVNUM resultCOUNT 123,1/125,1/128,1/130,5
	//
	// /cube r_info 3   ==> (Client -> Server) ���� NPC�� ����� �ִ� ������ �� 3��° �������� ����� �� �ʿ��� ������ ��û
	// /cube r_info 3 5 ==> (Client -> Server) ���� NPC�� ����� �ִ� ������ �� 3��° �����ۺ��� ���� 5���� �������� ����� �� �ʿ��� ��� ������ ��û
	//					   (Server -> Client) /cube m_info startIndex count 125,1|126,2|127,2|123,5&555,5&555,4/120000@125,1|126,2|127,2|123,5&555,5&555,4/120000
	//
	if (strArg1 == "r_info")
	{
		if (0 == arg2[0])
		{
			Cube_request_result_list (ch);
		}
		else
		{
			if (isdigit (*arg2))
			{
				int listIndex = 0, requestCount = 1;
				str_to_number (listIndex, arg2);

				if (0 != arg3[0] && isdigit (*arg3))
				{
					str_to_number (requestCount, arg3);
				}

				Cube_request_material_info (ch, listIndex, requestCount);
			}
		}

		return;
	}

	switch (LOWER (arg1[0]))
	{
		case 'o':	// open
			Cube_open (ch);
			break;

		case 'c':	// close
			Cube_close (ch);
			break;

		case 'l':	// list
			Cube_show_list (ch);
			break;

		case 'a':	// add cue_index inven_index
		{
			if (0 == arg2[0] || !isdigit (*arg2) ||
			0 == arg3[0] || !isdigit (*arg3))
			{
				return;
			}

			str_to_number (cube_index, arg2);
			str_to_number (inven_index, arg3);
			Cube_add_item (ch, cube_index, inven_index);
		}
		break;

		case 'd':	// delete
		{
			if (0 == arg2[0] || !isdigit (*arg2))
			{
				return;
			}

			str_to_number (cube_index, arg2);
			Cube_delete_item (ch, cube_index);
		}
		break;

		case 'm':	// make
			if (0 != arg2[0])
			{
				while (true == Cube_make (ch))
				{
					dev_log (LOG_DEB0, "cube make success");
				}
			}
			else
			{
				Cube_make (ch);
			}
			break;

		default:
			return;
	}
}

ACMD (do_in_game_mall)
{
	ch->ChatPacket (CHAT_TYPE_COMMAND, "mall http://metin2.co.kr/04_mall/mall/login.htm");
	return;
}

// �ֻ���
ACMD (do_dice)
{
	char arg1[256], arg2[256];
	int start = 1, end = 100;

	two_arguments (argument, arg1, sizeof (arg1), arg2, sizeof (arg2));

	if (*arg1 && *arg2)
	{
		start = atoi (arg1);
		end = atoi (arg2);
	}
	else if (*arg1 && !*arg2)
	{
		start = 1;
		end = atoi (arg1);
	}

	end = MAX (start, end);
	start = MIN (start, end);

	int n = number (start, end);

	if (ch->GetParty())
	{
		ch->GetParty()->ChatPacketToAllMember (CHAT_TYPE_INFO, LC_TEXT ("%s���� �ֻ����� ���� %d�� ���Խ��ϴ�. (%d-%d)"), ch->GetName(), n, start, end);
	}
	else
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT ("����� �ֻ����� ���� %d�� ���Խ��ϴ�. (%d-%d)"), n, start, end);
	}
}

ACMD (do_click_mall)
{
	ch->ChatPacket (CHAT_TYPE_COMMAND, "ShowMeMallPassword");
}

ACMD (do_ride)
{
	dev_log (LOG_DEB0, "[DO_RIDE] start");
	if (ch->IsDead() || ch->IsStun())
	{
		return;
	}

	// ������
	{
		if (ch->IsHorseRiding())
		{
			dev_log (LOG_DEB0, "[DO_RIDE] stop riding");
			ch->StopRiding();
			return;
		}

		if (ch->GetMountVnum())
		{
			dev_log (LOG_DEB0, "[DO_RIDE] unmount");
			do_unmount (ch, NULL, 0, 0);
			return;
		}
	}

	// Ÿ��
	{
		if (ch->GetHorse() != NULL)
		{
			dev_log (LOG_DEB0, "[DO_RIDE] start riding");
			ch->StartRiding();
			return;
		}

		for (BYTE i = 0; i < INVENTORY_MAX_NUM; ++i)
		{
			LPITEM item = ch->GetInventoryItem (i);
			if (NULL == item)
			{
				continue;
			}

			// ����ũ Ż�� ������
			if (item->IsRideItem())
			{
				if (NULL == ch->GetWear (WEAR_UNIQUE1) || NULL == ch->GetWear (WEAR_UNIQUE2))
				{
					dev_log (LOG_DEB0, "[DO_RIDE] USE UNIQUE ITEM");
					//ch->EquipItem(item);
					ch->UseItem (TItemPos (INVENTORY, i));
					return;
				}
			}

			// �Ϲ� Ż�� ������
			// TODO : Ż�Ϳ� SubType �߰�
			switch (item->GetVnum())
			{
				case 71114:	// �����̿��
				case 71116:	// ��߽��̿��
				case 71118:	// �������̿��
				case 71120:	// ���ڿ��̿��
					dev_log (LOG_DEB0, "[DO_RIDE] USE QUEST ITEM");
					ch->UseItem (TItemPos (INVENTORY, i));
					return;
			}

			// GF mantis #113524, 52001~52090 �� Ż��
			if ((item->GetVnum() > 52000) && (item->GetVnum() < 52091))
			{
				dev_log (LOG_DEB0, "[DO_RIDE] USE QUEST ITEM");
				ch->UseItem (TItemPos (INVENTORY, i));
				return;
			}
		}
	}


	// Ÿ�ų� ���� �� ������
	ch->ChatPacket (CHAT_TYPE_INFO, "[LS;501]"/* "Please call your Horse first." */);
}

#ifdef __AUCTION__
// temp_auction
ACMD (do_get_item_id_list)
{
	for (int i = 0; i < INVENTORY_MAX_NUM; i++)
	{
		LPITEM item = ch->GetInventoryItem (i);
		if (item != NULL)
		{
			ch->ChatPacket (CHAT_TYPE_INFO, "name : %s id : %d", item->GetProto()->szName, item->GetID());
		}
	}
}

// temp_auction

ACMD (do_enroll_auction)
{
	char arg1[256];
	char arg2[256];
	char arg3[256];
	char arg4[256];
	two_arguments (two_arguments (argument, arg1, sizeof (arg1), arg2, sizeof (arg2)), arg3, sizeof (arg3), arg4, sizeof (arg4));

	DWORD item_id = strtoul (arg1, NULL, 10);
	BYTE empire = strtoul (arg2, NULL, 10);
	int bidPrice = strtol (arg3, NULL, 10);
	int immidiatePurchasePrice = strtol (arg4, NULL, 10);

	LPITEM item = ITEM_MANAGER::instance().Find (item_id);
	if (item == NULL)
	{
		return;
	}

	AuctionManager::instance().enroll_auction (ch, item, empire, bidPrice, immidiatePurchasePrice);
}

ACMD (do_enroll_wish)
{
	char arg1[256];
	char arg2[256];
	char arg3[256];
	one_argument (two_arguments (argument, arg1, sizeof (arg1), arg2, sizeof (arg2)), arg3, sizeof (arg3));

	DWORD item_num = strtoul (arg1, NULL, 10);
	BYTE empire = strtoul (arg2, NULL, 10);
	int wishPrice = strtol (arg3, NULL, 10);

	AuctionManager::instance().enroll_wish (ch, item_num, empire, wishPrice);
}

ACMD (do_enroll_sale)
{
	char arg1[256];
	char arg2[256];
	char arg3[256];
	one_argument (two_arguments (argument, arg1, sizeof (arg1), arg2, sizeof (arg2)), arg3, sizeof (arg3));

	DWORD item_id = strtoul (arg1, NULL, 10);
	DWORD wisher_id = strtoul (arg2, NULL, 10);
	int salePrice = strtol (arg3, NULL, 10);

	LPITEM item = ITEM_MANAGER::instance().Find (item_id);
	if (item == NULL)
	{
		return;
	}

	AuctionManager::instance().enroll_sale (ch, item, wisher_id, salePrice);
}

// temp_auction
// packet���� ����ϰ� �ϰ�, �̰� �����ؾ��Ѵ�.
ACMD (do_get_auction_list)
{
	char arg1[256];
	char arg2[256];
	char arg3[256];
	two_arguments (one_argument (argument, arg1, sizeof (arg1)), arg2, sizeof (arg2), arg3, sizeof (arg3));

	AuctionManager::instance().get_auction_list (ch, strtoul (arg1, NULL, 10), strtoul (arg2, NULL, 10), strtoul (arg3, NULL, 10));
}
//
//ACMD(do_get_wish_list)
//{
//	char arg1[256];
//	char arg2[256];
//	char arg3[256];
//	two_arguments (one_argument (argument, arg1, sizeof(arg1)), arg2, sizeof(arg2), arg3, sizeof(arg3));
//
//	AuctionManager::instance().get_wish_list (ch, strtoul(arg1, NULL, 10), strtoul(arg2, NULL, 10), strtoul(arg3, NULL, 10));
//}
ACMD (do_get_my_auction_list)
{
	char arg1[256];
	char arg2[256];
	two_arguments (argument, arg1, sizeof (arg1), arg2, sizeof (arg2));

	AuctionManager::instance().get_my_auction_list (ch, strtoul (arg1, NULL, 10), strtoul (arg2, NULL, 10));
}

ACMD (do_get_my_purchase_list)
{
	char arg1[256];
	char arg2[256];
	two_arguments (argument, arg1, sizeof (arg1), arg2, sizeof (arg2));

	AuctionManager::instance().get_my_purchase_list (ch, strtoul (arg1, NULL, 10), strtoul (arg2, NULL, 10));
}

ACMD (do_auction_bid)
{
	char arg1[256];
	char arg2[256];
	two_arguments (argument, arg1, sizeof (arg1), arg2, sizeof (arg2));

	AuctionManager::instance().bid (ch, strtoul (arg1, NULL, 10), strtoul (arg2, NULL, 10));
}

ACMD (do_auction_impur)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	AuctionManager::instance().immediate_purchase (ch, strtoul (arg1, NULL, 10));
}

ACMD (do_get_auctioned_item)
{
	char arg1[256];
	char arg2[256];
	two_arguments (argument, arg1, sizeof (arg1), arg2, sizeof (arg2));

	AuctionManager::instance().get_auctioned_item (ch, strtoul (arg1, NULL, 10), strtoul (arg2, NULL, 10));
}

ACMD (do_buy_sold_item)
{
	char arg1[256];
	char arg2[256];
	one_argument (argument, arg1, sizeof (arg1));

	AuctionManager::instance().get_auctioned_item (ch, strtoul (arg1, NULL, 10), strtoul (arg2, NULL, 10));
}

ACMD (do_cancel_auction)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	AuctionManager::instance().cancel_auction (ch, strtoul (arg1, NULL, 10));
}

ACMD (do_cancel_wish)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	AuctionManager::instance().cancel_wish (ch, strtoul (arg1, NULL, 10));
}

ACMD (do_cancel_sale)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof (arg1));

	AuctionManager::instance().cancel_sale (ch, strtoul (arg1, NULL, 10));
}

ACMD (do_rebid)
{
	char arg1[256];
	char arg2[256];
	two_arguments (argument, arg1, sizeof (arg1), arg2, sizeof (arg2));

	AuctionManager::instance().rebid (ch, strtoul (arg1, NULL, 10), strtoul (arg2, NULL, 10));
}

ACMD (do_bid_cancel)
{
	char arg1[256];
	char arg2[256];
	two_arguments (argument, arg1, sizeof (arg1), arg2, sizeof (arg2));

	AuctionManager::instance().bid_cancel (ch, strtoul (arg1, NULL, 10));
}
#endif
