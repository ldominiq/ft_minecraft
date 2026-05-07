#pragma once

#include <soloud.h>
#include <soloud_speech.h>
#include <soloud_thread.h>
#include <soloud_wav.h>
#include <soloud_wavstream.h>
#include <soloud_bus.h>

#include <array>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Item.hpp"
#include "LivingEntity.hpp" // for LivingEntityType in MobAudioState

class Camera;
class Renderer;

// Stable identifiers for cached short SFX. Music is keyed by BiomeType separately.
enum class SoundId {
    // Footsteps (one entry per material; manager picks a random variation file).
    Footstep_Grass,
    Footstep_Stone,
    Footstep_Wood,
    Footstep_Sand,
    Footstep_Snow,
    Footstep_Gravel,
    Footstep_Leaves,
    Footstep_Water,

    // Block break / place (grouped by material so we don't need one per block id).
    Break_Stone,
    Break_Wood,
    Break_Dirt,
    Break_Sand,
    Break_Gravel,
    Break_Leaves,
    Break_Snow,
    Place_Stone,
    Place_Wood,
    Place_Dirt,
    Place_Sand,
	Place_Gravel,
    Place_Leaves,
    Place_Snow,

    // Mob sounds. Zombies have their own footstep set; creepers fall back to material footsteps.
    Zombie_Idle, Zombie_Hurt, Zombie_Death, Zombie_Step,
    Creeper_Idle, Creeper_Hurt, Creeper_Death, Creeper_Fuse, Creeper_Explode,

    // Player.
    Player_Jump,
    Player_Splash,
    Player_HeavySplash,
    Player_Swim,
    Player_AttackSwing,
    Player_FallSmall,    // landing after a short fall (>= 1.5 blocks, no damage)
    Player_FallBig,      // landing after a damaging fall (>= 4 blocks)
    Player_Hurt,         // generic player damage one-shot (hit*.wav)

    // Generic block-pop one-shot used for vegetation breaks/places.
    Block_Pop,

    // UI.
    UI_Click,

    _Count
};

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    // Boot the engine, create music/sfx buses, preload sounds. Returns false on failure.
    bool init();

    // Stop everything and release the audio device. Safe to call once.
    void shutdown();

    // Per-frame: drives 3D listener, biome music crossfade, footsteps, mob audio.
    // `isPlaying` mirrors App::GameState::Playing — false suspends music + mutes triggers.
    void update(float deltaTime, bool isPlaying, Camera& cam, Renderer& world);

    // Switch ambient music. Crossfades over ~3s
    void setBiome(BiomeType newBiome);

    // Fire-and-forget 3D one-shot at world position. `vel` enables doppler if non-zero.
    void playSfx3D(SoundId id, glm::dvec3 pos, glm::vec3 vel = glm::vec3(0.0f), float volume = 1.0f);

    // Non-positional SFX (UI, self-sounds you want fully present in both ears).
    void playSfx2D(SoundId id, float volume = 1.0f);

    // Volume sliders, range [0,1]. Master scales everything; the others scale a single bus.
    void setMasterVolume(float v);
    void setMusicVolume(float v);
    void setSfxVolume(float v);

    float getMasterVolume() const { return masterVolume; }
    float getMusicVolume() const { return musicVolume; }
    float getSfxVolume() const { return sfxVolume; }

    // Per-SoundId multiplier in [0,2], default 1.0. Applied on top of master/sfx volume
    // and the call-site `volume` argument; tuned at runtime from the audio settings panel.
    float getSfxScale(SoundId id) const;
    void  setSfxScale(SoundId id, float v);
    static const char* sfxName(SoundId id);

    // Public material lookups so packet handlers can pick the right break/place sound
    // without exposing the rest of the manager's internals.
    static SoundId footstepFor(BlockType b);
    static SoundId breakFor(BlockType b);
    static SoundId placeFor(BlockType b);
    // Per-entity-type hurt sound (Player_Hurt / Zombie_Hurt / Creeper_Hurt). Used by both
    // the local-player health-decrement edge and the remote-entity 0x10 hurt-flag handler.
    static SoundId hurtSoundFor(LivingEntityType t);

    // True when `worldPos` falls inside an active creeper-explosion suppression window. Block
    // break/place packets that resolve here should be silenced so the only thing the player
    // hears is the explosion itself (otherwise ~30 stone-break voices stack into a "weird noise").
    bool blockSfxSuppressed(glm::dvec3 worldPos) const;

    // Fire explosion sfx + push the block-sfx suppression window. Called from the death
    // packet handler so audio doesn't lag the visual blast. `key` is the LivingEntity*; it
    // marks deathSoundFired on the mobStates entry so the later death-edge sweep won't
    // double-fire (the client's death animation can outlive the suppression window's TTL).
    void onCreeperExploded(const void* key, glm::dvec3 epos, glm::dvec3 listenerPos);

private:
    // ---- engine + routing -------------------------------------------------
    SoLoud::Soloud engine{};
    SoLoud::Bus    musicBus{};
    SoLoud::Bus    sfxBus{};
    SoLoud::handle musicBusHandle = 0;
    SoLoud::handle sfxBusHandle   = 0;

    // ---- caches -----------------------------------------------------------
    // Each SFX entry can own multiple WAVs to randomize between (footstep variations).
    std::unordered_map<SoundId, std::vector<std::unique_ptr<SoLoud::Wav>>> sfxCache{};
    std::unordered_map<BiomeType, std::unique_ptr<SoLoud::WavStream>> musicCache{};

    // ---- music state ------------------------------------------------------
    BiomeType      currentBiome      = BiomeType::PLAINS;
    bool           biomeInitialized  = false;
    SoLoud::handle activeMusicHandle = 0;
    SoLoud::handle fadingOutHandle   = 0;
    bool           musicPaused       = false;

    // ---- volumes (cached so sliders survive restart of music tracks) ------
    float masterVolume = 1.0f;
    float musicVolume  = 0.0f;
    float sfxVolume    = 1.0f;

    // Per-SoundId multiplier; filled with 1.0 in init().
    std::array<float, static_cast<size_t>(SoundId::_Count)> sfxScale{};

    // ---- footstep state ---------------------------------------------------
    float footstepDistance = 0.0f;   // accumulated horizontal distance walked since last step
    bool  prevOnGround     = true;   // for jump rising-edge AND fall-impact rising-edge
    bool  prevUnderwater   = false;  // for splash on water entry
    float prevFallDist     = 0.0f;   // last frame's accumulatedFallDistance, latched for landing edge
    float lastPlayerHealth = 20.0f;  // local-player health from previous frame, for the hurt edge

    // ---- listener-jump detection ------------------------------------------
    // Player respawn / debug teleport moves the listener in a single frame. Active 3D voices
    // (especially long ones like the creeper fuse) keep playing at their absolute world coords,
    // and SoLoud's spatializer recomputes their pan/volume against the new listener — the
    // result is an audible click/glitch on the next buffer. Tracking the listener position
    // lets us detect that jump and stop the long-running 3D voices we own.
    glm::dvec3 prevListenerPos{0.0};
    bool       hasPrevListenerPos = false;

    // ---- creeper-explosion block-sfx suppression --------------------------
    // Each entry suppresses MODIFIED_BLOCK_DATA sfx within `radius` of `pos` for `ttl` seconds.
    // Pushed from updateMobAudio() when a primed creeper vanishes; ticked down in update().
    struct ExplosionWindow {
        glm::dvec3 pos;
        float      radius;
        float      ttl;
    };
    std::vector<ExplosionWindow> explosionWindows{};

    // ---- mob audio state --------------------------------------------------
    struct MobAudioState {
        float            idleCooldown = 0.0f;
        float            footstepDist = 0.0f;
        float            lastHealth   = 0.0f;     // reserved for a future hurt edge
        bool             initialized  = false;
        bool             prevPrimed   = false;    // creeper fuse rising-edge detect
        LivingEntityType type         = PLAYER;   // survives the entity for the death-edge sweep
        glm::dvec3       lastPos{};               // last seen position, used for 3D death/explode sfx
        // Position of the most recently consumed snapshot. We compute footstep deltas from THIS,
        // not from snapshots[size-2], because Renderer.cpp wipes & reseeds the snapshots vector
        // whenever there's a >100ms server-side gap — losing the first move-after-idle every
        // stop/go cycle (chase→attack→chase). Caching it here survives the reseed.
        glm::dvec3       lastConsumedPos{};
        bool             hasLastConsumedPos = false;
        // Time of the latest *consumed* snapshot, used to skip duplicate frames.
        double           lastSnapTime = -1.0;
        // Water audio (PLAYER only). prevUnderwater latches the underwater state so we can
        // detect the rising edge into water; prevFallDist mirrors the local-player trick of
        // reading *last* frame's accumulatedFallDistance because the server resets it to 0 on
        // water contact. swimDistance accumulates horizontal travel under water and fires a
        // Player_Swim every kSwimStrokeM blocks.
        bool             prevUnderwater = false;
        float            prevFallDist   = 0.0f;
        float            swimDistance   = 0.0f;
        // Latched from LivingEntity::diedByExplosion the frame before the entity vanishes; the
        // death sweep reads it to choose Creeper_Death vs Creeper_Explode (was guessed from
        // prevPrimed before, which mis-fired when a primed creeper got killed mid-fuse).
        bool             diedByExplosion = false;
        // SoLoud voice for the active creeper fuse one-shot. Tracked so we can stop it on
        // unprime / death / explode — otherwise the sample keeps playing at the creeper's old
        // position and glitches when the listener jumps (player death + respawn far away).
        SoLoud::handle   fuseHandle   = 0;
        // Set when the packet handler fires a death sfx; sweep skips it to avoid double-firing.
        bool             deathSoundFired = false;
    };
    // Keyed by raw LivingEntity*; entries cleaned up after the entity disappears (see updateMobAudio).
    std::unordered_map<const void*, MobAudioState> mobStates{};

    // ---- helpers ----------------------------------------------------------
    void loadAllAssets();
    bool loadSfx(SoundId id, const std::vector<std::string>& paths);
    bool loadMusic(BiomeType biome, const std::string& path);

    // Pick one wav at random from a SoundId's variation pool. nullptr if not loaded.
    SoLoud::Wav* pickVariation(SoundId id);

    void crossfadeTo(BiomeType b, float seconds = 3.0f);
    void updateFootsteps(float dt, Camera& cam, Renderer& world);
    void updateMobAudio(float dt, Camera& cam, Renderer& world);
    void applyVolumes();

    // Like playSfx3D but returns the SoLoud voice handle so the caller can stop it later.
    // 0 on missing asset.
    SoLoud::handle playSfx3DTracked(SoundId id, glm::dvec3 pos, glm::vec3 vel = glm::vec3(0.0f),
                                    float volume = 1.0f);
};
