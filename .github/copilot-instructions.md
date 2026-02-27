# ft_minecraft AI Agent Instructions

## Project Overview
A networked Minecraft-like voxel game built in C++20 with OpenGL 3.3+. Uses a client-server architecture with UDP networking, procedural terrain generation, and chunk-based world streaming.

## Architecture

### Client-Server Split
- **Client** (`ft_minecraft`): OpenGL rendering, camera, input handling, chunk mesh building
- **Server** (`ft_minecraft_server`): World generation, physics, authoritative game state
- **Shared**: Network protocol, chunk data structures, entity/physics logic, block types

Code is strictly separated:
- `src/client/` + `include/client/` → client executable only
- `src/server/` + `include/server/` → server executable only  
- `src/shared/` + `include/shared/` → compiled into both

### Core Components

**World Representation** (Template-based inheritance):
- `Chunk` (shared base): 16×256×16 block storage with palette compression via `BitPackedArray`
- `ChunkGeneration` (server): Extends `Chunk`, generates terrain with multi-octave noise
- `ChunkRenderer` (client): Extends `Chunk`, builds OpenGL mesh from block data
- `CommonWorld<ChunkT>` (shared template): Common world operations (raycasting, block queries)
- `World` (server): `CommonWorld<ChunkGeneration>` with region streaming, async generation
- `Renderer` (client): `CommonWorld<ChunkRenderer>` with chunk decompression, mesh building

**Networking** (`include/shared/Network.hpp`, `Protocol.hpp`):
- Custom UDP protocol with big-endian serialization (not JSON/binary)
- `BufferWriter`/`BufferReader` for manual byte packing
- `Packet` base class with factory registry via `AutoRegister<T>`
- Chunks sent compressed with ZSTD (see `NetChunkHeader` → `NetChunkData` flow)
- 20 TPS server tick rate (`Config.hpp`: `TPS = 20.0f`)

**Terrain Generation**:
- Multi-noise system: continentalness, erosion, peaks/valleys, temperature, humidity
- Spline-based height interpolation (see `ChunkGeneration::interpolateSpline`)
- Biomes computed from noise combinations (7 types: plains, desert, forest, tundra, swamp, ocean, mountain)
- Procedural caves planned but incomplete (see `generateCaves` TODOs)

**Block Storage**:
- Palette system: `std::vector<BlockType> palette` maps indices to block types
- `BitPackedArray` stores palette indices (4-8 bits per block, dynamically grows)
- Serialization via `saveToStream`/`loadFromStream` for region files

## Build & Run

### Dependencies
On Linux (Wayland/X11): `sudo apt install libwayland-dev libxkbcommon-dev xorg-dev`

### Build Process
```bash
./build.sh  # or manually:
mkdir -p build && cd build
cmake .. && make -j$(nproc)
```

CMake auto-fetches: GLFW, GLM, Glad (4.6), ImGui, FastNoiseLite, stb_image, ZSTD, FreeType

### Running
```bash
cd build
./ft_minecraft_server [optional_seed]  # Terminal 1
./ft_minecraft [optional_seed]         # Terminal 2
```

Server listens on UDP port 1234 (`Config.hpp`). Client connects to `127.0.0.1`.

### Debugging
- ASAN build: `mkdir build && cd build && cmake -DASAN=ON .. && make`
- Shaders auto-copy to `build/shaders/` on changes (see `copy_shaders` target)
- Server saves regions to `build/regions/` (currently `SAVES_ACTIVE = false` in `World.hpp`)

## Critical Patterns

### Chunk Coordinate Systems
```cpp
// Global world coords → Chunk coords + Local coords
int chunkX = globalX / Chunk::WIDTH;   // floor division for negatives!
int localX = globalX % Chunk::WIDTH;
// Use World::globalCoordsToLocalCoords() for proper negative handling
```

### Packet Handling
When adding a new packet:
1. Define enum in `PacketType` (`Network.hpp`)
2. Create struct in `Protocol.hpp` inheriting `Packet`
3. Implement `encode()`/`decode()` with `BufferWriter`/`BufferReader`
4. Add `inline AutoRegister<YourPacket> _reg_YourPacket;` for factory registration
5. Handle in `Server::dispatch()` or `UDPClient` callback

### Async Chunk Generation (Server)
- `World::generationFutures` holds `std::future<...>` for chunk generation
- Max concurrent tasks: `World::maxConcurrentGeneration` (default 4)
- Check futures with `wait_for(0s)` to avoid blocking main thread

### Chunk Streaming Protocol
1. Server sends `NetChunkHeader` with compressed/uncompressed sizes
2. Client stores in `Renderer::chunksData` map
3. Server sends one or more `NetChunkData` packets (max MAXLINE=1400 bytes each)
4. Client reassembles, ZSTD decompresses, calls `loadFromStream()`, builds mesh
5. Mesh upload happens in `ChunkRenderer::buildMesh()` (generates VAO/VBO)

### Controls Configuration
- `App` uses macro `CONTROL_LIST` to generate control enums
- Saved to `build/controls.cfg` in `key_name=GLFW_KEY_X` format
- Reload at runtime via `loadControlsFromFile()`

### ImGui Integration
- Compiled as static lib (`imgui` target in CMakeLists)
- Backend: `imgui_impl_glfw` + `imgui_impl_opengl3`
- Debug window toggleable with F3 (see `App::debugWindow()`)
- Mouse capture: check `ImGuiIO::WantCaptureMouse` before processing game input

## Common Gotchas

- **Negative coordinates**: C++ `%` and `/` don't floor for negatives. Use `World::floorDiv()` or `globalCoordsToLocalCoords()`.
- **Packet size limits**: `MAXLINE = 1400` bytes. Large chunks split across multiple `NetChunkData` packets.
- **Chunk adjacency**: `Chunk::hasAllAdjacentChunkLoaded()` must return true before visibility checks work correctly (see `isBlockVisible`).
- **Shader compilation**: Shaders loaded from `build/shaders/`. Check `Shader` constructor for error handling.
- **BitPackedArray growth**: When palette exceeds `2^bitsPerEntry`, array must be repacked (see `setBlock` logic).
- **Floating point precision**: Use `glm::ivec3` for block positions, `glm::vec3` for entity positions.

## Active TODOs (from codebase)
- Delta compression for `NetPlayerMove` packets (see `Protocol.hpp:101`)
- Multi-threaded chunk sending (`Server.cpp:219`)
- Region save/load logic with multiple players (`World.cpp:338`)
- Spaghetti cave generation (`ChunkGeneration.cpp:129`)
- Menu system refactor (`App.cpp:923`)

## Testing
No automated test suite. Manual testing:
1. Build with ASAN to catch memory errors
2. Test negative chunk coordinates (edge case prone)
3. Verify chunk loading/unloading at different `loadRadius` values (F3 menu)
4. Test packet loss scenarios (server restart while client connected)

## Key Files Reference
- Network protocol definition: `include/shared/Protocol.hpp`
- Server tick loop: `src/server/Server.cpp` → `Server::gameTick()`
- Client render loop: `src/client/App.cpp` → `App::render()`
- Terrain noise: `src/server/ChunkGeneration.cpp`
- Chunk mesh building: `src/client/ChunkRenderer.cpp`
- World streaming: `src/server/World.cpp` → `updateVisibleChunks()`
