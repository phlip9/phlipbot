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

// TODO(phlip9): eventually add path updating since it should be less expensive
//               to update a long path if the destination just changes slightly
// TODO(phlip9): log full PathFinder object on errors

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
PathFinder::PathFinder(MMapManager& m_mmap, uint32_t const m_mapId) noexcept
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

void PathFinder::setPathLengthLimit(float dist)
{
  m_pointPathLimit = std::min<uint32_t>(MAX_POINT_PATH_LENGTH,
                                        uint32_t(dist / SMOOTH_PATH_STEP_SIZE));
}

bool PathFinder::calculate(vec3 const& src,
                           vec3 const& dest,
                           bool const forceDest)
{
  // A m_navMeshQuery object is not thread safe, but a same PathFinder can be
  // shared between threads. So need to get a new one.
  m_navMeshQuery = m_mmap.GetNavMeshQuery(m_mapId);

  if (m_navMeshQuery) {
    m_navMesh = m_navMeshQuery->getAttachedNavMesh();
  }

  clear();

  vec3 oldDest = getEndPosition();
  setEndPosition(dest);
  setStartPosition(src);

  m_forceDestination = forceDest;
  m_type.reset();
  m_type.set(PathFlag::PATHFIND_BLANK);

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

  BuildPolyPath(src, dest);
  return true;
}

dtPolyRef PathFinder::FindWalkPoly(dtNavMeshQuery const* query,
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

dtPolyRef PathFinder::getPolyByLocation(float const* point,
                                        float* distance,
                                        uint32_t allowedFlags)
{
  float closestPoint[3] = {0.0f, 0.0f, 0.0f};
  dtQueryFilter filter;
  filter.setIncludeFlags(m_filter.getIncludeFlags() |
                         static_cast<uint16_t>(allowedFlags));
  dtPolyRef polyRef = FindWalkPoly(m_navMeshQuery, point, filter, closestPoint);
  if (m_navMesh->isValidPolyRef(polyRef)) {
    *distance = dtVdist(closestPoint, point);
    return polyRef;
  }
  return 0;
}

void PathFinder::BuildPolyPath(vec3 const& startPos, vec3 const& endPos)
{
  float distToStartPoly, distToEndPoly;
  float startPoint[3] = {startPos.y, startPos.z, startPos.x};
  float endPoint[3] = {endPos.y, endPos.z, endPos.x};

  dtPolyRef startPoly = getPolyByLocation(startPoint, &distToStartPoly);
  dtPolyRef endPoly =
    getPolyByLocation(endPoint, &distToEndPoly, m_targetAllowedFlags);

  bool startValid = m_navMesh->isValidPolyRef(startPoly);
  bool endValid = m_navMesh->isValidPolyRef(endPoly);

  // we have a hole in our mesh
  // make shortcut path and mark it as NOPATH
  // its up to caller how he will use this info
  if (!startValid || !endValid) {
    if (!startValid) {
      HADESMEM_DETAIL_TRACE_FORMAT_A(
        "Warning: hole in navmesh around point {%.03f, %.03f, %.03f}",
        startPoint[2], startPoint[0], startPoint[1]);
    }
    if (!endValid) {
      HADESMEM_DETAIL_TRACE_FORMAT_A(
        "Warning: hole in navmesh around point {%.03f, %.03f, %.03f}",
        endPoint[2], endPoint[0], endPoint[1]);
    }

    BuildShortcut();
    m_type.reset();
    m_type.set(PathFlag::PATHFIND_NOPATH);
    return;
  }

  dtStatus dtResult = m_navMeshQuery->findPath(
    startPoly, // start polygon
    endPoly, // end polygon
    startPoint, // start position
    endPoint, // end position
    &m_filter, // polygon search filter
    m_pathPolyRefs, // [out] path
    (int*)&m_polyLength, // [out] path length
    MAX_PATH_LENGTH); // max number of polygons in output path

  if (m_polyLength == 0 || dtStatusFailed(dtResult)) {
    // only happens if we passed bad data to findPath(), or navmesh is messed
    // up
    HADESMEM_DETAIL_TRACE_FORMAT_A(
      "findPath failed: path length = %d, res=0x%x", m_polyLength, dtResult);
    BuildShortcut();
    m_type.reset();
    m_type.set(PathFlag::PATHFIND_NOPATH);
    return;
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

void PathFinder::BuildPointPath(const float* startPoint, const float* endPoint)
{
  // generate the point-path out of our up-to-date poly-path
  float pathPoints[3 * MAX_POINT_PATH_LENGTH];
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
    HADESMEM_DETAIL_TRACE_FORMAT_A(
      "BuildPointPath failed: path size = %d, res = 0x%x", pointCount,
      dtResult);
    BuildShortcut();
    m_type.reset();
    m_type.set(PathFlag::PATHFIND_NOPATH);
    return;
  }

  m_pathPoints.resize(pointCount);
  for (uint32_t i = 0; i < pointCount; ++i) {
    m_pathPoints[i] =
      vec3{pathPoints[i * 3 + 2], pathPoints[i * 3 + 0], pathPoints[i * 3 + 1]};
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
  }
}

void PathFinder::BuildShortcut()
{
  clear();

  // make two point path, our curr pos is the start, and dest is the end
  m_pathPoints.resize(2);

  // set start and a default next position
  m_pathPoints[0] = getStartPosition();
  m_pathPoints[1] = getActualEndPosition();

  m_type.reset();
  m_type.set(PathFlag::PATHFIND_SHORTCUT);
}

bool PathFinder::HaveTiles(const vec3& p) const
{
  int tx, ty;
  float point[3] = {p.y, p.z, p.x};

  // check if the start and end point have a .mmtile loaded
  m_navMesh->calcTileLoc(point, &tx, &ty);
  return (m_navMesh->getTileAt(tx, ty, 0) != nullptr);
}

uint32_t PathFinder::fixupCorridor(dtPolyRef* path,
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

bool PathFinder::getSteerTarget(const float* startPos,
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
  float steerPath[3 * MAX_STEER_POINTS];
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
        !inRangeYZX(&steerPath[3 * ns], startPos, minTargetDist, 1000.0f))
      break;
    ns++;
  }
  // Failed to find good point to steer to.
  if (ns >= nsteerPath) return false;

  dtVcopy(steerPos, &steerPath[3 * ns]);
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

dtStatus PathFinder::findSmoothPath(const float* startPos,
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

  float iterPos[3], targetPos[3];
  int32_t nSkippedPoints = 0;
  if (dtStatusFailed(m_navMeshQuery->closestPointOnPolyBoundary(
        polys[0], startPos, iterPos)))
    return DT_FAILURE;

  if (dtStatusFailed(m_navMeshQuery->closestPointOnPolyBoundary(
        polys[npolys - 1], endPos, targetPos)))
    return DT_FAILURE;

  dtVcopy(&smoothPath[3 * nsmoothPath], iterPos);
  nsmoothPath++;

  // Move towards target a small advancement at a time until target reached or
  // when ran out of memory to store the path.
  while (npolys && nsmoothPath < maxSmoothPathSize) {
    // Find location to steer towards.
    float steerPos[3];
    unsigned char steerPosFlag;
    dtPolyRef steerPosRef;

    if (!getSteerTarget(iterPos, targetPos, SMOOTH_PATH_SLOP, polys, npolys,
                        steerPos, steerPosFlag, steerPosRef)) {
      break;
    }

    bool endOfPath = (steerPosFlag & DT_STRAIGHTPATH_END);
    bool offMeshConnection =
      (steerPosFlag & DT_STRAIGHTPATH_OFFMESH_CONNECTION);

    // Find movement delta.
    float delta[3];
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

    float moveTgt[3];
    dtVmad(moveTgt, iterPos, delta, len);

    // Move
    float result[3];
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
        if (nSkippedPoints) {
          dtVcopy(&smoothPath[3 * (nsmoothPath - 1)],
                  &smoothPath[3 * nsmoothPath]);
        }
        dtVcopy(&smoothPath[3 * nsmoothPath], iterPos);
        nsmoothPath++;
      }
      break;
    } else if (offMeshConnection &&
               inRangeYZX(iterPos, steerPos, SMOOTH_PATH_SLOP, 1.0f)) {
      // Advance the path up to and over the off-mesh connection.
      dtPolyRef prevRef;
      dtPolyRef polyRef = polys[0];
      uint32_t npos = 0;
      while (npos < npolys && polyRef != steerPosRef) {
        prevRef = polyRef;
        polyRef = polys[npos];
        npos++;
      }

      for (uint32_t i = npos; i < npolys; ++i) {
        polys[i - npos] = polys[i];
      }

      npolys -= npos;

      // Handle the connection.
      float startPos_[3], endPos_[3];
      if (dtStatusSucceed(m_navMesh->getOffMeshConnectionPolyEndPoints(
            prevRef, polyRef, startPos_, endPos_))) {
        if (nsmoothPath < maxSmoothPathSize) {
          dtVcopy(&smoothPath[3 * nsmoothPath], startPos_);
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
            Distance2DPointToLineYZX(&smoothPath[3 * (nsmoothPath - 2)],
                                     &smoothPath[3 * (nsmoothPath - 1)],
                                     iterPos) < 0.8f) // Replace the last point.
        {
          dtVcopy(&smoothPath[3 * nsmoothPath], iterPos);
          ++nSkippedPoints;
          continue;
        } else if (nSkippedPoints)
          dtVcopy(&smoothPath[3 * (nsmoothPath - 1)],
                  &smoothPath[3 * nsmoothPath]);
      }
      nSkippedPoints = 0;
      dtVcopy(&smoothPath[3 * nsmoothPath], iterPos);
      nsmoothPath++;
    }
  }

  *smoothPathSize = nsmoothPath;

  // this is most likely a loop
  return nsmoothPath < MAX_POINT_PATH_LENGTH ? DT_SUCCESS : DT_FAILURE;
}

float PathFinder::Length() const
{
  HADESMEM_DETAIL_ASSERT(m_pathPoints.size());
  float length = 0.0f;
  uint32_t maxIndex = m_pathPoints.size() - 1;
  for (uint32_t i = 1; i <= maxIndex; ++i)
    length += distance(m_pathPoints[i - 1], m_pathPoints[i]);
  return length;
}

bool PathFinder::inRangeYZX(const float* v1,
                            const float* v2,
                            float r,
                            float h) const
{
  const float dx = v2[0] - v1[0];
  const float dy = v2[1] - v1[1]; // elevation
  const float dz = v2[2] - v1[2];
  return (dx * dx + dz * dz) < r * r && fabsf(dy) < h;
}

bool PathFinder::inRange(vec3 const& p1, vec3 const& p2, float r, float h) const
{
  vec3 d = p1 - p2;
  vec2 d2d = d.xy;
  return dot(d2d, d2d) < r * r && fabsf(d.z) < h;
}

float PathFinder::dist3DSqr(vec3 const& p1, vec3 const& p2) const
{
  vec3 d = p1 - p2;
  return dot(d, d);
}

namespace test
{
TEST_CASE("PathFinder should be able to compute a path in Elwynn Forest")
{
  // Eastern Kingdoms
  uint32_t const map_id = 0;
  // Elwynn Forest
  vec2i const tile{48, 32};

  vec3 start{-8949.95f, -132.493f, 83.5312f};
  vec3 end{-9046.507f, -45.71962f, 88.33186f};

  char tile_filename[20];
  snprintf(tile_filename, sizeof(tile_filename), "%03u%02i%02i.mmtile", map_id,
           tile.x, tile.y);

  fs::path mmap_dir = "C:\\MaNGOS\\data\\__mmaps";
  fs::path tile_path = mmap_dir / tile_filename;

  REQUIRE(fs::exists(mmap_dir));
  REQUIRE(fs::exists(tile_path));

  MMapManager mmap{mmap_dir};
  REQUIRE(mmap.loadMap(map_id, tile));

  PathFinder path_info{mmap, map_id};
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
