/***********************-*-C++-*-********

  klondike.cpp  implements a patience card game

     Copyright (C) 1995  Paul Olav Tvete

 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.

//
// 7 positions, alternating red and black
//

****************************************/

#include "klondike.h"
#include <klocale.h>
#include <kmessagebox.h>
#include <card.h>
#include <kmainwindow.h>
#include <deck.h>
#include <pile.h>
#include <kdebug.h>
#include <assert.h>
#include <kaction.h>

Klondike::Klondike( bool easy, KMainWindow* parent, const char* _name )
  : Dealer( parent, _name )
{
    deck = new Deck(13, this);
    deck->move(10, 10);

    EasyRules = easy;

    pile = new Pile( 0, this);
    pile->move(100, 10);
    pile->setAddFlags(Pile::disallow);
    pile->setRemoveFlags(Pile::Default);

    for( int i = 0; i < 7; i++ ) {
        play[ i ] = new Pile( i + 5, this);
        play[i]->move(10 + 85 * i, 130);
        play[i]->setAddFlags(Pile::addSpread | Pile::several);
        play[i]->setRemoveFlags(Pile::several | Pile::autoTurnTop | Pile::wholeColumn);
        play[i]->setCheckIndex(0);
    }

    for( int i = 0; i < 4; i++ ) {
        target[ i ] = new Pile( i + 1, this );
        target[i]->move(265 + i * 85, 10);
        target[i]->setAddFlags(Pile::Default);
        if (!EasyRules)
            target[i]->setRemoveFlags(Pile::disallow);
        target[i]->setCheckIndex(1);
        target[i]->setTarget(true);
    }

    QList<KAction> actions;
    ahint = new KAction( i18n("&Hint"), QString::fromLatin1("wizard"), 0, this,
                         SLOT(hint()),
                         parent->actionCollection(), "game_hint");
    actions.append(ahint);
    parent->guiFactory()->plugActionList( parent, QString::fromLatin1("game_actions"), actions);

    deal();
}

void Klondike::changeDiffLevel( int l ) {
    if ( EasyRules == (l == 0) )
        return;

    int r = KMessageBox::warningContinueCancel(this,
                                               i18n("This will end the current game.\n"
                                                    "Are you sure you want to do this?"),
                                               QString::null,
                                               i18n("OK"));
    if(r == KMessageBox::Cancel) {
        // TODO cb->setCurrentItem(1-cb->currentItem());
        return;
    }

    EasyRules = (bool)(l == 0);
    restart();
}

void Klondike::hint() {
    unmarkAll();

    Card* t[7];
    for(int i=0; i<7;i++)
        t[i] = play[i]->top();

    for(int i=0; i<7; i++)
    {
        CardList list = play[i]->cards();

        for (CardList::ConstIterator it = list.begin(); it != list.end(); ++it)
        {
            if (!(*it)->isFaceUp())
                continue;

            CardList empty;
            empty.append(*it);

            for (int j = 0; j < 7; j++)
            {
                if (i == j)
                    continue;

                if (altStep(play[j], empty)) {
                    if (((*it)->value() != Card::King) || it != list.begin()) {
                        mark(*it);
                        break;
                    }
                }
            }
            break; // the first face up
        }

        if (findTarget(play[i]->top()))
            mark(play[i]->top());
    }
    if (!pile->isEmpty())
    {
        if (findTarget(pile->top()))
        {
            mark(pile->top());
        } else
        {
            for (int j = 0; j < 7; j++)
            {
                CardList empty;
                empty.append(pile->top());
                if (altStep(play[j], empty)) {
                    mark(pile->top());
                    break;
                }
            }
        }
    }
}

void Klondike::restart() {
    kdDebug() << "restart\n";
    deck->collectAndShuffle();
    deal();
}

void Klondike::deal3()
{
    int draw;

    if ( EasyRules) {
        draw = 1;
    } else {
        draw = 3;
    }

    if (deck->isEmpty())
    {
        redeal();
        return;
    }
    for (int flipped = 0; flipped < draw ; ++flipped) {

        Card *item = deck->nextCard();
        if (!item) {
            kdDebug() << "deck empty!!!\n";
            return;
        }
        pile->add(item, true, false); // facedown, nospread
        // move back to flip
        item->move(deck->x(), deck->y());

        item->flipTo( pile->x(), pile->y(), 8 * (flipped + 1) );
    }

}


//  Add cards from  pile to deck, in reverse direction
void Klondike::redeal() {

    CardList pilecards = pile->cards();
    if (EasyRules)
        // the remaining cards in deck should be on top
        // of the new deck
        pilecards += deck->cards();

    for (CardList::Iterator it = pilecards.fromLast(); it != pilecards.end(); --it)
    {
        (*it)->setAnimated(false);
        deck->add(*it, true, false); // facedown, nospread
    }

}

void Klondike::deal() {
    for(int round=0; round < 7; round++)
        for (int i = round; i < 7; i++ )
            play[i]->add(deck->nextCard(), i != round, true);
    canvas()->update();
}

bool Klondike::step1( const Pile* c1, const CardList& c2 ) const
{

    if (c2.isEmpty()) {
        return false;
    }
    Card *top = c1->top();

    Card *newone = c2.first();
    if (!top) {
        return (newone->value() == Card::Ace);
    }

    bool t = ((newone->value() == top->value() + 1)
               && (top->suit() == newone->suit()));
    return t;
}

bool Klondike::altStep(  const Pile* c1, const CardList& c2 ) const
{
    if (c2.isEmpty()) {
        return false;
    }
    Card *top = c1->top();

    Card *newone = c2.first();
    if (!top) {
        return (newone->value() == Card::King);
    }

    bool t = ((newone->value() == top->value() - 1)
               && (top->isRed() != newone->isRed()));
    return t;
}

bool Klondike::checkAdd   ( int checkIndex, const Pile *c1,
                            const CardList& c2) const
{
    if (checkIndex == 0)
        return altStep(c1, c2);
    else
        return step1(c1, c2);
}

void Klondike::cardClicked(Card *c) {
    kdDebug() << "card clicked " << c->name() << endl;

    Dealer::cardClicked(c);
    if (c->source() == deck) {
        pileClicked(deck);
        return;
    }

}

void Klondike::pileClicked(Pile *c) {
    kdDebug() << "pile clicked " << endl;
    Dealer::pileClicked(c);

    if (c == deck) {
        deal3();
    }
}

void Klondike::cardDblClicked(Card *c) {
    kdDebug() << "card dbl clicked " << c->name() << endl;

    Dealer::cardDblClicked(c);

    if (c->animated())
        return;

    if (c == c->source()->top() && c->realFace()) {
        Pile *tgt = findTarget(c);
        if (tgt) {
            CardList empty;
            empty.append(c);
            c->source()->moveCards(empty, tgt);
            canvas()->update();
        }
    }
}

bool Klondike::tryToDrop(Card *t)
{
    if (!t || !t->realFace() || t->takenDown())
        return false;

    kdDebug() << "tryToDrop " << t->name() << endl;

    if (t->value() <= Card::Two || t->value() <= lowest_card[t->isRed() ? 1 : 0] + 1)
    {
        Pile *tgt = findTarget(t);
        if (tgt) {
            kdDebug() << "found target for " << t->name() << endl;
            CardList empty;
            empty.append(t);
            t->setAnimated(false);
            t->turn(true);
            int x = int(t->x());
            int y = int(t->y());
            t->source()->moveCards(empty, tgt);
            t->move(x, y);
            t->animatedMove(t->source()->x(), t->source()->y(), t->z(), 8);
            return true;
        }
    }
    return false;
}

bool Klondike::startAutoDrop()
{
    int tops[4] = {0, 0, 0, 0};

    for( int i = 0; i < 4; i++ )
    {
        Card *c = target[i]->top();
        if (!c) continue;
        tops[c->suit() - 1] = c->value();
    }

    lowest_card[0] = static_cast<Card::Values>(QMIN(tops[1], tops[2])); // red
    lowest_card[1] = static_cast<Card::Values>(QMIN(tops[0], tops[3])); //black

    kdDebug() << "startAutoDrop red:" << lowest_card[0] << " black:" << lowest_card[1] << endl;

    for( int i = 0; i < 7; i++ )
    {
        if (tryToDrop(play[i]->top()))
            return true;
    }
    if (tryToDrop(pile->top())) {
        if (pile->isEmpty())
            deal3();
        return true;
    }

    return false;
}

static class LocalDealerInfo0 : public DealerInfo
{
public:
    LocalDealerInfo0() : DealerInfo(I18N_NOOP("&Klondike"), 0) {}
    virtual Dealer *createGame(KMainWindow *parent) { return new Klondike(true, parent); }
} ldi0;

#include "klondike.moc"
