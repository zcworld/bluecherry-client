/*
 * Copyright 2010-2019 Bluecherry, LLC
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIVEVIEWGRADIENTS_H
#define LIVEVIEWGRADIENTS_H

#include <QQuickImageProvider>

class LiveViewGradients : public QQuickImageProvider
{
public:
    LiveViewGradients();

    virtual QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize);
};

#endif // LIVEVIEWGRADIENTS_H
