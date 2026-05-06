#pragma once

#include <soloud.h>
#include <soloud_speech.h>
#include <soloud_thread.h>
#include <soloud_wav.h>
#include <soloud_wavstream.h>
#include <soloud_bus.h>

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
    Player_Swim,
    Player_AttackSwing,

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

    // Public material lookups so packet handlers can pick the right break/place sound
    // without exposing the rest of the manager's internals.
    static SoundId footstepFor(BlockType b);
    static SoundId breakFor(BlockType b);
    static SoundId placeFor(BlockType b);

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

    // ---- footstep state ---------------------------------------------------
    float footstepDistance = 0.0f;   // accumulated horizontal distance walked since last step
    bool  prevOnGround     = true;   // for jump rising-edge
    bool  prevUnderwater   = false;  // for splash on water entry

    // ---- mob audio state --------------------------------------------------
    struct MobAudioState {
        float            idleCooldown = 0.0f;
        float            footstepDist = 0.0f;
        float            lastHealth   = 0.0f;     // reserved for a future hurt edge
        bool             initialized  = false;
        bool             prevPrimed   = false;    // creeper fuse rising-edge detect
        LivingEntityType type         = PLAYER;   // survives the entity for the death-edge sweep
        glm::dvec3       lastPos{};               // last seen position, used for 3D death/explode sfx
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
};
