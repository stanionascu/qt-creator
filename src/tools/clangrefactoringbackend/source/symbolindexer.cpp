/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "symbolindexer.h"

namespace ClangBackEnd {

SymbolIndexer::SymbolIndexer(SymbolsCollectorInterface &symbolsCollector,
                             SymbolStorageInterface &symbolStorage,
                             ClangPathWatcherInterface &pathWatcher,
                             FilePathCachingInterface &filePathCache,
                             Sqlite::TransactionInterface &transactionInterface)
    : m_symbolsCollector(symbolsCollector),
      m_symbolStorage(symbolStorage),
      m_pathWatcher(pathWatcher),
      m_filePathCache(filePathCache),
      m_transactionInterface(transactionInterface)
{
    pathWatcher.setNotifier(this);
}

void SymbolIndexer::updateProjectParts(V2::ProjectPartContainers &&projectParts, V2::FileContainers &&generatedFiles)
{
        for (V2::ProjectPartContainer &projectPart : projectParts)
            updateProjectPart(std::move(projectPart), generatedFiles);
}

void SymbolIndexer::updateProjectPart(V2::ProjectPartContainer &&projectPart,
                                      const V2::FileContainers &generatedFiles)
{
    m_symbolsCollector.clear();

    m_symbolsCollector.addFiles(projectPart.sourcePathIds(), projectPart.arguments());

    m_symbolsCollector.addUnsavedFiles(generatedFiles);

    m_symbolsCollector.collectSymbols();

    Sqlite::ImmediateTransaction transaction{m_transactionInterface};

    m_symbolStorage.addSymbolsAndSourceLocations(m_symbolsCollector.symbols(),
                                                 m_symbolsCollector.sourceLocations());

    m_symbolStorage.insertOrUpdateProjectPart(projectPart.projectPartId(),
                                              projectPart.arguments());
    m_symbolStorage.updateProjectPartSources(projectPart.projectPartId(),
                                             m_symbolsCollector.sourceFiles());

    m_symbolStorage.insertOrUpdateUsedMacros(m_symbolsCollector.usedMacros());

    transaction.commit();

}

void SymbolIndexer::pathsWithIdsChanged(const Utils::SmallStringVector &)
{
}

void SymbolIndexer::pathsChanged(const FilePathIds &filePathIds)
{

}

} // namespace ClangBackEnd
