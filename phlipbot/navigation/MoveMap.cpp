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

#include "MoveMap.hpp"

#include <fstream>
#include <sstream>
#include <string>

#include <hadesmem/detail/assert.hpp>
#include <hadesmem/detail/trace.hpp>
#include <hadesmem/error.hpp>

#include <doctest.h>

#include "../wow_constants.hpp"
#include "MoveMapSharedDefines.hpp"

using std::mutex;
using std::shared_lock;
using std::shared_mutex;
using std::thread;
using std::unique_lock;

using std::ifstream;
using std::make_tuple;
using std::make_unique;
using std::move;
using std::unique_ptr;
using std::unordered_map;

using std::string;
using std::stringstream;

namespace fs = std::filesystem;
namespace this_thread = std::this_thread;

using hadesmem::ErrorString;

// TODO(phlip9): unloadMap() needs synchronization
// TODO(phlip9): I don't really trust the synchronization here...

namespace
{
template <size_t N>
constexpr size_t length(char const (&)[N])
{
  return N - 1;
}

template <typename T>
string to_str(T t)
{
  stringstream ss;
  ss << t;
  return ss.str();
}
}

namespace phlipbot
{
// ######################## MMapManager ########################
bool MMapManager::loadMapData(uint32_t mapId)
{
  {
    // acquire reader lock
    shared_lock<shared_mutex> lock(loadedMMaps_lock);

    // do we already have this map loaded?
    if (loadedMMaps.find(mapId) != loadedMMaps.end()) {
      return true;
    }
  }

  // load and init dtNavMesh - read parameters from file
  constexpr size_t filename_len = length("%03u.mmap") + 1;
  char filename[filename_len];
  snprintf(filename, filename_len, "%03u.mmap", mapId);

  fs::path mmap_path = mmapDir / filename;

  dtNavMeshParams params;
  {
    ifstream mmap_fstream{mmap_path, std::ios::binary};
    if (!mmap_fstream) {
      HADESMEM_DETAIL_THROW_EXCEPTION(hadesmem::Error{}
                                      << ErrorString{"Failed to open mmap file"}
                                      << ErrorFile{mmap_path});
    }

    if (!mmap_fstream.read(reinterpret_cast<char*>(&params), sizeof(params))) {
      HADESMEM_DETAIL_THROW_EXCEPTION(
        hadesmem::Error{}
        << ErrorString{"Failed to read dtNavMeshParams from file"}
        << ErrorFile{mmap_path});
    }
  }

  auto mesh = make_unique<dtNavMesh>();
  HADESMEM_DETAIL_ASSERT(mesh && "Failed to allocate dtNavMesh");
  dtStatus res = mesh->init(&params);
  if (dtStatusFailed(res)) {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{}
      << ErrorString{"Failed to initialize dtNavMesh for mmap"}
      << ErrorMapId{mapId} << ErrorFile{mmap_path} << ErrorDTResult{res});
  }

  HADESMEM_DETAIL_TRACE_FORMAT_A("Loaded %03u.mmap", mapId);

  // store inside our map list
  auto mmap_data = make_unique<MMapData>(move(mesh));

  {
    // acquire writer lock
    unique_lock<shared_mutex> lock(loadedMMaps_lock);
    if (loadedMMaps.find(mapId) == loadedMMaps.end()) {
      loadedMMaps.insert({mapId, move(mmap_data)});
    }
  }

  return true;
}

uint32_t MMapManager::packTileID(int32_t x, int32_t y)
{
  return static_cast<uint32_t>(x << 16 | y);
}

bool MMapManager::loadMap(uint32_t mapId, int32_t x, int32_t y)
{
  // make sure the mmap is loaded and ready to load tiles
  if (!loadMapData(mapId)) {
    return false;
  }

  // get this mmap data
  MMapData* mmap;
  {
    // acquire reader lock
    shared_lock<shared_mutex> lock{loadedMMaps_lock};
    mmap = loadedMMaps[mapId].get();
  };

  HADESMEM_DETAIL_ASSERT(mmap->navMesh && "MMapData not initialized");

  // check if we already have this tile loaded
  uint32_t packedGridPos = packTileID(x, y);

  // exclusively acquire the loading lock
  unique_lock<mutex> lock{mmap->tilesLoading_lock};

  // mmap already loaded
  if (mmap->mmapLoadedTiles.find(packedGridPos) !=
      mmap->mmapLoadedTiles.end()) {
    return false;
  }

  // load this tile :: mmaps/MMMXXYY.mmtile
  constexpr size_t filename_len = length("%03u%02d%02d.mmtile") + 1;
  char filename[filename_len];
  snprintf(filename, filename_len, "%03u%02d%02d.mmtile", mapId, y, x);

  fs::path mmtile_path = mmapDir / filename;
  ifstream mmtile_fstream{mmtile_path, std::ios::binary};

  if (!mmtile_fstream) {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{} << ErrorString{"Could not open mmtile file"}
                        << ErrorMapId{mapId} << ErrorMapTile{make_tuple(x, y)}
                        << ErrorFile{mmtile_path});
  }

  // read header
  MmapTileHeader file_header;
  if (!mmtile_fstream.read(reinterpret_cast<char*>(&file_header),
                           sizeof(file_header))) {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{}
      << ErrorString{"Failed to read MmapTileHeader from file"}
      << ErrorFile{mmtile_path});
  }

  if (file_header.mmapMagic != MMAP_MAGIC) {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{} << ErrorString{"Invalid magic in mmap header"}
                        << ErrorMapId{mapId} << ErrorMapTile{make_tuple(x, y)}
                        << ErrorFile{mmtile_path}
                        << ErrorHeaderMagic{file_header.mmapMagic});
  }

  if (file_header.mmapVersion != MMAP_VERSION) {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{} << ErrorString{"Bad version in mmap header"}
                        << ErrorMapId{mapId} << ErrorMapTile{make_tuple(x, y)}
                        << ErrorFile{mmtile_path}
                        << ErrorHeaderVersion{file_header.mmapVersion});
  }

  auto data = unique_ptr<unsigned char[]>(new unsigned char[file_header.size]);
  HADESMEM_DETAIL_ASSERT(data && "Failed to allocate mmap tile data");

  if (!mmtile_fstream.read(reinterpret_cast<char*>(data.get()),
                           file_header.size)) {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{} << ErrorString{"Failed to read tile map data"}
                        << ErrorMapId{mapId} << ErrorMapTile{make_tuple(x, y)}
                        << ErrorFile{mmtile_path});
  }

  dtTileRef tileRef = 0;

  // memory allocated for data is now managed by detour, and will be deallocated
  // when the tile is removed
  dtStatus res = mmap->navMesh->addTile(data.get(), file_header.size,
                                        DT_TILE_FREE_DATA, 0, &tileRef);
  if (dtStatusSucceed(res) && tileRef != 0) {
    data.release();
    mmap->mmapLoadedTiles.insert({packedGridPos, tileRef});
    ++loadedTiles;

    HADESMEM_DETAIL_TRACE_FORMAT_A("Loaded mmap tile %03u%02d%02d.mmtile",
                                   mapId, y, x);
    return true;
  } else {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{} << ErrorString{"Failed to load mmap tile into nav mesh"}
                        << ErrorMapId{mapId} << ErrorMapTile{make_tuple(x, y)}
                        << ErrorFile{mmtile_path} << ErrorDTResult{res});
  }

  return false;
}

bool MMapManager::unloadMap(uint32_t mapId, int32_t x, int32_t y)
{
  // check if we have this map loaded
  if (loadedMMaps.find(mapId) == loadedMMaps.end()) {
    // file may not exist, therefore not loaded
    HADESMEM_DETAIL_TRACE_FORMAT_A(
      "Trying to unload mmap that hasn't been loaded. %03u%02d%02d.mmtile",
      mapId, y, x);
    return false;
  }

  MMapData* mmap = loadedMMaps[mapId].get();

  // check if we have this tile loaded
  uint32_t packedGridPos = packTileID(x, y);
  if (mmap->mmapLoadedTiles.find(packedGridPos) ==
      mmap->mmapLoadedTiles.end()) {
    // file may not exist, therefore not loaded
    HADESMEM_DETAIL_TRACE_FORMAT_A(
      "Trying to unload mmap tile that hasn't been loaded. %03u%02d%02d.mmtile",
      mapId, y, x);
    return false;
  }

  dtTileRef tileRef = mmap->mmapLoadedTiles[packedGridPos];

  // unload, and mark as non loaded
  dtStatus res = mmap->navMesh->removeTile(tileRef, nullptr, nullptr);
  if (dtStatusFailed(res)) {
    // this is technically a memory leak
    // if the grid is later reloaded, dtNavMesh::addTile will return error but
    // no extra memory is used we cannot recover from this error
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{}
      << ErrorString{"Failed to unload mmap tile from navmesh"}
      << ErrorMapId{mapId} << ErrorMapTile{make_tuple(x, y)});
  }

  mmap->mmapLoadedTiles.erase(packedGridPos);
  --loadedTiles;

  HADESMEM_DETAIL_TRACE_FORMAT_A("Unloaded mmap tile %03u%02d%02d.mmtile",
                                 mapId, y, x);

  return true;
}

bool MMapManager::unloadMap(uint32_t mapId)
{
  if (loadedMMaps.find(mapId) == loadedMMaps.end()) {
    // file may not exist, therefore not loaded
    HADESMEM_DETAIL_TRACE_FORMAT_A(
      "Trying to unload navmesh map %03u that hasn't been loaded", mapId);
    return false;
  }

  bool success_flag = true;

  // unload all tiles from given map
  MMapData* mmap = loadedMMaps[mapId].get();
  for (auto& pair : mmap->mmapLoadedTiles) {
    int32_t x = (pair.first >> 16);
    int32_t y = (pair.first & 0x0000FFFF);

    dtStatus res = mmap->navMesh->removeTile(pair.second, nullptr, nullptr);
    if (dtStatusFailed(res)) {
      HADESMEM_DETAIL_TRACE_FORMAT_A(
        "Could not unload %03u%02i%02i.mmtile from navmesh", mapId, y, x);
      success_flag = false;
    } else {
      HADESMEM_DETAIL_TRACE_FORMAT_A(
        "Unloaded %03u%02i%02i.mmtile from navmesh", mapId, y, x);
      --loadedTiles;
    }
  }

  loadedMMaps.erase(mapId);

  if (success_flag) {
    HADESMEM_DETAIL_TRACE_FORMAT_A("Unloaded %03u.mmap from navmesh", mapId);
  } else {
    HADESMEM_DETAIL_TRACE_FORMAT_A("Could not unload %03u.mmap from navmesh",
                                   mapId);
  }

  return success_flag;
}

bool MMapManager::unloadMapInstance(uint32_t mapId, thread::id tid)
{
  // check if we have this map loaded
  if (loadedMMaps.find(mapId) == loadedMMaps.end()) {
    // file may not exist, therefore not loaded
    HADESMEM_DETAIL_TRACE_FORMAT_A(
      "Trying to unload navmesh map %03u that hasn't been loaded", mapId);
    return false;
  }

  MMapData* mmap = loadedMMaps[mapId].get();
  if (mmap->navMeshQueries.find(tid) == mmap->navMeshQueries.end()) {
    HADESMEM_DETAIL_TRACE_FORMAT_A("Trying to unload dtNavMeshQuery mapId %03u "
                                   "tid %s that hasn't been loaded",
                                   mapId, to_str(tid).c_str());
    return false;
  }

  mmap->navMeshQueries.erase(tid);
  HADESMEM_DETAIL_TRACE_FORMAT_A("Unloaded mapId %03u instanceId %s", mapId,
                                 to_str(tid).c_str());
  return true;
}

dtNavMesh const* MMapManager::GetNavMesh(uint32_t mapId)
{
  if (loadedMMaps.find(mapId) == loadedMMaps.end()) {
    return nullptr;
  }

  return loadedMMaps[mapId].get()->navMesh.get();
}

dtNavMeshQuery const* MMapManager::GetNavMeshQuery(uint32_t mapId)
{
  if (loadedMMaps.find(mapId) == loadedMMaps.end()) {
    return nullptr;
  }

  // uint32_t tid = ACE_Based::Thread::currentId();
  thread::id tid{this_thread::get_id()};
  MMapData* mmap = loadedMMaps[mapId].get();

  // acquire reader lock
  mmap->navMeshQueries_lock.lock_shared();

  auto it = mmap->navMeshQueries.find(tid);
  if (it == mmap->navMeshQueries.end()) {
    // upgrade to writer lock
    mmap->navMeshQueries_lock.unlock_shared();
    unique_lock<shared_mutex> lock{mmap->navMeshQueries_lock};

    // allocate mesh query
    auto newNavMeshQuery = make_unique<dtNavMeshQuery>();

    HADESMEM_DETAIL_ASSERT(newNavMeshQuery &&
                           "Failed to allocate new nav mesh query");
    dtStatus res = newNavMeshQuery->init(mmap->navMesh.get(), 2048);
    if (res != DT_SUCCESS) {
      HADESMEM_DETAIL_THROW_EXCEPTION(
        hadesmem::Error{} << ErrorString{"Failed to initialize dtNavMeshQuery"}
                          << ErrorMapId{mapId} << ErrorDTResult{res});
    }

    HADESMEM_DETAIL_TRACE_FORMAT_A(
      "Created dtNavMeshQuery for mapId %03u tid %s", mapId,
      to_str(tid).c_str());

    auto* navMeshQuery = newNavMeshQuery.get();
    mmap->navMeshQueries.insert({tid, move(newNavMeshQuery)});
    return navMeshQuery;
  }

  auto* navMeshQuery = it->second.get();
  mmap->navMeshQueries_lock.unlock_shared();

  return navMeshQuery;
}

bool MMapManager::loadGameObject(uint32_t displayId)
{
  // we already have this map loaded?
  if (loadedModels.find(displayId) != loadedModels.end()) {
    return true;
  }

  // load and init dtNavMesh - read parameters from file
  constexpr size_t filename_len = length("go%04u.mmap") + 1;
  char filename[filename_len];
  snprintf(filename, filename_len, "go%04u.mmap", displayId);

  fs::path go_path = mmapDir / filename;

  ifstream go_fstream{go_path, std::ios::binary};
  if (!go_fstream) {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{} << ErrorString{"Failed to open game object file"}
                        << ErrorGODisplayId{displayId} << ErrorFile{go_path});
  }

  MmapTileHeader file_header;
  if (!go_fstream.read(reinterpret_cast<char*>(&file_header),
                       sizeof(file_header))) {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{}
      << ErrorString{"Failed to read MmapTileHeader from file"}
      << ErrorGODisplayId{displayId} << ErrorFile{go_path});
  }

  if (file_header.mmapMagic != MMAP_MAGIC) {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{} << ErrorString{"Invalid magic in mmap header"}
                        << ErrorGODisplayId{displayId} << ErrorFile{go_path}
                        << ErrorHeaderMagic{file_header.mmapMagic});
  }

  if (file_header.mmapVersion != MMAP_VERSION) {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{} << ErrorString{"Bad version in mmap header"}
                        << ErrorGODisplayId{displayId} << ErrorFile{go_path}
                        << ErrorHeaderVersion{file_header.mmapVersion});
  }

  auto data = unique_ptr<unsigned char[]>(new unsigned char[file_header.size]);
  HADESMEM_DETAIL_ASSERT(data && "Failed to allocate data");

  if (!go_fstream.read(reinterpret_cast<char*>(data.get()), file_header.size)) {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{} << ErrorString{"Failed to read data from file"}
                        << ErrorGODisplayId{displayId} << ErrorFile{go_path});
  }

  auto mesh = make_unique<dtNavMesh>();
  HADESMEM_DETAIL_ASSERT(mesh && "Failed to allocate dtNavMesh");

  // transfer ownership of data to the nav mesh
  dtStatus res = mesh->init(data.get(), file_header.size, DT_TILE_FREE_DATA);
  if (dtStatusFailed(res)) {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{} << ErrorString{"Failed to init nav mesh"}
                        << ErrorGODisplayId{displayId} << ErrorFile{go_path}
                        << ErrorDTResult{res});
  }

  data.release();
  HADESMEM_DETAIL_TRACE_FORMAT_A("Loaded file %s [size=%u]", filename,
                                 file_header.size);

  auto mmap_data = make_unique<MMapData>(move(mesh));
  loadedModels.insert({displayId, move(mmap_data)});

  return true;
}

dtNavMeshQuery const* MMapManager::GetModelNavMeshQuery(uint32_t displayId)
{
  if (loadedModels.find(displayId) == loadedModels.end()) {
    return nullptr;
  }

  thread::id tid = this_thread::get_id();

  MMapData* mmap = loadedModels[displayId].get();
  if (mmap->navMeshQueries.find(tid) == mmap->navMeshQueries.end()) {
    unique_lock<mutex> lock(lockForModels);
    if (mmap->navMeshQueries.find(tid) == mmap->navMeshQueries.end()) {
      // allocate mesh query
      auto query = make_unique<dtNavMeshQuery>();
      HADESMEM_DETAIL_ASSERT(query && "Failed to allocate dtNavMeshQuery");

      if (dtStatusFailed(query->init(mmap->navMesh.get(), 2048))) {
        HADESMEM_DETAIL_THROW_EXCEPTION(
          hadesmem::Error{} << ErrorString{"Failed to init dtNavMeshQuery"}
                            << ErrorGODisplayId{displayId});
      }

      HADESMEM_DETAIL_TRACE_FORMAT_A(
        "Created dtNavMeshQuery for displayId %03u tid %s", displayId,
        to_str(tid).c_str());

      mmap->navMeshQueries.insert({tid, move(query)});
    }
  }

  return mmap->navMeshQueries[tid].get();
}

namespace test
{
TEST_CASE("MMapManager can load a map")
{
  // TODO(phlip9): move the test tile into phlipbot repo so our tests aren't
  //               reliant on my global filesystem setup

  // Eastern Kingdoms
  uint32_t map_id = 0;
  // Elwynn Forest
  int32_t x = 32;
  int32_t y = 48;

  vec3 start{-8949.95f, -132.493f, 83.5312f};
  vec3 end{-9046.507f, -45.71962f, 88.33186f};

  constexpr size_t filename_len = length("%03u%02i%02i.mmtile") + 1;
  char tile_filename[filename_len];
  snprintf(tile_filename, filename_len, "%03u%02i%02i.mmtile", map_id, y, x);

  fs::path mmap_dir = "C:\\MaNGOS\\data\\__mmaps";
  fs::path tile_path = mmap_dir / tile_filename;

  REQUIRE(fs::exists(mmap_dir));
  REQUIRE(fs::exists(tile_path));

  MMapManager mmap{mmap_dir};
  CHECK(mmap.getLoadedMapsCount() == 0);
  CHECK(mmap.getLoadedTilesCount() == 0);
  CHECK(mmap.GetNavMesh(map_id) == nullptr);
  CHECK(mmap.GetNavMeshQuery(map_id) == nullptr);

  // try loading the map
  CHECK(mmap.loadMap(map_id, x, y));
  CHECK(mmap.getLoadedMapsCount() > 0);
  CHECK(mmap.getLoadedTilesCount() > 0);

  // we should be able to retrieve the navmesh for Eastern Kingdoms
  dtNavMesh const* navmesh = mmap.GetNavMesh(map_id);
  REQUIRE(navmesh != nullptr);

  dtNavMeshQuery const* query = mmap.GetNavMeshQuery(map_id);
  REQUIRE(query != nullptr);

  dtMeshTile const* tile = navmesh->getTile(0);
  REQUIRE(tile != nullptr);

  dtPoly const* p = &tile->polys[0];
  CHECK(p != nullptr);

  dtPolyDetail const* pd = &tile->detailMeshes[0];
  CHECK(pd != nullptr);
  CHECK(pd->triCount > 0);
  CHECK(pd->vertCount > 0);

  dtPolyRef p2;
  float center[3] = {start.y, start.z, start.x};
  float extants[3] = {10.0f, 10.0f, 10.0f};
  float nearest[3] = {0, 0, 0};
  dtQueryFilter filter;
  filter.setIncludeFlags(0xFFFF);
  filter.setExcludeFlags(0x0);
  dtStatus res = query->findNearestPoly(center, extants, &filter, &p2, nearest);
  CHECK(dtStatusSucceed(res));
  CHECK(p2 != 0);
}
}
}
