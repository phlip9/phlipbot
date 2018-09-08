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
  constexpr size_t filename_len = length("%03i.mmap") + 1;
  char filename[filename_len];
  snprintf(filename, filename_len, "%03i.mmap", mapId);

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
  if (res != DT_SUCCESS) {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{}
      << ErrorString{"Failed to initialize dtNavMesh for mmap"}
      << ErrorMapId{mapId} << ErrorFile{mmap_path} << ErrorDTResult{res});
    return false;
  }

  HADESMEM_DETAIL_TRACE_FORMAT_A("Loaded %03i.mmap", mapId);

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
  constexpr size_t filename_len = length("%03i%02i%02i.mmtile") + 1;
  char filename[filename_len];
  snprintf(filename, filename_len, "%03i%02i%02i.mmtile", mapId, y, x);

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
  if (dtStatusSucceed(res)) {
    data.release();
    mmap->mmapLoadedTiles.insert({packedGridPos, tileRef});
    ++loadedTiles;
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
      "Trying to unload mmap that hasn't been loaded. %03u%02i%02i.mmtile",
      mapId, x, y);
    return false;
  }

  MMapData* mmap = loadedMMaps[mapId].get();

  // check if we have this tile loaded
  uint32_t packedGridPos = packTileID(x, y);
  if (mmap->mmapLoadedTiles.find(packedGridPos) ==
      mmap->mmapLoadedTiles.end()) {
    // file may not exist, therefore not loaded
    HADESMEM_DETAIL_TRACE_FORMAT_A(
      "Trying to unload mmap tile that hasn't been loaded. %03u%02i%02i.mmtile",
      mapId, x, y);
    return false;
  }

  dtTileRef tileRef = mmap->mmapLoadedTiles[packedGridPos];

  // unload, and mark as non loaded
  if (DT_SUCCESS != mmap->navMesh->removeTile(tileRef, nullptr, nullptr)) {
    // this is technically a memory leak
    // if the grid is later reloaded, dtNavMesh::addTile will return error but
    // no extra memory is used we cannot recover from this error - assert out
    HADESMEM_DETAIL_TRACE_FORMAT_A(
      "Could not unload %03u%02i%02i.mmtile from navmesh", mapId, x, y);
    HADESMEM_DETAIL_ASSERT(false);
  }

  mmap->mmapLoadedTiles.erase(packedGridPos);
  --loadedTiles;
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

  // unload all tiles from given map
  MMapData* mmap = loadedMMaps[mapId].get();
  for (auto& pair : mmap->mmapLoadedTiles) {
    uint32_t x = (pair.first >> 16);
    uint32_t y = (pair.first & 0x0000FFFF);
    if (DT_SUCCESS !=
        mmap->navMesh->removeTile(pair.second, nullptr, nullptr)) {
      HADESMEM_DETAIL_TRACE_FORMAT_A(
        "Could not unload %03u%02i%02i.mmtile from navmesh", mapId, x, y);
    } else {
      --loadedTiles;
    }
  }

  loadedMMaps.erase(mapId);
  HADESMEM_DETAIL_TRACE_FORMAT_A("Could not unload %03u.mmap from navmesh",
                                 mapId);

  return true;
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
  HADESMEM_DETAIL_TRACE_FORMAT_A("Unloaded mapId %03u instanceId %u", mapId,
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
  constexpr size_t filename_len = length("%04i.mmap") + 1;
  char filename[filename_len];
  snprintf(filename, filename_len, "%04i.mmap", displayId);

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

}
