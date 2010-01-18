/*
 * Copyright (C) 1997 Rodolfo Borges <barrett@labma.ufrj.br>
 * Copyright (C) 1998-2009 Stephan Kulow <coolo@kde.org>
 * Copyright (C) 2010 Parker Coates <parker.coates@kdemail.net>
 *
 * License of original code:
 * -------------------------------------------------------------------------
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 *   This file is provided AS IS with no warranties of any kind.  The author
 *   shall have no liability with respect to the infringement of copyrights,
 *   trade secrets or any patents by this file or any part thereof.  In no
 *   event will the author be liable for any lost revenue or profits or
 *   other special, indirect and consequential damages.
 * -------------------------------------------------------------------------
 *
 * License of modifications/additions made after 2009-01-01:
 * -------------------------------------------------------------------------
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License as
 *   published by the Free Software Foundation; either version 2 of 
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * -------------------------------------------------------------------------
 */

#include "freecell.h"

#include "dealerinfo.h"
#include "pileutils.h"
#include "speeds.h"
#include "patsolve/freecellsolver.h"

#include <KDebug>
#include <KLocale>

#include <QtCore/QList>
#include <QtCore/QTimer>


const int CHUNKSIZE = 100;

void FreecellPile::moveCards(QList<Card*> &c, Pile *to)
{
    if (c.count() == 1) {
        Pile::moveCards(c, to);
        return;
    }
    Freecell *b = dynamic_cast<Freecell*>(scene());
    PatPile *p = dynamic_cast<PatPile*>(to);
    if (b && p) {
        b->moveCards(c, this, p);
    }
}

//-------------------------------------------------------------------------//

Freecell::Freecell()
    : DealerScene()
{
    setupDeck( new CardDeck() );

    const qreal topRowDist = 1.08;
    const qreal bottomRowDist = 1.13;
    const qreal targetOffsetDist = ( 7 * bottomRowDist + 1 ) - ( 3 * topRowDist + 1 );

    for (int i = 0; i < 8; i++)
    {
        FreecellPile *p = new FreecellPile(1 + i, QString( "store%1" ).arg( i ));
        store[i] = p;
        p->setPileRole(PatPile::Tableau);
        p->setPilePos( bottomRowDist * i, 1.3 );
        p->setReservedSpace( QSizeF( 1.0, 3.5 ) );
        addPile(p);
    }

    for (int i = 0; i < 4; i++)
    {
        freecell[i] = new PatPile (1 + 8 + i, QString( "freecell%1" ).arg( i ));
        freecell[i]->setPileRole(PatPile::Cell);
        freecell[i]->setPilePos(topRowDist * i, 0);
        addPile(freecell[i]);
    }

    for (int i = 0; i < 4; i++)
    {
        target[i] = new PatPile(1 + 8 + 4 + i, QString( "target%1" ).arg( i ));
        target[i]->setPileRole(PatPile::Foundation);
        target[i]->setFoundation(true);
        target[i]->setPilePos(targetOffsetDist + topRowDist * i, 0);
        target[i]->setSpread(0, 0);
        addPile(target[i]);
    }

    setActions(DealerScene::Demo | DealerScene::Hint);
    setSolver( new FreecellSolver( this ) );
    setNeededFutureMoves( 4 ); // reserve some
}

Freecell::~Freecell()
{
}
//-------------------------------------------------------------------------//

void Freecell::restart()
{
    deck()->returnAllCards();
    deck()->shuffle( gameNumber() );
    deal();
}

void Freecell::countFreeCells(int &free_cells, int &free_stores) const
{
    free_cells = 0;
    free_stores = 0;

    for (int i = 0; i < 4; i++)
        if (freecell[i]->isEmpty()) free_cells++;
    for (int i = 0; i < 8; i++)
        if (store[i]->isEmpty()) free_stores++;
}

void Freecell::moveCards(QList<Card*> &c, FreecellPile *from, PatPile *to)
{
    Q_ASSERT(c.count() > 1);

    from->moveCardsBack(c);
    waitfor = c.last();
    connect(waitfor, SIGNAL(animationStopped(Card*)), SLOT(waitForMoving(Card*)));

    QList<PatPile*> fcs;

    for (int i = 0; i < 4; i++)
        if (freecell[i]->isEmpty()) fcs.append(freecell[i]);

    QList<PatPile*> fss;

    for (int i = 0; i < 8; i++)
        if (store[i]->isEmpty() && to != store[i]) fss.append(store[i]);

    if (fcs.count() == 0) {
        Q_ASSERT(fss.count());
        fcs.append(fss.last());
        fss.erase(--fss.end());
    }
    while (moves.count()) { delete moves.first(); moves.erase(moves.begin()); }

    sum_moves = 0;
    current_weight = 1000;
    QList<StandardCard*> cards = castCardList(c);
    movePileToPile(cards, to, fss, fcs, 0, c.count(), 0);

    if (!waitfor->animated())
        QTimer::singleShot(0, this, SLOT(startMoving()));
}

struct MoveAway {
    PatPile *firstfree;
    int start;
    int count;
};

void Freecell::movePileToPile(QList<StandardCard*> &c, PatPile *to, QList<PatPile*> & fss, QList<PatPile*> & fcs, int start, int count, int debug_level)
{
    kDebug() << debug_level << "movePileToPile" << c.count() << " " << start  << " " << count;
    int moveaway = 0;
    if (count > fcs.count() + 1) {
        moveaway = (fcs.count() + 1);
        while (moveaway * 2 < count)
            moveaway <<= 1;
    }
    kDebug() << debug_level << "moveaway" << moveaway;

    QList<MoveAway> moves_away;

    if (count - moveaway < (fcs.count() + 1) && (count <= 2 * (fcs.count() + 1))) {
        moveaway = count - (fcs.count() + 1);
    }
    while (count > fcs.count() + 1) {
        Q_ASSERT(fss.count());
        MoveAway ma;
        ma.firstfree = fss[0];
        ma.start = start;
        ma.count = moveaway;
        moves_away.append(ma);
        fss.erase(fss.begin());
        movePileToPile(c, ma.firstfree, fss, fcs, start, moveaway, debug_level + 1);
        start += moveaway;
        count -= moveaway;
        moveaway >>= 1;
        if ((count > (fcs.count() + 1)) && (count <= 2 * (fcs.count() + 1)))
            moveaway = count - (fcs.count() + 1);
    }
    int moving = qMin(count, qMin(c.count() - start, fcs.count() + 1));
    Q_ASSERT(moving);

    for (int i = 0; i < moving - 1; i++) {
        moves.append(new MoveHint(c[c.count() - i - 1 - start], fcs[i], current_weight));
        sum_moves += current_weight;
        current_weight *= 0.9;
    }
    moves.append(new MoveHint(c[c.count() - start - 1 - (moving - 1)], to, current_weight));
    sum_moves += current_weight;
    current_weight *= 0.9;

    for (int i = moving - 2; i >= 0; --i) {
        moves.append(new MoveHint(c[c.count() - i - 1 - start], to, current_weight));
        sum_moves += current_weight;
        current_weight *= 0.9;
    }

    while (moves_away.count())
    {
        MoveAway ma = moves_away.last();
        moves_away.erase(--moves_away.end());
        movePileToPile(c, to, fss, fcs, ma.start, ma.count, debug_level + 1);
        fss.append(ma.firstfree);
    }
}

void Freecell::startMoving()
{
    kDebug() << "startMoving\n";
    if (moves.isEmpty()) {
        takeState();
        return;
    }

    MoveHint *mh = moves.first();
    moves.erase(moves.begin());
    QList<Card*> empty;
    empty.append(mh->card());
    Q_ASSERT(mh->card() == mh->card()->source()->top());
    Q_ASSERT(allowedToAdd(mh->pile(), empty));
    mh->pile()->add(mh->card());

    int duration = qMax( DURATION_MOVEBACK * mh->priority() / 1000, 1 );
    mh->pile()->moveCardsBack(empty, duration );
    waitfor = mh->card();
    kDebug() << "wait for moving end" << mh->card()->rank() << " " << mh->card()->suit() << mh->priority();
    connect(mh->card(), SIGNAL(animationStopped(Card*)), SLOT(waitForMoving(Card*)));
    delete mh;
}

void Freecell::newDemoMove(Card *m)
{
    DealerScene::newDemoMove(m);
    if (m != m->source()->top())
        m->disconnect( this );
}

void Freecell::waitForMoving(Card *c)
{
    if (waitfor != c)
        return;
    c->disconnect(this);
    startMoving();
}

bool Freecell::cardDoubleClicked(Card *c)
{
    // target move
    if (DealerScene::cardDoubleClicked(c))
        return true;

    if (c->animated())
        return false;

    if (c == c->source()->top() && c->realFace())
    {
        for (int i = 0; i < 4; i++)
        {
            if (freecell[i]->isEmpty())
            {
                c->source()->moveCards(QList<Card*>() << c, freecell[i]);
                onGameStateAlteredByUser();
                return true;
            }
        }
    }
    return false;
}

bool Freecell::canPutStore(const PatPile *c1, const QList<StandardCard*> &c2) const
{
    int fcs, fss;
    countFreeCells(fcs, fss);

    if (c1->isEmpty()) // destination is empty
        fss--;

    if (int(c2.count()) > ((fcs)+1)<<fss)
        return false;

    // ok if the target is empty
    if (c1->isEmpty())
        return true;

    StandardCard *c = c2.first(); // we assume there are only valid sequences

    // ok if in sequence, alternate colors
    return ((c1->top()->rank() == (c->rank()+1))
            && (c1->top()->isRed() != c->isRed()));
}

bool Freecell::checkAdd(const PatPile * pile, const QList<StandardCard*> & oldCards, const QList<StandardCard*> & newCards) const
{
    switch (pile->pileRole())
    {
    case PatPile::Tableau:
        return canPutStore(pile, newCards);
    case PatPile::Cell:
        return oldCards.isEmpty() && newCards.size() == 1;
    case PatPile::Foundation:
        return checkAddSameSuitAscendingFromAce(oldCards, newCards);
    default:
        return false;
    }
}

bool Freecell::checkRemove(const PatPile * pile, const QList<StandardCard*> & cards) const
{
    switch (pile->pileRole())
    {
    case PatPile::Tableau:
        return isAlternateColorDescending(cards);
    case PatPile::Cell:
        return cards.first() == pile->top();
    case PatPile::Foundation:
    default:
        return false;
    }
}

void Freecell::getHints()
{
    getSolverHints();

    if ( demoActive() )
        return;

    foreach (PatPile * store, patPiles())
    {
        if (store->isEmpty())
            continue;

        QList<StandardCard*> cards = store->cards();
        while (cards.count() && !cards.first()->realFace())
            cards.erase(cards.begin());

        QList<StandardCard*>::Iterator iti = cards.begin();
        while (iti != cards.end())
        {
            if (allowedToRemove(store, (*iti)))
            {
                foreach (PatPile * dest, patPiles())
                {
                    int cardIndex = store->indexOf(*iti);
                    if (cardIndex == 0 && dest->isEmpty() && !dest->isFoundation())
                        continue;

                    if (!checkAdd(dest, dest->cards(), cards))
                        continue;

                    if ( dest->isFoundation() ) // taken care by solver
                        continue;

                    QList<StandardCard*> cardsBelow = cards.mid(0, cardIndex);
                    // if it could be here as well, then it's no use
                    if ((cardsBelow.isEmpty() && !dest->isEmpty()) || !checkAdd(store, cardsBelow, cards))
                    {
                        newHint(new MoveHint(*iti, dest, 0));
                    }
                    else if (checkPrefering( dest, dest->cards(), cards )
                             && !checkPrefering( store, cardsBelow, cards ))
                    { // if checkPrefers says so, we add it nonetheless
                        newHint(new MoveHint(*iti, dest, 0));
                    }
                }
            }
            cards.erase(iti);
            iti = cards.begin();
        }
    }
}

//-------------------------------------------------------------------------//

void Freecell::deal()
{
    int column = 0;
    while (deck()->hasUndealtCards())
    {
        Card *c = deck()->takeCard();
        addCardForDeal( store[column], c, true, store[0]->pos() );
        column = (column + 1) % 8;
    }
    startDealAnimation();
}



static class FreecellDealerInfo : public DealerInfo
{
public:
    FreecellDealerInfo()
      : DealerInfo(I18N_NOOP("Freecell"), FreecellId)
    {}

    virtual DealerScene *createGame() const
    {
        return new Freecell();
    }
} freecellDealerInfo;


#include "freecell.moc"
