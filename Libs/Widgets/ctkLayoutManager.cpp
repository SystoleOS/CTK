/*=========================================================================

  Library:   CTK

  Copyright (c) Kitware Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

=========================================================================*/

// Qt includes
#include <QDebug>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

// CTK includes
#include "ctkLayoutManager.h"
#include "ctkLayoutManager_p.h"
#include "ctkLayoutViewFactory.h"

//-----------------------------------------------------------------------------
ctkLayoutManagerPrivate::ctkLayoutManagerPrivate(ctkLayoutManager& object)
  :q_ptr(&object)
{
  this->Spacing = 0;
}

//-----------------------------------------------------------------------------
ctkLayoutManagerPrivate::~ctkLayoutManagerPrivate()
{
}

//-----------------------------------------------------------------------------
void ctkLayoutManagerPrivate::init()
{
  //Q_Q(ctkLayoutManager);
}

//-----------------------------------------------------------------------------
QWidget* ctkLayoutManagerPrivate::viewportForWidget(QWidget* widget)const
{
  if (!widget)
    {
    return 0;
    }
  QString viewportName = this->viewportNameForWidget(widget);
  return this->viewport(viewportName);
}

//-----------------------------------------------------------------------------
QWidget* ctkLayoutManagerPrivate::viewport(const QString& viewportName)const
{
  QMap<QString, QWidget*>::const_iterator viewportIt = this->Viewports.find(viewportName);
  if (viewportIt == this->Viewports.end())
    {
    return 0;
    }
  return viewportIt.value();
}

//-----------------------------------------------------------------------------
QString ctkLayoutManagerPrivate::viewportNameForWidget(QWidget* widget)const
{
  if (!widget)
    {
    return "";
    }
  return widget->property("LayoutManagerViewportName").toString();
}

//-----------------------------------------------------------------------------
void ctkLayoutManagerPrivate::setViewportNameForWidget(QWidget* widget, const QString& viewportName)
{
  if (!widget)
    {
    return;
    }
  widget->setProperty("LayoutManagerViewportName", viewportName);
}

//-----------------------------------------------------------------------------
bool ctkLayoutManagerPrivate::isViewportUsedInLayout(QWidget* viewport)const
{
  if (!viewport)
    {
    return false;
    }
  return viewport->property("LayoutManagerUsedInLayout").toBool();
}

//-----------------------------------------------------------------------------
void ctkLayoutManagerPrivate::setViewportUsedInLayout(QWidget* viewport, bool owned)
{
  if (!viewport)
    {
    return;
    }
  viewport->setProperty("LayoutManagerUsedInLayout", owned);
}

//-----------------------------------------------------------------------------
void ctkLayoutManagerPrivate::clearWidget(QWidget* widget, QLayout* parentLayout)
{
  if (!this->LayoutWidgets.contains(widget))
    {
    widget->setVisible(false);
    if (parentLayout)
      {
      parentLayout->removeWidget(widget);
      }
    widget->setParent(this->viewportForWidget(widget));
    }
  else
    {
    if (widget->layout())
      {
      this->clearLayout(widget->layout());
      }
    else if (qobject_cast<QTabWidget*>(widget))
      {
      QTabWidget* tabWidget = qobject_cast<QTabWidget*>(widget);
      while (tabWidget->count())
        {
        QWidget* page = tabWidget->widget(0);
        this->clearWidget(page);
        }
      }
    else if (qobject_cast<QSplitter*>(widget))
      {
      QSplitter* splitterWidget = qobject_cast<QSplitter*>(widget);
      // Hide the splitter before removing pages. Removing pages
      // would resize the other children if the splitter is visible.
      // We don't want to resize the children as we are trying to clear the
      // layout, it would set intermediate sizes to widgets.
      // See QSplitter::childEvent
      splitterWidget->setVisible(false);
      while (splitterWidget->count())
        {
        QWidget* page = splitterWidget->widget(0);
        this->clearWidget(page);
        }
      }
    this->LayoutWidgets.remove(widget);
    if (parentLayout)
      {
      parentLayout->removeWidget(widget);
      }
    delete widget;
    }
}

//-----------------------------------------------------------------------------
void ctkLayoutManagerPrivate::clearLayout(QLayout* layout)
{
  if (!layout)
    {
    return;
    }
  layout->setEnabled(false);
  QLayoutItem * layoutItem = 0;
  while ((layoutItem = layout->itemAt(0)) != 0)
    {
    if (layoutItem->widget())
      {
      this->clearWidget(layoutItem->widget(), layout);
      }
    else if (layoutItem->layout())
      {
      /// Warning, this might delete the layouts of "custom" widgets, not just
      /// the layouts generated by ctkLayoutManager
      this->clearLayout(layoutItem->layout());
      layout->removeItem(layoutItem);
      delete layoutItem;
      }
    }
  if (layout->parentWidget() && layout->parentWidget()->layout() == layout)
    {
    delete layout;
    }
}

//-----------------------------------------------------------------------------
// ctkLayoutManager
//-----------------------------------------------------------------------------
ctkLayoutManager::ctkLayoutManager(QObject* parentObject)
  : QObject(parentObject)
  , d_ptr(new ctkLayoutManagerPrivate(*this))
{
  Q_D(ctkLayoutManager);
  d->init();
}

//-----------------------------------------------------------------------------
ctkLayoutManager::ctkLayoutManager(QWidget* viewport, QObject* parentObject)
  : QObject(parentObject)
  , d_ptr(new ctkLayoutManagerPrivate(*this))
{
  Q_D(ctkLayoutManager);
  d->init();
  this->setViewport(viewport);
}

//-----------------------------------------------------------------------------
ctkLayoutManager::ctkLayoutManager(ctkLayoutManagerPrivate* ptr,
                                   QWidget* viewport, QObject* parentObject)
  : QObject(parentObject)
  , d_ptr(ptr)
{
  Q_D(ctkLayoutManager);
  d->init();
  this->setViewport(viewport);
}

//-----------------------------------------------------------------------------
ctkLayoutManager::~ctkLayoutManager()
{
}

//-----------------------------------------------------------------------------
int ctkLayoutManager::spacing()const
{
  Q_D(const ctkLayoutManager);
  return d->Spacing;
}

//-----------------------------------------------------------------------------
void ctkLayoutManager::setSpacing(int spacing)
{
  Q_D(ctkLayoutManager);
  d->Spacing = spacing;
  this->refresh();
}

//-----------------------------------------------------------------------------
void ctkLayoutManager::refresh()
{
  Q_D(ctkLayoutManager);
  QMap<QWidget*, bool> updatesEnabled;
  foreach(QWidget* viewport, d->Viewports)
    {
    if (!viewport)
      {
      continue;
      }
    updatesEnabled[viewport] = viewport->updatesEnabled();
    viewport->setUpdatesEnabled(false);
    }
  // TODO: post an event on the event queue
  this->clearLayout();
  this->setupLayout();
  foreach(QWidget* viewport, d->Viewports)
    {
    QMap<QWidget*, bool>::iterator updatesEnabledIt = updatesEnabled.find(viewport);
    if (updatesEnabledIt == updatesEnabled.end())
      {
      continue;
      }
    viewport->setUpdatesEnabled(updatesEnabledIt.value());
    }
}

//-----------------------------------------------------------------------------
void ctkLayoutManager::clearLayout()
{
  Q_D(ctkLayoutManager);
  foreach(QWidget* viewport, d->Viewports)
    {
    if (!viewport)
      {
      continue;
      }
    // TODO: post an event on the event queue
    d->clearLayout(viewport->layout());
    Q_ASSERT(d->LayoutWidgets.size() == 0);
    }
}

//-----------------------------------------------------------------------------
void ctkLayoutManager::setupLayout()
{
  Q_D(ctkLayoutManager);
  if (d->Layout.isNull() ||
      d->Layout.documentElement().isNull())
    {
    return;
    }
  d->Views.clear();
  d->LayoutWidgets.clear();
  QStringList viewportNamesUsedInLayout;
  if (d->Layout.documentElement().tagName() == "viewports")
    {
    for (QDomNode child = d->Layout.documentElement().firstChild();
      !child.isNull();
      child = child.nextSibling())
      {
      // ignore children that are not QDomElement
      if (child.toElement().isNull())
        {
        continue;
        }
      if (child.toElement().tagName() != "layout")
        {
        qWarning() << "Expected layout XML element, found " << child.toElement().tagName();
        continue;
        }
      QString viewportName = child.toElement().attribute("name").toUtf8();
      if (viewportNamesUsedInLayout.contains(viewportName))
        {
        qWarning() << "Viewport name" << viewportName << "already used in layout";
        continue;
        }
      viewportNamesUsedInLayout << viewportName;
      this->setupViewport(child.toElement(), viewportName);
      }
    }
  else if (d->Layout.documentElement().tagName() == "layout")
    {
    QString viewportName = d->Layout.documentElement().attribute("name").toUtf8();
    this->setupViewport(d->Layout.documentElement(), viewportName);
    viewportNamesUsedInLayout << viewportName;
    }
  else
    {
    qWarning() << "Expected 'viewports' or 'layout' as XML root element, found" << d->Layout.documentElement().tagName();
    }
  foreach (const QString& viewportName, d->Viewports.keys())
  {
    bool usedInLayout = viewportNamesUsedInLayout.contains(viewportName);
    QWidget* viewport = d->viewport(viewportName);
    if (d->isViewportUsedInLayout(viewport) == usedInLayout)
      {
      // no change
      continue;
      }
    d->setViewportUsedInLayout(viewport, usedInLayout);
    this->onViewportUsageChanged(viewportName);
  }
}

//-----------------------------------------------------------------------------
QWidget* ctkLayoutManager::createViewport(const QDomElement& layoutElement, const QString& viewportName)
{
  Q_UNUSED(layoutElement);
  Q_UNUSED(viewportName);
  /// Derived classes may implement instantiation of a new QWidget() and
  /// take ownership of that widget.
  return 0;
}

//-----------------------------------------------------------------------------
void ctkLayoutManager::setupViewport(const QDomElement& layoutElement, const QString& viewportName)
{
  Q_D(ctkLayoutManager);
  if (layoutElement.isNull())
    {
    return;
    }
  QWidget* viewport = d->viewport(viewportName);
  if (!viewport)
    {
    viewport = this->createViewport(layoutElement, viewportName);
    if (!viewport)
      {
      qWarning() << "Failed to create viewport by name" << viewportName;
      return;
      }
    d->Viewports[viewportName] = viewport;
    }
  Q_ASSERT(!viewport->layout());
  QLayoutItem* layoutItem = this->processElement(layoutElement);
  Q_ASSERT(layoutItem);
  QLayout* layout = layoutItem->layout();
  if (!layout)
    {
    QHBoxLayout* hboxLayout = new QHBoxLayout(0);
    hboxLayout->setContentsMargins(0, 0, 0, 0);
    hboxLayout->addItem(layoutItem);
    layout = hboxLayout;
    }
  viewport->setLayout(layout);
}

//-----------------------------------------------------------------------------
void ctkLayoutManager::setViewport(QWidget* viewport)
{
  this->setViewport(viewport, "");
}

//-----------------------------------------------------------------------------
QWidget* ctkLayoutManager::viewport()const
{
  return this->viewport("");
}

//-----------------------------------------------------------------------------
void ctkLayoutManager::setViewport(QWidget* viewport, const QString& viewportName)
{
  Q_D(ctkLayoutManager);
  QWidget* oldViewport = d->viewport(viewportName);
  if (viewport == oldViewport)
    {
    return;
    }
  if (oldViewport)
    {
    if (oldViewport->layout())
      {
      d->clearLayout(oldViewport->layout());
      }
    foreach(QWidget * view, d->Views)
      {
      if (view->parent() == oldViewport)
        {
        view->setParent(0);
        // reparenting looses the visibility attribute and we want them hidden
        view->setVisible(false);
        }
      }
    }
  d->Viewports[viewportName] = viewport;
  this->onViewportChanged();
}

//-----------------------------------------------------------------------------
QWidget* ctkLayoutManager::viewport(const QString& viewportName)const
{
  Q_D(const ctkLayoutManager);
  return d->viewport(viewportName);
}

//-----------------------------------------------------------------------------
void ctkLayoutManager::onViewportChanged()
{
  this->refresh();
}

//-----------------------------------------------------------------------------
void ctkLayoutManager::onViewportUsageChanged(const QString& viewportName)
{
  Q_UNUSED(viewportName);
  // Derived classes may show/hide viewport widgets here
}

//-----------------------------------------------------------------------------
void ctkLayoutManager::setLayout(const QDomDocument& newLayout)
{
  Q_D(ctkLayoutManager);
  if (newLayout == d->Layout)
    {
    return;
    }
  d->Layout = newLayout;
  this->refresh();
}

//-----------------------------------------------------------------------------
const QDomDocument ctkLayoutManager::layout()const
{
  Q_D(const ctkLayoutManager);
  return d->Layout;
}

//-----------------------------------------------------------------------------
QLayoutItem* ctkLayoutManager::processElement(QDomElement element)
{
  Q_ASSERT(!element.isNull());
  if (element.tagName() == "layout")
    {
    return this->processLayoutElement(element);
    }
  else
    {
    // 'view' or other custom element type
    return this->widgetItemFromXML(element);
    }
}

//-----------------------------------------------------------------------------
QLayoutItem* ctkLayoutManager::processLayoutElement(QDomElement layoutElement)
{
  Q_D(ctkLayoutManager);
  Q_ASSERT(layoutElement.tagName() == "layout");

  QLayoutItem* layoutItem = this->layoutFromXML(layoutElement);
  QLayout* layout = layoutItem->layout();
  QWidget* widget = layoutItem->widget();

  if (layout)
    {
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(d->Spacing);
    }
  else if (widget)
    {
    // mark the widget as ctkLayoutManager own widget so that it can be
    // deleted when the layout is cleared.
    d->LayoutWidgets << widget;
    }
  QList<int> splitSizes;
  for(QDomNode child = layoutElement.firstChild();
      !child.isNull();
      child = child.nextSibling())
    {
    // ignore children that are not QDomElement
    if (child.toElement().isNull())
      {
      continue;
      }
    int splitSize = child.toElement().attribute("splitSize", QString::number(0)).toInt();
    splitSizes << splitSize;
    this->processItemElement(child.toElement(), layoutItem);
    }

  // Set child item split sizes (initial position of the splitter)
  QSplitter* splitter = qobject_cast<QSplitter*>(widget);
  if (splitter)
    {
    bool splitSizeSpecified = false;
    foreach(int i, splitSizes)
      {
      if (i > 0)
        {
        splitSizeSpecified = true;
        break;
        }
      }
    if (splitSizeSpecified)
      {
      splitter->setSizes(splitSizes);
      }
    }

  return layoutItem;
}

//-----------------------------------------------------------------------------
QLayoutItem* ctkLayoutManager::layoutFromXML(QDomElement layoutElement)
{
  Q_ASSERT(layoutElement.tagName() == "layout");
  QString type = layoutElement.attribute("type", "horizontal");
  bool split = layoutElement.attribute("split", "false") == "true";
  if (type == "vertical")
    {
    if (split)
      {
      return new QWidgetItem(new QSplitter(Qt::Vertical));
      }
    return new QVBoxLayout();
    }
  else if (type == "horizontal")
    {
    if (split)
      {
      return new QWidgetItem(new QSplitter(Qt::Horizontal));
      }
    return new QHBoxLayout();
    }
  else if (type == "grid")
    {
    return new QGridLayout();
    }
  else if (type == "tab")
    {
    return new QWidgetItem(new QTabWidget());
    }
  return 0;
}

//-----------------------------------------------------------------------------
void ctkLayoutManager::processItemElement(QDomElement itemElement, QLayoutItem* layoutItem)
{
  Q_ASSERT(itemElement.tagName() == "item");
  Q_ASSERT(itemElement.childNodes().count() == 1);
  bool multiple = itemElement.attribute("multiple", "false") == "true";
  QList<QLayoutItem*> childrenItem;
  if (multiple)
    {
    childrenItem = this->widgetItemsFromXML(itemElement.firstChild().toElement());
    }
  else
    {
    QLayoutItem* layoutItem = this->processElement(itemElement.firstChild().toElement());
    if (layoutItem)
      {
      childrenItem << layoutItem;
      }
    }
  foreach(QLayoutItem* item, childrenItem)
    {
    this->addChildItemToLayout(itemElement, item, layoutItem);
    }
}

//-----------------------------------------------------------------------------
void ctkLayoutManager::addChildItemToLayout(QDomElement itemElement, QLayoutItem* childItem, QLayoutItem* layoutItem)
{
  Q_D(ctkLayoutManager);
  Q_ASSERT(childItem);
  QString itemName = itemElement.attribute("name");
  if (itemName.isEmpty() && childItem->widget())
    {
    itemName = childItem->widget()->windowTitle();
    }
  QLayout* layout = layoutItem->layout();
  QGridLayout* gridLayout = qobject_cast<QGridLayout*>(layout);
  QLayout* genericLayout = qobject_cast<QLayout*>(layout);
  QTabWidget* tabWidget = qobject_cast<QTabWidget*>(layoutItem->widget());
  QSplitter* splitter = qobject_cast<QSplitter*>(layoutItem->widget());
  if (gridLayout)
    {
    int row = itemElement.attribute("row", QString::number(0)).toInt();
    int col = itemElement.attribute("column", QString::number(0)).toInt();
    int rowSpan = itemElement.attribute("rowspan", QString::number(1)).toInt();
    int colSpan = itemElement.attribute("colspan", QString::number(1)).toInt();
    gridLayout->addItem(childItem, row, col, rowSpan, colSpan);
    }
  else if (genericLayout)
    {
    genericLayout->addItem(childItem);
    }
  else if (tabWidget || splitter)
    {
    QWidget* childWidget = childItem->widget();
    if (!childWidget)
      {
      childWidget = new QWidget();
      d->LayoutWidgets << childWidget;
      childWidget->setLayout(childItem->layout());
      }
    if (tabWidget)
      {
      tabWidget->addTab(childWidget, itemName);
      }
    else
      {
      splitter->addWidget(childWidget);
      }
    }
}

//-----------------------------------------------------------------------------
QWidgetItem* ctkLayoutManager::widgetItemFromXML(QDomElement viewElement)
{
  //Q_ASSERT(viewElement.tagName() == "view");
  QWidget* view = this->viewFromXML(viewElement);
  if (!view)
    {
    return 0;
    }
  this->setupView(viewElement, view);
  return new QWidgetItem(view);
}

//-----------------------------------------------------------------------------
void ctkLayoutManager::setupView(QDomElement viewElement, QWidget* view)
{
  Q_UNUSED(viewElement);
  Q_D(ctkLayoutManager);
  if (!view)
    {
    return;
    }
  view->setVisible(true);
  d->Views.insert(view);
}

//-----------------------------------------------------------------------------
QList<QLayoutItem*> ctkLayoutManager::widgetItemsFromXML(QDomElement viewElement)
{
  ///Q_ASSERT(viewElement.tagName() == "view");
  QList<QLayoutItem*> res;
  QList<QWidget*> views = this->viewsFromXML(viewElement);
  Q_ASSERT(views.count());
  foreach(QWidget* view, views)
    {
    this->setupView(viewElement, view);
    res << new QWidgetItem(view);
    }
  return res;
}

//-----------------------------------------------------------------------------
QList<QWidget*> ctkLayoutManager::viewsFromXML(QDomElement viewElement)
{
  QList<QWidget*> res;
  res << this->viewFromXML(viewElement);
  return res;
}

//-----------------------------------------------------------------------------
QStringList ctkLayoutManager::viewportNames()const
{
  Q_D(const ctkLayoutManager);
  return d->Viewports.keys();
}

//-----------------------------------------------------------------------------
bool ctkLayoutManager::isViewportUsedInLayout(const QString& viewportName)const
{
  Q_D(const ctkLayoutManager);
  QWidget* viewport = d->viewport(viewportName);
  if (!viewport)
    {
    return false;
    }
  return d->isViewportUsedInLayout(viewport);
}
