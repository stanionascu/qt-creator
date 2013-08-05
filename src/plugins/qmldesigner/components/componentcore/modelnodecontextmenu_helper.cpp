/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "modelnodecontextmenu_helper.h"

#include <nodemetainfo.h>
#include <modelnode.h>
#include <qmlitemnode.h>
#include <bindingproperty.h>
#include <nodeproperty.h>
#include <qmldesignerplugin.h>

namespace QmlDesigner {

static inline bool itemsHaveSameParent(const QList<ModelNode> &siblingList)
{
    if (siblingList.isEmpty())
        return false;


    QmlItemNode item(siblingList.first());
    if (!item.isValid())
        return false;

    if (item.isRootModelNode())
        return false;

    QmlItemNode parent = item.instanceParent().toQmlItemNode();
    if (!parent.isValid())
        return false;

    foreach (const ModelNode &node, siblingList) {
        QmlItemNode currentItem(node);
        if (!currentItem.isValid())
            return false;
        QmlItemNode currentParent = currentItem.instanceParent().toQmlItemNode();
        if (!currentParent.isValid())
            return false;
        if (currentItem.instanceIsInLayoutable())
            return false;
        if (currentParent != parent)
            return false;
    }
    return true;
}

namespace SelectionContextFunctors {

bool singleSelectionItemIsAnchored(const SelectionContext &selectionState)
{
    QmlItemNode itemNode(selectionState.currentSingleSelectedNode());
    if (selectionState.isInBaseState() && itemNode.isValid()) {
        bool anchored = itemNode.instanceHasAnchors();
        return anchored;
    }
    return false;
}

bool singleSelectionItemIsNotAnchored(const SelectionContext &selectionState)
{
    QmlItemNode itemNode(selectionState.currentSingleSelectedNode());
    if (selectionState.isInBaseState() && itemNode.isValid()) {
        bool anchored = itemNode.instanceHasAnchors();
        return !anchored;
    }
    return false;
}

bool selectionHasSameParent(const SelectionContext &selectionState)
{
    return !selectionState.selectedModelNodes().isEmpty() && itemsHaveSameParent(selectionState.selectedModelNodes());
}

bool selectionIsComponent(const SelectionContext &selectionState)
{
    return selectionState.currentSingleSelectedNode().isValid()
            && selectionState.currentSingleSelectedNode().isComponent();
}

} //SelectionStateFunctors

} //QmlDesigner
