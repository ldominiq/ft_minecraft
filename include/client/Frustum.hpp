#ifndef FRUSTUM_HPP
#define FRUSTUM_HPP

#include <glm/glm.hpp>
#include <array>

/**
 * Camera frustum extracted from a View-Projection matrix.
 * Used for AABB frustum culling to skip off-screen chunks.
 *
 * Plane normals point INWARD (a point is visible when dot(plane,point) >= 0).
 */
class Frustum {
public:
    /// Extract the 6 frustum planes from a combined View*Projection matrix.
    /// Uses the Gribb-Hartmann method (row extraction).
    void update(const glm::mat4& vp) {
        // Left:   row3 + row0
        planes[0] = glm::vec4(vp[0][3] + vp[0][0],
                              vp[1][3] + vp[1][0],
                              vp[2][3] + vp[2][0],
                              vp[3][3] + vp[3][0]);
        // Right:  row3 - row0
        planes[1] = glm::vec4(vp[0][3] - vp[0][0],
                              vp[1][3] - vp[1][0],
                              vp[2][3] - vp[2][0],
                              vp[3][3] - vp[3][0]);
        // Bottom: row3 + row1
        planes[2] = glm::vec4(vp[0][3] + vp[0][1],
                              vp[1][3] + vp[1][1],
                              vp[2][3] + vp[2][1],
                              vp[3][3] + vp[3][1]);
        // Top:    row3 - row1
        planes[3] = glm::vec4(vp[0][3] - vp[0][1],
                              vp[1][3] - vp[1][1],
                              vp[2][3] - vp[2][1],
                              vp[3][3] - vp[3][1]);
        // Near:   row3 + row2
        planes[4] = glm::vec4(vp[0][3] + vp[0][2],
                              vp[1][3] + vp[1][2],
                              vp[2][3] + vp[2][2],
                              vp[3][3] + vp[3][2]);
        // Far:    row3 - row2
        planes[5] = glm::vec4(vp[0][3] - vp[0][2],
                              vp[1][3] - vp[1][2],
                              vp[2][3] - vp[2][2],
                              vp[3][3] - vp[3][2]);

        // Normalize all planes
        for (auto& p : planes) {
            float len = glm::length(glm::vec3(p));
            if (len > 0.0f) p /= len;
        }
    }

    /// Test an AABB against the frustum.
    /// Returns true if the box is at least partially inside (should be rendered).
    bool isBoxVisible(const glm::vec3& minP, const glm::vec3& maxP) const {
        for (const auto& plane : planes) {
            // Find the AABB corner most in the direction of the plane normal (p-vertex)
            glm::vec3 pVertex(
                (plane.x >= 0.0f) ? maxP.x : minP.x,
                (plane.y >= 0.0f) ? maxP.y : minP.y,
                (plane.z >= 0.0f) ? maxP.z : minP.z
            );

            // If the p-vertex is behind the plane, the entire AABB is outside
            if (glm::dot(glm::vec3(plane), pVertex) + plane.w < 0.0f)
                return false;
        }
        return true;
    }

    /// Access the 6 frustum planes (Left, Right, Bottom, Top, Near, Far).
    const std::array<glm::vec4, 6>& getPlanes() const { return planes; }

private:
    std::array<glm::vec4, 6> planes; // Left, Right, Bottom, Top, Near, Far
};

#endif // FRUSTUM_HPP
