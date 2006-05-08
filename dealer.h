#ifndef _DEALER_H_
#define _DEALER_H_

#include "pile.h"
#include "hint.h"
#include <krandomsequence.h>

#include <QWheelEvent>
#include <QPixmap>
#include <Q3PtrList>
#include <QList>
#include <QResizeEvent>
#include <QMouseEvent>
class QPixmap;
class QDomDocument;
class KMainWindow;
class Dealer;
class DealerInfo;
class KAction;
class KSelectAction;
class KToggleAction;

class DealerInfoList {
public:
    static DealerInfoList *self();
    void add(DealerInfo *);

    const QList<DealerInfo*> games() const { return list; }
private:
    QList<DealerInfo*> list;
    static DealerInfoList *_self;
};

class DealerInfo {
public:
    DealerInfo(const char *_name, int _index)
        : name(_name),
          gameindex(_index)
{
    DealerInfoList::self()->add(this);
}
	virtual ~DealerInfo(){}
    const char *name;
    int gameindex;
    virtual Dealer *createGame(KMainWindow *parent) = 0;
};

class CardState;

typedef QList<CardState> CardStateList;

struct State
{
    CardStateList cards;
    QString gameData;
};


/***************************************************************

  Dealer -- abstract base class of all varieties of patience

***************************************************************/
class Dealer: public Q3CanvasView
{
    Q_OBJECT

public:

    Dealer( KMainWindow* parent = 0, const char* name = 0 );
    virtual ~Dealer();

    static const Dealer *instance();

    void enlargeCanvas(Q3CanvasRectangle *c);
    void setGameNumber(long gmn);
    long gameNumber() const;

    virtual bool isGameWon() const;
    virtual bool isGameLost() const;

    void setViewSize(const QSize &size);

    void addPile(Pile *p);
    void removePile(Pile *p);

    virtual bool checkRemove( int checkIndex, const Pile *c1, const Card *c) const;
    virtual bool checkAdd   ( int checkIndex, const Pile *c1, const CardList& c2) const;
    virtual bool checkPrefering( int checkIndex, const Pile *c1, const CardList& c2) const;

    virtual Card *demoNewCards();

    virtual void setupActions();

    bool demoActive() const;

    void drawPile(QPixmap &, Pile *p, bool selected);

    QColor midColor() const { return _midcolor; }
    void setBackgroundPixmap(const QPixmap &background, const QColor &midcolor);

    void saveGame(QDomDocument &doc);
    void openGame(QDomDocument &doc);

    void setGameId(int id) { _id = id; }
	 int gameId() const { return _id; }

    void setTakeTargetForHints(bool e) { takeTargets = e; }
    bool takeTargetForHints() const { return takeTargets; }

    bool isMoving(Card *c) const;

    virtual QSize minimumCardSize() const;
    virtual void resizeEvent(QResizeEvent *);

    int freeCells() const;

    QString anchorName() const;
    void setAnchorName(const QString &name);

    void setAutoDropEnabled(bool a);
    bool autoDrop() const { return _autodrop; }

    int getMoves() const { return undoList.count(); }

public slots:

    // restart is pure virtual, so we need something else
    virtual void startNew();
    void undo();
    virtual void takeState();
    virtual bool startAutoDrop();
    void hint();
    void slotTakeState(Card *c);

signals:
    void undoPossible(bool poss);
    void gameWon(bool withhelp);
    void gameLost();
    void saveGame(); // emergency
    void gameInfo(const QString &info);
    void updateMoves();

public slots:
    virtual void demo();
    void waitForDemo(Card *);
    void toggleDemo();
    virtual void stopDemo();
    void waitForAutoDrop(Card *);

protected:

    enum { None = 0, Hint = 1, Demo = 2, Redeal = 4 } Actions;

    void setActions(int actions) { myActions = actions; }
    int actions() const { return myActions; }

    virtual void restart() = 0;

    virtual void contentsMousePressEvent(QMouseEvent* e);
    virtual void contentsMouseMoveEvent( QMouseEvent* );
    virtual void contentsMouseReleaseEvent( QMouseEvent* );
    virtual void contentsMouseDoubleClickEvent( QMouseEvent* );
    virtual void wheelEvent( QWheelEvent *e );

    void unmarkAll();
    void mark(Card *c);
    Pile *findTarget(Card *c);
    virtual bool cardClicked(Card *);
    virtual void pileClicked(Pile *);
    virtual bool cardDblClicked(Card *);
    void won();

    virtual void getHints();
    void newHint(MoveHint *mh);
    void clearHints();
    // it's not const because it changes the random seed
    virtual MoveHint *chooseHint();

    KMainWindow *parent() const;

    bool waiting() const { return _waiting != 0; }
    void setWaiting(bool w);

protected:
    PileList piles;

    State *getState();
    void setState(State *);

    // reimplement this to add game-specific information in the state structure
    virtual QString getGameState() const { return QString::null; }
    // reimplement this to use the game-specific information from the state structure
    virtual void setGameState( const QString & ) {}

    virtual void newDemoMove(Card *m);

    bool moved;
    CardList movingCards;
    Q3CanvasItemList marked;
    QPoint moving_start;
    Dealer( Dealer& );  // don't allow copies or assignments
    void operator = ( Dealer& );  // don't allow copies or assignments
    Q3Canvas myCanvas;
    QSize minsize;
    QSize viewsize;
    Q3PtrList<State> undoList;
    long gamenumber;
    QList<MoveHint*> hints;
    Card *towait;
    QTimer *demotimer;
    int myActions;
    bool toldAboutLostGame;

    KToggleAction *ademo;
    KAction *ahint, *aredeal;

    KRandomSequence randseq;
    QColor _midcolor;
    quint32 _id;
    bool takeTargets;
    bool _won;
    int _waiting;
    bool stop_demo_next;
    QString ac;
    static Dealer *s_instance;
    bool _autodrop;
    bool _gameRecorded;

private:
	 void countLoss();
	 void countGame();
};

#endif
