/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "AcceptInvitationAction.h"

#include "Event.h"
#include "ObjectAccessor.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotSecurity.h"
#include "Playerbots.h"
#include "WorldPacket.h"

bool AcceptInvitationAction::Execute(Event event)
{
    Group* grp = bot->GetGroupInvite();
    if (!grp)
        return false;
    WorldPacket packet = event.getPacket();
    uint8 flag;
    std::string name;
    packet >> flag >> name;

    // Player* inviter = ObjectAccessor::FindPlayer(grp->GetLeaderGUID());
    Player* inviter = ObjectAccessor::FindPlayerByName(name, true);
    if (!inviter)
        return false;


    // 判断当前机器人是否是玩家机器人，且没有master，如果是就不接受邀请
    bool isRandomBot = sRandomPlayerbotMgr->IsRandomBot(bot->GetGUID().GetCounter());
    if (!isRandomBot && botAI->GetMaster() == nullptr)
    {
        // WorldPacket data(SMSG_GROUP_CANCEL, 10);
        // data << bot->GetName();
        // inviter->SendDirectMessage(&data);

        inviter->GetSession()->SendPartyResult(PARTY_OP_INVITE, bot->GetName(), ERR_ALREADY_IN_GROUP_S);

        bot->UninviteFromGroup();
        return false;
    }
    // 如果是玩家机器人，且邀请者不是机器人当前的master，不接受邀请
    else if (!isRandomBot && botAI->GetMaster() && botAI->GetMaster() != inviter)
    {
        // WorldPacket data(SMSG_GROUP_CANCEL, 10);
        // data << bot->GetName();
        // inviter->SendDirectMessage(&data);

        inviter->GetSession()->SendPartyResult(PARTY_OP_INVITE, bot->GetName(), ERR_ALREADY_IN_GROUP_S);

        bot->UninviteFromGroup();
        return false;
    }
    // end -----------------

    // 判断在队伍中的机器人数量，如果超过n个就不接受邀请
    if (!inviter->IsGameMaster())
    {
        // 看队伍人数
        uint16 _grpbotsnum = 0;
        if (grp)
        {
            for (GroupReference* itr = grp->GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (!member)
                    continue;

                if (member->GetSession()->IsBot())
                {
                    _grpbotsnum = _grpbotsnum + 1;
                }
            }
        }
        if (_grpbotsnum > 3)  // 如果当前队伍已经有3个或3个以上的机器人了，就不接受，>3表示允许组4个机器人
        {
            std::ostringstream out;
            out << "Team Robot is full.";
            std::string const text = out.str();
            // LOG_ERROR("xx", "Whisper leader {}", inviter->GetGUID().GetCounter());//测试
            bot->Whisper(text, LANG_UNIVERSAL, inviter);
            bot->UninviteFromGroup();
            return false;
        }
        /*
        //如果已经召唤了账号下的机器人，计算数量
        PlayerbotMgr* inviterBotMgr = nullptr;
        if (inviter)
            inviterBotMgr = GET_PLAYERBOT_MGR(inviter);

        uint8 getbots = 0;
        for (PlayerBotMap::const_iterator i = inviterBotMgr->GetPlayerBotsBegin(); i !=
        inviterBotMgr->GetPlayerBotsEnd(); ++i)
        {
            if (Player* bot = i->second)
            {
                PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
                if (botAI->GetMaster()->GetGUID() == inviter->GetGUID())
                {
                    getbots++;
                    if (getbots >= 4)
                    {
                        break;
                    }
                }
            }
        }

        //如果还没有3个AI机器人，可以召唤新机器人
        if (getbots < 4)
        {
            //LOG_ERROR("xx", "_grpbotsnum {}", _grpbotsnum);//测试
            if (_grpbotsnum + getbots > 3)//如果当前队伍已经有3个或3个以上的机器人了，就不接受，>3表示允许组4个机器人
            {
                std::ostringstream out;
                out << "Team Robot is full.";
                std::string const text = out.str();
                //LOG_ERROR("xx", "Whisper leader {}", inviter->GetGUID().GetCounter());//测试
                bot->Whisper(text, LANG_UNIVERSAL, inviter);
                bot->UninviteFromGroup();
                return false;
            }
        }
        else
        {
            std::ostringstream out;
            out << "Robot is full.";
            std::string const text = out.str();
            //LOG_ERROR("xx", "Whisper leader {}", inviter->GetGUID().GetCounter());//测试
            bot->Whisper(text, LANG_UNIVERSAL, inviter);
            bot->UninviteFromGroup();
            return false;
        }
        */

    }


    if (!botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_INVITE, false, inviter))
    {
        WorldPacket data(SMSG_GROUP_DECLINE, 10);
        data << bot->GetName();
        inviter->SendDirectMessage(&data);
        bot->UninviteFromGroup();
        return false;
    }

    WorldPacket p;
    uint32 roles_mask = 0;
    p << roles_mask;
    bot->GetSession()->HandleGroupAcceptOpcode(p);

    if (sRandomPlayerbotMgr->IsRandomBot(bot))
        botAI->SetMaster(inviter);
    // else
    // sPlayerbotDbStore->Save(botAI);

    botAI->ResetStrategies();
    botAI->ChangeStrategy("+follow,-lfg,-bg", BOT_STATE_NON_COMBAT);
    botAI->Reset();

    botAI->TellMaster("Hello");

    if (sPlayerbotAIConfig->summonWhenGroup && bot->GetDistance(inviter) > sPlayerbotAIConfig->sightDistance)
    {
        Teleport(inviter, bot);
    }
    return true;
}
