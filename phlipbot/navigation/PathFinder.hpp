#pragma once

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

#include <bitset>
#include <thread>
#include <vector>

#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>

#include "MoveMap.hpp"
#include "MoveMapSharedDefines.hpp"

#include "../wow_constants.hpp"

namespace phlipbot
{
using PointsArray = std::vector<vec3>;

// TODO(phlip9): currently we assume we're pathfinding on ground terrain, but
//               eventually we should be able to query the current terrain


// 64*6.0f=384y  number_of_points*interval = max_path_len
// this is way more than actual evade range
// I think we can safely cut those down even more
#define MAX_PATH_LENGTH 256
#define MAX_POINT_PATH_LENGTH 256

#define VERTEX_SIZE 3
#define INVALID_POLYREF 0

// lh-server/core : game/Maps/GridMap.h
#define MAP_LIQUID_TYPE_NO_WATER 0x00
#define MAP_LIQUID_TYPE_MAGMA 0x01
#define MAP_LIQUID_TYPE_OCEAN 0x02
#define MAP_LIQUID_TYPE_SLIME 0x04
#define MAP_LIQUID_TYPE_WATER 0x08

#define MAP_ALL_LIQUIDS                                                        \
  (MAP_LIQUID_TYPE_WATER | MAP_LIQUID_TYPE_MAGMA | MAP_LIQUID_TYPE_OCEAN |     \
   MAP_LIQUID_TYPE_SLIME)

#define MAP_LIQUID_TYPE_DARK_WATER 0x10
#define MAP_LIQUID_TYPE_WMO_WATER 0x20

struct GridMapLiquidData {
  uint32_t type_flags;
  uint32_t entry;
  float level;
  float depth_level;
};

namespace PathFlag
{
size_t const
  // path not built yet
  PATHFIND_BLANK = 1,
  // normal path
  PATHFIND_NORMAL = 2,
  // travel through obstacles, terrain, air, etc (old behavior)
  PATHFIND_SHORTCUT = 3,
  // we have partial path to follow - getting closer to target
  PATHFIND_INCOMPLETE = 4,
  // no valid path at all or error in generating one
  PATHFIND_NOPATH = 5,
  // used when we are either flying/swiming or on map w/o mmaps
  PATHFIND_NOT_USING_PATH = 6,
  // NOSTALRIUS: forced destination
  PATHFIND_DEST_FORCED = 7,
  // ? for flying creatures ?
  PATHFIND_FLYPATH = 8,
  // ? used when swimming ?
  PATHFIND_UNDERWATER = 9,
  // ? used for casters ?
  PATHFIND_CASTER = 10;
};

struct PathInfo
{
  explicit PathInfo(MMapManager& mmgr, uint32_t const m_mapId);

  // return value : true if new path was calculated
  bool calculate(vec3& src, vec3& dest, bool forceDest = false);

  void setUseStrightPath(bool useStraightPath)
  {
    m_useStraightPath = useStraightPath;
  };
  void setPathLengthLimit(float distance);

  inline void getStartPosition(vec3& pos) { pos = m_startPosition; }
  inline void getEndPosition(vec3& pos) { pos = m_endPosition; }
  inline void getActualEndPosition(vec3& pos) { pos = m_actualEndPosition; }

  inline vec3 getStartPosition() const { return m_startPosition; }
  inline vec3 getEndPosition() const { return m_endPosition; }
  inline vec3 getActualEndPosition() const { return m_actualEndPosition; }

  inline PointsArray& getFullPath() { return m_pathPoints; }
  inline PointsArray const& getPath() const { return m_pathPoints; }
  inline auto getPathType() const { return m_type; }

  float Length() const;

private:
  MMapManager& m_mmap;

  uint32_t const m_mapId;

  dtPolyRef m_pathPolyRefs[MAX_PATH_LENGTH]; // array of detour polygon
                                             // references
  uint32_t m_polyLength; // number of polygons in the path

  PointsArray m_pathPoints; // our actual (x,y,z) path to the target
  std::bitset<10> m_type; // tells what kind of path this is

  bool m_useStraightPath; // type of path will be generated
  bool m_forceDestination; // when set, we will always arrive at given point
  uint32_t m_pointPathLimit; // limit point path size; min(this,
                             // MAX_POINT_PATH_LENGTH)

  vec3 m_startPosition; // {x, y, z} of current location
  vec3 m_endPosition; // {x, y, z} of the destination
  vec3 m_actualEndPosition; // {x, y, z} of the closest possible point
                            // to given destination
  dtNavMesh const* m_navMesh; // the nav mesh
  dtNavMeshQuery const* m_navMeshQuery; // the nav mesh query used to find the
                                        // path
  uint32_t m_targetAllowedFlags;

  dtQueryFilter m_filter; // use single filter for all movements, update it when
                          // needed

  inline void setStartPosition(vec3 const& point) { m_startPosition = point; }
  inline void setEndPosition(vec3 const& point)
  {
    m_actualEndPosition = point;
    m_endPosition = point;
  }
  inline void setActualEndPosition(vec3 const& point)
  {
    m_actualEndPosition = point;
  }

  inline void clear()
  {
    m_polyLength = 0;
    m_pathPoints.clear();
  }
  bool inRange(vec3 const& p1, vec3 const& p2, float r, float h) const;
  float dist3DSqr(vec3 const& p1, vec3 const& p2) const;
  bool inRangeYZX(float const* v1, float const* v2, float r, float h) const;

  static dtPolyRef FindWalkPoly(dtNavMeshQuery const* query,
                                float const* pointYZX,
                                dtQueryFilter const& filter,
                                float* closestPointYZX,
                                float zSearchDist = 10.0f);

  dtPolyRef
  getPolyByLocation(float const* point, float* distance, uint32_t flags = 0);
  bool HaveTiles(vec3 const& p) const;

  void BuildPolyPath(vec3 const& startPos, vec3 const& endPos);
  void BuildPointPath(float const* startPoint, float const* endPoint);
  void BuildShortcut();
  // void BuildUnderwaterPath();

  // smooth path functions
  uint32_t fixupCorridor(dtPolyRef* path,
                         uint32_t const npath,
                         uint32_t const maxPath,
                         dtPolyRef const* visited,
                         uint32_t const nvisited);
  bool getSteerTarget(float const* startPos,
                      float const* endPos,
                      float const minTargetDist,
                      dtPolyRef const* path,
                      uint32_t const pathSize,
                      float* steerPos,
                      unsigned char& steerPosFlag,
                      dtPolyRef& steerPosRef);
  dtStatus findSmoothPath(float const* startPos,
                          float const* endPos,
                          dtPolyRef const* polyPath,
                          uint32_t polyPathSize,
                          float* smoothPath,
                          int* smoothPathSize,
                          uint32_t smoothPathMaxSize);
};
}
