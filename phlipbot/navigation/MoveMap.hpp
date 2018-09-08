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

#include <thread>
#include <filesystem>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <tuple>
#include <unordered_map>

#include <boost/exception/error_info.hpp>

#include <DetourAlloc.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>

// TODO(phlip9): make MMapManager::mmapDir configurable

//  move map related classes
namespace phlipbot
{
using ErrorMapTile =
  boost::error_info<struct TagErrorMap, std::tuple<int32_t, int32_t>>;
using ErrorMapId = boost::error_info<struct TagErrorMapId, uint32_t>;
using ErrorHeaderVersion =
  boost::error_info<struct TagErrorHeaderVersion, uint32_t>;
using ErrorHeaderMagic =
  boost::error_info<struct TagErrorHeaderMagic, uint32_t>;
using ErrorFile =
  boost::error_info<struct TagErrorFileOpen, std::filesystem::path>;
using ErrorDTResult = boost::error_info<struct TagErrorDTResult, uint32_t>;
using ErrorGODisplayId =
  boost::error_info<struct TagErrorGODisplayId, uint32_t>;

using MMapTileSet = std::unordered_map<uint32_t, dtTileRef>;
using NavMeshQuerySet =
  std::unordered_map<std::thread::id, std::unique_ptr<dtNavMeshQuery>>;

// dummy struct to hold map's mmap data
struct MMapData {
  MMapData(std::unique_ptr<dtNavMesh>&& mesh) : navMesh(std::move(mesh)) {}

  std::unique_ptr<dtNavMesh> navMesh;

  // we have to use single dtNavMeshQuery for every instance, since those are
  // not thread safe
  NavMeshQuerySet navMeshQueries; // threadId to query
  std::shared_mutex navMeshQueries_lock;
  MMapTileSet mmapLoadedTiles; // maps [map grid coords] to [dtTile]
  std::mutex tilesLoading_lock;
};

using MMapDataSet = std::unordered_map<uint32_t, std::unique_ptr<MMapData>>;

// singelton class
// holds all all access to mmap loading unloading and meshes
class MMapManager
{
public:
  explicit MMapManager(std::filesystem::path const& _mmapDir)
    : mmapDir(_mmapDir), loadedTiles(0)
  {
  }

  bool loadMap(uint32_t mapId, int32_t x, int32_t y);
  bool loadGameObject(uint32_t displayId);
  bool unloadMap(uint32_t mapId, int32_t x, int32_t y);
  bool unloadMap(uint32_t mapId);
  bool unloadMapInstance(uint32_t mapId, std::thread::id tid);

  // The returned [dtNavMeshQuery const*] is NOT threadsafe
  // Returns a NavMeshQuery valid for current thread only.
  dtNavMeshQuery const* GetNavMeshQuery(uint32_t mapId);
  dtNavMeshQuery const* GetModelNavMeshQuery(uint32_t displayId);
  dtNavMesh const* GetNavMesh(uint32_t mapId);

  uint32_t getLoadedTilesCount() const { return loadedTiles; }
  uint32_t getLoadedMapsCount() const { return loadedMMaps.size(); }

private:
  bool loadMapData(uint32_t mapId);
  uint32_t packTileID(int32_t x, int32_t y);

  std::filesystem::path mmapDir;

  MMapDataSet loadedMMaps;
  std::shared_mutex loadedMMaps_lock;
  MMapDataSet loadedModels;
  uint32_t loadedTiles;
  std::mutex lockForModels;
};
}
