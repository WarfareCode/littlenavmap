/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#include "mappainterroute.h"
#include "navmapwidget.h"
#include "mapscale.h"
#include "symbolpainter.h"
#include "mapgui/mapquery.h"
#include "common/mapcolors.h"
#include "geo/calculations.h"

#include <algorithm>
#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>
#include <marble/MarbleWidget.h>
#include <marble/ViewportParams.h>
#include <route/routecontroller.h>

using namespace Marble;
using namespace atools::geo;

MapPainterRoute::MapPainterRoute(NavMapWidget *widget, MapQuery *mapQuery, MapScale *mapScale,
                                 RouteController *controller, bool verbose)
  : MapPainter(widget, mapQuery, mapScale, verbose), routeController(controller), navMapWidget(widget)
{
}

MapPainterRoute::~MapPainterRoute()
{

}

void MapPainterRoute::paint(const PaintContext *context)
{
  if(!context->objectTypes.testFlag(maptypes::ROUTE))
    return;

  bool drawFast = widget->viewContext() == Marble::Animation;
  setRenderHints(context->painter);

  context->painter->save();
  // Use layer independent of declutter factor
  paintRoute(context->mapLayerEffective, context->painter, drawFast);
  context->painter->restore();
}

void MapPainterRoute::paintRoute(const MapLayer *mapLayer, GeoPainter *painter, bool fast)
{
  Q_UNUSED(fast);

  const QList<RouteMapObject> routeMapObjects = routeController->getRouteMapObjects();

  painter->setBrush(Qt::NoBrush);

  QList<GeoDataCoordinates> textCoords;
  QList<qreal> textBearing;

  // Collect coordinates for text placement and lines first
  int x, y;
  QList<QPoint> startPoints;
  GeoDataLineString linestring;
  linestring.setTessellate(true);

  for(const RouteMapObject& obj : routeMapObjects)
  {
    GeoDataCoordinates to(obj.getPosition().getLonX(), obj.getPosition().getLatY(), 0,
                          GeoDataCoordinates::Degree);
    if(!linestring.isEmpty())
    {
      const GeoDataCoordinates& from = linestring.last();
      qreal init = normalizeCourse(from.bearing(to, GeoDataCoordinates::Degree,
                                                GeoDataCoordinates::InitialBearing));
      qreal final = normalizeCourse(from.bearing(to, GeoDataCoordinates::Degree,
                                                 GeoDataCoordinates::FinalBearing));
      textBearing.append((init + final) / 2.);
      textCoords.append(from.interpolate(to, 0.5));
    }

    linestring.append(to);
    wToS(obj.getPosition(), x, y);
    startPoints.append(QPoint(x, y));
  }

  // Draw outer line
  painter->setPen(QPen(QColor(Qt::black), 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter->drawPolyline(linestring);

  // Draw innner line
  painter->setPen(QPen(QColor(Qt::yellow), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter->drawPolyline(linestring);

  // Draw text along lines
  painter->setPen(QColor(Qt::black));
  int i = 0;
  for(const GeoDataCoordinates& coord : textCoords)
  {
    if(wToS(coord, x, y))
    {
      const RouteMapObject& obj = routeMapObjects.at(i + 1);
      if(obj.hasPredecessor())
      {
        QPoint p1 = startPoints.at(i);
        QPoint p2 = startPoints.at(i + 1);

        int lineLength = simpleDistance(p1.x(), p1.y(), p2.x(), p2.y());

        if(lineLength > 40)
        {
          QString text(QString::number(obj.getDistanceTo(), 'f', 0) + " nm" + " / " +
                       QString::number(obj.getCourseRhumb(), 'f', 0) + "°M");

          Qt::TextElideMode elide = Qt::ElideRight;
          qreal rotate, brg = textBearing.at(i);
          if(brg > 180.)
          {
            text = "<<  " + text;
            elide = Qt::ElideRight;
            rotate = brg + 90.;
          }
          else
          {
            text += "  >>";
            elide = Qt::ElideLeft;
            rotate = brg - 90.;
          }

          text = painter->fontMetrics().elidedText(text, elide, lineLength);

          painter->translate(x, y);
          painter->rotate(rotate);
          painter->drawText(-painter->fontMetrics().width(text) / 2,
                            -painter->fontMetrics().descent(), text);
          painter->resetTransform();
        }
      }
    }
    i++;
  }

  // Draw symbols
  i = 0;
  for(const QPoint& pt : startPoints)
  {
    if(!pt.isNull())
    {
      x = pt.x();
      y = pt.y();
      const RouteMapObject& obj = routeMapObjects.at(i);
      maptypes::MapObjectTypes type = obj.getMapObjectType();
      switch(type)
      {
        case maptypes::INVALID:
          paintWaypoint(mapLayer, painter, mapcolors::routeInvalidPointColor, x, y, obj.getWaypoint());
          break;
        case maptypes::USER:
          paintUserpoint(mapLayer, painter, x, y);
          break;
        case maptypes::AIRPORT:
          paintAirport(mapLayer, painter, x, y, obj.getAirport());
          break;
        case maptypes::VOR:
          paintVor(mapLayer, painter, x, y, obj.getVor());
          break;
        case maptypes::NDB:
          paintNdb(mapLayer, painter, x, y, obj.getNdb());
          break;
        case maptypes::WAYPOINT:
          paintWaypoint(mapLayer, painter, QColor(), x, y, obj.getWaypoint());
          break;
      }
    }
    i++;
  }

  // Draw symbol text
  i = 0;
  for(const QPoint& pt : startPoints)
  {
    if(!pt.isNull())
    {
      x = pt.x();
      y = pt.y();
      const RouteMapObject& obj = routeMapObjects.at(i);
      maptypes::MapObjectTypes type = obj.getMapObjectType();
      switch(type)
      {
        case maptypes::INVALID:
          paintText(mapLayer, painter, mapcolors::routeInvalidPointColor, x, y, obj.getIdent());
          break;
        case maptypes::USER:
          paintText(mapLayer, painter, mapcolors::routeUserPointColor, x, y, obj.getIdent());
          break;
        case maptypes::AIRPORT:
          paintAirportText(mapLayer, painter, x, y, obj.getAirport());
          break;
        case maptypes::VOR:
          paintVorText(mapLayer, painter, x, y, obj.getVor());
          break;
        case maptypes::NDB:
          paintNdbText(mapLayer, painter, x, y, obj.getNdb());
          break;
        case maptypes::WAYPOINT:
          paintWaypointText(mapLayer, painter, x, y, obj.getWaypoint());
          break;
      }
    }
    i++;
  }
}

void MapPainterRoute::paintAirport(const MapLayer *mapLayer, GeoPainter *painter, int x, int y,
                                   const maptypes::MapAirport& obj)
{
  symbolPainter->drawAirportSymbol(painter, obj, x, y, mapLayer->getAirportSymbolSize(), false, false);
}

void MapPainterRoute::paintAirportText(const MapLayer *mapLayer, GeoPainter *painter, int x, int y,
                                       const maptypes::MapAirport& obj)
{
  textflags::TextFlags flags = textflags::IDENT | textflags::ROUTE_TEXT;

  if(mapLayer != nullptr && mapLayer->isAirportRouteInfo())
    flags |= textflags::NAME | textflags::INFO;

  symbolPainter->drawAirportText(painter, obj, x, y, flags,
                                 mapLayer->getAirportSymbolSize(), false, true, false);
}

void MapPainterRoute::paintVor(const MapLayer *mapLayer, GeoPainter *painter, int x, int y,
                               const maptypes::MapVor& obj)
{
  bool large = mapLayer != nullptr ? mapLayer->isVorLarge() : false;
  symbolPainter->drawVorSymbol(painter, obj, x, y, mapLayer->getVorSymbolSize(), true, false, large);
}

void MapPainterRoute::paintVorText(const MapLayer *mapLayer, GeoPainter *painter, int x, int y,
                                   const maptypes::MapVor& obj)
{
  textflags::TextFlags flags = textflags::ROUTE_TEXT;
  if(mapLayer != nullptr && mapLayer->isVorRouteIdent())
    flags |= textflags::IDENT;

  if(mapLayer != nullptr && mapLayer->isVorRouteInfo())
    flags |= textflags::FREQ | textflags::INFO | textflags::TYPE;

  symbolPainter->drawVorText(painter, obj, x, y, flags, mapLayer->getVorSymbolSize(), true, false);
}

void MapPainterRoute::paintNdb(const MapLayer *mapLayer, GeoPainter *painter, int x, int y,
                               const maptypes::MapNdb& obj)
{
  symbolPainter->drawNdbSymbol(painter, obj, x, y, mapLayer->getNdbSymbolSize(), true, false);
}

void MapPainterRoute::paintNdbText(const MapLayer *mapLayer, GeoPainter *painter, int x, int y,
                                   const maptypes::MapNdb& obj)
{
  textflags::TextFlags flags = textflags::ROUTE_TEXT;
  if(mapLayer != nullptr && mapLayer->isNdbRouteIdent())
    flags |= textflags::IDENT;

  if(mapLayer != nullptr && mapLayer->isNdbRouteInfo())
    flags |= textflags::FREQ | textflags::INFO | textflags::TYPE;

  symbolPainter->drawNdbText(painter, obj, x, y, flags, mapLayer->getNdbSymbolSize(), true, false);
}

void MapPainterRoute::paintWaypoint(const MapLayer *mapLayer, GeoPainter *painter, const QColor& col,
                                    int x, int y, const maptypes::MapWaypoint& obj)
{
  int size = mapLayer != nullptr ? mapLayer->getWaypointSymbolSize() : 10;
  symbolPainter->drawWaypointSymbol(painter, obj, col, x, y, size, true, false);
}

void MapPainterRoute::paintWaypointText(const MapLayer *mapLayer, GeoPainter *painter, int x, int y,
                                        const maptypes::MapWaypoint& obj)
{
  textflags::TextFlags flags = textflags::ROUTE_TEXT;
  if(mapLayer != nullptr && mapLayer->isWaypointRouteName())
    flags |= textflags::IDENT;

  symbolPainter->drawWaypointText(painter, obj, x, y, flags, mapLayer->getWaypointSymbolSize(), true, false);
}

void MapPainterRoute::paintUserpoint(const MapLayer *mapLayer, GeoPainter *painter, int x, int y)
{
  symbolPainter->drawUserpointSymbol(painter, x, y, mapLayer->getWaypointSymbolSize(), true, false);
}

void MapPainterRoute::paintText(const MapLayer *mapLayer, GeoPainter *painter, const QColor& color,
                                int x, int y, const QString& text)
{
  if(text.isEmpty())
    return;

  symbolPainter->textBox(painter, {text}, color,
                         x + mapLayer->getWaypointSymbolSize() / 2 + 2, y, textatt::BOLD, 255);
}
