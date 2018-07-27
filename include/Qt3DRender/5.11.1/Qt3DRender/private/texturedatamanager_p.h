/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QT3DRENDER_RENDER_TEXTUREDATAMANAGER_H
#define QT3DRENDER_RENDER_TEXTUREDATAMANAGER_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QMutex>
#include <QMutexLocker>
#include <Qt3DRender/qtexture.h>
#include <Qt3DRender/qtextureimagedata.h>
#include <Qt3DRender/qtexturegenerator.h>
#include <Qt3DRender/qtextureimagedatagenerator.h>
#include <Qt3DRender/private/gltexture_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

/**
 * The texture data managers associates each texture data generator
 * with the data objects generated by them. That is, either
 *
 *   QTextureImageDataGenerator -> QTextureImageData, or
 *   QTextureGenerator -> QTextureData
 *
 * This way, texture classes only need to refer to the texture functors used.
 * Aspect jobs will make sure that at the start of each frame, all generators
 * registered with the GeneratorDataManagers have been executed.
 *
 * This guarantees that no texture data generator is executed twice.
 *
 * Each Generator is associated with a number of textures that reference it.
 * If the last texture disassociates from a generator, the QTextureData will
 * be deleted.
 */
template <class GeneratorPtr, class DataPtr, class ReferencedType>
class GeneratorDataManager
{
public:
    GeneratorDataManager() {}

    /*!
     * If no data for the given generator exists, make sure that the
     * generators are executed the next frame. Reference generator by
     * given texture
     *
     * Returns true if the Entry for a given generator had to be created
     */
    bool requestData(const GeneratorPtr &generator, ReferencedType r)
    {
        QMutexLocker lock(&m_mutex);

        Entry *entry = findEntry(generator);
        const bool needsToBeCreated = (entry == nullptr);
        if (needsToBeCreated)
            entry = createEntry(generator);
        Q_ASSERT(entry);
        if (!entry->referencingObjects.contains(r))
            entry->referencingObjects.push_back(r);
        return needsToBeCreated;
    }

    /*!
     * Dereference given generator from texture. If no other textures still reference
     * the generator, the associated data will be deleted
     */
    void releaseData(const GeneratorPtr &generator, ReferencedType r)
    {
        QMutexLocker lock(&m_mutex);

        const auto end = m_data.end();
        for (auto it = m_data.begin(); it != end; ++it) {
            Entry &entry = *it;
            if (*entry.generator == *generator) {
                entry.referencingObjects.removeAll(r);
                // delete, if that was the last reference
                if (entry.referencingObjects.empty()) {
                    m_data.erase(it);
                    return;
                }
            }
        }
    }

    /*!
     * Return data associated with given generator, if existent
     */
    DataPtr getData(const GeneratorPtr &generator)
    {
        QMutexLocker lock(&m_mutex);

        const Entry *entry = findEntry(generator);
        return entry ? entry->data : DataPtr();
    }

    /*!
     * Returns all generators that were not yet executed
     */
    QVector<GeneratorPtr> pendingGenerators()
    {
        QMutexLocker lock(&m_mutex);

        QVector<GeneratorPtr> ret;
        for (const Entry &entry : m_data)
            if (!entry.data && !ret.contains(entry.generator))
                ret.push_back(entry.generator);
        return ret;
    }

    /*!
     * Assigns a piece of data to the generator that was used to
     * create it.
     */
    void assignData(const GeneratorPtr &generator, const DataPtr &data)
    {
        QMutexLocker lock(&m_mutex);

        Entry *entry = findEntry(generator);
        if (!entry) {
            qWarning() << "[TextureDataManager] assignData() called with non-existent generator";
            return;
        }
        entry->data = data;
    }

    bool contains(const GeneratorPtr &generator)
    {
        return findEntry(generator) != nullptr;
    }

private:

    struct Entry {
        GeneratorPtr generator;
        QVector<ReferencedType> referencingObjects;
        DataPtr data;
    };

    /*!
     * Helper function: return entry for given generator if it exists, nullptr
     * otherwise.
     */
    Entry* findEntry(const GeneratorPtr &generator)
    {
        for (int i = 0, sz = m_data.size(); i < sz; ++i)
            if (*m_data[i].generator == *generator)
                return &m_data[i];
        return nullptr;
    }

    Entry *createEntry(const GeneratorPtr &generator)
    {
        Entry newEntry;
        newEntry.generator = generator;

        m_data.push_back(newEntry);
        return &m_data.back();
    }

    QMutex m_mutex;
    QVector<Entry> m_data;
};

class Q_AUTOTEST_EXPORT TextureDataManager
        : public GeneratorDataManager<QTextureGeneratorPtr, QTextureDataPtr, GLTexture*>
{
};

class Q_AUTOTEST_EXPORT TextureImageDataManager
        : public GeneratorDataManager<QTextureImageDataGeneratorPtr, QTextureImageDataPtr, Qt3DCore::QNodeId>
{
};

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE

#endif // TEXTUREDATAMANAGER_H
