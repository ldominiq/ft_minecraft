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
    constexpr float kAttenMax = 24.0f;

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

    // Per-sound multipliers default to 1.0 — the audio settings panel mutates them at runtime.
    sfxScale.fill(1.0f);

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
    explosionWindows.clear();
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
    loadSfx(SoundId::Footstep_Leaves, {
        "assets/sounds/footsteps/grass/grass1.wav",
        "assets/sounds/footsteps/grass/grass2.wav",
        "assets/sounds/footsteps/grass/grass3.wav",
        "assets/sounds/footsteps/grass/grass4.wav",
        "assets/sounds/footsteps/grass/grass5.wav",
        "assets/sounds/footsteps/grass/grass6.wav",
        });
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
    loadSfx(SoundId::Place_Gravel, {
        "assets/sounds/blocks/gravel/gravel1.wav",
        "assets/sounds/blocks/gravel/gravel2.wav",
        "assets/sounds/blocks/gravel/gravel3.wav",
        "assets/sounds/blocks/gravel/gravel4.wav",
        });
    loadSfx(SoundId::Place_Leaves, {
        "assets/sounds/blocks/grass/grass1.wav",
        "assets/sounds/blocks/grass/grass2.wav",
        "assets/sounds/blocks/grass/grass3.wav",
        "assets/sounds/blocks/grass/grass4.wav",
        });
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

    loadSfx(SoundId::Player_Jump,        {"assets/sounds/player/jump.wav"});
    loadSfx(SoundId::Player_Splash,      {
        "assets/sounds/liquids/splash.wav",
        "assets/sounds/liquids/splash2.wav",
        });
    loadSfx(SoundId::Player_HeavySplash, {"assets/sounds/liquids/heavy_splash.wav"});
    loadSfx(SoundId::Player_Swim,        {
        "assets/sounds/liquids/swim1.wav",
        "assets/sounds/liquids/swim2.wav",
        "assets/sounds/liquids/swim3.wav",
        "assets/sounds/liquids/swim4.wav",
        "assets/sounds/liquids/swim5.wav",
        "assets/sounds/liquids/swim6.wav",
        "assets/sounds/liquids/swim7.wav",
        "assets/sounds/liquids/swim8.wav",
        "assets/sounds/liquids/swim9.wav",
        "assets/sounds/liquids/swim10.wav",
        "assets/sounds/liquids/swim11.wav",
        "assets/sounds/liquids/swim12.wav",
        "assets/sounds/liquids/swim13.wav",
        "assets/sounds/liquids/swim14.wav",
        "assets/sounds/liquids/swim15.wav",
        "assets/sounds/liquids/swim16.wav",
        "assets/sounds/liquids/swim17.wav",
        "assets/sounds/liquids/swim18.wav",
        });
    loadSfx(SoundId::Player_AttackSwing, {
        "assets/sounds/player/attack/strong1.wav",
        "assets/sounds/player/attack/strong2.wav",
        "assets/sounds/player/attack/strong3.wav",
        "assets/sounds/player/attack/strong4.wav",
        "assets/sounds/player/attack/strong5.wav",
        "assets/sounds/player/attack/strong6.wav",
        });
    loadSfx(SoundId::Player_FallSmall,   {"assets/sounds/player/damage/fallsmall.wav"});
    loadSfx(SoundId::Player_FallBig,     {"assets/sounds/player/damage/fallbig.wav"});
    loadSfx(SoundId::Player_Hurt, {
        "assets/sounds/player/damage/hit1.wav",
        "assets/sounds/player/damage/hit2.wav",
        "assets/sounds/player/damage/hit3.wav"});
    // Generic Minecraft-style "block pop" used for pickup up items
    loadSfx(SoundId::Block_Pop,          {"assets/sounds/player/pop.wav"});
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

// Vegetation (flowers, tall grass, kelp, coral, dead bush, etc.). Breaks/places route to
// Break_Dirt / Place_Dirt
static bool isVegetation(BlockType b) {
    switch (b) {
        case BlockType::SHORT_GRASS:
        case BlockType::CORNFLOWER:
        case BlockType::POPPY:
        case BlockType::PINK_TULIP:
        case BlockType::ORANGE_TULIP:
        case BlockType::RED_TULIP:
        case BlockType::WHITE_TULIP:
        case BlockType::BLUE_ORCHID:
        case BlockType::ALLIUM:
        case BlockType::AZURE_BLUET:
        case BlockType::OXEYE_DAISY:
        case BlockType::LILY_OF_THE_VALLEY:
        case BlockType::WITHER_ROSE:
        case BlockType::DANDELION:
        case BlockType::RED_MUSHROOM:
        case BlockType::BROWN_MUSHROOM:
        case BlockType::DEAD_BUSH:
        case BlockType::SEAGRASS:
        case BlockType::TALL_SEAGRASS_BOTTOM:
        case BlockType::TALL_SEAGRASS_TOP:
        case BlockType::KELP:
        case BlockType::KELP_PLANT:
        case BlockType::BRAIN_CORAL:
        case BlockType::BRAIN_CORAL_FAN:
        case BlockType::BUBBLE_CORAL:
        case BlockType::BUBBLE_CORAL_FAN:
        case BlockType::FIRE_CORAL:
        case BlockType::FIRE_CORAL_FAN:
        case BlockType::HORN_CORAL:
        case BlockType::HORN_CORAL_FAN:
        case BlockType::TUBE_CORAL:
        case BlockType::TUBE_CORAL_FAN:
        case BlockType::CACTUS:
            return true;
        default:
            return false;
    }
}

SoundId AudioManager::breakFor(BlockType b) {
    if (isVegetation(b)) return SoundId::Break_Dirt;
    switch (b) {
        case BlockType::GRASS:
        case BlockType::DIRT:
        case BlockType::COARSE_DIRT:
        case BlockType::CLAY:            return SoundId::Break_Dirt;
        case BlockType::GRAVEL:          return SoundId::Break_Gravel;
        case BlockType::SNOW:            return SoundId::Break_Snow;
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
        case BlockType::DARK_OAK_LEAVES: return SoundId::Break_Dirt;
        case BlockType::OAK_LOG:
        case BlockType::BIRCH_LOG:
        case BlockType::ACACIA_LOG:
        case BlockType::SPRUCE_LOG:
        case BlockType::JUNGLE_LOG:
        case BlockType::DARK_OAK_LOG:    return SoundId::Break_Wood;
        default:                         return SoundId::Break_Stone;
    }
}

SoundId AudioManager::hurtSoundFor(LivingEntityType t) {
    switch (t) {
        case ZOMBIE:  return SoundId::Zombie_Hurt;
        case CREEPER: return SoundId::Creeper_Hurt;
        case PLAYER:
        default:      return SoundId::Player_Hurt;
    }
}

bool AudioManager::blockSfxSuppressed(glm::dvec3 worldPos) const {
    // Linear scan — windows live for <1s and there's almost never more than 1 active at a time.
    for (const auto& w : explosionWindows) {
        glm::dvec3 d = worldPos - w.pos;
        double r = static_cast<double>(w.radius);
        if (glm::dot(d, d) <= r * r) return true;
    }
    return false;
}

void AudioManager::onCreeperExploded(const void* key, glm::dvec3 epos, glm::dvec3 listenerPos) {
    // operator[] auto-creates a stub for creepers that exploded same-frame as spawning
    // (never tracked). Type/diedByExplosion populated so the sweep treats it correctly.
    if (key) {
        auto& st = mobStates[key];
        st.deathSoundFired = true;
        st.diedByExplosion = true;
        st.type            = CREEPER;
        if (st.fuseHandle) { engine.stop(st.fuseHandle); st.fuseHandle = 0; }
    }

    // 2D at point-blank: 3D thins the sample when the listener is on top of the source
    // and being knocked; positional cue only matters past ~10m.
    glm::dvec3 d = epos - listenerPos;
    constexpr double kExplode2DRadius = 10.0;
    if (glm::dot(d, d) <= kExplode2DRadius * kExplode2DRadius)
        playSfx2D(SoundId::Creeper_Explode, 1.0f);
    else
        playSfx3D(SoundId::Creeper_Explode, epos, glm::vec3(0.0f), 1.0f);
    // Mutes the crater's MODIFIED_BLOCK_DATA break-sound cascade.
    explosionWindows.push_back({epos, 6.0f, 0.7f});
}

SoundId AudioManager::placeFor(BlockType b) {
    if (isVegetation(b)) return SoundId::Place_Dirt;
    switch (b) {
        case BlockType::GRASS:
        case BlockType::DIRT:
        case BlockType::COARSE_DIRT:
        case BlockType::CLAY:            return SoundId::Place_Dirt;
        case BlockType::GRAVEL:          return SoundId::Place_Gravel;
        case BlockType::SNOW:            return SoundId::Place_Snow;
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
        case BlockType::DARK_OAK_LEAVES: return SoundId::Place_Dirt;
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

    const float fx = static_cast<float>(pos.x);
    const float fy = static_cast<float>(pos.y);
    const float fz = static_cast<float>(pos.z);

    volume *= sfxScale[static_cast<size_t>(id)];

    // bus.play3d() routes through the SFX bus; engine.play(..., busIdx) takes a legacy
    // 0-31 channel index instead, not what we want.
    SoLoud::handle h = sfxBus.play3d(*w, fx, fy, fz, vel.x, vel.y, vel.z, volume);
    engine.set3dSourceMinMaxDistance(h, kAttenMin, kAttenMax);
    // LINEAR cleanly reaches 0 at kAttenMax. INVERSE floors at ~5% and setMinMaxDistance
    // clamps distance, so far voices keep mushing forever at the floor.
    engine.set3dSourceAttenuation(h, SoLoud::AudioSource::LINEAR_DISTANCE, 1.0f);
}

void AudioManager::playSfx2D(SoundId id, float volume) {
    SoLoud::Wav* w = pickVariation(id);
    if (!w) return;
    sfxBus.play(*w, volume * sfxScale[static_cast<size_t>(id)]);
}

SoLoud::handle AudioManager::playSfx3DTracked(SoundId id, glm::dvec3 pos, glm::vec3 vel, float volume) {
    SoLoud::Wav* w = pickVariation(id);
    if (!w) return 0;

    const float fx = static_cast<float>(pos.x);
    const float fy = static_cast<float>(pos.y);
    const float fz = static_cast<float>(pos.z);

    volume *= sfxScale[static_cast<size_t>(id)];
    SoLoud::handle h = sfxBus.play3d(*w, fx, fy, fz, vel.x, vel.y, vel.z, volume);
    engine.set3dSourceMinMaxDistance(h, kAttenMin, kAttenMax);
    engine.set3dSourceAttenuation(h, SoLoud::AudioSource::LINEAR_DISTANCE, 1.0f);
    return h;
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

float AudioManager::getSfxScale(SoundId id) const {
    return sfxScale[static_cast<size_t>(id)];
}

void AudioManager::setSfxScale(SoundId id, float v) {
    // 0..2: 0 mutes, 1 = call-site default, >1 boosts assets that need headroom.
    sfxScale[static_cast<size_t>(id)] = std::clamp(v, 0.0f, 2.0f);
}

const char* AudioManager::sfxName(SoundId id) {
    switch (id) {
        case SoundId::Footstep_Grass:  return "Footstep Grass";
        case SoundId::Footstep_Stone:  return "Footstep Stone";
        case SoundId::Footstep_Wood:   return "Footstep Wood";
        case SoundId::Footstep_Sand:   return "Footstep Sand";
        case SoundId::Footstep_Snow:   return "Footstep Snow";
        case SoundId::Footstep_Gravel: return "Footstep Gravel";
        case SoundId::Footstep_Leaves: return "Footstep Leaves";
        case SoundId::Footstep_Water:  return "Footstep Water";
        case SoundId::Break_Stone:     return "Break Stone";
        case SoundId::Break_Wood:      return "Break Wood";
        case SoundId::Break_Dirt:      return "Break Dirt";
        case SoundId::Break_Sand:      return "Break Sand";
        case SoundId::Break_Gravel:    return "Break Gravel";
        case SoundId::Break_Leaves:    return "Break Leaves";
        case SoundId::Break_Snow:      return "Break Snow";
        case SoundId::Place_Stone:     return "Place Stone";
        case SoundId::Place_Wood:      return "Place Wood";
        case SoundId::Place_Dirt:      return "Place Dirt";
        case SoundId::Place_Sand:      return "Place Sand";
        case SoundId::Place_Gravel:    return "Place Gravel";
        case SoundId::Place_Leaves:    return "Place Leaves";
        case SoundId::Place_Snow:      return "Place Snow";
        case SoundId::Zombie_Idle:     return "Zombie Idle";
        case SoundId::Zombie_Hurt:     return "Zombie Hurt";
        case SoundId::Zombie_Death:    return "Zombie Death";
        case SoundId::Zombie_Step:     return "Zombie Step";
        case SoundId::Creeper_Idle:    return "Creeper Idle";
        case SoundId::Creeper_Hurt:    return "Creeper Hurt";
        case SoundId::Creeper_Death:   return "Creeper Death";
        case SoundId::Creeper_Fuse:    return "Creeper Fuse";
        case SoundId::Creeper_Explode: return "Creeper Explode";
        case SoundId::Player_Jump:        return "Player Jump";
        case SoundId::Player_Splash:      return "Player Splash";
        case SoundId::Player_HeavySplash: return "Player Heavy Splash";
        case SoundId::Player_Swim:        return "Player Swim";
        case SoundId::Player_AttackSwing: return "Player Attack Swing";
        case SoundId::Player_FallSmall:   return "Player Fall Small";
        case SoundId::Player_FallBig:     return "Player Fall Big";
        case SoundId::Player_Hurt:        return "Player Hurt";
        case SoundId::Block_Pop:          return "Block Pop";
        case SoundId::UI_Click:           return "UI Click";
        case SoundId::_Count:             return "?";
    }
    return "?";
}

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
        // Listener velocity is forced to zero (no doppler). Real player velocity spikes
        // hard during creeper-explosion knockback / fall recovery, and SoLoud's doppler then
        // pitch-shifts every active 3D voice — heard as a "crackle" during/after the blast.
        // Doppler isn't worth those artifacts in a survival game.
        glm::vec3 vel  = glm::vec3(0.0f);

        // Listener-jump detection: a >16m single-frame move means respawn or debug teleport, not
        // normal motion (max sprint ~7 m/s × 16 ms ≈ 0.11 m). Stop tracked long-running 3D voices
        // so they don't replay at the wrong spatialization for the new listener position.
        if (hasPrevListenerPos) {
            glm::dvec3 jump = pos - prevListenerPos;
            constexpr double kJumpThresh = 16.0;
            if (glm::dot(jump, jump) > kJumpThresh * kJumpThresh) {
                for (auto& kv : mobStates) {
                    if (kv.second.fuseHandle) {
                        engine.stop(kv.second.fuseHandle);
                        kv.second.fuseHandle = 0;
                    }
                    // Force a fresh rising-edge so a still-primed creeper can re-trigger its
                    // fuse the next time we get into earshot.
                    kv.second.prevPrimed = false;
                }
            }
        }
        prevListenerPos    = pos;
        hasPrevListenerPos = true;

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

    // Detect water entry → splash one-shot. Use prevFallDist (latched at the bottom of this
    // function from last frame's accumulatedFallDistance) — by the time the underwater edge
    // fires, the server has likely already zeroed the counter, same hazard the fall-landing
    // block below documents. Threshold matches Player_FallBig's 4-block fall-damage boundary.
    bool underwater = world.isUnderwater(ppos);
    if (underwater && !prevUnderwater) {
        if (prevFallDist >= 4.0f)
            playSfx2D(SoundId::Player_HeavySplash, 1.0f);
        else
            playSfx2D(SoundId::Player_Splash, 0.4f);
    }
    prevUnderwater = underwater;

    // Jump rising edge: was on ground, no longer on ground, moving upward.
    bool onGround = player->isOnGround();
    if (!onGround && prevOnGround && pvel.y > 0.05f) {
        playSfx2D(SoundId::Player_Jump, 0.8f);
    }

    // Fall-impact rising edge: was airborne, just touched ground. Use the *previous* frame's
    // accumulatedFallDistance because the server resets it to 0 on landing — by the time we see
    // onGround=true the counter is already 0. Thresholds match vanilla:
    //   >=4 blocks → fallbig (matches the SAFE_FALL_DISTANCE+1 boundary that triggers damage)
    //   >=2.0      → fallsmall (just a thump, no damage)
    float curFallDist = player->getAccumulatedFallDistance();
    if (onGround && !prevOnGround) {
        if (prevFallDist >= 4.0f)
            playSfx2D(SoundId::Player_FallBig,   1.0f);
        else if (prevFallDist >= 1.8f)
            playSfx2D(SoundId::Player_FallSmall, 0.8f);
    }
    // Latch *after* the edge check so the value we read is from "the frame before landing".
    prevFallDist = curFallDist;
    prevOnGround = onGround;

    // Local-player hurt edge: any decrement in health triggers a 2D hit one-shot. Health is
    // server-authoritative and arrives via reconciliation; client-side prediction never lowers
    // it, so a drop is always a real hit (mob attack, fall damage, void). The 0->20 respawn
    // case is an *increase*, so the strict `<` keeps it silent.
    float curHealth = player->health;
    if (curHealth < lastPlayerHealth)
        playSfx2D(SoundId::Player_Hurt, 0.9f);
    lastPlayerHealth = curHealth;

    // Underwater: replace footsteps with periodic swim strokes.
    if (underwater) {
        if (speed > 0.05f) {
            footstepDistance += speed * dt;
            if (footstepDistance >= kSwimStrokeM) {
                footstepDistance = 0.0f;
                playSfx2D(SoundId::Player_Swim, 0.1f);
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
    // map and skip emission this tick instead. Stop any tracked voices first (e.g. fuses) so they
    // don't keep emitting from the old positions after we forget about them.
    if (dt > 0.2f) {
        for (auto& kv : mobStates)
            if (kv.second.fuseHandle) engine.stop(kv.second.fuseHandle);
        mobStates.clear();
        explosionWindows.clear();
        return;
    }

    // Tick down active explosion suppression windows (set when a primed creeper vanishes below).
    // Windows are typically 0.5–0.8s — long enough to swallow the crater's MODIFIED_BLOCK_DATA
    // burst that arrives over the following few packets without muting unrelated mining nearby.
    for (auto it = explosionWindows.begin(); it != explosionWindows.end(); ) {
        it->ttl -= dt;
        if (it->ttl <= 0.0f) it = explosionWindows.erase(it);
        else                 ++it;
    }

    auto localPlayer = cam.getPlayer();
    const void* localKey = localPlayer.get();

    // Listener position for the per-mob audibility gate. One-shots beyond kAttenMax are killed
    // by SoLoud's inaudible-behavior, but only after one buffer fires — that buffer is the
    // "split-second of zombie death" the player hears when a mob dies far away. Skip them here.
    const glm::dvec3 listenerPos = cam.getEyePosD();
    const double kAudibleD2 = static_cast<double>(kAttenMax) * static_cast<double>(kAttenMax);

    for (auto& wle : world.livingEntities) {
        // livingEntities is std::vector<std::shared_ptr<LivingEntity>> — already strong refs.
        auto le = wle;
        if (!le) continue;
        if (le.get() == localKey) continue; // skip self (defensive — not normally present)

        const void* key = le.get();
        MobAudioState& st = mobStates[key];

        // Stash type + position + diedByExplosion every tick so the death-edge sweep below
        // can fire the right 3D one-shot at the entity's last known location, after the entity
        // itself is gone. diedByExplosion is set by Renderer.cpp when the death packet's
        // positionFlags bit 0x20 is on; the value lingers on LivingEntity until it's removed.
        st.type            = le->getLivingEntityType();
        st.lastPos         = le->getPositionD();
        st.diedByExplosion = le->diedByExplosion;

        glm::dvec3 epos = st.lastPos;
        glm::dvec3 dToListener = epos - listenerPos;
        bool audible = glm::dot(dToListener, dToListener) <= kAudibleD2;

        // ---- per-snapshot horizontal delta ---------------------------------
        // Reading dpos from snapshots[size-2..size-1] is unreliable: Renderer.cpp wipes & reseeds
        // the snapshots vector whenever there's a >100ms server-side gap (`stale` path), and the
        // reseed has both entries at the SAME position. Mobs that stop-and-go (zombie chase →
        // attack pause → chase) hit that path every cycle, losing the first move-after-idle.
        // Caching `lastConsumedPos` ourselves survives the reseed and gives us the true distance
        // travelled since last consumption. We compute horizDelta unconditionally so both the
        // footstep stride and the swim stroke (PLAYER underwater) can credit it.
        float horizDelta = 0.0f;
        if (!le->snapshots.empty()) {
            const auto& s1 = le->snapshots.back();
            if (s1.time != st.lastSnapTime) {
                if (st.hasLastConsumedPos) {
                    glm::dvec3 dpos = s1.position - st.lastConsumedPos;
                    glm::vec2  horiz(static_cast<float>(dpos.x), static_cast<float>(dpos.z));
                    float dist = glm::length(horiz);
                    // Sanity cap: a >5m single-snapshot delta is a teleport / chunk-load, not
                    // walking/swimming. Don't credit it.
                    if (dist < 5.0f) horizDelta = dist;
                }
                st.lastConsumedPos    = s1.position;
                st.hasLastConsumedPos = true;
                st.lastSnapTime       = s1.time;
            }
        }

        // ---- footstep stride accumulator (on-ground only) ------------------
        if (le->snapshots.empty() || !le->isOnGround())
            st.footstepDist = 0.0f; // airborne or no data — reset stride accumulator
        else
            st.footstepDist += horizDelta;

        if (st.footstepDist >= kFootstepStrideM) {
            st.footstepDist = 0.0f;

            glm::ivec3 below = glm::ivec3(glm::floor(epos)) + glm::ivec3(0, -1, 0);
            BlockType ground = world.getBlockWorld(below);
            if (ground != BlockType::AIR && audible) {
                // Pre-attenuation volumes; LINEAR_DISTANCE has high mid-range gain so values
                // stay <1.0 to keep mob steps from dominating the mix.
                switch (st.type) {
                    case PLAYER:
                        playSfx3D(footstepFor(ground), epos, glm::vec3(0.0f), 0.55f);
                        break;
                    case ZOMBIE:
                        // Material-independent zombie shuffle (5 variations on disk).
                        playSfx3D(SoundId::Zombie_Step, epos, glm::vec3(0.0f), 0.85f);
                        break;
                    case CREEPER:
                        // No creeper-specific step asset; fall back to material footsteps.
                        playSfx3D(footstepFor(ground), epos, glm::vec3(0.0f), 0.65f);
                        break;
                }
            }
        }

        // ---- water audio (PLAYER only — splash on entry, swim while submerged) ---
        // Mirrors the local-player branch in updateFootsteps(), but emitted as 3D one-shots at
        // the remote player's position so they pan/attenuate from the listener's POV.
        if (st.type == PLAYER) {
            bool   underwater   = world.isUnderwater(epos);
            float  curFallDist  = le->getAccumulatedFallDistance();

            // Water-entry rising edge → splash (random of 2 variations) or heavy splash on a
            // fall-damage-threshold drop. Read prevFallDist (last frame's value) — server zeroes
            // accumulatedFallDistance on contact with water, same hazard the local code documents.
            if (underwater && !st.prevUnderwater && audible) {
                if (st.prevFallDist >= 4.0f)
                    playSfx3D(SoundId::Player_HeavySplash, epos, glm::vec3(0.0f), 1.0f);
                else
                    playSfx3D(SoundId::Player_Splash, epos, glm::vec3(0.0f), 0.4f);
            }

            // Swim strokes — accumulate horizontal travel ignoring isOnGround() (you're floating
            // in water, not standing on the bottom). One stroke per kSwimStrokeM blocks.
            if (underwater) {
                st.swimDistance += horizDelta;
                if (st.swimDistance >= kSwimStrokeM) {
                    st.swimDistance = 0.0f;
                    if (audible)
                        playSfx3D(SoundId::Player_Swim, epos, glm::vec3(0.0f), 0.1f);
                }
            } else {
                st.swimDistance = 0.0f;
            }

            st.prevUnderwater = underwater;
            st.prevFallDist   = curFallDist;
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
                if (audible) {
                    SoundId idleId = (st.type == ZOMBIE) ? SoundId::Zombie_Idle : SoundId::Creeper_Idle;
                    playSfx3D(idleId, epos, glm::vec3(0.0f), 0.5f);
                }
                // 6–14 s — vanilla cadence. Random within range so two nearby mobs don't sync.
                st.idleCooldown = 6.0f + frand01() * 8.0f;
            }

            // Creeper fuse rising-edge. clientPrimed is mirrored from NetEntityMove's 0x08 flag.
            // Track the SoLoud handle so we can stop the (long) fuse sample when the creeper
            // unprimes, dies, or explodes — otherwise the voice keeps playing at its old position
            // and glitches when the listener jumps (e.g. you die mid-fuse and respawn far away).
            if (st.type == CREEPER) {
                bool primed = false;
                if (auto cc = std::dynamic_pointer_cast<ClientCreeper>(le))
                    primed = cc->clientPrimed;
                if (primed && !st.prevPrimed) {
                    if (audible)
                        st.fuseHandle = playSfx3DTracked(SoundId::Creeper_Fuse, epos, glm::vec3(0.0f), 0.75f);
                } else if (!primed && st.prevPrimed) {
                    if (st.fuseHandle) { engine.stop(st.fuseHandle); st.fuseHandle = 0; }
                }
                st.prevPrimed = primed;
            }
        }
    }

    // ---- death / explode sweep -------------------------------------------------
    // Anything in `mobStates` that's no longer in `livingEntities` just disappeared this frame —
    // fire the appropriate one-shot before erasing. Creepers that exploded were primed at vanish
    // time; otherwise it's a regular death. Distance-gated to kAttenMax so a zombie dying past
    // the audible envelope doesn't pop one buffer of "uggh!" before SoLoud kills the voice.
    if (!mobStates.empty()) {
        std::unordered_set<const void*> alive;
        alive.reserve(world.livingEntities.size());
        for (auto& le : world.livingEntities)
            if (le) alive.insert(le.get());
        for (auto it = mobStates.begin(); it != mobStates.end(); ) {
            if (alive.count(it->first) == 0) {
                MobAudioState& st = it->second;
                // Stop any still-playing fuse voice for this creeper. Otherwise the sample
                // keeps emitting from `lastPos` long after the creeper is gone (and worse, the
                // listener may have teleported, which produces an audible jump on the voice).
                if (st.fuseHandle) { engine.stop(st.fuseHandle); st.fuseHandle = 0; }

                glm::dvec3 d = st.lastPos - listenerPos;
                bool deathAudible = glm::dot(d, d) <= kAudibleD2;

                // Fallback for anything that bypassed App.cpp's immediate death-sound path
                // (which sets deathSoundFired). diedByExplosion picks Explode vs Death — a
                // primed creeper killed mid-fuse plays Death, not Explode.
                if (!st.deathSoundFired) {
                    switch (st.type) {
                        case CREEPER:
                            if (st.diedByExplosion)
                                onCreeperExploded(it->first, st.lastPos, listenerPos);
                            else if (deathAudible)
                                playSfx3D(SoundId::Creeper_Death, st.lastPos, glm::vec3(0.0f), 1.0f);
                            break;
                        case ZOMBIE:
                            if (deathAudible)
                                playSfx3D(SoundId::Zombie_Death, st.lastPos, glm::vec3(0.0f), 1.0f);
                            break;
                        case PLAYER:
                            break;
                    }
                }
                it = mobStates.erase(it);
            } else {
                ++it;
            }
        }
    }
}
