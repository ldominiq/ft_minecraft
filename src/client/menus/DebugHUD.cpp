
#include "DebugHUD.hpp"

DebugHUD::DebugHUD(float width, float height)
    : Menu(width, height)
{
    build();
}

void DebugHUD::update(const DebugStats& stats)
{
    m_stats = stats;
}

void DebugHUD::build()
{
    textRenderer.setProjection(fullscreenWidth, fullscreenHeight);
}

void DebugHUD::resize(float width, float height)
{
    // Keep design == screen so menuScale stays 1.0 and we use raw pixel coords.
    DESIGN_WIDTH  = static_cast<int>(width);
    DESIGN_HEIGHT = static_cast<int>(height);
    Menu::resize(width, height);
}

void DebugHUD::onRender()
{
    constexpr float pad     = 8.0f;
    constexpr float lineH   = 18.0f;
    constexpr int   nLines  = 4;
    constexpr float panelW  = 230.0f;
    constexpr float panelH  = nLines * lineH + pad * 2.0f;

    const float panelX = pad;
    const float panelY = static_cast<float>(fullscreenHeight) - panelH - pad;

    // Semi-transparent dark background
    drawSimpleQuad(panelX, panelY, panelW, panelH, {0.0f, 0.0f, 0.0f, 0.20f});

    // Text baseline starts near the top of the panel, stepping downward.
    // In bottom-left origin coords, "downward" means decreasing y.
    const float textX  = panelX + pad;
    float       textY  = panelY + panelH - pad - 4.0f; // -4 for cap height
    const glm::vec3 white{1.0f, 1.0f, 1.0f};

    char buf[64];

    snprintf(buf, sizeof(buf), "FPS: %.0f", m_stats.fps);
    textRenderer.renderText(buf, textX, textY, white);
    textY -= lineH;

    snprintf(buf, sizeof(buf), "Triangles: %zu", m_stats.triangles);
    textRenderer.renderText(buf, textX, textY, white);
    textY -= lineH;

    snprintf(buf, sizeof(buf), "Cubes: ~%zu", m_stats.cubes);
    textRenderer.renderText(buf, textX, textY, white);
    textY -= lineH;

    snprintf(buf, sizeof(buf), "Chunks: %zu / %zu", m_stats.visibleChunks, m_stats.totalChunks);
    textRenderer.renderText(buf, textX, textY, white);
}
