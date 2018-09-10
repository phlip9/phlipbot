/*
 * Copyright (C) 2005-2011 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "PathFinder.hpp"

#include <algorithm>
#include <filesystem>

#include <glm/geometric.hpp>

#include <DetourCommon.h>

#include <hadesmem/detail/assert.hpp>
#include <hadesmem/detail/trace.hpp>

#include <doctest.h>

#define SMOOTH_PATH_STEP_SIZE 4.0f
#define SMOOTH_PATH_SLOP 0.3f

using glm::distance;
using glm::dot;

namespace fs = std::filesystem;

namespace
{
dtQueryFilter createFilter()
{
  dtQueryFilter filter;
  unsigned short includeFlags = 0x0;
  unsigned short excludeFlags = 0x0;

  // Assuming player navigation
  includeFlags |= NAV_GROUND;
  includeFlags |= NAV_WATER;

  filter.setIncludeFlags(includeFlags);
  filter.setExcludeFlags(excludeFlags);
  return filter;
}
}

namespace phlipbot
{
////////////////// PathInfo //////////////////
PathInfo::PathInfo(MMapManager& m_mmap, uint32_t const m_mapId)
  : m_mmap(m_mmap),
    m_mapId(m_mapId),
    m_polyLength(0),
    m_type(),
    m_useStraightPath(false),
    m_forceDestination(false),
    m_pointPathLimit(MAX_POINT_PATH_LENGTH),
    m_navMesh(nullptr),
    m_navMeshQuery(nullptr),
    m_targetAllowedFlags(0),
    m_filter(createFilter())
{
  m_type.set(PathFlag::PATHFIND_BLANK);
}

void PathInfo::setPathLengthLimit(float dist)
{
  m_pointPathLimit = std::min<uint32_t>(MAX_POINT_PATH_LENGTH,
                                        uint32_t(dist / SMOOTH_PATH_STEP_SIZE));
}

bool PathInfo::calculate(vec3& src, vec3& dest, bool forceDest)
{
  // A m_navMeshQuery object is not thread safe, but a same PathInfo can be
  // shared between threads. So need to get a new one.
  m_navMeshQuery = m_mmap.GetNavMeshQuery(m_mapId);

  if (m_navMeshQuery) {
    m_navMesh = m_navMeshQuery->getAttachedNavMesh();
  }

  m_pathPoints.clear();

  vec3 oldDest = getEndPosition();
  setEndPosition(dest);
  setStartPosition(src);

  m_forceDestination = forceDest;
  m_type.reset();
  m_type.set(PathFlag::PATHFIND_BLANK);

  // DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ PathFinder::calculate() for %u
  // \n", m_sourceUnit->GetGUIDLow());

  // make sure navMesh works - we can run on map w/o mmap
  // check if the start and end point have a .mmtile loaded (can we pass via not
  // loaded tile on the way?)
  if (!m_navMesh || !m_navMeshQuery || !HaveTiles(src) || !HaveTiles(dest)) {
    BuildShortcut();
    m_type.reset();
    m_type.set(PathFlag::PATHFIND_NORMAL);
    m_type.set(PathFlag::PATHFIND_NOT_USING_PATH);
    return true;
  }

  // check if destination moved - if not we can optimize something here
  // we are following old, precalculated path?
  float dist = 0.2f; // m_sourceUnit->GetObjectBoundingRadius();
  if (inRange(oldDest, dest, dist, dist) && m_pathPoints.size() > 2) {
    // our target is not moving - we just coming closer
    // we are moving on precalculated path - enjoy the ride
    // DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ PathFinder::calculate::
    // precalculated path\n");
    m_pathPoints.erase(m_pathPoints.begin());
    return false;
  } else {
    // target moved, so we need to update the poly path
    BuildPolyPath(src, dest);
    return true;
  }
}

dtPolyRef PathInfo::FindWalkPoly(dtNavMeshQuery const* query,
                                 float const* pointYZX,
                                 dtQueryFilter const& filter,
                                 float* closestPointYZX,
                                 float zSearchDist)
{
  HADESMEM_DETAIL_ASSERT(query);

  // WARNING : Nav mesh coords are Y, Z, X (and not X, Y, Z)
  float extents[3] = {5.0f, zSearchDist, 5.0f};

  // Default recastnavigation method
  dtPolyRef polyRef;
  if (dtStatusFailed(query->findNearestPoly(pointYZX, extents, &filter,
                                            &polyRef, closestPointYZX))) {
    return 0;
  }

  // Do not select points over player pos
  if (closestPointYZX[1] > pointYZX[1] + 3.0f) {
    return 0;
  }

  return polyRef;
}

dtPolyRef PathInfo::getPolyByLocation(float const* point,
                                      float* distance,
                                      uint32_t allowedFlags)
{
  float closestPoint[VERTEX_SIZE] = {0.0f, 0.0f, 0.0f};
  dtQueryFilter filter;
  filter.setIncludeFlags(m_filter.getIncludeFlags() |
                         static_cast<uint16_t>(allowedFlags));
  dtPolyRef polyRef = FindWalkPoly(m_navMeshQuery, point, filter, closestPoint);
  if (polyRef != INVALID_POLYREF) {
    *distance = dtVdist(closestPoint, point);
    return polyRef;
  }
  return INVALID_POLYREF;
}

void PathInfo::BuildPolyPath(vec3 const& startPos, vec3 const& endPos)
{
  // *** getting start/end poly logic ***

  float distToStartPoly, distToEndPoly;
  float startPoint[VERTEX_SIZE] = {startPos.y, startPos.z, startPos.x};
  float endPoint[VERTEX_SIZE] = {endPos.y, endPos.z, endPos.x};

  //// First case : easy flying / swimming
  // if ((m_sourceUnit->CanSwim() &&
  //     m_sourceUnit->GetTerrain()->IsInWater(endPos.x, endPos.y, endPos.z)) ||
  //    m_sourceUnit->CanFly()) {
  //  if (!m_sourceUnit->GetMap()->FindCollisionModel(
  //        startPos.x, startPos.y, startPos.z, endPos.x, endPos.y, endPos.z)) {
  //    if (m_sourceUnit->CanSwim())
  //      BuildUnderwaterPath();
  //    else {
  //      BuildShortcut();
  //      m_type = PathFlag(PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH);
  //      if (m_sourceUnit->CanFly()) m_type |= PATHFIND_FLYPATH;
  //    }
  //    return;
  //  } else if (m_sourceUnit->CanFly())
  //    m_forceDestination = true;
  //}

  dtPolyRef startPoly = getPolyByLocation(startPoint, &distToStartPoly);
  dtPolyRef endPoly =
    getPolyByLocation(endPoint, &distToEndPoly, m_targetAllowedFlags);

  // we have a hole in our mesh
  // make shortcut path and mark it as NOPATH ( with flying exception )
  // its up to caller how he will use this info
  if (startPoly == INVALID_POLYREF || endPoly == INVALID_POLYREF) {
    // DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ BuildPolyPath :: (startPoly
    // == 0 || endPoly == 0)\n");
    BuildShortcut();
    // m_type =
    //  (m_sourceUnit->GetTypeId() == TYPEID_UNIT &&
    //   ((Creature*)m_sourceUnit)->CanFly()) ?
    //    PathFlag(PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH | PATHFIND_FLYPATH)
    //    : PATHFIND_NOPATH;
    m_type.reset();
    m_type.set(PathFlag::PATHFIND_NORMAL);
    return;
  }

  // we may need a better number here
  bool farFromPoly = (distToStartPoly > 7.0f || distToEndPoly > 7.0f);
  if (farFromPoly) {
    // DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ BuildPolyPath :: farFromPoly
    // distToStartPoly=%.3f distToEndPoly=%.3f\n", distToStartPoly,
    // distToEndPoly);
    bool buildShortcut = false;
    vec3 p = (distToStartPoly > 7.0f) ? startPos : endPos;
    // if (m_sourceUnit->GetTerrain()->IsInWater(p.x, p.y, p.z)) {
    //  // DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ BuildPolyPath ::
    //  // underWater case\n");
    //  if (m_sourceUnit->CanSwim()) {
    //    BuildUnderwaterPath();
    //    return;
    //  }
    //}
    // if (m_sourceUnit->CanFly()) buildShortcut = true;

    if (buildShortcut) {
      BuildShortcut();
      m_type.reset();
      m_type.set(PathFlag::PATHFIND_NORMAL);
      m_type.set(PathFlag::PATHFIND_NOT_USING_PATH);
      return;
    } else {
      float closestPoint[VERTEX_SIZE];
      if (dtStatusSucceed(m_navMeshQuery->closestPointOnPolyBoundary(
            endPoly, endPoint, closestPoint))) {
        dtVcopy(endPoint, closestPoint);
        setActualEndPosition(vec3(endPoint[2], endPoint[0], endPoint[1]));
      }

      m_type.reset();
      m_type.set(PathFlag::PATHFIND_INCOMPLETE);
    }
  }

  // *** poly path generating logic ***

  // look for startPoly/endPoly in current path
  // TODO: we can merge it with getPathPolyByPosition() loop
  bool startPolyFound = false;
  bool endPolyFound = false;
  uint32_t pathStartIndex, pathEndIndex;

  if (m_polyLength) {
    for (pathStartIndex = 0; pathStartIndex < m_polyLength; ++pathStartIndex) {
      // here to carch few bugs
      HADESMEM_DETAIL_ASSERT(m_pathPolyRefs[pathStartIndex] != INVALID_POLYREF);

      if (m_pathPolyRefs[pathStartIndex] == startPoly) {
        startPolyFound = true;
        break;
      }
    }

    for (pathEndIndex = m_polyLength - 1; pathEndIndex > pathStartIndex;
         --pathEndIndex)
      if (m_pathPolyRefs[pathEndIndex] == endPoly) {
        endPolyFound = true;
        break;
      }
  }

  if (startPolyFound && endPolyFound) {
    // DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ BuildPolyPath ::
    // (startPolyFound && endPolyFound)\n");

    // we moved along the path and the target did not move out of our old
    // poly-path our path is a simple subpath case, we have all the data we need
    // just "cut" it out

    m_polyLength = pathEndIndex - pathStartIndex + 1;
    memmove(m_pathPolyRefs, m_pathPolyRefs + pathStartIndex,
            m_polyLength * sizeof(dtPolyRef));
  } else if (startPolyFound && !endPolyFound) {
    // DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ BuildPolyPath ::
    // (startPolyFound && !endPolyFound)\n");

    // we are moving on the old path but target moved out
    // so we have atleast part of poly-path ready

    m_polyLength -= pathStartIndex;

    // try to adjust the suffix of the path instead of recalculating entire
    // length at given interval the target cannot get too far from its last
    // location thus we have less poly to cover sub-path of optimal path is
    // optimal

    // take ~80% of the original length
    // TODO : play with the values here
    uint32_t prefixPolyLength = uint32_t(m_polyLength * 0.8f + 0.5f);
    memmove(m_pathPolyRefs, m_pathPolyRefs + pathStartIndex,
            prefixPolyLength * sizeof(dtPolyRef));

    dtPolyRef suffixStartPoly = m_pathPolyRefs[prefixPolyLength - 1];

    // we need any point on our suffix start poly to generate poly-path, so we
    // need last poly in prefix data
    float suffixEndPoint[VERTEX_SIZE];
    bool PosOverBody = false;
    if (dtStatusFailed(m_navMeshQuery->closestPointOnPoly(
          suffixStartPoly, endPoint, suffixEndPoint, &PosOverBody))) {
      // we can hit offmesh connection as last poly - closestPointOnPoly() don't
      // like that try to recover by using prev polyref
      --prefixPolyLength;
      suffixStartPoly = m_pathPolyRefs[prefixPolyLength - 1];
      if (dtStatusFailed(m_navMeshQuery->closestPointOnPoly(
            suffixStartPoly, endPoint, suffixEndPoint, &PosOverBody))) {
        // suffixStartPoly is still invalid, error state
        BuildShortcut();
        m_type.reset();
        m_type.set(PathFlag::PATHFIND_NOPATH);
        return;
      }
    }

    // generate suffix
    uint32_t suffixPolyLength = 0;
    dtStatus dtResult = m_navMeshQuery->findPath(
      suffixStartPoly, // start polygon
      endPoly, // end polygon
      suffixEndPoint, // start position
      endPoint, // end position
      &m_filter, // polygon search filter
      m_pathPolyRefs + prefixPolyLength - 1, // [out] path
      (int*)&suffixPolyLength,
      MAX_PATH_LENGTH -
        prefixPolyLength); // max number of polygons in output path

    if (!suffixPolyLength || dtStatusFailed(dtResult)) {
      // this is probably an error state, but we'll leave it
      // and hopefully recover on the next Update
      // we still need to copy our preffix
      HADESMEM_DETAIL_TRACE_FORMAT_A(
        "Path Build failed: 0 length path res=0x%x", dtResult);
    }

    // DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++  m_polyLength=%u
    // prefixPolyLength=%u suffixPolyLength=%u \n",m_polyLength,
    // prefixPolyLength, suffixPolyLength);

    // new path = prefix + suffix - overlap
    m_polyLength = prefixPolyLength + suffixPolyLength - 1;
  } else {
    // DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ BuildPolyPath ::
    // (!startPolyFound && !endPolyFound)\n");

    // either we have no path at all -> first run
    // or something went really wrong -> we aren't moving along the path to the
    // target just generate new path

    // free and invalidate old path data
    clear();

    // unsigned int const threadId = ACE_Based::Thread::currentId();
    // if (threadId != m_navMeshQuery->m_owningThread)
    // sLog.outError("CRASH: We are using a dtNavMeshQuery from thread %u which
    // belongs to thread %u!", threadId, m_navMeshQuery->m_owningThread);

    dtStatus dtResult = m_navMeshQuery->findPath(
      startPoly, // start polygon
      endPoly, // end polygon
      startPoint, // start position
      endPoint, // end position
      &m_filter, // polygon search filter
      m_pathPolyRefs, // [out] path
      (int*)&m_polyLength,
      MAX_PATH_LENGTH); // max number of polygons in output path

    if (!m_polyLength || dtStatusFailed(dtResult)) {
      // only happens if we passed bad data to findPath(), or navmesh is messed
      // up
      HADESMEM_DETAIL_TRACE_FORMAT_A(
        "Path Build failed: 0 length path. res=0x%x", dtResult);
      BuildShortcut();
      m_type.reset();
      m_type.set(PathFlag::PATHFIND_NOPATH);
      return;
    }
  }

  // by now we know what type of path we can get
  if (m_pathPolyRefs[m_polyLength - 1] == endPoly &&
      !m_type.test(PathFlag::PATHFIND_INCOMPLETE) &&
      !m_type.test(PathFlag::PATHFIND_NOPATH)) {
    m_type.reset();
    m_type.set(PathFlag::PATHFIND_NORMAL);
  } else {
    m_type.reset();
    m_type.set(PathFlag::PATHFIND_INCOMPLETE);
  }

  BuildPointPath(startPoint, endPoint);
}

void PathInfo::BuildPointPath(const float* startPoint, const float* endPoint)
{
  // generate the point-path out of our up-to-date poly-path
  float pathPoints[MAX_POINT_PATH_LENGTH * VERTEX_SIZE];
  uint32_t pointCount = 0;
  dtStatus dtResult = DT_FAILURE;
  if (m_useStraightPath) {
    dtResult = m_navMeshQuery->findStraightPath(
      startPoint, // start position
      endPoint, // end position
      m_pathPolyRefs, // current path
      m_polyLength, // lenth of current path
      pathPoints, // [out] path corner points
      NULL, // [out] flags
      NULL, // [out] shortened path
      (int*)&pointCount,
      m_pointPathLimit); // maximum number of points/polygons to use
  } else {
    dtResult = findSmoothPath(startPoint, // start position
                              endPoint, // end position
                              m_pathPolyRefs, // current path
                              m_polyLength, // length of current path
                              pathPoints, // [out] path corner points
                              (int*)&pointCount,
                              m_pointPathLimit); // maximum number of points
  }

  if (pointCount < 2 || dtStatusFailed(dtResult)) {
    // only happens if pass bad data to findStraightPath or navmesh is broken
    // single point paths can be generated here
    // TODO : check the exact cases
    // DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ PathFinder::BuildPointPath
    // FAILED! path sized %d returned\n", pointCount);
    BuildShortcut();
    m_type.reset();
    m_type.set(PathFlag::PATHFIND_NOPATH);
    return;
  }

  m_pathPoints.resize(pointCount);
  for (uint32_t i = 0; i < pointCount; ++i) {
    m_pathPoints[i] =
      vec3{pathPoints[i * VERTEX_SIZE + 2], pathPoints[i * VERTEX_SIZE + 0],
           pathPoints[i * VERTEX_SIZE + 1]};
  }

  // first point is always our current location - we need the next one
  setActualEndPosition(m_pathPoints[pointCount - 1]);

  // force the given destination, if needed
  bool forceDestination =
    (m_forceDestination &&
     (!m_type.test(PathFlag::PATHFIND_NORMAL) ||
      !inRange(getEndPosition(), getActualEndPosition(), 1.0f, 1.0f)));
  if (forceDestination) {
    // we may want to keep partial subpath
    if (dist3DSqr(getActualEndPosition(), getEndPosition()) <
        0.3f * dist3DSqr(getStartPosition(), getEndPosition())) {
      setActualEndPosition(getEndPosition());
      m_pathPoints[m_pathPoints.size() - 1] = getEndPosition();
    } else {
      setActualEndPosition(getEndPosition());
      BuildShortcut();
    }

    m_type.reset();
    m_type.set(PathFlag::PATHFIND_NORMAL);
    m_type.set(PathFlag::PATHFIND_NOT_USING_PATH);
    m_type.set(PathFlag::PATHFIND_DEST_FORCED);

    // if (m_sourceUnit->CanFly()) m_type |= PATHFIND_FLYPATH;
  }

  // DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ PathFinder::BuildPointPath
  // path type %d size %d poly-size %d\n", m_type, pointCount, m_polyLength);
}

void PathInfo::BuildShortcut()
{
  clear();

  // make two point path, our curr pos is the start, and dest is the end
  m_pathPoints.resize(2);

  // set start and a default next position
  m_pathPoints[0] = getStartPosition();
  m_pathPoints[1] = getActualEndPosition();

  m_type.reset();
  m_type.set(PathFlag::PATHFIND_SHORTCUT);

  // if (m_sourceUnit->CanFly()) m_type |= PATHFIND_FLYPATH | PATHFIND_NORMAL;
}

// void PathInfo::BuildUnderwaterPath()
//{
//  clear();
//
//  // make two point path, our curr pos is the start, and dest is the end
//  m_pathPoints.resize(2);
//
//  // set start and a default next position
//  m_pathPoints[0] = getStartPosition();
//  m_pathPoints[1] = getActualEndPosition();
//  float ground = 0.0f;
//  GridMapLiquidData liquidData;
//  uint32_t liquidStatus = m_sourceUnit->GetTerrain()->getLiquidStatus(
//    getActualEndPosition().x, getActualEndPosition().y,
//    getActualEndPosition().z, MAP_ALL_LIQUIDS, &liquidData);
//  // No water here ...
//  if (liquidStatus == LIQUID_MAP_NO_WATER) {
//    m_type = PATHFIND_SHORTCUT;
//    if (m_sourceUnit->CanWalk()) {
//      // Find real height
//      m_type |= PATHFIND_NORMAL;
//      m_sourceUnit->UpdateGroundPositionZ(m_pathPoints[1].x,
//      m_pathPoints[1].y,
//                                          m_pathPoints[1].z);
//    } else {
//      m_type |= PATHFIND_INCOMPLETE;
//      m_pathPoints[1] = getStartPosition();
//    }
//    return;
//  }
//  m_type = PATHFIND_BLANK;
//  if (m_pathPoints[1].z > liquidData.level) {
//    if (!m_sourceUnit->CanFly()) {
//      m_pathPoints[1].z = liquidData.level;
//      if (m_pathPoints[1].z > (liquidData.level + 2))
//        m_type |= PATHFIND_INCOMPLETE;
//    }
//  }
//  if (!(m_type & PATHFIND_INCOMPLETE)) m_type |= PATHFIND_NORMAL;
//  m_type |= PATHFIND_UNDERWATER;
//}

// void PathInfo::FillTargetAllowedFlags(Unit* target)
//{
//  m_targetAllowedFlags = 0;
//  if (target->CanSwim())
//    m_targetAllowedFlags |= NAV_WATER | NAV_SLIME | NAV_MAGMA;
//  if (target->CanWalk()) m_targetAllowedFlags |= NAV_GROUND;
//  if (!target->IsPlayer()) m_targetAllowedFlags |= NAV_STEEP_SLOPES;
//}

bool PathInfo::HaveTiles(const vec3& p) const
{
  int tx, ty;
  float point[VERTEX_SIZE] = {p.y, p.z, p.x};

  // check if the start and end point have a .mmtile loaded
  m_navMesh->calcTileLoc(point, &tx, &ty);
  return (m_navMesh->getTileAt(tx, ty, 0) != nullptr);
}

uint32_t PathInfo::fixupCorridor(dtPolyRef* path,
                                 const uint32_t npath,
                                 const uint32_t maxPath,
                                 const dtPolyRef* visited,
                                 const uint32_t nvisited)
{
  int32_t furthestPath = -1;
  int32_t furthestVisited = -1;

  // Find furthest common polygon.
  for (int32_t i = npath - 1; i >= 0; --i) {
    bool found = false;
    for (int32_t j = nvisited - 1; j >= 0; --j) {
      if (path[i] == visited[j]) {
        furthestPath = i;
        furthestVisited = j;
        found = true;
      }
    }
    if (found) break;
  }

  // If no intersection found just return current path.
  if (furthestPath == -1 || furthestVisited == -1) {
    return npath;
  }

  // Concatenate paths.

  // Adjust beginning of the buffer to include the visited.
  uint32_t req = nvisited - furthestVisited;
  uint32_t orig = uint32_t(furthestPath + 1) < npath ? furthestPath + 1 : npath;
  uint32_t size = npath > orig ? npath - orig : 0;
  if (req + size > maxPath) size = maxPath - req;

  if (size) memmove(path + req, path + orig, size * sizeof(dtPolyRef));

  // Store visited
  for (uint32_t i = 0; i < req; ++i) {
    path[i] = visited[(nvisited - 1) - i];
  }

  return req + size;
}

int fixupShortcuts(dtPolyRef* path, int npath, dtNavMeshQuery const* navQuery)
{
  if (npath < 3) return npath;

  // Get connected polygons
  static const int maxNeis = 16;
  dtPolyRef neis[maxNeis];
  int nneis = 0;

  const dtMeshTile* tile = 0;
  const dtPoly* poly = 0;
  if (dtStatusFailed(navQuery->getAttachedNavMesh()->getTileAndPolyByRef(
        path[0], &tile, &poly)))
    return npath;

  for (unsigned int k = poly->firstLink; k != DT_NULL_LINK;
       k = tile->links[k].next) {
    const dtLink* link = &tile->links[k];
    if (link->ref != 0) {
      if (nneis < maxNeis) neis[nneis++] = link->ref;
    }
  }

  // If any of the neighbour polygons is within the next few polygons
  // in the path, short cut to that polygon directly.
  static const int maxLookAhead = 6;
  int cut = 0;
  for (int i = dtMin(maxLookAhead, npath) - 1; i > 1 && cut == 0; i--) {
    for (int j = 0; j < nneis; j++) {
      if (path[i] == neis[j]) {
        cut = i;
        break;
      }
    }
  }
  if (cut > 1) {
    int offset = cut - 1;
    npath -= offset;
    for (int i = 1; i < npath; i++)
      path[i] = path[i + offset];
  }

  return npath;
}

bool PathInfo::getSteerTarget(const float* startPos,
                              const float* endPos,
                              const float minTargetDist,
                              const dtPolyRef* path,
                              const uint32_t pathSize,
                              float* steerPos,
                              unsigned char& steerPosFlag,
                              dtPolyRef& steerPosRef)
{
  // Find steer target.
  static const uint32_t MAX_STEER_POINTS = 3;
  float steerPath[MAX_STEER_POINTS * VERTEX_SIZE];
  unsigned char steerPathFlags[MAX_STEER_POINTS];
  dtPolyRef steerPathPolys[MAX_STEER_POINTS];
  uint32_t nsteerPath = 0;
  dtStatus dtResult = m_navMeshQuery->findStraightPath(
    startPos, endPos, path, pathSize, steerPath, steerPathFlags, steerPathPolys,
    (int*)&nsteerPath, MAX_STEER_POINTS);
  if (!nsteerPath || dtStatusFailed(dtResult)) return false;

  // Find vertex far enough to steer to.
  uint32_t ns = 0;
  while (ns < nsteerPath) {
    // Stop at Off-Mesh link or when point is further than slop away.
    if ((steerPathFlags[ns] & DT_STRAIGHTPATH_OFFMESH_CONNECTION) ||
        !inRangeYZX(&steerPath[ns * VERTEX_SIZE], startPos, minTargetDist,
                    1000.0f))
      break;
    ns++;
  }
  // Failed to find good point to steer to.
  if (ns >= nsteerPath) return false;

  dtVcopy(steerPos, &steerPath[ns * VERTEX_SIZE]);
  steerPos[1] = startPos[1]; // keep Z value
  steerPosFlag = steerPathFlags[ns];
  steerPosRef = steerPathPolys[ns];

  return true;
}

// Compute the cross product AB x AC
float CrossProduct(float* pointA, float* pointB, float* pointC)
{
  float AB[2];
  float AC[2];
  AB[0] = pointB[0] - pointA[0];
  AB[1] = pointB[1] - pointA[1];
  AC[0] = pointC[0] - pointA[0];
  AC[1] = pointC[1] - pointA[1];
  float cross = AB[0] * AC[1] - AB[1] * AC[0];

  return cross;
}

// Compute the distance from A to B
float Distance(float* pointA, float* pointB)
{
  float d1 = pointA[0] - pointB[0];
  float d2 = pointA[2] - pointB[2];

  return sqrt(d1 * d1 + d2 * d2);
}

float Distance2DPointToLineYZX(float* lineA, float* lineB, float* point)
{
  return std::abs(CrossProduct(lineA, lineB, point) / Distance(lineA, lineB));
}

dtStatus PathInfo::findSmoothPath(const float* startPos,
                                  const float* endPos,
                                  const dtPolyRef* polyPath,
                                  uint32_t polyPathSize,
                                  float* smoothPath,
                                  int* smoothPathSize,
                                  uint32_t maxSmoothPathSize)
{
  bool simplifyPath = false;
  *smoothPathSize = 0;
  uint32_t nsmoothPath = 0;

  dtPolyRef polys[MAX_PATH_LENGTH];
  memcpy(polys, polyPath, sizeof(dtPolyRef) * polyPathSize);
  uint32_t npolys = polyPathSize;

  float iterPos[VERTEX_SIZE], targetPos[VERTEX_SIZE];
  int32_t nSkippedPoints = 0;
  if (dtStatusFailed(m_navMeshQuery->closestPointOnPolyBoundary(
        polys[0], startPos, iterPos)))
    return DT_FAILURE;

  if (dtStatusFailed(m_navMeshQuery->closestPointOnPolyBoundary(
        polys[npolys - 1], endPos, targetPos)))
    return DT_FAILURE;

  dtVcopy(&smoothPath[nsmoothPath * VERTEX_SIZE], iterPos);
  nsmoothPath++;

  // Move towards target a small advancement at a time until target reached or
  // when ran out of memory to store the path.
  while (npolys && nsmoothPath < maxSmoothPathSize) {
    // Find location to steer towards.
    float steerPos[VERTEX_SIZE];
    unsigned char steerPosFlag;
    dtPolyRef steerPosRef = INVALID_POLYREF;

    if (!getSteerTarget(iterPos, targetPos, SMOOTH_PATH_SLOP, polys, npolys,
                        steerPos, steerPosFlag, steerPosRef))
      break;

    bool endOfPath = (steerPosFlag & DT_STRAIGHTPATH_END);
    bool offMeshConnection =
      (steerPosFlag & DT_STRAIGHTPATH_OFFMESH_CONNECTION);

    // Find movement delta.
    float delta[VERTEX_SIZE];
    dtVsub(delta, steerPos, iterPos);
    float len = sqrtf(dtVdot(delta, delta));
    // If the steer target is end of path or off-mesh link, do not move past the
    // location.
    if ((endOfPath || offMeshConnection) && len < SMOOTH_PATH_STEP_SIZE)
      len = 1.0f;
    else if (len < SMOOTH_PATH_STEP_SIZE * 4)
      len = SMOOTH_PATH_STEP_SIZE / len;
    else
      len = SMOOTH_PATH_STEP_SIZE * 4 / len;

    float moveTgt[VERTEX_SIZE];
    dtVmad(moveTgt, iterPos, delta, len);

    // Move
    float result[VERTEX_SIZE];
    const static uint32_t MAX_VISIT_POLY = 16;
    dtPolyRef visited[MAX_VISIT_POLY];

    uint32_t nvisited = 0;
    m_navMeshQuery->moveAlongSurface(polys[0], iterPos, moveTgt, &m_filter,
                                     result, visited, (int*)&nvisited,
                                     MAX_VISIT_POLY);
    npolys = fixupCorridor(polys, npolys, MAX_PATH_LENGTH, visited, nvisited);
    npolys = fixupShortcuts(polys, npolys, m_navMeshQuery);

    m_navMeshQuery->getPolyHeight(polys[0], result, &result[1]);
    result[1] += 0.5f;
    dtVcopy(iterPos, result);

    // Handle end of path and off-mesh links when close enough.
    if (endOfPath && inRangeYZX(iterPos, steerPos, SMOOTH_PATH_SLOP, 1.0f)) {
      // Reached end of path.
      if (dtStatusSucceed(m_navMeshQuery->getPolyHeight(polys[0], targetPos,
                                                        &targetPos[1]))) {
        targetPos[1] += 0.5f;
        dtVcopy(iterPos, targetPos);
      }
      if (nsmoothPath < maxSmoothPathSize) {
        if (nSkippedPoints)
          dtVcopy(&smoothPath[(nsmoothPath - 1) * VERTEX_SIZE],
                  &smoothPath[nsmoothPath * VERTEX_SIZE]);
        dtVcopy(&smoothPath[nsmoothPath * VERTEX_SIZE], iterPos);
        nsmoothPath++;
      }
      break;
    } else if (offMeshConnection &&
               inRangeYZX(iterPos, steerPos, SMOOTH_PATH_SLOP, 1.0f)) {
      // Advance the path up to and over the off-mesh connection.
      dtPolyRef prevRef = INVALID_POLYREF;
      dtPolyRef polyRef = polys[0];
      uint32_t npos = 0;
      while (npos < npolys && polyRef != steerPosRef) {
        prevRef = polyRef;
        polyRef = polys[npos];
        npos++;
      }

      for (uint32_t i = npos; i < npolys; ++i)
        polys[i - npos] = polys[i];

      npolys -= npos;

      // Handle the connection.
      float startPos_[VERTEX_SIZE], endPos_[VERTEX_SIZE];
      if (dtStatusSucceed(m_navMesh->getOffMeshConnectionPolyEndPoints(
            prevRef, polyRef, startPos_, endPos_))) {
        if (nsmoothPath < maxSmoothPathSize) {
          dtVcopy(&smoothPath[nsmoothPath * VERTEX_SIZE], startPos_);
          nsmoothPath++;
        }
        // Move position at the other side of the off-mesh link.
        dtVcopy(iterPos, endPos_);

        m_navMeshQuery->getPolyHeight(polys[0], iterPos, &iterPos[1]);
        iterPos[1] += 0.2f;
      }
    }

    // Store results.
    if (nsmoothPath < maxSmoothPathSize) {
      // Nostalrius: do we need to store this point ? Don't use 10 points to
      // make a straight line: 2 are enough ! We eventually remove the last one
      if (simplifyPath && nsmoothPath >= 2 && nSkippedPoints >= 0) {
        // Check z-delta. Needed ... ?
        /*
        float currentZ = iterPos[1];
        float lastZ = smoothPath[(nsmoothPath - 1)* VERTEX_SIZE + 1];
        float lastLastZ = smoothPath[(nsmoothPath - 2)* VERTEX_SIZE + 1];*/
        if (nSkippedPoints < 20 && // Prevent infinite loop here
            Distance2DPointToLineYZX(
              &smoothPath[(nsmoothPath - 2) * VERTEX_SIZE],
              &smoothPath[(nsmoothPath - 1) * VERTEX_SIZE],
              iterPos) < 0.8f) // Replace the last point.
        {
          dtVcopy(&smoothPath[nsmoothPath * VERTEX_SIZE], iterPos);
          ++nSkippedPoints;
          continue;
        } else if (nSkippedPoints)
          dtVcopy(&smoothPath[(nsmoothPath - 1) * VERTEX_SIZE],
                  &smoothPath[nsmoothPath * VERTEX_SIZE]);
      }
      nSkippedPoints = 0;
      dtVcopy(&smoothPath[nsmoothPath * VERTEX_SIZE], iterPos);
      nsmoothPath++;
    }
  }

  *smoothPathSize = nsmoothPath;

  // this is most likely a loop
  return nsmoothPath < MAX_POINT_PATH_LENGTH ? DT_SUCCESS : DT_FAILURE;
}

float PathInfo::Length() const
{
  HADESMEM_DETAIL_ASSERT(m_pathPoints.size());
  float length = 0.0f;
  uint32_t maxIndex = m_pathPoints.size() - 1;
  for (uint32_t i = 1; i <= maxIndex; ++i)
    length += distance(m_pathPoints[i - 1], m_pathPoints[i]);
  return length;
}

bool PathInfo::inRangeYZX(const float* v1,
                          const float* v2,
                          float r,
                          float h) const
{
  const float dx = v2[0] - v1[0];
  const float dy = v2[1] - v1[1]; // elevation
  const float dz = v2[2] - v1[2];
  return (dx * dx + dz * dz) < r * r && fabsf(dy) < h;
}

bool PathInfo::inRange(vec3 const& p1, vec3 const& p2, float r, float h) const
{
  vec3 d = p1 - p2;
  vec2 d2d = d.xy;
  return dot(d2d, d2d) < r * r && fabsf(d.z) < h;
}

float PathInfo::dist3DSqr(vec3 const& p1, vec3 const& p2) const
{
  vec3 d = p1 - p2;
  return dot(d, d);
}

namespace test
{
TEST_CASE("PathFinder should be able to compute a path in Elwynn Forest")
{
  // Eastern Kingdoms
  uint32_t map_id = 0;
  // Elwynn Forest
  int32_t x = 32;
  int32_t y = 48;

  vec3 start{-8949.95f, -132.493f, 83.5312f};
  vec3 end{-9046.507f, -45.71962f, 88.33186f};

  char tile_filename[20];
  snprintf(tile_filename, sizeof(tile_filename), "%03u%02i%02i.mmtile", map_id,
           y, x);

  fs::path mmap_dir = "C:\\MaNGOS\\data\\__mmaps";
  fs::path tile_path = mmap_dir / tile_filename;

  REQUIRE(fs::exists(mmap_dir));
  REQUIRE(fs::exists(tile_path));

  MMapManager mmap{mmap_dir};
  REQUIRE(mmap.loadMap(map_id, x, y));

  PathInfo path_info{mmap, map_id};
  REQUIRE(path_info.calculate(start, end, false));
  CHECK(path_info.Length() > 0.0f);

  auto path_type = path_info.getPathType();
  CHECK(!path_type.test(PathFlag::PATHFIND_BLANK));
  CHECK(!path_type.test(PathFlag::PATHFIND_SHORTCUT));
  CHECK(!path_type.test(PathFlag::PATHFIND_DEST_FORCED));
  CHECK(!path_type.test(PathFlag::PATHFIND_INCOMPLETE));
  CHECK(path_type.test(PathFlag::PATHFIND_NORMAL));
}
}
}
