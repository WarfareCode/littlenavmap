/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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
#include "abstractinfobuilder.h"

AbstractInfoBuilder::AbstractInfoBuilder(QObject *parent)
  : QObject(parent)
{
  contentTypeHeader = "text/plain";
}

AbstractInfoBuilder::~AbstractInfoBuilder()
{

}

QByteArray AbstractInfoBuilder::airport(AirportInfoData airportInfoData) const
{
    return "not implemented";
}

QString AbstractInfoBuilder::getHeadingsStringByMagVar(float heading, float magvar) const {

    return courseTextFromTrue(heading, magvar) +", "+ courseTextFromTrue(opposedCourseDeg(heading), magvar);

}

QString AbstractInfoBuilder::formatComFrequency(int frequency) const {

    return locale.toString(roundComFrequency(frequency), 'f', 3) + tr(" MHz");

}

QString AbstractInfoBuilder::getCoordinatesString(const atools::sql::SqlRecord *rec) const
{
  if(rec != nullptr && rec->contains("lonx") && rec->contains("laty"))
    return getCoordinatesString(Pos(rec->valueFloat("lonx"), rec->valueFloat("laty"), rec->valueFloat("altitude", 0.f)));
}

QString AbstractInfoBuilder::getCoordinatesString(const Pos& pos) const
{
  if(pos.isValid())
    return Unit::coords(pos);
}