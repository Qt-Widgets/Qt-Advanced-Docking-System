#include "ads/SectionTitleWidget.h"

#include <QString>
#include <QApplication>
#include <QBoxLayout>
#include <QMouseEvent>
#include <QMimeData>
#include <QDrag>
#include <QCursor>
#include <QStyle>
#include <QSplitter>
#include <QPushButton>

#include <iostream>

#ifdef ADS_ANIMATIONS_ENABLED
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#endif

#include "ads/Internal.h"
#include "ads/DropOverlay.h"
#include "ads/SectionContent.h"
#include "ads/SectionWidget.h"
#include "ads/FloatingWidget.h"
#include <ads/MainContainerWidget.h>

#include <iostream>

ADS_NAMESPACE_BEGIN

SectionTitleWidget::SectionTitleWidget(SectionContent::RefPtr content, QWidget* parent) :
	QFrame(parent),
    m_Content(content),
    m_TabMoving(false),
    m_IsActiveTab(false)
{
	QBoxLayout* l = new QBoxLayout(QBoxLayout::LeftToRight);
	l->setContentsMargins(0, 0, 0, 0);
	l->setSpacing(0);
	l->addWidget(content->titleWidget());
	setLayout(l);
}


SectionTitleWidget::~SectionTitleWidget()
{
    layout()->removeWidget(m_Content->titleWidget());
}

bool SectionTitleWidget::isActiveTab() const
{
    return m_IsActiveTab;
}

void SectionTitleWidget::setActiveTab(bool active)
{
    if (active != m_IsActiveTab)
	{
        m_IsActiveTab = active;

		style()->unpolish(this);
		style()->polish(this);
		update();

		emit activeTabChanged();
	}
}

void SectionTitleWidget::mousePressEvent(QMouseEvent* ev)
{
	if (ev->button() == Qt::LeftButton)
	{
		ev->accept();
        m_DragStartMousePosition = ev->pos();
        m_DragStartGlobalMousePosition = ev->globalPos();
        m_DragStartPosition = mapToGlobal(this->pos());
		return;
	}
	QFrame::mousePressEvent(ev);
}

void SectionTitleWidget::mouseReleaseEvent(QMouseEvent* ev)
{
	SectionWidget* section = nullptr;
	MainContainerWidget* cw = findParentContainerWidget(this);
	std::cout << "SectionTitleWidget::mouseReleaseEvent" << std::endl;

	m_FloatingWidget.clear();
	// End of tab moving, change order now
	if (m_TabMoving && (section = findParentSectionWidget(this)) != nullptr)
	{
		// Find tab under mouse
		QPoint pos = ev->globalPos();
		pos = section->mapFromGlobal(pos);
        int fromIndex = section->indexOfContent(m_Content);
        int toIndex = section->indexOfContentByTitlePos(pos, this);
        qInfo() << "fromIndex: " << fromIndex << " toIndex: " << toIndex;
        if (-1 == toIndex)
        {
            toIndex = section->indexOfContent(section->contents().last());
        }
		section->moveContent(fromIndex, toIndex);
	}

    if (!m_DragStartMousePosition.isNull())
    {
		emit clicked();
    }

	// Reset
    m_DragStartMousePosition = QPoint();
    m_TabMoving = false;
	cw->m_SectionDropOverlay->hideDropOverlay();
	cw->hideContainerOverlay();
	QFrame::mouseReleaseEvent(ev);
}


void SectionTitleWidget::moveFloatingWidget(QMouseEvent* ev, MainContainerWidget* cw)
{
	std::cout << "SectionTitleWidget::moveFloatingWidget" << std::endl;
	/*const QPoint DragDistance = ev->globalPos() - m_DragStartGlobalMousePosition;
	const QPoint moveToGlobalPos = m_DragStartPosition + DragDistance;
    m_FloatingWidget->move(moveToGlobalPos);*/

	const QPoint moveToPos = ev->globalPos() - m_DragStartMousePosition;
    m_FloatingWidget->move(moveToPos);
}


void SectionTitleWidget::startFloating(QMouseEvent* ev, MainContainerWidget* cw, SectionWidget* sectionwidget)
{
	std::cout << "SectionTitleWidget::startFloating" << std::endl;
	QPoint moveToPos = ev->globalPos() - m_DragStartMousePosition;

    InternalContentData data;
    if (!sectionwidget->takeContent(m_Content->uid(), data))
    {
        qWarning() << "THIS SHOULD NOT HAPPEN!!" << m_Content->uid();
        return;
    }

    FloatingWidget* fw = new FloatingWidget(cw, data.content, data.titleWidget, data.contentWidget, cw);
    fw->resize(sectionwidget->size());
    fw->setObjectName("FloatingWidget");
    fw->startFloating(m_DragStartMousePosition);

    // Delete old section, if it is empty now.
    if (sectionwidget->contents().isEmpty())
    {
        delete sectionwidget;
        sectionwidget = NULL;
    }
    deleteEmptySplitter(cw);

    DropOverlay* ContainerDropOverlay = cw->dropOverlay();
	ContainerDropOverlay->setAllowedAreas(OuterAreas);
	ContainerDropOverlay->showDropOverlay(this);
	ContainerDropOverlay->raise();
}

/*
FloatingWidget* MainContainerWidget::startFloating(SectionWidget* sectionwidget, int ContentUid, const QPoint& TargetPos)
{
    // Create floating widget.
    InternalContentData data;
    if (!sectionwidget->takeContent(ContentUid, data))
    {
        qWarning() << "THIS SHOULD NOT HAPPEN!!" << ContentUid;
        return 0;
    }

    FloatingWidget* fw = new FloatingWidget(this, data.content, data.titleWidget, data.contentWidget, this);
    fw->resize(sectionwidget->size());
    fw->setObjectName("FloatingWidget");
    fw->startFloating(TargetPos);

    // Delete old section, if it is empty now.
    if (sectionwidget->contents().isEmpty())
    {
        delete sectionwidget;
        sectionwidget = NULL;
    }
    deleteEmptySplitter(this);

	m_ContainerDropOverlay->setAllowedAreas(OuterAreas);
	m_ContainerDropOverlay->showDropOverlay(this);
	m_ContainerDropOverlay->raise();
    return fw;
}*/


void SectionTitleWidget::moveTab(QMouseEvent* ev)
{
    ev->accept();
    int left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    QPoint moveToPos = mapToParent(ev->pos()) - m_DragStartMousePosition;
    moveToPos.setY(0/* + top*/);
    move(moveToPos);
}


bool SectionTitleWidget::isDraggingFloatingWidget() const
{
	return !m_FloatingWidget.isNull();
}

void SectionTitleWidget::mouseMoveEvent(QMouseEvent* ev)
{
	std::cout << "SectionTitleWidget::mouseMoveEvent" << std::endl;
    if (!(ev->buttons() & Qt::LeftButton))
    {
        QFrame::mouseMoveEvent(ev);
        return;
    }

    // TODO make a member with the main container widget and assign it on
    // creation
    MainContainerWidget* MainContainerWidget = findParentContainerWidget(this);
    ev->accept();

    // Move already existing FloatingWidget
    if (isDraggingFloatingWidget())
	{
    	std::cout << "SectionTitleWidget isDraggingFloatingWidget()" << std::endl;
        //moveFloatingWidget(ev, MainContainerWidget);
		return;
	}


    SectionWidget* sectionwidget = findParentSectionWidget(this);
    if (!sectionwidget)
    {
        QFrame::mouseMoveEvent(ev);
        return;
    }

    // move tab
    if (m_TabMoving)
    {
        moveTab(ev);
    }

    // leave if dragging is not active
    if (m_DragStartMousePosition.isNull())
    {
        QFrame::mouseMoveEvent(ev);
        return;
    }

	// Begin to drag/float the SectionContent.
    if (!sectionwidget->titleAreaGeometry().contains(sectionwidget->mapFromGlobal(ev->globalPos())))
	{
        startFloating(ev, MainContainerWidget, sectionwidget);
		return;
	}
	// Begin to drag title inside the title area to switch its position inside the SectionWidget.
    else if ((ev->pos() - m_DragStartMousePosition).manhattanLength() >= QApplication::startDragDistance() // Wait a few pixels before start moving
              && sectionwidget->titleAreaGeometry().contains(sectionwidget->mapFromGlobal(ev->globalPos())))
	{
        m_TabMoving = true;
		raise(); // Raise current title-widget above other tabs
		return;
	}
	QFrame::mouseMoveEvent(ev);
}


bool SectionTitleWidget::event(QEvent *e)
{
	return QFrame::event(e);
}

ADS_NAMESPACE_END
