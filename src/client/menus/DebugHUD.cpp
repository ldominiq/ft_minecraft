
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

void DebugHUD::onRender()
{
    const float pad = 8.0f * menuScale;
    const float lineH = 18.0f * menuScale * textScale * 3;
    const float bgPadX = 2.0f * menuScale;   // horizontal inset inside each bg
    const glm::vec4 bgColor{ 0.0f, 0.0f, 0.0f, 0.0f };
    const glm::vec3 white{ 1.0f, 1.0f, 1.0f };

    const float textX = pad;
    float textY = static_cast<float>(fullscreenHeight) - pad - 4.0f * menuScale * textScale * 5;

    char buf[64];

    auto drawLine = [&](const char* text) {
        float w = static_cast<float>(textRenderer.getPixelSizeOfString(text));
        drawSimpleQuad(textX - bgPadX, textY - 3.0f * menuScale, w + bgPadX * 2.0f, lineH - 2.0f * menuScale, bgColor);
        textRenderer.renderText(text, textX, textY, white);
        textY -= lineH;
    };

    auto formatInt = [](size_t n) {
        std::string s = std::to_string(n);
        int i = static_cast<int>(s.size()) - 3;
        while (i > 0) { s.insert(i, ","); i -= 3; }
        return s;
    };

    snprintf(buf, sizeof(buf), "FPS: %.0f", m_stats.fps);
    drawLine(buf);

    snprintf(buf, sizeof(buf), "Render time: %.2f ms", m_stats.cpuFrameMs);
    drawLine(buf);

    snprintf(buf, sizeof(buf), "Player Position: X=%.0f Y=%.0f Z=%.0f", m_stats.playerPos.x, m_stats.playerPos.y, m_stats.playerPos.z);
    drawLine(buf);

    snprintf(buf, sizeof(buf), "Triangles: %s", formatInt(m_stats.triangles).c_str());
    drawLine(buf);

    snprintf(buf, sizeof(buf), "Cubes: ~%s", formatInt(m_stats.cubes).c_str());
    drawLine(buf);

    snprintf(buf, sizeof(buf), "Chunks: %zu / %zu", m_stats.visibleChunks, m_stats.totalChunks);
    drawLine(buf);

    snprintf(buf, sizeof(buf), "Terrain Draw Calls: %zu", m_stats.terrainDrawCalls);
    drawLine(buf);

    snprintf(buf, sizeof(buf), "Ping: %.0f ms", m_stats.pingMs);
    drawLine(buf);

}
