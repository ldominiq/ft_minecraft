#include "AudioManager.hpp"

#include "Camera.hpp"
#include "Renderer.hpp"
#include "ClientPlayer.hpp"
#include "ClientCreeper.hpp"
#include "Config.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <unordered_set>

namespace {
    // Distance attenuation envelope. min: full volume, max: silent.
    constexpr float kAttenMin = 1.0f;
    constexpr float kAttenMax = 64.0f;

    // Footstep cadence: trigger one step per ~1.6 m walked. Matches Minecraft-ish feel.
    constexpr float kFootstepStrideM = 1.6f;
    constexpr float kSwimStrokeM     = 1.2f;

    float frand01() {
        return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
    }
}

AudioManager::AudioManager() = default;

AudioManager::~AudioManager() {
    shutdown();
}

bool AudioManager::init() {
    SoLoud::result r = engine.init();
    if (r != SoLoud::SO_NO_ERROR) {
        std::cerr << "[Audio] SoLoud init failed: " << r << std::endl;
        return false;
    }

    // Inverse-distance attenuation matches what people expect from "3D audio".
    engine.set3dSoundSpeed(343.0f); // speed of sound (m/s) — affects doppler

    // Route music & sfx through their own buses so volumes can be controlled independently.
    musicBusHandle = engine.play(musicBus);
    sfxBusHandle   = engine.play(sfxBus);

    loadAllAssets();
    applyVolumes();

    std::cout << "[Audio] SoLoud ready (backend: " << engine.getBackendString() << ")" << std::endl;
    return true;
}

void AudioManager::shutdown() {
    // Idempotent: deinit() is safe to call multiple times on SoLoud.
    engine.stopAll();
    engine.deinit();
    sfxCache.clear();
    musicCache.clear();
    mobStates.clear();
}

// ---------------------------------------------------------------------------
// Asset loading
// ---------------------------------------------------------------------------

bool AudioManager::loadSfx(SoundId id, const std::vector<std::string>& paths) {
    auto& vec = sfxCache[id];
    for (const auto& p : paths) {
        auto wav = std::make_unique<SoLoud::Wav>();
        SoLoud::result r = wav->load(p.c_str());
        if (r != SoLoud::SO_NO_ERROR) {
            // Missing assets are fine — log once and move on. Game still runs silent for that id.
            std::cerr << "[Audio] missing sfx: " << p << std::endl;
            continue;
        }
        // Voices that drift out of audible range get killed instead of muted-but-running.
        wav->setInaudibleBehavior(false, true);
        vec.push_back(std::move(wav));
    }
    return !vec.empty();
}

bool AudioManager::loadMusic(BiomeType biome, const std::string& path) {
    auto stream = std::make_unique<SoLoud::WavStream>();
    SoLoud::result r = stream->load(path.c_str());
    if (r != SoLoud::SO_NO_ERROR) {
        std::cerr << "[Audio] missing music: " << path << std::endl;
        return false;
    }
    stream->setLooping(true);
    musicCache[biome] = std::move(stream);
    return true;
}

void AudioManager::loadAllAssets() {
    // Footsteps — variations make the loop sound less robotic.
    loadSfx(SoundId::Footstep_Grass, {
        "assets/sounds/footsteps/grass/grass1.wav",
        "assets/sounds/footsteps/grass/grass2.wav",
        "assets/sounds/footsteps/grass/grass3.wav",
        "assets/sounds/footsteps/grass/grass4.wav",
        "assets/sounds/footsteps/grass/grass5.wav",
        "assets/sounds/footsteps/grass/grass6.wav",
        });
    loadSfx(SoundId::Footstep_Stone, {
        "assets/sounds/footsteps/stone/stone1.wav",
        "assets/sounds/footsteps/stone/stone2.wav",
        "assets/sounds/footsteps/stone/stone3.wav",
        "assets/sounds/footsteps/stone/stone4.wav",
        "assets/sounds/footsteps/stone/stone5.wav",
        "assets/sounds/footsteps/stone/stone6.wav",
        });
    loadSfx(SoundId::Footstep_Wood, {
        "assets/sounds/footsteps/wood/wood1.wav",
        "assets/sounds/footsteps/wood/wood2.wav",
        "assets/sounds/footsteps/wood/wood3.wav",
        "assets/sounds/footsteps/wood/wood4.wav",
        "assets/sounds/footsteps/wood/wood5.wav",
        "assets/sounds/footsteps/wood/wood6.wav",
        });
    loadSfx(SoundId::Footstep_Sand,    {
        "assets/sounds/footsteps/sand/sand1.wav",
        "assets/sounds/footsteps/sand/sand2.wav",
        "assets/sounds/footsteps/sand/sand3.wav",
        "assets/sounds/footsteps/sand/sand4.wav",
        "assets/sounds/footsteps/sand/sand5.wav",
        });
    loadSfx(SoundId::Footstep_Snow,    {
        "assets/sounds/footsteps/snow/snow1.wav",
        "assets/sounds/footsteps/snow/snow2.wav",
        "assets/sounds/footsteps/snow/snow3.wav",
        "assets/sounds/footsteps/snow/snow4.wav",
        });
    loadSfx(SoundId::Footstep_Gravel,  {
        "assets/sounds/footsteps/gravel/gravel1.wav",
        "assets/sounds/footsteps/gravel/gravel2.wav",
        "assets/sounds/footsteps/gravel/gravel3.wav",
        "assets/sounds/footsteps/gravel/gravel4.wav",
        });
    loadSfx(SoundId::Footstep_Leaves,  {"assets/sounds/footsteps/leaves/leaves1.wav"});
    loadSfx(SoundId::Footstep_Water,   {"assets/sounds/footsteps/water/splash1.wav"});

    // Block break / place — keyed by material group.
    loadSfx(SoundId::Break_Stone,  {
        "assets/sounds/blocks/stone/stone1.wav",
        "assets/sounds/blocks/stone/stone2.wav",
        "assets/sounds/blocks/stone/stone3.wav",
        "assets/sounds/blocks/stone/stone4.wav",
        });
    loadSfx(SoundId::Break_Wood,   {
        "assets/sounds/blocks/wood/wood1.wav",
        "assets/sounds/blocks/wood/wood2.wav",
        "assets/sounds/blocks/wood/wood3.wav",
        "assets/sounds/blocks/wood/wood4.wav",
        });
    loadSfx(SoundId::Break_Dirt,   {
        "assets/sounds/blocks/grass/grass1.wav",
        "assets/sounds/blocks/grass/grass2.wav",
        "assets/sounds/blocks/grass/grass3.wav",
        "assets/sounds/blocks/grass/grass4.wav",
        });
    loadSfx(SoundId::Break_Sand,   {
        "assets/sounds/blocks/sand/sand1.wav",
        "assets/sounds/blocks/sand/sand2.wav",
        "assets/sounds/blocks/sand/sand3.wav",
        "assets/sounds/blocks/sand/sand4.wav",
        });
    loadSfx(SoundId::Break_Gravel, {
        "assets/sounds/blocks/gravel/gravel1.wav",
        "assets/sounds/blocks/gravel/gravel2.wav",
        "assets/sounds/blocks/gravel/gravel3.wav",
        "assets/sounds/blocks/gravel/gravel4.wav",
        });
    loadSfx(SoundId::Break_Leaves, {"assets/sounds/blocks/break/leaves.wav"});
    loadSfx(SoundId::Break_Snow, {
        "assets/sounds/blocks/snow/snow1.wav",
        "assets/sounds/blocks/snow/snow2.wav",
        "assets/sounds/blocks/snow/snow3.wav",
        "assets/sounds/blocks/snow/snow4.wav",
        });
    loadSfx(SoundId::Place_Stone,  {
        "assets/sounds/blocks/stone/stone1.wav",
        "assets/sounds/blocks/stone/stone2.wav",
        "assets/sounds/blocks/stone/stone3.wav",
        "assets/sounds/blocks/stone/stone4.wav",
        });
    loadSfx(SoundId::Place_Wood,   {
        "assets/sounds/blocks/wood/wood1.wav",
        "assets/sounds/blocks/wood/wood2.wav",
        "assets/sounds/blocks/wood/wood3.wav",
        "assets/sounds/blocks/wood/wood4.wav",
        });
    loadSfx(SoundId::Place_Dirt,   {
        "assets/sounds/blocks/grass/grass1.wav",
        "assets/sounds/blocks/grass/grass2.wav",
        "assets/sounds/blocks/grass/grass3.wav",
        "assets/sounds/blocks/grass/grass4.wav",
        });
    loadSfx(SoundId::Place_Sand,   {
        "assets/sounds/blocks/sand/sand1.wav",
        "assets/sounds/blocks/sand/sand2.wav",
        "assets/sounds/blocks/sand/sand3.wav",
        "assets/sounds/blocks/sand/sand4.wav",
        });
    loadSfx(SoundId::Place_Leaves, {"assets/sounds/blocks/place/leaves.wav"});
    loadSfx(SoundId::Place_Snow, {
        "assets/sounds/blocks/snow/snow1.wav",
        "assets/sounds/blocks/snow/snow2.wav",
        "assets/sounds/blocks/snow/snow3.wav",
        "assets/sounds/blocks/snow/snow4.wav",
        });

    // Mobs. Filenames here mirror what's actually on disk under assets/sounds/mobs/ —
    // zombies have their own per-step set; creepers reuse the player's material footsteps.
    loadSfx(SoundId::Zombie_Idle, {
        "assets/sounds/mobs/zombie/say1.wav",
        "assets/sounds/mobs/zombie/say2.wav",
        "assets/sounds/mobs/zombie/say3.wav"});
    loadSfx(SoundId::Zombie_Hurt, {
        "assets/sounds/mobs/zombie/hurt1.wav",
        "assets/sounds/mobs/zombie/hurt2.wav"});
    loadSfx(SoundId::Zombie_Death,   {"assets/sounds/mobs/zombie/death.wav"});
    loadSfx(SoundId::Zombie_Step, {
        "assets/sounds/mobs/zombie/step1.wav",
        "assets/sounds/mobs/zombie/step2.wav",
        "assets/sounds/mobs/zombie/step3.wav",
        "assets/sounds/mobs/zombie/step4.wav",
        "assets/sounds/mobs/zombie/step5.wav"});
    // Creeper hurt reuses the say files (no dedicated hurt asset; vanilla does the same).
    loadSfx(SoundId::Creeper_Idle, {
        "assets/sounds/mobs/creeper/say1.wav",
        "assets/sounds/mobs/creeper/say2.wav",
        "assets/sounds/mobs/creeper/say3.wav",
        "assets/sounds/mobs/creeper/say4.wav"});
    loadSfx(SoundId::Creeper_Hurt, {
        "assets/sounds/mobs/creeper/say1.wav",
        "assets/sounds/mobs/creeper/say2.wav",
        "assets/sounds/mobs/creeper/say3.wav",
        "assets/sounds/mobs/creeper/say4.wav"});
    loadSfx(SoundId::Creeper_Death,  {"assets/sounds/mobs/creeper/death.wav"});
    loadSfx(SoundId::Creeper_Fuse,   {"assets/sounds/mobs/creeper/fuse.wav"});
    loadSfx(SoundId::Creeper_Explode, {
        "assets/sounds/mobs/creeper/explode.wav",
        "assets/sounds/mobs/creeper/explode1.wav",
        "assets/sounds/mobs/creeper/explode2.wav",
        "assets/sounds/mobs/creeper/explode3.wav",
        "assets/sounds/mobs/creeper/explode4.wav"});

    // Player + UI.
    loadSfx(SoundId::Player_Jump,        {"assets/sounds/player/jump.wav"});
    loadSfx(SoundId::Player_Splash,      {"assets/sounds/player/splash.wav"});
    loadSfx(SoundId::Player_Swim,        {"assets/sounds/player/swim.wav"});
    loadSfx(SoundId::Player_AttackSwing, {
        "assets/sounds/player/attack/strong1.wav",
        "assets/sounds/player/attack/strong2.wav",
        "assets/sounds/player/attack/strong3.wav",
        "assets/sounds/player/attack/strong4.wav",
        "assets/sounds/player/attack/strong5.wav",
        "assets/sounds/player/attack/strong6.wav",
        });
    loadSfx(SoundId::UI_Click,           {"assets/sounds/ui/button_click.wav"});

    // Per-biome ambient music. Streamed (no full decode in RAM).
    loadMusic(BiomeType::PLAINS,         "assets/sounds/music/plains.ogg");
    loadMusic(BiomeType::DESERT,         "assets/sounds/music/desert.ogg");
    loadMusic(BiomeType::DARK_FOREST,    "assets/sounds/music/forest.ogg");
    loadMusic(BiomeType::JUNGLE,         "assets/sounds/music/jungle.ogg");
    loadMusic(BiomeType::SAVANNA,        "assets/sounds/music/savanna.ogg");
    loadMusic(BiomeType::BIRCH_FOREST,   "assets/sounds/music/birch.ogg");
    loadMusic(BiomeType::MESA,           "assets/sounds/music/mesa.ogg");
    loadMusic(BiomeType::TUNDRA,         "assets/sounds/music/tundra.ogg");
    loadMusic(BiomeType::SWAMP,          "assets/sounds/music/swamp.ogg");
    loadMusic(BiomeType::OCEAN,          "assets/sounds/music/ocean.ogg");
    loadMusic(BiomeType::MOUNTAIN,       "assets/sounds/music/mountain.ogg");
    loadMusic(BiomeType::ICE_PLAINS,     "assets/sounds/music/ice.ogg");
    loadMusic(BiomeType::VOLCANIC,       "assets/sounds/music/volcanic.ogg");
    loadMusic(BiomeType::RED_DESERT,     "assets/sounds/music/red_desert.ogg");
    loadMusic(BiomeType::NETHER,         "assets/sounds/music/nether.ogg");
    loadMusic(BiomeType::MUSHROOM_ISLAND,"assets/sounds/music/mushroom.ogg");
}

SoLoud::Wav* AudioManager::pickVariation(SoundId id) {
    auto it = sfxCache.find(id);
    if (it == sfxCache.end() || it->second.empty()) return nullptr;
    const auto& vec = it->second;
    return vec[std::rand() % vec.size()].get();
}

// ---------------------------------------------------------------------------
// Block -> material lookup. Hand-rolled groups; covers most common blocks.
// Anything not listed falls through to Stone/Dirt as a safe default.
// ---------------------------------------------------------------------------

SoundId AudioManager::footstepFor(BlockType b) {
    switch (b) {
        case BlockType::GRASS:
        case BlockType::DIRT:
        case BlockType::COARSE_DIRT:
        case BlockType::CLAY:            return SoundId::Footstep_Grass;
        case BlockType::SAND:
        case BlockType::RED_SAND:
        case BlockType::SOUL_SAND:       return SoundId::Footstep_Sand;
        case BlockType::SNOW:
        case BlockType::ICE:
        case BlockType::PACKED_ICE:
        case BlockType::BLUE_ICE:        return SoundId::Footstep_Snow;
        case BlockType::GRAVEL:          return SoundId::Footstep_Gravel;
        case BlockType::OAK_LEAVES:
        case BlockType::BIRCH_LEAVES:
        case BlockType::ACACIA_LEAVES:
        case BlockType::SPRUCE_LEAVES:
        case BlockType::JUNGLE_LEAVES:
        case BlockType::DARK_OAK_LEAVES: return SoundId::Footstep_Leaves;
        case BlockType::OAK_LOG:
        case BlockType::BIRCH_LOG:
        case BlockType::ACACIA_LOG:
        case BlockType::SPRUCE_LOG:
        case BlockType::JUNGLE_LOG:
        case BlockType::DARK_OAK_LOG:    return SoundId::Footstep_Wood;
        case BlockType::WATER:           return SoundId::Footstep_Water;
        default:                         return SoundId::Footstep_Stone; // stone, cobble, andesite, etc.
    }
}

SoundId AudioManager::breakFor(BlockType b) {
    switch (b) {
        case BlockType::GRASS:
        case BlockType::DIRT:
        case BlockType::COARSE_DIRT:
        case BlockType::CLAY:            return SoundId::Break_Dirt;
        case BlockType::SAND:
        case BlockType::RED_SAND:
        case BlockType::SOUL_SAND:
        case BlockType::SANDSTONE:
        case BlockType::RED_SANDSTONE:   return SoundId::Break_Sand;
        case BlockType::OAK_LEAVES:
        case BlockType::BIRCH_LEAVES:
        case BlockType::ACACIA_LEAVES:
        case BlockType::SPRUCE_LEAVES:
        case BlockType::JUNGLE_LEAVES:
        case BlockType::DARK_OAK_LEAVES: return SoundId::Break_Leaves;
        case BlockType::OAK_LOG:
        case BlockType::BIRCH_LOG:
        case BlockType::ACACIA_LOG:
        case BlockType::SPRUCE_LOG:
        case BlockType::JUNGLE_LOG:
        case BlockType::DARK_OAK_LOG:    return SoundId::Break_Wood;
        default:                         return SoundId::Break_Stone;
    }
}

SoundId AudioManager::placeFor(BlockType b) {
    switch (b) {
        case BlockType::GRASS:
        case BlockType::DIRT:
        case BlockType::COARSE_DIRT:
        case BlockType::CLAY:            return SoundId::Place_Dirt;
        case BlockType::SAND:
        case BlockType::RED_SAND:
        case BlockType::SOUL_SAND:
        case BlockType::SANDSTONE:
        case BlockType::RED_SANDSTONE:   return SoundId::Place_Sand;
        case BlockType::OAK_LEAVES:
        case BlockType::BIRCH_LEAVES:
        case BlockType::ACACIA_LEAVES:
        case BlockType::SPRUCE_LEAVES:
        case BlockType::JUNGLE_LEAVES:
        case BlockType::DARK_OAK_LEAVES: return SoundId::Place_Leaves;
        case BlockType::OAK_LOG:
        case BlockType::BIRCH_LOG:
        case BlockType::ACACIA_LOG:
        case BlockType::SPRUCE_LOG:
        case BlockType::JUNGLE_LOG:
        case BlockType::DARK_OAK_LOG:    return SoundId::Place_Wood;
        default:                         return SoundId::Place_Stone;
    }
}

// ---------------------------------------------------------------------------
// Public play API
// ---------------------------------------------------------------------------

void AudioManager::playSfx3D(SoundId id, glm::dvec3 pos, glm::vec3 vel, float volume) {
    SoLoud::Wav* w = pickVariation(id);
    if (!w) return;

    // SoLoud takes float coords. Audible range is small (<= kAttenMax meters), so the
    // double->float cast is fine even at large absolute world coords (relative error <<< 1m).
    const float fx = static_cast<float>(pos.x);
    const float fy = static_cast<float>(pos.y);
    const float fz = static_cast<float>(pos.z);

    // Route through the SFX bus so the SFX volume slider applies. Note: SoLoud's
    // `engine.play(..., aBus)` parameter is a 0-31 channel index (legacy), NOT a voice
    // handle — to route through a Bus object you must call bus.play3d(...).
    SoLoud::handle h = sfxBus.play3d(*w, fx, fy, fz, vel.x, vel.y, vel.z, volume);
    engine.set3dSourceMinMaxDistance(h, kAttenMin, kAttenMax);
    engine.set3dSourceAttenuation(h, SoLoud::AudioSource::INVERSE_DISTANCE, 1.0f);
}

void AudioManager::playSfx2D(SoundId id, float volume) {
    SoLoud::Wav* w = pickVariation(id);
    if (!w) return;
    sfxBus.play(*w, volume);
}

// ---------------------------------------------------------------------------
// Biome music + crossfade
// ---------------------------------------------------------------------------

void AudioManager::setBiome(BiomeType newBiome) {
    if (biomeInitialized && newBiome == currentBiome) return;
    crossfadeTo(newBiome, biomeInitialized ? 3.0f : 0.5f);
    currentBiome = newBiome;
    biomeInitialized = true;
}

void AudioManager::crossfadeTo(BiomeType b, float seconds) {
    auto it = musicCache.find(b);
    if (it == musicCache.end()) {
        // No track for this biome — just fade the current one out.
        if (activeMusicHandle) {
            engine.fadeVolume(activeMusicHandle, 0.0f, seconds);
            engine.scheduleStop(activeMusicHandle, seconds);
            fadingOutHandle = activeMusicHandle;
            activeMusicHandle = 0;
        }
        return;
    }

    // Push the current track to the "fading out" slot.
    if (activeMusicHandle) {
        engine.fadeVolume(activeMusicHandle, 0.0f, seconds);
        engine.scheduleStop(activeMusicHandle, seconds);
        fadingOutHandle = activeMusicHandle;
    }

    // Start the new track silent on the music bus, then fade in. Must use musicBus.play()
    SoLoud::handle h = musicBus.play(*it->second, 0.0f);
    engine.setLooping(h, true);
    engine.fadeVolume(h, musicVolume, seconds);
    activeMusicHandle = h;
    musicPaused = false;
}

// ---------------------------------------------------------------------------
// Volume sliders
// ---------------------------------------------------------------------------

void AudioManager::setMasterVolume(float v) { masterVolume = std::clamp(v, 0.0f, 1.0f); applyVolumes(); }
void AudioManager::setMusicVolume (float v) { musicVolume  = std::clamp(v, 0.0f, 1.0f); applyVolumes(); }
void AudioManager::setSfxVolume   (float v) { sfxVolume    = std::clamp(v, 0.0f, 1.0f); applyVolumes(); }

void AudioManager::applyVolumes() {
    // Master applies globally; per-bus volumes scale music/sfx independently underneath it.
    engine.setGlobalVolume(masterVolume);
    if (musicBusHandle) engine.setVolume(musicBusHandle, musicVolume);
    if (sfxBusHandle)   engine.setVolume(sfxBusHandle,   sfxVolume);

    // If music is currently playing (faded in), re-apply its target volume so slider drags
    // during playback are immediately audible.
    if (activeMusicHandle) engine.setVolume(activeMusicHandle, musicVolume);
}

// ---------------------------------------------------------------------------
// Per-frame update
// ---------------------------------------------------------------------------

void AudioManager::update(float deltaTime, bool isPlaying, Camera& cam, Renderer& world) {
    // In menus: pause music, skip emitter logic. Don't deinit — resume is instant on return.
    if (!isPlaying) {
        if (activeMusicHandle && !musicPaused) {
            engine.setPause(activeMusicHandle, true);
            musicPaused = true;
        }
        return;
    }
    if (musicPaused && activeMusicHandle) {
        engine.setPause(activeMusicHandle, false);
        musicPaused = false;
    }

    // ---- Update 3D listener from camera/player every frame --------------
    auto player = cam.getPlayer();
    if (player) {
        glm::dvec3 pos = cam.getEyePosD();
        glm::vec3 fwd  = player->Front;
        glm::vec3 up   = player->WorldUp;
        // Velocity comes from the most recent snapshot (Entity::velocity is protected).
        // The physics layer stores velocity in *blocks per tick* (Minecraft-style integrator),
        // so scale to m/s for SoLoud's doppler model.
        glm::vec3 vel  = cam.getLatestSnapshot().velocity * TPS;

        engine.set3dListenerParameters(
            static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z),
            fwd.x, fwd.y, fwd.z,
            up.x, up.y, up.z,
            vel.x, vel.y, vel.z);
    }

    // SoLoud requires this once per frame to apply 3D parameters to all live voices.
    engine.update3dAudio();

    // ---- Drive trigger logic --------------------------------------------
    updateFootsteps(deltaTime, cam, world);
    updateMobAudio(deltaTime, cam, world);
}

void AudioManager::updateFootsteps(float dt, Camera& cam, Renderer& world) {
    auto player = cam.getPlayer();
    if (!player) return;

    glm::dvec3 ppos  = player->getPositionD();
    // Snapshot velocity is in blocks-per-tick (Minecraft-style integrator); convert to m/s
    // so it lines up with `speed * dt` distance accumulation below.
    glm::vec3  pvel  = cam.getLatestSnapshot().velocity * TPS;

    // Horizontal speed only — we don't want the y-axis fall to count as walking.
    glm::vec2 horiz(pvel.x, pvel.z);
    float speed = glm::length(horiz);

    // The local player's own sounds are 2D — playing them as 3D emitters at the feet
    // while the listener sits at the eyes makes every step pan slightly below-and-behind

    // Detect water entry → splash one-shot.
    bool underwater = world.isUnderwater(ppos);
    if (underwater && !prevUnderwater) {
        playSfx2D(SoundId::Player_Splash, 1.0f);
    }
    prevUnderwater = underwater;

    // Jump rising edge: was on ground, no longer on ground, moving upward.
    bool onGround = player->isOnGround();
    if (!onGround && prevOnGround && pvel.y > 0.05f) {
        playSfx2D(SoundId::Player_Jump, 0.8f);
    }
    prevOnGround = onGround;

    // Underwater: replace footsteps with periodic swim strokes.
    if (underwater) {
        if (speed > 0.05f) {
            footstepDistance += speed * dt;
            if (footstepDistance >= kSwimStrokeM) {
                footstepDistance = 0.0f;
                playSfx2D(SoundId::Player_Swim, 0.7f);
            }
        }
        return;
    }

    // On-ground footsteps: accumulate distance until a stride boundary is crossed.
    if (!onGround || speed < 0.05f) {
        footstepDistance = 0.0f;
        return;
    }
    footstepDistance += speed * dt;
    if (footstepDistance < kFootstepStrideM) return;
    footstepDistance = 0.0f;

    // Sample the block one cell below the player's feet to pick the right material.
    glm::ivec3 below = glm::ivec3(glm::floor(ppos)) + glm::ivec3(0, -1, 0);
    BlockType ground = world.getBlockWorld(below);
    if (ground == BlockType::AIR) return; // stepping over a hole — skip the click

    playSfx2D(footstepFor(ground), 0.5f);
}

void AudioManager::updateMobAudio(float dt, Camera& cam, Renderer& world) {
    // Drives all non-self entity audio: remote-player footsteps, mob idle/footstep/fuse, and
    // the death/explode one-shots fired when an entity disappears from `livingEntities`.
    //
    // The local player lives on Camera (not in `livingEntities`), so iterating this list is
    // effectively "everyone but me". Server packets don't carry velocity (NetEntityMove), so
    // we derive horizontal speed from successive snapshot positions — same trick as the player
    // branch above.

    // Stall guard: a long frame (loading screen, freeze, world unload, big teleport) would otherwise
    // mark every mob as "vanished" the next frame and burst-play a chorus of death sounds. Wipe the
    // map and skip emission this tick instead.
    if (dt > 0.2f) {
        mobStates.clear();
        return;
    }

    auto localPlayer = cam.getPlayer();
    const void* localKey = localPlayer.get();

    for (auto& wle : world.livingEntities) {
        // livingEntities is std::vector<std::shared_ptr<LivingEntity>> — already strong refs.
        auto le = wle;
        if (!le) continue;
        if (le.get() == localKey) continue; // skip self (defensive — not normally present)

        const void* key = le.get();
        MobAudioState& st = mobStates[key];

        // Stash type + position every tick so the death-edge sweep below can play a 3D one-shot
        // at the entity's last known location after the entity itself is gone.
        st.type    = le->getLivingEntityType();
        st.lastPos = le->getPositionD();

        // Need at least two snapshots to derive a speed. Bail until we have history.
        if (le->snapshots.size() < 2) {
            st.footstepDist = 0.0f;
            continue;
        }
        const auto& s1 = le->snapshots.back();
        const auto& s0 = le->snapshots[le->snapshots.size() - 2];
        double snapDt = s1.time - s0.time;
        if (snapDt <= 1e-4) continue;

        glm::dvec3 dpos = s1.position - s0.position;
        glm::vec2 horiz(static_cast<float>(dpos.x), static_cast<float>(dpos.z));
        float speed = glm::length(horiz) / static_cast<float>(snapDt); // m/s

        glm::dvec3 epos = st.lastPos;

        // ---- footsteps -----------------------------------------------------
        bool moving = le->isOnGround() && speed >= 0.05f;
        if (!moving) {
            st.footstepDist = 0.0f;
        } else {
            st.footstepDist += speed * dt;
            if (st.footstepDist >= kFootstepStrideM) {
                st.footstepDist = 0.0f;

                glm::ivec3 below = glm::ivec3(glm::floor(epos)) + glm::ivec3(0, -1, 0);
                BlockType ground = world.getBlockWorld(below);
                if (ground != BlockType::AIR) {
                    switch (st.type) {
                        case PLAYER:
                            playSfx3D(footstepFor(ground), epos, glm::vec3(0.0f), 0.8f);
                            break;
                        case ZOMBIE:
                            // Material-independent zombie shuffle (5 variations on disk).
                            playSfx3D(SoundId::Zombie_Step, epos, glm::vec3(0.0f), 0.7f);
                            break;
                        case CREEPER:
                            // No creeper-specific step asset; fall back to material footsteps.
                            playSfx3D(footstepFor(ground), epos, glm::vec3(0.0f), 0.5f);
                            break;
                    }
                }
            }
        }

        // ---- mob-only ambient + per-mob edges ------------------------------
        if (st.type == ZOMBIE || st.type == CREEPER) {
            // Seed the cooldown with a random offset so a horde doesn't chirp in lockstep on the
            // first frame they enter the audio loop.
            if (!st.initialized) {
                st.idleCooldown = frand01() * 8.0f;
                st.initialized  = true;
            }

            st.idleCooldown -= dt;
            if (st.idleCooldown <= 0.0f) {
                SoundId idleId = (st.type == ZOMBIE) ? SoundId::Zombie_Idle : SoundId::Creeper_Idle;
                playSfx3D(idleId, epos, glm::vec3(0.0f), 0.7f);
                // 6–14 s — vanilla cadence. Random within range so two nearby mobs don't sync.
                st.idleCooldown = 6.0f + frand01() * 8.0f;
            }

            // Creeper fuse rising-edge. clientPrimed is mirrored from NetEntityMove's 0x08 flag.
            if (st.type == CREEPER) {
                bool primed = false;
                if (auto cc = std::dynamic_pointer_cast<ClientCreeper>(le))
                    primed = cc->clientPrimed;
                if (primed && !st.prevPrimed)
                    playSfx3D(SoundId::Creeper_Fuse, epos, glm::vec3(0.0f), 1.0f);
                st.prevPrimed = primed;
            }
        }
    }

    // ---- death / explode sweep -------------------------------------------------
    // Anything in `mobStates` that's no longer in `livingEntities` just disappeared this frame —
    // fire the appropriate one-shot before erasing. Creepers that exploded were primed at vanish
    // time; otherwise it's a regular death.
    if (!mobStates.empty()) {
        std::unordered_set<const void*> alive;
        alive.reserve(world.livingEntities.size());
        for (auto& le : world.livingEntities)
            if (le) alive.insert(le.get());
        for (auto it = mobStates.begin(); it != mobStates.end(); ) {
            if (alive.count(it->first) == 0) {
                const MobAudioState& st = it->second;
                switch (st.type) {
                    case CREEPER:
                        playSfx3D(st.prevPrimed ? SoundId::Creeper_Explode
                                                : SoundId::Creeper_Death,
                                  st.lastPos, glm::vec3(0.0f), 1.0f);
                        break;
                    case ZOMBIE:
                        playSfx3D(SoundId::Zombie_Death, st.lastPos, glm::vec3(0.0f), 1.0f);
                        break;
                    case PLAYER:
                        // No remote-player death sound today.
                        break;
                }
                it = mobStates.erase(it);
            } else {
                ++it;
            }
        }
    }
}
