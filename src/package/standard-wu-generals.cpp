#include "general.h"
#include "standard.h"
#include "skill.h"
#include "engine.h"
#include "client.h"
#include "serverplayer.h"
#include "room.h"
#include "ai.h"
#include "settings.h"

ZhihengCard::ZhihengCard() {
    target_fixed = true;
}

void ZhihengCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const{
    if (source->isAlive())
        room->drawCards(source, subcards.length());
}

class Zhiheng: public ViewAsSkill {
public:
    Zhiheng(): ViewAsSkill("zhiheng") {
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const{
        return !Self->isJilei(to_select) && selected.length() < Self->getMaxHp();
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const{
        if (cards.isEmpty())
            return NULL;

        ZhihengCard *zhiheng_card = new ZhihengCard;
        zhiheng_card->addSubcards(cards);
        zhiheng_card->setSkillName(objectName());
        zhiheng_card->setShowSkill(objectName());
        return zhiheng_card;
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        return player->canDiscard(player, "he") && !player->hasUsed("ZhihengCard");
    }
};

class Qixi: public OneCardViewAsSkill {
public:
    Qixi(): OneCardViewAsSkill("qixi") {
    }

    virtual bool viewFilter(const Card *to_select) const{
        return to_select->isBlack();
    }

    virtual const Card *viewAs(const Card *originalCard) const{
        Dismantlement *dismantlement = new Dismantlement(originalCard->getSuit(), originalCard->getNumber());
        dismantlement->addSubcard(originalCard->getId());
        dismantlement->setSkillName(objectName());
        dismantlement->setShowSkill(objectName());
        return dismantlement;
    }
};

class KejiGlobal: public TriggerSkill{
public:
    KejiGlobal(): TriggerSkill("keji-global"){
        global = true;
        events << PreCardUsed << CardResponded;
    }

    virtual bool triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *ask_who /* = NULL */) const{
        return player != NULL && player->isAlive();
    }

    virtual bool cost(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const{
        return true;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const{
        CardStar card = NULL;
        if (triggerEvent == PreCardUsed)
            card = data.value<CardUseStruct>().card;
        else
            card = data.value<CardResponseStruct>().m_card;
        if (card->isKindOf("Slash") && player->getPhase() == Player::Play)
            player->setFlags("KejiSlashInPlayPhase");

        return false;
    }
};

class Keji: public TriggerSkill {
public:
    Keji(): TriggerSkill("keji") {
        events << EventPhaseChanging;
        frequency = Frequent;
    }

    virtual bool triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *ask_who /* = NULL */) const{
        if (TriggerSkill::triggerable(triggerEvent, room, player, data, ask_who)){
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (player->hasFlag("KejiSlashInPlayPhase") && change.to == Player::Discard)
                return true;
        }
        return false;
    }

    virtual bool cost(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const{
        if (player->askForSkillInvoke(objectName())){
            if (player->getHandcardNum() > player->getMaxCards()) {
                room->broadcastSkillInvoke(objectName());
            }
            return true;
        }
        return false;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *lvmeng, QVariant &data) const{
        lvmeng->skip(Player::Discard);
        return false;
    }
};

KurouCard::KurouCard() {
    target_fixed = true;
}

void KurouCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const{
    room->loseHp(source);
    if (source->isAlive())
        room->drawCards(source, 2);
}

class Kurou: public ZeroCardViewAsSkill {
public:
    Kurou(): ZeroCardViewAsSkill("kurou") {
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        return player->getHp() > 0;
    }

    virtual const Card *viewAs() const{
        Card *card = new KurouCard;
        card->setShowSkill(objectName());
        return card;
    }
};

class Yingzi: public DrawCardsSkill {
public:
    Yingzi(): DrawCardsSkill("yingzi") {
        frequency = Frequent;
    }

    virtual bool cost(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const{
        if (player->askForSkillInvoke(objectName())){
            room->broadcastSkillInvoke(objectName());
            return true;
        }
        return false;
    }

    virtual int getDrawNum(ServerPlayer *zhouyu, int n) const{
        return n + 1;
    }
};

FanjianCard::FanjianCard() {
}

void FanjianCard::onEffect(const CardEffectStruct &effect) const{
    ServerPlayer *zhouyu = effect.from;
    ServerPlayer *target = effect.to;
    Room *room = zhouyu->getRoom();

    int card_id = zhouyu->getRandomHandCardId();
    const Card *card = Sanguosha->getCard(card_id);
    Card::Suit suit = room->askForSuit(target, "fanjian");

    LogMessage log;
    log.type = "#ChooseSuit";
    log.from = target;
    log.arg = Card::Suit2String(suit);
    room->sendLog(log);

    room->getThread()->delay();
    target->obtainCard(card);
    room->showCard(target, card_id);

    if (card->getSuit() != suit)
        room->damage(DamageStruct("fanjian", zhouyu, target));
}

class Fanjian: public ZeroCardViewAsSkill {
public:
    Fanjian(): ZeroCardViewAsSkill("fanjian") {
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        return !player->isKongcheng() && !player->hasUsed("FanjianCard");
    }

    virtual const Card *viewAs() const{
        FanjianCard *fj = new FanjianCard;
        fj->setShowSkill(objectName());
        return fj;
    }
};

class Guose: public OneCardViewAsSkill {
public:
    Guose(): OneCardViewAsSkill("guose") {
        filter_pattern = ".|diamond";
    }

    virtual const Card *viewAs(const Card *originalCard) const{
        Indulgence *indulgence = new Indulgence(originalCard->getSuit(), originalCard->getNumber());
        indulgence->addSubcard(originalCard->getId());
        indulgence->setSkillName(objectName());
        indulgence->setShowSkill(objectName());
        return indulgence;
    }
};

LiuliCard::LiuliCard() {
    //mute = true;
}

bool LiuliCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const{
    if (!targets.isEmpty())
        return false;

    if (to_select->hasFlag("LiuliSlashSource") || to_select == Self)
        return false;

    const Player *from = NULL;
    foreach (const Player *p, Self->getAliveSiblings()) {
        if (p->hasFlag("LiuliSlashSource")) {
            from = p;
            break;
        }
    }

    const Card *slash = Card::Parse(Self->property("liuli").toString());
    if (from && !from->canSlash(to_select, slash, false))
        return false;

    int card_id = subcards.first();
    int range_fix = 0;
    if (Self->getWeapon() && Self->getWeapon()->getId() == card_id) {
        const Weapon *weapon = qobject_cast<const Weapon *>(Self->getWeapon()->getRealCard());
        range_fix += weapon->getRange() - Self->getAttackRange(false);
    } else if (Self->getOffensiveHorse() && Self->getOffensiveHorse()->getId() == card_id) {
        range_fix += 1;
    }

    return Self->distanceTo(to_select, range_fix) <= Self->getAttackRange();
}

void LiuliCard::onEffect(const CardEffectStruct &effect) const{
    effect.to->setFlags("LiuliTarget");
}

class LiuliViewAsSkill: public OneCardViewAsSkill {
public:
    LiuliViewAsSkill(): OneCardViewAsSkill("liuli") {
        filter_pattern = ".!";
        response_pattern = "@@liuli";
    }

    virtual const Card *viewAs(const Card *originalCard) const{
        LiuliCard *liuli_card = new LiuliCard;
        liuli_card->addSubcard(originalCard);
        liuli_card->setShowSkill(objectName());
        return liuli_card;
    }
};

class Liuli: public TriggerSkill {
public:
    Liuli(): TriggerSkill("liuli") {
        events << TargetConfirming;
        view_as_skill = new LiuliViewAsSkill;
    }

    virtual bool triggerable(TriggerEvent, Room *room, ServerPlayer *daqiao, QVariant &data, ServerPlayer *ask_who) const {
        CardUseStruct use = data.value<CardUseStruct>();

        if (use.card->isKindOf("Slash") && use.to.contains(daqiao) && daqiao->canDiscard(daqiao, "he")) {
            QList<ServerPlayer *> players = room->getOtherPlayers(daqiao);
            players.removeOne(use.from);

            bool can_invoke = false;
            foreach (ServerPlayer *p, players) {
                if (use.from->canSlash(p, use.card, false) && daqiao->inMyAttackRange(p)) {
                    can_invoke = true;
                    break;
                }
            }

            return can_invoke;
        }

        return false;
    }

    virtual bool cost(TriggerEvent, Room *room, ServerPlayer *daqiao, QVariant &data) const{
        CardUseStruct use = data.value<CardUseStruct>();
        QString prompt = "@liuli:" + use.from->objectName();
        room->setPlayerFlag(use.from, "LiuliSlashSource");
        // a temp nasty trick
        daqiao->tag["liuli-card"] = QVariant::fromValue((CardStar)use.card); // for the server (AI)
        room->setPlayerProperty(daqiao, "liuli", use.card->toString()); // for the client (UI)
        if (room->askForUseCard(daqiao, "@@liuli", prompt, -1, Card::MethodDiscard))
            return true;
        else {
            daqiao->tag.remove("liuli-card");
            room->setPlayerProperty(daqiao, "liuli", QString());
            room->setPlayerFlag(use.from, "-LiuliSlashSource");
        }
        return false;
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *daqiao, QVariant &data) const{
        CardUseStruct use = data.value<CardUseStruct>();
        daqiao->tag.remove("liuli-card");
        room->setPlayerProperty(daqiao, "liuli", QString());
        room->setPlayerFlag(use.from, "-LiuliSlashSource");
        QList<ServerPlayer *> players = room->getOtherPlayers(daqiao);
        foreach (ServerPlayer *p, players) {
            if (p->hasFlag("LiuliTarget")) {
                p->setFlags("-LiuliTarget");
                use.to.removeOne(daqiao);
                use.to.append(p);
                room->sortByActionOrder(use.to);
                data = QVariant::fromValue(use);
                room->getThread()->trigger(TargetConfirming, room, p, data);
                return false;
            }
        }

        return false;
    }
};

class Qianxun: public TriggerSkill {
public:
    Qianxun(): TriggerSkill("qianxun") {
        events << TargetConfirming;
        frequency = Compulsory;
    }

    virtual bool cost(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const{
        CardUseStruct use = data.value<CardUseStruct>();
        if (!use.card || use.card->getTypeId() != Card::TypeTrick)
            return false;
        if (!use.card->isKindOf("Snatch") && !use.card->isKindOf("Indulgence")) return false;
        if (player->hasShownSkill(this)) return true;
        return player->askForSkillInvoke(objectName());
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const{
        CardUseStruct use = data.value<CardUseStruct>();
        room->broadcastSkillInvoke(objectName());
        room->notifySkillInvoked(player, objectName());
        LogMessage log;
        if (use.from) {
            log.type = "$CancelTarget";
            log.from = use.from;
        } else {
            log.type = "$CancelTargetNoUser";
        }
        log.to = use.to;
        log.arg = use.card->objectName();
        room->sendLog(log);


        use.to.removeOne(player);
        data = QVariant::fromValue(use);
        return false;
    }
};

class DuoshiViewAsSkill: public OneCardViewAsSkill {
public:
    DuoshiViewAsSkill(): OneCardViewAsSkill("duoshi") {
        filter_pattern = ".|red|.|hand!";
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        return player->getMark("duoshi") < 4;
    }

    virtual const Card *viewAs(const Card *originalcard) const{
        AwaitExhausted *await = new AwaitExhausted(originalcard->getSuit(), originalcard->getNumber());
        await->addSubcard(originalcard->getId());
        await->setSkillName("duoshi");
        await->setShowSkill(objectName());
        return await;
    }
};

class Duoshi: public TriggerSkill {
public:
    Duoshi():TriggerSkill("duoshi") {
        events << CardUsed;
        view_as_skill = new DuoshiViewAsSkill;
    }

    virtual bool triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *ask_who) const {
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card && use.card->isKindOf("AwaitExhausted") && use.card->getSkillName() == "duoshi")
            room->addPlayerMark(player, "duoshi");
        return false;
    }
};

JieyinCard::JieyinCard() {
}

bool JieyinCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const{
    if (!targets.isEmpty())
        return false;

    return to_select->isMale() && to_select->isWounded() && to_select != Self;
}

void JieyinCard::onEffect(const CardEffectStruct &effect) const{
    Room *room = effect.from->getRoom();
    RecoverStruct recover;
    recover.card = this;
    recover.who = effect.from;

    room->recover(effect.from, recover, true);
    room->recover(effect.to, recover, true);
}

class Jieyin: public ViewAsSkill {
public:
    Jieyin(): ViewAsSkill("jieyin") {
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        return player->getHandcardNum() >= 2 && !player->hasUsed("JieyinCard");
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const{
        if (selected.length() > 1 || Self->isJilei(to_select))
            return false;

        return !to_select->isEquipped();
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const{
        if (cards.length() != 2)
            return NULL;

        JieyinCard *jieyin_card = new JieyinCard();
        jieyin_card->addSubcards(cards);
        jieyin_card->setShowSkill(objectName());
        return jieyin_card;
    }
};

class Xiaoji: public TriggerSkill {
public:
    Xiaoji(): TriggerSkill("xiaoji") {
        events << CardsMoveOneTime;
        frequency = Frequent;
    }

    virtual bool triggerable(TriggerEvent, Room *room, ServerPlayer *sunshangxiang, QVariant &data, ServerPlayer *ask_who) const{
        CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
        if (move.from == sunshangxiang && move.from_places.contains(Player::PlaceEquip)) {
            for (int i = 0; i < move.card_ids.size(); i++) {
                if (!sunshangxiang->isAlive())
                    return false;
                if (move.from_places[i] == Player::PlaceEquip)
                    return true;
            }
        }
        return false;
    }

    virtual bool cost(TriggerEvent, Room *room, ServerPlayer *sunshangxiang, QVariant &data) const{
        return room->askForSkillInvoke(sunshangxiang, objectName());
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *sunshangxiang, QVariant &data) const{
        room->broadcastSkillInvoke(objectName());
        sunshangxiang->drawCards(2);

        return false;
    }
};

class Yinghun: public PhaseChangeSkill {
public:
    Yinghun(): PhaseChangeSkill("yinghun") {
    }

    virtual bool triggerable(TriggerEvent, Room *room, ServerPlayer *target, QVariant &data, ServerPlayer *ask_who) const{
        return PhaseChangeSkill::triggerable(target)
               && target->getPhase() == Player::Start
               && target->isWounded();
    }
	
    virtual bool cost(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const{
        ServerPlayer *to = room->askForPlayerChosen(player, room->getOtherPlayers(player), objectName(), "yinghun-invoke", true, true);
        if (to) {
            player->tag["yinghun_target"] = QVariant::fromValue(to);
            return true;
        }
        return false;
    }

    virtual bool onPhaseChange(ServerPlayer *sunjian) const{
        Room *room = sunjian->getRoom();
        PlayerStar to = sunjian->tag["yinghun_target"].value<PlayerStar>();
        if (to) {
            int x = sunjian->getLostHp();

            int index = 1;
            if (!sunjian->hasInnateSkill("yinghun") && sunjian->hasSkill("hunzi"))
                index += 2;

            if (x == 1) {
                room->broadcastSkillInvoke(objectName(), index);

                to->drawCards(1);
                room->askForDiscard(to, objectName(), 1, 1, false, true);
            } else {
                to->setFlags("YinghunTarget");
                QString choice = room->askForChoice(sunjian, objectName(), "d1tx+dxt1");
                to->setFlags("-YinghunTarget");
                if (choice == "d1tx") {
                    room->broadcastSkillInvoke(objectName(), index + 1);

                    to->drawCards(1);
                    room->askForDiscard(to, objectName(), x, x, false, true);
                } else {
                    room->broadcastSkillInvoke(objectName(), index);

                    to->drawCards(x);
                    room->askForDiscard(to, objectName(), 1, 1, false, true);
                }
            }
        }
        return false;
    }
};

TianxiangCard::TianxiangCard() {
}

void TianxiangCard::onEffect(const CardEffectStruct &effect) const{
    Room *room = effect.to->getRoom();

    effect.to->addMark("TianxiangTarget");
    DamageStruct damage = effect.from->tag.value("TianxiangDamage").value<DamageStruct>();

    if (damage.card && damage.card->isKindOf("Slash"))
        effect.from->removeQinggangTag(damage.card);

    damage.to = effect.to;
    damage.transfer = true;
    try {
        room->damage(damage);
    }
    catch (TriggerEvent triggerEvent) {
        if (triggerEvent == TurnBroken || triggerEvent == StageChange)
            effect.to->removeMark("TianxiangTarget");
        throw triggerEvent;
    }
}

class TianxiangViewAsSkill: public OneCardViewAsSkill {
public:
    TianxiangViewAsSkill(): OneCardViewAsSkill("tianxiang") {
        filter_pattern = ".|heart|.|hand!";
        response_pattern = "@@tianxiang";
    }

    virtual const Card *viewAs(const Card *originalCard) const{
        TianxiangCard *tianxiangCard = new TianxiangCard;
        tianxiangCard->addSubcard(originalCard);
        tianxiangCard->setShowSkill(objectName());
        return tianxiangCard;
    }
};

class Tianxiang: public TriggerSkill {
public:
    Tianxiang(): TriggerSkill("tianxiang") {
        events << DamageInflicted;
        view_as_skill = new TianxiangViewAsSkill;
    }

    virtual bool cost(TriggerEvent triggerEvent, Room *room, ServerPlayer *xiaoqiao, QVariant &data) const{
        if (xiaoqiao->canDiscard(xiaoqiao, "h")) {
            xiaoqiao->tag["TianxiangDamage"] = data;
            return room->askForUseCard(xiaoqiao, "@@tianxiang", "@tianxiang-card", -1, Card::MethodDiscard);
        }
        return false;
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *xiaoqiao, QVariant &data) const{
        return true;
    }
};

class TianxiangDraw: public TriggerSkill {
public:
    TianxiangDraw(): TriggerSkill("#tianxiang") {
        events << DamageComplete;
    }

    virtual bool triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *ask_who /* = NULL */) const{
        return player != NULL;
    }

    virtual bool cost(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const{
        DamageStruct damage = data.value<DamageStruct>();
        if (player->isAlive() && player->getMark("TianxiangTarget") > 0 && damage.transfer) {
            player->drawCards(player->getLostHp());
            player->removeMark("TianxiangTarget");
        }

        return false;
    }

    virtual bool effect(TriggerEvent, Room *, ServerPlayer *player, QVariant &data) const{
        return false;
    }
};

class Hongyan: public FilterSkill {
public:
    Hongyan(): FilterSkill("hongyan") {
    }

    static WrappedCard *changeToHeart(int cardId) {
        WrappedCard *new_card = Sanguosha->getWrappedCard(cardId);
        new_card->setSkillName("hongyan");
        new_card->setSuit(Card::Heart);
        new_card->setModified(true);
        return new_card;
    }

    virtual bool viewFilter(const Card *to_select) const{
        Room *room = Sanguosha->currentRoom();
        foreach (ServerPlayer *p, room->getPlayers())
            if (p->ownSkill(objectName()) && p->hasShownSkill(this))
                return to_select->getSuit() == Card::Spade;
        return false;
    }

    virtual const Card *viewAs(const Card *originalCard) const{
        return changeToHeart(originalCard->getEffectiveId());
    }

    virtual int getEffectIndex(const ServerPlayer *, const Card *) const{
        return -2;
    }
};

void StandardPackage::addWuGenerals()
{
    General *sunquan = new General(this, "sunquan", "wu"); // WU 001
    sunquan->addCompanion("zhoutai");
    sunquan->addSkill(new Zhiheng);

    General *ganning = new General(this, "ganning", "wu"); // WU 002
    ganning->addSkill(new Qixi);

    General *lvmeng = new General(this, "lvmeng", "wu"); // WU 003
    lvmeng->addSkill(new Keji);

    General *huanggai = new General(this, "huanggai", "wu"); // WU 004
    huanggai->addSkill(new Kurou);

    General *zhouyu = new General(this, "zhouyu", "wu", 3); // WU 005
    zhouyu->addCompanion("huanggai");
    zhouyu->addCompanion("xiaoqiao");
    zhouyu->addSkill(new Yingzi);
    zhouyu->addSkill(new Fanjian);

    General *daqiao = new General(this, "daqiao", "wu", 3, false); // WU 006
    daqiao->addCompanion("xiaoqiao");
    daqiao->addSkill(new Guose);
    daqiao->addSkill(new Liuli);

    General *luxun = new General(this, "luxun", "wu", 3); // WU 007
    luxun->addSkill(new Qianxun);
    luxun->addSkill(new Duoshi);

    General *sunshangxiang = new General(this, "sunshangxiang", "wu", 3, false); // WU 008
    sunshangxiang->addSkill(new Jieyin);
    sunshangxiang->addSkill(new Xiaoji);

    General *sunjian = new General(this, "sunjian", "wu"); // WU 009
    sunjian->addSkill(new Yinghun);

    General *xiaoqiao = new General(this, "xiaoqiao", "wu", 3, false); // WU 011
    xiaoqiao->addSkill(new Tianxiang);
    xiaoqiao->addSkill(new TianxiangDraw);
    xiaoqiao->addSkill(new Hongyan);
    related_skills.insertMulti("tianxiang", "#tianxiang");

    addMetaObject<ZhihengCard>();
    addMetaObject<KurouCard>();
    addMetaObject<FanjianCard>();
    addMetaObject<LiuliCard>();
    addMetaObject<JieyinCard>();
    addMetaObject<TianxiangCard>();

    skills << new KejiGlobal;
}