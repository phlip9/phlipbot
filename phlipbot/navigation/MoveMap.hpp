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

#ifndef _MOVE_MAP_H
#define _MOVE_MAP_H

#include <ace/Guard_T.h>
#include <ace/RW_Mutex.h>
#include <ace/Thread_Mutex.h>

#include "Utilities/UnorderedMapSet.h"

#include "Detour/Include/DetourAlloc.h"
#include "Detour/Include/DetourNavMesh.h"
#include "Detour/Include/DetourNavMeshQuery.h"

//  memory management
inline void* dtCustomAlloc(int size, dtAllocHint /*hint*/)
{
  return (void*)new unsigned char[size];
}

inline void dtCustomFree(void* ptr) { delete[](unsigned char*) ptr; }

//  move map related classes
namespace MMAP
{
typedef UNORDERED_MAP<uint32, dtTileRef> MMapTileSet;
typedef UNORDERED_MAP<uint32, dtNavMeshQuery*> NavMeshQuerySet;

// dummy struct to hold map's mmap data
struct MMapData {
  MMapData(dtNavMesh* mesh) : navMesh(mesh) {}
  ~MMapData()
  {
    for (NavMeshQuerySet::iterator i = navMeshQueries.begin();
         i != navMeshQueries.end(); ++i)
      dtFreeNavMeshQuery(i->second);

    if (navMesh) dtFreeNavMesh(navMesh);
  }

  dtNavMesh* navMesh;

  // we have to use single dtNavMeshQuery for every instance, since those are
  // not thread safe
  NavMeshQuerySet navMeshQueries; // threadId to query
  ACE_RW_Mutex navMeshQueries_lock;
  MMapTileSet mmapLoadedTiles; // maps [map grid coords] to [dtTile]
  ACE_Thread_Mutex tilesLoading_lock;
};

typedef UNORDERED_MAP<uint32, MMapData*> MMapDataSet;

// singelton class
// holds all all access to mmap loading unloading and meshes
class MMapManager
{
public:
  MMapManager() : loadedTiles(0) {}
  ~MMapManager();

  bool loadMap(uint32 mapId, int32 x, int32 y);
  bool loadGameObject(uint32 displayId);
  bool unloadMap(uint32 mapId, int32 x, int32 y);
  bool unloadMap(uint32 mapId);
  bool unloadMapInstance(uint32 mapId, uint32 instanceId);

  // The returned [dtNavMeshQuery const*] is NOT threadsafe
  // Returns a NavMeshQuery valid for current thread only.
  dtNavMeshQuery const* GetNavMeshQuery(uint32 mapId);
  dtNavMeshQuery const* GetModelNavMeshQuery(uint32 displayId);
  dtNavMesh const* GetNavMesh(uint32 mapId);

  uint32 getLoadedTilesCount() const { return loadedTiles; }
  uint32 getLoadedMapsCount() const { return loadedMMaps.size(); }

private:
  bool loadMapData(uint32 mapId);
  uint32 packTileID(int32 x, int32 y);

  MMapDataSet loadedMMaps;
  ACE_RW_Mutex loadedMMaps_lock;
  MMapDataSet loadedModels;
  uint32 loadedTiles;
  ACE_Thread_Mutex lockForModels;
};

// static class
// holds all mmap global data
// access point to MMapManager singelton
class MMapFactory
{
public:
  static MMapManager* createOrGetMMapManager();
  static void clear();
};
}

#endif // _MOVE_MAP_H
