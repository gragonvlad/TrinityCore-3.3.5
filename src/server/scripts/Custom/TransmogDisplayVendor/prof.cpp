#include "ScriptPCH.h"

#include "Bag.h"
#include "Common.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "DBCStructure.h"
#include "Define.h"
#include "Field.h"
#include "GameEventMgr.h"
#include "GossipDef.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Language.h"
#include "Log.h"
#include "Player.h"
#include "ObjectGuid.h"
#include "WorldSession.h"
#include "ObjectMgr.h"
#include "QueryResult.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "PassiveAI.h"
#include "Creature.h"
#include "SharedDefines.h"
#include "Transaction.h"
#include "WorldSession.h"
#include <sstream>
#include <string>
#include "GossipDef.h"
#include "Chat.h"
#include "World.h"
#include "WorldPacket.h"

#include "AchievementMgr.h"
#include "CharacterDatabaseCleaner.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "SpellMgr.h"


enum LogFilterType
{
	LOG_FILTER_GENERAL = 0
};


class maestro_profesiones : public CreatureScript


{
public:

	maestro_profesiones() : CreatureScript("maestro_profesiones") { }

	
	struct maestro_profesionesAI : public ScriptedAI
	{
		maestro_profesionesAI(Creature* creature) : ScriptedAI(creature) { }

	
		uint32 FireboltTimer;

		void Reset()
		{
			FireboltTimer = 2000;

			me->ApplySpellImmune(0, IMMUNITY_SCHOOL, SPELL_SCHOOL_MASK_FIRE, true);
		}

		void EnterCombat(Unit* /*who*/) { }

		void UpdateAI(uint32 diff)
		{
			//Return since we have no target
			if (!UpdateVictim())
				return;

			if (FireboltTimer <= diff)
			{
				DoCastVictim(30050);
				FireboltTimer = 2200;
			}
			else FireboltTimer -= diff;

			DoMeleeAttackIfReady();
		}

	void CreatureWhisperBasedOnBool(const char *text, Creature *_creature, Player *pPlayer, bool value)
	{
		if (value)
			pPlayer->Whisper(text,LANG_UNIVERSAL, pPlayer);
	}

	uint32 PlayerMaxLevel() const
	{
		return sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);
	}


	void MainMenu(Player *pPlayer, Creature* _creature)
	{
		AddGossipItemFor(pPlayer, 9, "[Profesiones Primarias] ->", GOSSIP_SENDER_MAIN, 196);
		AddGossipItemFor(pPlayer, 9, "[Profesiones Secundarias] ->", GOSSIP_SENDER_MAIN, 197);
		AddGossipItemFor(pPlayer, 2, "Olvidalo", GOSSIP_SENDER_MAIN, 2050);
		SendGossipMenuFor(pPlayer, 907, _creature->GetGUID());
	}

	bool PlayerHasItemOrSpell(const Player *plr, uint32 itemId, uint32 spellId) const
	{
		return plr->HasItemCount(itemId, 1, true) || plr->HasSpell(spellId);
	}

	bool OnGossipHello(Player* pPlayer, Creature* pCreature)
	{
		MainMenu(pPlayer, pCreature);
		return true;
	}

	bool PlayerAlreadyHasTwoProfessions(const Player *pPlayer) const
	{
		uint32 skillCount = 0;

		if (pPlayer->HasSkill(SKILL_MINING))
			skillCount++;
		if (pPlayer->HasSkill(SKILL_SKINNING))
			skillCount++;
		if (pPlayer->HasSkill(SKILL_HERBALISM))
			skillCount++;

		if (skillCount >= sWorld->getIntConfig(CONFIG_MAX_PRIMARY_TRADE_SKILL))
			return true;

		for (uint32 i = 1; i < sSkillLineStore.GetNumRows(); ++i)
		{
			SkillLineEntry const *SkillInfo = sSkillLineStore.LookupEntry(i);
			if (!SkillInfo)
				continue;

			if (SkillInfo->categoryId == SKILL_CATEGORY_SECONDARY)
				continue;

			if ((SkillInfo->categoryId != SKILL_CATEGORY_PROFESSION) || !SkillInfo->canLink)
				continue;

			const uint32 skillID = SkillInfo->id;
			if (pPlayer->HasSkill(skillID))
				skillCount++;

			if (skillCount >= sWorld->getIntConfig(CONFIG_MAX_PRIMARY_TRADE_SKILL))
				return true;
		}

		return false;
	}

	bool LearnAllRecipesInProfession(Player *pPlayer, SkillType skill)
	{
		ChatHandler handler(pPlayer->GetSession());
		char* skill_name;

		SkillLineEntry const *SkillInfo = sSkillLineStore.LookupEntry(skill);
		skill_name = SkillInfo->name[handler.GetSessionDbcLocale()];

		if (!SkillInfo)
		{
			//TRINITY
			//TC_LOG_ERROR(LOG_FILTER_GENERAL, "Teleport NPC: received non-valid skill ID (LearnAllRecipesInProfession)");


			//MYTH
			//sLog->outError("Teleport NPC: received non-valid skill ID (LearnAllRecipesInProfession)");

			return false;
		}

		LearnSkillRecipesHelper(pPlayer, SkillInfo->id);

		uint16 maxLevel = pPlayer->GetPureMaxSkillValue(SkillInfo->id);
		pPlayer->SetSkill(SkillInfo->id, pPlayer->GetSkillStep(SkillInfo->id), maxLevel, maxLevel);
		handler.PSendSysMessage(592, skill_name);

		return true;
	}

	void LearnSkillRecipesHelper(Player *player, uint32 skill_id)
	{
		uint32 classmask = player->getClassMask();

		for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
		{
			SkillLineAbilityEntry const *skillLine = sSkillLineAbilityStore.LookupEntry(j);
			if (!skillLine)
				continue;

			// wrong skill
			if (skillLine->skillId != skill_id)
				continue;

			// not high rank
			if (skillLine->forward_spellid)
				continue;

			// skip racial skills
			if (skillLine->racemask != 0)
				continue;

			// skip wrong class skills
			if (skillLine->classmask && (skillLine->classmask & classmask) == 0)
				continue;


			//MYTH
			//SpellEntry const* spellInfo = sSpellStore.LookupEntry(skillLine->spellId);
			//TRINITY
			SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(skillLine->spellId);
			if (!spellInfo || !SpellMgr::IsSpellValid(spellInfo))
				continue;

			player->LearnSpell(skillLine->spellId, false);
		}
	}

	bool IsSecondarySkill(SkillType skill) const
	{
		return skill == SKILL_COOKING || skill == SKILL_FIRST_AID || skill == SKILL_FISHING;
	}

	void CompleteLearnProfession(Player *pPlayer, Creature *pCreature, SkillType skill)
	{
		if (PlayerAlreadyHasTwoProfessions(pPlayer) && !IsSecondarySkill(skill))
			pPlayer->Whisper("Ya sabes dos profesiones!", LANG_UNIVERSAL, pPlayer);
		else
		{
			if (!LearnAllRecipesInProfession(pPlayer, skill))
				pPlayer->Whisper("Error interno!", LANG_UNIVERSAL, pPlayer);
			
		}
	}

	bool OnGossipSelect(Player* pPlayer, Creature* _creature, uint32 uiSender, uint32 uiAction)
	{
		pPlayer->PlayerTalkClass->ClearMenus();

		if (uiSender == GOSSIP_SENDER_MAIN)
		{

			switch (uiAction)
			{
			case 196:
				AddGossipItemFor(pPlayer, 3, "Alquimia", GOSSIP_SENDER_MAIN, 1);
				AddGossipItemFor(pPlayer, 3, "Herreria", GOSSIP_SENDER_MAIN, 2);
				AddGossipItemFor(pPlayer, 3, "Peleteria", GOSSIP_SENDER_MAIN, 3);
				AddGossipItemFor(pPlayer, 3, "Sastreria", GOSSIP_SENDER_MAIN, 4);
				AddGossipItemFor(pPlayer, 3, "Ingenieria", GOSSIP_SENDER_MAIN, 5);
				AddGossipItemFor(pPlayer, 3, "Encantador", GOSSIP_SENDER_MAIN, 6);
				AddGossipItemFor(pPlayer, 3, "Joyeria", GOSSIP_SENDER_MAIN, 7);
				AddGossipItemFor(pPlayer, 3, "Inscripcion", GOSSIP_SENDER_MAIN, 8);
				AddGossipItemFor(pPlayer, 3, "Mineria", GOSSIP_SENDER_MAIN, 11);
				AddGossipItemFor(pPlayer, 3, "Herboristeria", GOSSIP_SENDER_MAIN, 12);
				AddGossipItemFor(pPlayer, 3, "Desuello", GOSSIP_SENDER_MAIN, 13);
				AddGossipItemFor(pPlayer, 2, "<- Volver al Menu Principal.", GOSSIP_SENDER_MAIN, 2051);
				AddGossipItemFor(pPlayer, 2, "Olvidalo", GOSSIP_SENDER_MAIN, 2050);

				SendGossipMenuFor(pPlayer, 907, _creature->GetGUID());
				break;

			case 197:

				AddGossipItemFor(pPlayer, 3, "Cocinero", GOSSIP_SENDER_MAIN, 9);
				AddGossipItemFor(pPlayer, 3, "Primeros Auxilios", GOSSIP_SENDER_MAIN, 10);
				AddGossipItemFor(pPlayer, 3, "Pesca", GOSSIP_SENDER_MAIN, 14);
				AddGossipItemFor(pPlayer, 2, "<- Volver al Menu Principal.", GOSSIP_SENDER_MAIN, 2051);
				AddGossipItemFor(pPlayer, 2, "Olvidalo", GOSSIP_SENDER_MAIN, 2050);
				SendGossipMenuFor(pPlayer, 907, _creature->GetGUID());
				break;

			case 1:
				CompleteLearnProfession(pPlayer, _creature, SKILL_ALCHEMY);

				CloseGossipMenuFor(pPlayer);
				break;
			case 2:
				CompleteLearnProfession(pPlayer, _creature, SKILL_BLACKSMITHING);

				CloseGossipMenuFor(pPlayer);
				break;
			case 3:
				CompleteLearnProfession(pPlayer, _creature, SKILL_LEATHERWORKING);

				CloseGossipMenuFor(pPlayer);
				break;
			case 4:
				CompleteLearnProfession(pPlayer, _creature, SKILL_TAILORING);

				CloseGossipMenuFor(pPlayer);
				break;
			case 5:
				//TRINITY
				CompleteLearnProfession(pPlayer, _creature, SKILL_ENGINEERING);
				//MYTH
				//CompleteLearnProfession(pPlayer, _creature, SKILL_ENGINERING);

				CloseGossipMenuFor(pPlayer);
				break;
			case 6:
				CompleteLearnProfession(pPlayer, _creature, SKILL_ENCHANTING);

				CloseGossipMenuFor(pPlayer);
				break;
			case 7:
				CompleteLearnProfession(pPlayer, _creature, SKILL_JEWELCRAFTING);

				CloseGossipMenuFor(pPlayer);
				break;
			case 8:
				CompleteLearnProfession(pPlayer, _creature, SKILL_INSCRIPTION);

				CloseGossipMenuFor(pPlayer);
				break;
			case 9:
				CompleteLearnProfession(pPlayer, _creature, SKILL_COOKING);

				CloseGossipMenuFor(pPlayer);
				break;
			case 10:
				CompleteLearnProfession(pPlayer, _creature, SKILL_FIRST_AID);

				CloseGossipMenuFor(pPlayer);
				break;
			case 11:
				CompleteLearnProfession(pPlayer, _creature, SKILL_MINING);

				CloseGossipMenuFor(pPlayer);
				break;
			case 12:
				CompleteLearnProfession(pPlayer, _creature, SKILL_HERBALISM);

				CloseGossipMenuFor(pPlayer);
				break;
			case 13:
				CompleteLearnProfession(pPlayer, _creature, SKILL_SKINNING);

				CloseGossipMenuFor(pPlayer);
				break;
			case 14:
				CompleteLearnProfession(pPlayer, _creature, SKILL_FISHING);

				CloseGossipMenuFor(pPlayer);
				break;

			case 2050:
				CloseGossipMenuFor(pPlayer);
				break;

			case 2051:
				AddGossipItemFor(pPlayer, 9, "[Profesiones Primarias] ->", GOSSIP_SENDER_MAIN, 196);
				AddGossipItemFor(pPlayer, 9, "[Profesiones Secundarias] ->", GOSSIP_SENDER_MAIN, 197);
				AddGossipItemFor(pPlayer, 2, "Olvidalo", GOSSIP_SENDER_MAIN, 2050);
				SendGossipMenuFor(pPlayer, 907, _creature->GetGUID());
				break;
			}


		}
		return true;
	}

	};
	CreatureAI* GetAI(Creature* creature) const
	{
		return new maestro_profesionesAI(creature);
	}
};

void AddSC_maestro_profesiones()
{
	new maestro_profesiones();
};