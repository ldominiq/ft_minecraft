#ifndef SKIN_BOX_HPP
#define SKIN_BOX_HPP

#include <array>
#include <glm/glm.hpp>

// Minecraft-style box unwrap. Given the top-left (u,v) of the "cross" on a
// (texW x texH) skin PNG and box pixel dims (w,h,d), returns 6 face UV rects
// in normalized skin-auth space (V=0 at top of PNG). Indexed by our face
// convention: 0=+X(front) 1=-X(back) 2=+Y(top) 3=-Y(bottom) 4=-Z(right) 5=+Z(left).
//
// Cross layout on the skin:
//         [top w×d] [bot w×d]
//   [R d×h] [F w×h] [L d×h] [B w×h]
std::array<glm::vec4, 6> boxUVs(int u, int v, int w, int h, int d,
                                int texW, int texH);

// Variant: select a vertical sub-slice of each side face's UV rect, so a limb
// that's modeled as two stacked cubes (upper + lower) can sample the top half
// then the bottom half of the same skin region. vTop and vBottom are in [0,1]
// along the limb's h dimension (0 = top of the skin region, 1 = bottom).
// Top and bottom faces are unaffected (they always map to the full top/bottom).
std::array<glm::vec4, 6> boxUVsSlice(int u, int v, int w, int h, int d,
                                     int texW, int texH,
                                     float vTop, float vBottom);

#endif
