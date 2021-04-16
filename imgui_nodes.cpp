#include "imgui_nodes.hpp"

#include <imgui.h>

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif // IMGUI_DEFINE_MATH_OPERATORS

#include <imgui_internal.h>

#include <CubicSpline.h>

#include <math.h>
#include <vector>
#include <algorithm>
#include <array>
#include <limits>

#ifdef IMGUI_NODES_DEBUG
#include <sstream>

std::stringstream debug;
#endif

namespace nodes {

namespace {

// initializing na ImRect with these bounds allows setting min/max by calling ImRect::add with min
// and max. This used to be the default behavior of the ImRect default constructor until ImGui 1.75.
const ImRect defaultBoundsRect = ImRect(
    ImVec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::max()),
    ImVec2(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()));

// from imgui.cpp
static bool IsKeyPressedMap(ImGuiKey key, bool repeat = true)
{
    const int key_index = GImGui->IO.KeyMap[key];
    return (key_index >= 0) ? ImGui::IsKeyPressed(key_index, repeat) : false;
}

inline ImVec2 operator+(const ImVec2& lhs, const float rhs) { return ImVec2(lhs.x + rhs, lhs.y + rhs); }
inline ImVec2 operator-(const ImVec2& lhs, const float rhs) { return ImVec2(lhs.x - rhs, lhs.y - rhs); }

inline bool operator==(const ImVec2& lhs, const ImVec2& rhs) { return lhs.x == rhs.x && lhs.y == rhs.y; }
inline bool operator!=(const ImVec2& lhs, const ImVec2& rhs) { return !(lhs == rhs); }

inline ImVec2 operator-(const ImVec2& lhs) { return ImVec2(-lhs.x, -lhs.y); }

inline float ImVec2Dot(const ImVec2& S1, const ImVec2& S2) { return (S1.x*S2.x + S1.y*S2.y); }

float distanceToBezierSquared(const ImVec2& point, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4)
{
    static CubicSplineTest::ClosestPointSolver staticSolver;

    std::array<CubicSplineTest::WorldSpace, 4> cp = { {
        { p1.x, p1.y },
        { p2.x, p2.y },
        { p3.x, p3.y },
        { p4.x, p4.y }
    } };

    CubicSplineTest::CubicBezierPath path(cp.data(), (int)cp.size());
    CubicSplineTest::WorldSpace closestws = path.ClosestPointToPath({ point.x, point.y }, &staticSolver);
    ImVec2 closest(closestws.x, closestws.y);
    return ImLengthSqr(point - closest);
}

bool closeToBezier(const ImVec2& point, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, float maxDist)
{
    ImRect aabb = defaultBoundsRect;
    aabb.Add(p1); aabb.Add(p2); aabb.Add(p3); aabb.Add(p4);
    aabb.Expand(maxDist);
    if (aabb.Contains(point)) {
        float d = distanceToBezierSquared(point, p1, p2, p3, p4);
        return d < maxDist * maxDist;
    }
    return false;
}

template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

/*based on http://mysite.verizon.net/res148h4j/javascript/script_exact_cubic.html#the%20source%20code */
std::array<float, 3> cubicRoots(ImVec4 const &P)
{
    float A = P.y / P.x;
    float B = P.z / P.x;
    float C = P.w / P.x;

    float Q = (3.f * B - pow(A, 2.f)) / 9.f;
    float R = (9.f * A*B - 27.f * C - 2.f * pow(A, 3.f)) / 54.f;
    float D = pow(Q, 3.f) + pow(R, 2.f);  // polynomial discriminant

    std::array<float, 3> t;

    if (D >= 0) // complex or duplicate roots
    {
        float S = sgn(R + sqrt(D))*pow(abs(R + sqrt(D)), (1.f / 3.f));
        float T = sgn(R - sqrt(D))*pow(abs(R - sqrt(D)), (1.f / 3.f));

        t[0] = -A / 3.f + (S + T);                  // real root
        t[1] = -A / 3.f - (S + T) / 2.f;            // real part of complex root
        t[2] = -A / 3.f - (S + T) / 2.f;            // real part of complex root
        float Im = abs(sqrt(3.f)*(S - T) / 2.f);    // complex part of root pair

                                                    /*discard complex roots*/
        if (Im != 0.f)
        {
            t[1] = -1.f;
            t[2] = -1.f;
        }

    }
    else // distinct real roots
    {
        float th = acos(R / sqrt(-pow(Q, 3.f)));

        t[0] = 2.f * sqrt(-Q)*cos(th / 3.f) - A / 3.f;
        t[1] = 2.f * sqrt(-Q)*cos((th + 2.f * IM_PI) / 3.f) - A / 3.f;
        t[2] = 2.f * sqrt(-Q)*cos((th + 4.f * IM_PI) / 3.f) - A / 3.f;
    }

    return t;
}

// From https://www.particleincell.com/2013/cubic-line-intersection/
bool IntersectBezierAndLine(ImVec2 const &a, ImVec2 const &b, ImVec2 const &p0, ImVec2 const &p1, ImVec2 const &p2, ImVec2 const &p3) {

    auto bezierCoeffs = [](float p0, float p1, float p2, float p3) {
        std::array<float, 4> result;
        result[0] = -p0 + 3.f * p1 + -3.f * p2 + p3;
        result[1] = 3.f * p0 - 6.f * p1 + 3.f * p2;
        result[2] = -3.f * p0 + 3.f * p1;
        result[3] = p0;
        return result;
    };

    float A = b.y - a.y;    //A=y2-y1
    float B = a.x - b.x;    //B=x1-x2
    float C = a.x * (a.y - b.y) +
        a.y * (b.x - a.x);  //C=x1*(y1-y2)+y1*(x2-x1)

    if (A == 0.f && B == 0.f && C == 0.f) {
        return false;
    }

    std::array<float, 4> bx = bezierCoeffs(p0.x, p1.x, p2.x, p3.x);
    std::array<float, 4> by = bezierCoeffs(p0.y, p1.y, p2.y, p3.y);

    ImVec4 P;
    P.x = A*bx[0] + B*by[0];      /*t^3*/
    P.y = A*bx[1] + B*by[1];      /*t^2*/
    P.z = A*bx[2] + B*by[2];      /*t*/
    P.w = A*bx[3] + B*by[3] + C;  /*1*/

    std::array<float, 3> r = cubicRoots(P);

    /*verify the roots are in bounds of the linear segment*/
    for (int i = 0; i < 3; ++i)
    {
        float t = r[i];
        if (t < 0.f || t > 1.f) {
            continue;
        }

        ImVec2 X;

        X.x = bx[0] * t*t*t + bx[1] * t*t + bx[2] * t + bx[3];
        X.y = by[0] * t*t*t + by[1] * t*t + by[2] * t + by[3];

        /*above is intersection point assuming infinitely long line segment,
        make sure we are also in bounds of the line*/
        float s;
        if ((b.x - a.x) != 0)           /*if not vertical line*/
            s = (X.x - a.x) / (b.x - a.x);
        else
            s = (X.y - a.y) / (b.y - a.y);

        /*in bounds?*/
        if (s >= 0.f && s <= 1.f)
        {
            // X is intersection point
            return true;
        }
    }
    return false;
}

bool isInSelectedRect(NodeArea &area, ImRect rect) {
    ImRect selectionRect = defaultBoundsRect;
    selectionRect.Add(area.state.dragStart);
    selectionRect.Add(area.state.dragEnd);
    if (ImGui::GetIO().KeyAlt) {
        return selectionRect.Contains(rect);
    } else {
        return selectionRect.Overlaps(rect);
    }
}

bool handleNodeDragSelection(NodeArea &area, int nodeId, ImRect rect) {
    if (isInSelectedRect(area, rect)) {
        if (area.state.mode == NodeArea::Mode::SelectionCaptureAdd) {
            area.state.selectedNodes.addToSelection(nodeId);
        } else if (area.state.mode == NodeArea::Mode::SelectionCaptureRemove) {
            area.state.selectedNodes.removeFromSelection(nodeId);
        }
        return area.state.mode == NodeArea::Mode::Selecting;
    }
    return false;
}

bool handleEdgeDragSelection(NodeArea &area, int edgeId, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4) {
    if (area.state.mode != NodeArea::Mode::None) {
        ImRect selectionRect = defaultBoundsRect;
        selectionRect.Add(area.state.dragStart);
        selectionRect.Add(area.state.dragEnd);

        bool contained = selectionRect.Contains(p1) && selectionRect.Contains(p4);
        if (!contained && !ImGui::GetIO().KeyAlt) {
            ImVec2 q0 = selectionRect.Min;
            ImVec2 q1 = ImVec2(selectionRect.Max.x, selectionRect.Min.y);
            ImVec2 q2 = ImVec2(selectionRect.Min.x, selectionRect.Max.y);
            ImVec2 q3 = selectionRect.Max;

            contained =
                IntersectBezierAndLine(q0, q1, p1, p2, p3, p4) ||
                IntersectBezierAndLine(q0, q2, p1, p2, p3, p4) ||
                IntersectBezierAndLine(q3, q1, p1, p2, p3, p4) ||
                IntersectBezierAndLine(q3, q2, p1, p2, p3, p4);
        }

        if (contained) {
            if (area.state.mode == NodeArea::Mode::SelectionCaptureAdd) {
                area.state.selectedEdges.addToSelection(edgeId);
            } else if (area.state.mode == NodeArea::Mode::SelectionCaptureRemove) {
                area.state.selectedEdges.removeFromSelection(edgeId);
            }
            return area.state.mode == NodeArea::Mode::Selecting;
        }
    }
    return false;
}

ImGuiContext* setupInnerContext(ImGuiContext* outerContext) {
    ImGuiContext *innerContext = ImGui::CreateContext(outerContext->IO.Fonts);

    innerContext->IO.BackendFlags = outerContext->IO.BackendFlags;
    innerContext->IO.BackendPlatformName = outerContext->IO.BackendPlatformName;

    memcpy(innerContext->IO.KeyMap, outerContext->IO.KeyMap, ImGuiKey_COUNT * sizeof(int));

    innerContext->IO.SetClipboardTextFn = outerContext->IO.SetClipboardTextFn;
    innerContext->IO.GetClipboardTextFn = outerContext->IO.GetClipboardTextFn;
    innerContext->IO.ClipboardUserData = outerContext->IO.ClipboardUserData;
    innerContext->IO.ImeSetInputScreenPosFn = outerContext->IO.ImeSetInputScreenPosFn;
    innerContext->IO.ImeWindowHandle = outerContext->IO.ImeWindowHandle;
    innerContext->Style = outerContext->Style;
    return innerContext;
}

void innerContextNewFrame(ImGuiContext const* outerContext, ImGuiContext* innerContext, float scale, ImGuiIO& outerIo, ImVec2 outerWindowSize, ImVec2 outerWindowPos, bool active, bool hovered) {
    ImGuiIO& innerIo = innerContext->IO;

    innerIo.DisplaySize = outerWindowSize;
    innerIo.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

    innerIo.DeltaTime = outerIo.DeltaTime;
    innerIo.FontGlobalScale = outerIo.FontGlobalScale;


    if (active || hovered) {
        innerIo.MousePos = (outerIo.MousePos - outerWindowPos) / scale;
    }

    for (int i = 0; i < 3; i++) {
        if (active || !active && innerIo.MouseDown[i]) {
            innerIo.MouseDown[i] = outerIo.MouseDown[i];
        }
    }

    innerIo.MouseDrawCursor = outerIo.MouseDrawCursor;

    if (active) {
        innerIo.MouseWheel = outerIo.MouseWheel;
        innerIo.MouseWheelH = outerIo.MouseWheelH;
        memcpy(innerIo.KeysDown, outerIo.KeysDown, sizeof(innerIo.KeysDown));
        memcpy(innerIo.NavInputs, outerIo.NavInputs, sizeof(innerIo.NavInputs));
        innerIo.InputQueueCharacters = outerIo.InputQueueCharacters;
    } else {
        innerIo.MouseWheel = 0.f;
        innerIo.MouseWheelH = 0.f;
        memset(innerIo.KeysDown, 0, sizeof(innerIo.KeysDown));
        memset(innerIo.NavInputs, 0, sizeof(innerIo.NavInputs));
        innerIo.InputQueueCharacters.clear();
    }

    innerContext->IO.KeyCtrl = outerContext->IO.KeyCtrl;
    innerContext->IO.KeyShift = outerContext->IO.KeyShift;
    innerContext->IO.KeyAlt = outerContext->IO.KeyAlt;
    innerContext->IO.KeySuper = outerContext->IO.KeySuper;
}

void copyTransformDrawList(ImDrawList *targetDrawList, ImDrawList const*sourceDrawList, ImVec2 scale = ImVec2(1.f, 1.f), ImVec2 translate = {}) {
    targetDrawList->Flags = sourceDrawList->Flags;
    ImRect targetClip(targetDrawList->_ClipRectStack.back());

    int indexBase = targetDrawList->_VtxCurrentIdx;

    // Currently not supporting very large graphs with more than 64k indices. If this check
    // fails for your use case, you can try to configure ImDrawIdx to be uint in imconfig.h
    IM_ASSERT(sourceDrawList->VtxBuffer.size() < std::numeric_limits<ImDrawIdx>::max());

    targetDrawList->PrimReserve(0, sourceDrawList->VtxBuffer.size());

    // PrimReserve(0, numVertices) will usually increment _VtxCurrentIdx by numVertices. In case
    // _VtxCurrentIdx exceeds 64k indices, it may also insert a new DrawCmd with a VtxOffset and
    // reset _VtxCurrentIdx on the way.
    if (targetDrawList->_VtxCurrentIdx == 0) {
        indexBase = 0;
    }

    for (int vtx = 0; vtx < sourceDrawList->VtxBuffer.size(); ++vtx) {
        auto const& vertex = sourceDrawList->VtxBuffer[vtx];
        targetDrawList->PrimWriteVtx(vertex.pos * scale + translate, vertex.uv, vertex.col);
    }

    for (int dc = 0; dc < sourceDrawList->CmdBuffer.size(); ++dc) {
        ImDrawCmd const& sourceDrawCmd = sourceDrawList->CmdBuffer[dc];

        IM_ASSERT(sourceDrawCmd.VtxOffset == 0);
        targetDrawList->AddDrawCmd();

        ImDrawCmd& cmdBuffer = targetDrawList->CmdBuffer.back();
        cmdBuffer.TextureId = sourceDrawCmd.TextureId;
        cmdBuffer.ElemCount = 0;
        cmdBuffer.IdxOffset = targetDrawList->IdxBuffer.Size;

        targetDrawList->PrimReserve(sourceDrawCmd.ElemCount, 0);

        int sourceIdx = sourceDrawCmd.IdxOffset;
        for (unsigned int idx = 0; idx < sourceDrawCmd.ElemCount; ++idx, ++sourceIdx) {
            targetDrawList->PrimWriteIdx((ImDrawIdx)(sourceDrawList->IdxBuffer[sourceIdx] + indexBase));
        }

        ImVec2 clipRectMin(sourceDrawCmd.ClipRect.x, sourceDrawCmd.ClipRect.y);
        ImVec2 clipRectMax(sourceDrawCmd.ClipRect.z, sourceDrawCmd.ClipRect.w);

        ImRect clipRect(clipRectMin * scale + translate, clipRectMax * scale + translate);
        clipRect.ClipWith(targetClip);

        if (clipRect.Max.x <= clipRect.Min.x || clipRect.Max.y <= clipRect.Min.y) {
            cmdBuffer.UserCallback = (ImDrawCallback)([](const ImDrawList*, const ImDrawCmd*) {
                // This would be fully clipped, so skip it to avoid errors.
                // Adding an empty user callback for rendering is the easiest way to do so ...
                // ... Still it would be better to skip the relevant data in the vertex/index buffers
                // and omit the draw cmd (TODO).
            });
        }

        cmdBuffer.ClipRect = ImVec4(clipRect.Min.x, clipRect.Min.y, clipRect.Max.x, clipRect.Max.y);
    }
}

// Copies all the draw call, vertex and index data from the inner imgui context to our current draw list:
// * Translate and scale the vertices and clip rects accordingly
// * Rebases the indices to fit into the outer index buffer
// * Reclips the clip rects to our outer clip rect
void copyTransformDrawCmds(ImDrawData* sourceDrawData, float scale, ImVec2 translate) {
    ImDrawList *targetDrawList = ImGui::GetWindowDrawList();

    for (int i = 0; i < sourceDrawData->CmdListsCount; ++i) {
        ImDrawList const *sourceDrawList = sourceDrawData->CmdLists[i];
        copyTransformDrawList(targetDrawList, sourceDrawList, ImVec2(scale, scale), translate);

        targetDrawList->_FringeScale = sourceDrawList->_FringeScale / scale;
    }
    // make sure no one messes with our copied draw calls
    targetDrawList->AddDrawCmd();
}

bool WasItemActive()
{
    ImGuiContext& g = *GImGui;
    if (g.ActiveIdPreviousFrame)
    {
        ImGuiWindow* window = g.CurrentWindow;
        return g.ActiveIdPreviousFrame == window->DC.LastItemId;
    }
    return false;
}

void paintGrid(Style const &style) {
    if (style.gridSpacing <= 0.f) {
        return;
    }

    // Uses the current clip rects extent for painting. This allows for large node areas,
    // without the performance loss of emitting grid lines that end up culled.
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec4 clipRect = draw_list->_ClipRectStack[draw_list->_ClipRectStack.size() - 1];

    ImVec2 windowPos = ImGui::GetWindowPos();

    int repx = (int)(abs(windowPos.x) / style.gridSpacing);
    for (float x = clipRect.x + fmodf(windowPos.x, style.gridSpacing); x < clipRect.z; (x += style.gridSpacing), ++repx) {
        for (auto const &grid : style.grid) {
            if (repx % grid.spacingMultiplier == 0) {
                draw_list->AddLine(ImVec2(x, clipRect.y - 1.f), ImVec2(x, clipRect.w + 1.f), grid.lineColor, grid.thickness);
                break;
            }
        }
    }

    int repy = (int)(abs(windowPos.y) / style.gridSpacing);
    for (float y = clipRect.y + fmodf(windowPos.y, style.gridSpacing); y < clipRect.w; (y += style.gridSpacing), ++repy) {
        for (auto const &grid : style.grid) {
            if (repy % grid.spacingMultiplier == 0) {
                draw_list->AddLine(ImVec2(clipRect.x - 1.f, y), ImVec2(clipRect.z + 1.f, y), grid.lineColor, grid.thickness);
                break;
            }
        }
    }
}

void invisibleButtonNoResize(const char *name, ImVec2 size)
{
    ImGuiWindow* wnd = ImGui::GetCurrentWindow();
    ImVec2 maxPos = wnd->DC.CursorMaxPos;
    ImGui::InvisibleButton(name, size);
    wnd->DC.CursorMaxPos = maxPos;
}

} // anonymous namespace

void NodeArea::Selection::clearSelection() {
    selectedItems.clear();
    selectedCount = 0;
}

void NodeArea::Selection::addToSelection(int id) {
    IM_ASSERT(id >= 0);
    if (id >= (int)selectedItems.size()) {
        selectedItems.resize(id + 1);
    }
    if (!selectedItems[id]) {
        selectedItems[id] = true;
        ++selectedCount;
    }
}

void NodeArea::Selection::removeFromSelection(int id) {
    IM_ASSERT(id >= 0);
    if (id >= (int)selectedItems.size()) {
        return;
    }
    if (selectedItems[id]) {
        selectedItems[id] = false;
        --selectedCount;
    }
}

void NodeArea::Selection::toggleSelection(int id) {
    if (isSelected(id)) {
        removeFromSelection(id);
    } else {
        addToSelection(id);
    }
}

bool NodeArea::Selection::isSelected(int id) const {
    if (id >= (int)selectedItems.size()) {
        return false;
    }
    return selectedItems[id];
}

void NodeArea::BeginNodeArea(std::function<void(UserAction)> actionCallback, NodeAreaFlags flags) {
    bool setWindowPos = false;

    if (state.flags & NodeAreaFlags_ZoomToFit) {
        if (state.lowerBound.x != std::numeric_limits<float>::max()) {
            ImVec2 extent = state.upperBound - state.lowerBound;
            ImVec2 factor = ImGui::GetWindowSize() / extent;
            state.zoom = std::max(std::min(std::min(factor.x, factor.y) * 0.95f, 1.f), 0.15f);
            state.innerWndPos = -state.lowerBound + (ImGui::GetWindowSize() / 2.f) / state.zoom - extent / 2.f;
            setWindowPos = true;
        } else {
            state.zoom = 1.f;
            state.innerWndPos = -state.nodeAreaSize / 2.f + ImGui::GetWindowSize() / 2.f;
            setWindowPos = true;
        }
    }

    state.flags = flags;
    state.outerContext = ImGui::GetCurrentContext();
    if (!state.initialized) {
        state.zoom = 1.f;
        state.innerWndPos = -state.nodeAreaSize / 2.f + ImGui::GetWindowSize() / 2.f;
        state.mode = Mode::None;
        state.scrolling = false;
        state.anyItemActive = false;
        state.innerContext = setupInnerContext(state.outerContext);
        state.initialized = true;
    }
    if (state.flags & NodeAreaFlags_UpdateStyle) {
        state.innerContext->Style = state.outerContext->Style;
    }

#ifdef IMGUI_NODES_DEBUG
    debug.str("");
    debug.clear();
#endif

    ImVec2 windowSize = ImGui::GetWindowSize() / state.zoom;
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImGuiIO& outerIo = ImGui::GetIO();
    bool outerWindowHovered = ImGui::IsWindowHovered();
    if (outerWindowHovered) {
        for (int i = 0; i < IM_ARRAYSIZE(outerIo.MouseDown); ++i) {
            if (outerIo.MouseDown[i] && !ImGui::IsMouseDragging(i)) {
                ImGui::SetWindowFocus();
                break;
            }
        }
    }

    state.outerWindowFocused = ImGui::IsWindowFocused();
    state.outerWindowHovered = ImGui::IsWindowHovered();

    if (state.outerWindowFocused && state.hoveredNode == -1 && !state.anyItemActive && ImGui::IsKeyReleased(outerIo.KeyMap[ImGuiKey_Home])) {
        state.flags |= NodeAreaFlags_ZoomToFit;
    }

    if (state.outerWindowHovered && state.hoveredNode == -1 && outerIo.MouseWheel != 0.f && state.innerContext->OpenPopupStack.empty()) {
        const float factor = 1.25f;
        float newZoom = outerIo.MouseWheel > 0 ? (state.zoom * factor) : (state.zoom / factor);
        if (newZoom > 0.15f && newZoom < 16.f) {
            //ImVec2 pos = ImGui::GetWindowSize() / 2.f; // zoom to window center
            ImVec2 pos = outerIo.MousePos - ImGui::GetWindowPos(); // zoom to mouse position
            state.innerWndPos = (pos * state.zoom - pos * newZoom + state.innerWndPos * newZoom * state.zoom) / (newZoom * state.zoom);
            setWindowPos = true;
            state.zoom = newZoom;
        }
    }

    ImGui::SetCurrentContext(state.innerContext);

    innerContextNewFrame(state.outerContext, state.innerContext, state.zoom,
        outerIo, windowSize, windowPos, state.outerWindowFocused, state.outerWindowHovered);

    ImGui::NewFrame();
#ifdef IMGUI_NODES_DEBUG
    debug << "BeginNodeArea " << ImGui::IsAnyItemActive() << std::endl;
#endif
    ImGui::SetNextWindowPos(state.innerWndPos, setWindowPos ? ImGuiCond_Always : ImGuiCond_Once);
    ImGui::SetNextWindowSize(state.nodeAreaSize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4());
    int wndFlags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("node_graph", nullptr, wndFlags);

    if (!setWindowPos) {
        state.innerWndPos = ImGui::GetCurrentWindowRead()->Pos;
    }

    // Avoids early clipping when zoomed in
    ImGui::PushClipRect(ImVec2(-1.f, -1.f), ImVec2(windowSize) + ImVec2(1.f, 1.f), false);

    // Window movement
    if (state.outerWindowFocused && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseDown(2) && !ImGui::IsMouseDragging(2, 1.0f)) {
        state.scrolling = true;
    }
    if (state.outerWindowFocused && state.scrolling && ImGui::IsMouseDragging(2, 0.0f)) {
        ImGui::ClearActiveID();
        state.innerWndPos = ImGui::GetCurrentWindowRead()->Pos + ImGui::GetIO().MouseDelta;
        ImGui::SetWindowPos(state.innerWndPos);
    } else {
        state.scrolling = false;
    }

    if (state.outerWindowFocused)
    {
        // Selection cruft
        switch (state.mode) {
        case Mode::SelectionCaptureAdd:
        case Mode::SelectionCaptureRemove:
            state.mode = Mode::None;
            break;
        case Mode::Selecting: {
            if (ImGui::IsKeyPressed(ImGui::GetIO().KeyMap[ImGuiKey_Escape])) {
                state.mode = Mode::Escaped;
                break;
            }

            if (ImGui::IsMouseReleased(0)) {
                state.mode = Mode::SelectionCaptureAdd;
                if (!ImGui::GetIO().KeyShift) {
                    clearAllSelections();
                }
                if (ImGui::GetIO().KeyShift && ImGui::GetIO().KeyCtrl) {
                    state.mode = Mode::SelectionCaptureRemove;
                }
            }
            break;
        }
        case Mode::None: {
            if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() &&
                state.hoveredNode == -1 && state.hoveredEdge == -1)
            {
                if (ImGui::IsMouseDown(0) && !ImGui::IsMouseDragging(0, 1.0f)) {
                    state.dragStart = ImGui::GetMousePos() - ImGui::GetWindowPos();
                    state.mode = Mode::Selecting;
                } else if (ImGui::IsMouseReleased(0)) {
                    clearAllSelections();
                }
            }
            break;
        }
        }

        if (state.mode == Mode::Selecting ||
            state.mode == Mode::DraggingEdgeInput ||
            state.mode == Mode::DraggingEdgeOutput)
        {
            state.dragEnd = ImGui::GetMousePos() - ImGui::GetWindowPos();
        }

        if (state.mode == Mode::DraggingEdgeInput ||
            state.mode == Mode::DraggingEdgeOutput)
        {
            bool differentStartAndEnd = state.edgeStartNode != state.edgeEndNode;
            bool startNodeValid = state.edgeStartNode != -1 && state.edgeStartSlot != -1;
            bool endNodeValid = state.edgeEndNode != -1 && state.edgeEndSlot != -1;

            if (ImGui::IsMouseReleased(0) && differentStartAndEnd &&
                (startNodeValid || endNodeValid) &&
                (startNodeValid && endNodeValid || state.hoveredNode == -1))
            {
                actionCallback(UserAction::NewEdge);
                state.mode = Mode::None;
            }
            else if (ImGui::IsMouseReleased(0) || ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
            {
                state.mode = Mode::None;
            }
        }

        if (!ImGui::IsAnyItemActive())
        {

            ImGuiIO const& io = ImGui::GetIO();
            // from imgui.cpp
            const bool is_shortcut_key_only = (io.KeyCtrl && !io.KeySuper) && !io.KeyAlt && !io.KeyShift;

            if (is_shortcut_key_only) {
                if (IsKeyPressedMap(ImGuiKey_X)) {
                    actionCallback(UserAction::Cut);
                } else if (IsKeyPressedMap(ImGuiKey_C)) {
                    actionCallback(UserAction::Copy);
                } else if (IsKeyPressedMap(ImGuiKey_V)) {
                    actionCallback(UserAction::Paste);
                } else if (IsKeyPressedMap(ImGuiKey_Z)) {
                    actionCallback(UserAction::Undo);
                } else if (IsKeyPressedMap(ImGuiKey_Y)) {
                    actionCallback(UserAction::Redo);
                } else if (IsKeyPressedMap(ImGuiKey_A) && state.mode == Mode::None) {
                    state.mode = Mode::SelectAll;
                }
            }
            if (IsKeyPressedMap(ImGuiKey_Delete)) {
                actionCallback(UserAction::Delete);
            }
        }
    } else /*if (!state.outerWindowFocused) */ {
        if (state.mode == Mode::Selecting) {
            // cancel selection - another component is active.
            state.mode = Mode::None;
        }
        ImGui::ClearActiveID();
    }

    ImVec2 unclampedPos = ImGui::GetCurrentWindowRead()->Pos;
    ImVec2 clampedPos = ImClamp(unclampedPos, -ImGui::GetWindowSize() + windowSize, ImVec2());
    if (unclampedPos != clampedPos) {
        ImGui::SetWindowPos(clampedPos);
    }

    paintGrid(style);

    state.activeNode = -1;
    state.hoveredNode = -1;
    state.hoveredEdge = -1;
    state.edgeEndNode = -1;
    state.edgeEndSlot = -1;
    state.anySizeChanged = false;

    if (state.flags & NodeAreaFlags_ZoomToFit) {
        float max = std::numeric_limits<float>::max();
        float min = std::numeric_limits<float>::lowest();
        state.lowerBound = ImVec2(max, max);
        state.upperBound = ImVec2(min, min);
    }

#ifdef IMGUI_NODES_DEBUG
    debug
        << "BeginNodeArea " << ImGui::IsAnyItemActive()
        << " scrolling " << state.scrolling
        << " mpos: " << ImGui::GetMousePos().x << ", " << ImGui::GetMousePos().y
        << " abs " << (ImGui::GetWindowPos() + ImGui::GetMousePos()).x << ", " << (ImGui::GetWindowPos() + ImGui::GetMousePos()).y
        << " hovered " << ImGui::IsWindowHovered()
        << std::endl;
#endif
}

void NodeArea::EndNodeArea() {
#ifdef IMGUI_NODES_DEBUG
    debug << "EndNodeArea " << ImGui::IsAnyItemActive() << " " << state.anySizeChanged << std::endl;
#endif

    switch (state.mode)
    {
    case Mode::DraggingNodes:
    case Mode::Escaped:
        if (ImGui::IsMouseReleased(0)) {
            state.mode = Mode::None;
        }
        break;
    case Mode::None:
        if (state.activeNode != -1 && ImGui::IsMouseDragging(0)) {
            if (!state.selectedNodes.isSelected(state.activeNode) || state.selectedNodes.selectedCount == 0) {
                state.selectedEdges.clearSelection();
                state.selectedNodes.clearSelection();

                state.selectedNodes.addToSelection(state.activeNode);
            }
            state.mode = Mode::DraggingNodes;
        }
        break;
    case Mode::SelectAll:
        state.mode = Mode::None;
        break;
    }

    if (state.mode == Mode::Selecting)
    {
        ImDrawList *draw_list = ImGui::GetWindowDrawList();
        ImVec2 offset = ImGui::GetWindowPos();
        if (state.dragStart != state.dragEnd) {
            draw_list->AddRectFilled(offset + state.dragStart, offset + state.dragEnd, style[Style_SelectionFill]);
            draw_list->AddRect(offset + state.dragStart, offset + state.dragEnd, style[Style_SelectionBorder]);
        }
    }

    if (state.mode == Mode::DraggingEdgeInput ||
        state.mode == Mode::DraggingEdgeOutput)
    {
        ImDrawList *draw_list = ImGui::GetWindowDrawList();
        ImVec2 offset = ImGui::GetWindowPos();

        ImVec2 p1 = offset + (state.mode == Mode::DraggingEdgeInput ? state.dragStart : state.dragEnd);
        ImVec2 cp1 = p1 + ImVec2(+50, 0);
        ImVec2 p2 = offset + (state.mode == Mode::DraggingEdgeInput ? state.dragEnd : state.dragStart);
        ImVec2 cp2 = p2 + ImVec2(-50, 0);

        draw_list->AddBezierCubic(p1, cp1, cp2, p2, style[Style_EdgeDragging], style[Style_EdgeDraggingSize]);
    }

    state.anyItemActive = ImGui::IsAnyItemActive();
    ImGui::PopClipRect();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::Render();
    ImDrawData* innerDrawData = ImGui::GetDrawData();
    IM_ASSERT(innerDrawData->Valid);

    ImGui::SetCurrentContext(state.outerContext);

    // ImGui snaps geometry to whole pixels. This leads to jaggy movement when zooming in.
    // We fix this by translating by the fract of the exact position.
    // This is also why we push a one pixel bigger clip-rect than usually necessary.
    ImVec2 fractInnerWndPos(
        floor(fmodf(state.innerWndPos.x, 1.f) * state.zoom),
        floor(fmodf(state.innerWndPos.y, 1.f) * state.zoom));

    ImVec2 translate = state.outerContext->CurrentWindow->Pos + fractInnerWndPos;
    float scale = state.zoom;

    if ((state.flags & NodeAreaFlags_NoCopy) == 0) {
        copyTransformDrawCmds(innerDrawData, scale, translate);
    }

#ifdef IMGUI_NODES_DEBUG
    debug << "active node " << state.activeNode << std::endl;

    ImGui::SetCursorPos(ImVec2(10.f, 5.f));
    ImGui::Text("x %.2f, y %.2f\nz %.2f\n%s", state.innerWndPos.x, state.innerWndPos.y, state.zoom, debug.str().c_str());
#endif
}

bool NodeArea::BeginNode(NodeState &node, bool resizeable) {
    bool oldSkip = node.skip;
    node.skip = false;

    node.forceRedraw = node.forceRedraw || node.size.x < 0.0f || state.flags & NodeAreaFlags_ForceRedraw;

    if (!node.forceRedraw) {
        ImVec2 origin = state.innerWndPos + node.pos;
        ImVec2 originAndSize = origin + node.size + style[Style_NodePadding] * 2.f + style[Style_SlotRadius] * 2.f;
        ImRect clip(origin, originAndSize);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec4 windowClipRect = draw_list->_ClipRectStack[draw_list->_ClipRectStack.size() - 1];

        node.skip = !clip.Overlaps(windowClipRect);
    }

    char buf[64];
#ifdef _MSC_VER
    sprintf_s(buf, "##node%d", node.id);
#else
    sprintf(buf, "##node%d", node.id);
#endif

    ImGui::SetNextWindowPos(state.innerWndPos + node.pos);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style[Style_NodePadding] + style[Style_SlotRadius]);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

    if (oldSkip && !node.skip) {
        ImGui::SetNextWindowSize(node.size + style[Style_SlotRadius] * 2.f);
    }

    // Overriding the DisplaySize is a workaround to keep ImGui::CalcSizeAutoFit
    // from downsizing our nodes if they do not fit our "virtual screen".
    ImVec2 oldDisplaySize = state.innerContext->IO.DisplaySize;
    state.innerContext->IO.DisplaySize = ImVec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());

    int wndFlags =
        ImGuiWindowFlags_AlwaysAutoResize |
        // ImGuiWindowFlags_ShowBorders |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;

    // put nodes into a separate "child" window so we can paint them on top of edges
    ImGui::Begin(buf, nullptr, wndFlags);

    // restore DisplaySize
    state.innerContext->IO.DisplaySize = oldDisplaySize;

    ImGui::PushID(&node);

    if (node.skip && !node.forceRedraw) {
        return false;
    }
    node.forceRedraw = false;


    node.inputSlots.resize(0);
    node.outputSlots.resize(0);

    bool hovered = state.hoveredNode == node.id;
    bool selected = state.selectedNodes.isSelected(node.id);
    bool wouldSelect = handleNodeDragSelection(*this, node.id, ImRect(node.pos, node.pos + node.size));

    ImU32 nodeBg = hovered ? style[Style_NodeFillHovered] : style[Style_NodeFill];
    ImU32 nodeBorder = (selected || wouldSelect) ? style[Style_NodeBorderSelected] : style[Style_NodeBorder];

    ImVec2 offset = ImGui::GetWindowPos() + style[Style_SlotRadius];

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    if (resizeable)
    {
        const float window_rounding = style[Style_NodeRounding];
        const float resize_corner_size = ImMax(state.innerContext->FontSize * 1.35f, window_rounding + 1.0f + state.innerContext->FontSize * 0.2f);
        const ImVec2 br = offset + node.size;
        bool resizeHovered, resizeHeld;

        const ImRect resize_rect(br - ImFloor(ImVec2(resize_corner_size * 0.75f, resize_corner_size * 0.75f)), br);
        const ImGuiID resize_id = ImGui::GetID(&node);
        ImGui::ButtonBehavior(resize_rect, resize_id, &resizeHovered, &resizeHeld, ImGuiButtonFlags_FlattenChildren);
        ImU32 resize_col = ImGui::GetColorU32(resizeHeld ? ImGuiCol_ResizeGripActive : resizeHovered ? ImGuiCol_ResizeGripHovered : ImGuiCol_ResizeGrip);

        if (resizeHeld && (state.mode == Mode::None || state.mode == Mode::ResizingNode)) {
            state.mode = Mode::ResizingNode;
            node.size += ImGui::GetIO().MouseDelta;
        } else if (!resizeHeld && state.mode == Mode::ResizingNode) {
            state.mode = Mode::None;
        }

        draw_list->PathLineTo(br + ImVec2(-resize_corner_size, -style[Style_NodeBorderSize]));
        draw_list->PathLineTo(br + ImVec2(-style[Style_NodeBorderSize], -resize_corner_size));
        draw_list->PathArcToFast(ImVec2(br.x - window_rounding - style[Style_NodeBorderSize], br.y - window_rounding - style[Style_NodeBorderSize]), window_rounding, 0, 3);
        draw_list->PathFillConvex(resize_col);
    }

    draw_list->PushClipRect(state.innerWndPos + node.pos, state.innerWndPos + node.pos + ImGui::GetWindowSize());

    draw_list->AddRectFilled(offset, offset + node.size, nodeBg, style[Style_NodeRounding]);
    draw_list->AddRect(offset, offset + node.size, nodeBorder, style[Style_NodeRounding], ImDrawFlags_RoundCornersAll, style[Style_NodeBorderSize]);

    draw_list->PopClipRect();

    ImGui::SetCursorPos(ImVec2(0.f, 0.f) + style[Style_SlotRadius] + style[Style_NodePadding]);

    ImGui::BeginGroup(); // Lock horizontal position

#ifdef IMGUI_NODES_DEBUG
    debug << "BeginNode " << node.id << " " << ImGui::IsAnyItemActive() << std::endl;
#endif
    return true;
}

void NodeArea::EndNode(NodeState &node) {
    bool hovered = false;
    bool selected = false;
    if (!node.skip) {
#ifdef IMGUI_NODES_DEBUG
        debug << "EndNode " << node.id << " " << ImGui::IsAnyItemActive() << std::endl;
#endif

        ImGui::EndGroup();

        if (state.mode != Mode::ResizingNode) {
            ImVec2 newSize = ImGui::GetWindowSize() - style[Style_SlotRadius] * 2.f;
            if (newSize != node.size) {
                // The dampening on the size adaptation fixes possible feedback loops
                // with automatically resizing UI controls and automatically resizing nodes.
                // Factors closer to one show the jittering longer but the sizes of other nodes
                // converge faster. Factors closer to zero fix the jittering faster, but with lower
                // values one can see the nodes grow or shrink slowly.
                const float sizeAdaptationDampening = 0.75f;
                node.size += (newSize - node.size) * sizeAdaptationDampening;

                node.forceRedraw = true;
                state.anySizeChanged = true;
            }
        }

        ImGui::SetCursorPos(ImVec2() + style[Style_SlotRadius]);
        invisibleButtonNoResize("node", node.size);
        bool itemActive = state.outerWindowFocused && ImGui::IsItemActive();
        bool itemWasActive = state.outerWindowFocused && WasItemActive();
#ifdef IMGUI_NODES_DEBUG
        debug << "active: " << itemActive << " was active: " << itemWasActive << std::endl;
#endif
        hovered = state.outerWindowHovered && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

        if (itemWasActive && ImGui::IsMouseReleased(0) && state.mode == Mode::None)
        {
            if (ImGui::GetIO().KeyShift) {
                state.selectedNodes.toggleSelection(node.id);
            } else {
                // Allows selection of nodes with a simple click.
                // Clears other selections.
                clearAllSelections();
                state.selectedNodes.addToSelection(node.id);
            }
        }
        if (itemActive) {
            state.activeNode = node.id;
        }
        if (hovered) {
            state.hoveredNode = node.id;
        }
    }
    if (state.mode == Mode::SelectAll) {
        state.selectedNodes.addToSelection(node.id);
    }
    selected = state.selectedNodes.isSelected(node.id);

    bool canMove = selected || (hovered && state.mode == Mode::None);
    if (state.mode == Mode::DraggingNodes && canMove && ImGui::GetIO().MouseDelta != ImVec2()) {
        node.posFloat = node.posFloat + ImGui::GetIO().MouseDelta;
        if (ImGui::GetIO().KeyShift) {
            node.pos = node.posFloat;
        } else {
            node.pos = ImVec2(
                floor((node.posFloat.x) / state.snapGrid) * state.snapGrid,
                floor((node.posFloat.y) / state.snapGrid) * state.snapGrid
            ) - style[Style_EdgeSize];
        }
        node.forceRedraw = true;
    } else if (state.mode == Mode::None) {
        node.posFloat = node.pos;
    }
    if (state.flags & NodeAreaFlags_SnapToGrid) {
        node.posFloat = node.pos = ImVec2(
            floor((node.pos.x) / state.snapGrid) * state.snapGrid,
            floor((node.pos.y) / state.snapGrid) * state.snapGrid
        ) - style[Style_EdgeSize];
        node.forceRedraw = true;
    }
    if (state.flags & NodeAreaFlags_ZoomToFit) {
        ImRect r(state.lowerBound, state.upperBound);
        r.Add(node.pos);
        r.Add(node.pos + node.size);
        state.lowerBound = r.Min;
        state.upperBound = r.Max;
    }
    ImGui::PopID();
    ImGui::End();
    ImGui::PopStyleVar(3);
}

void NodeArea::BeginSlot(NodeState &node) {
    ImVec2 offset = ImGui::GetWindowPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->PushClipRect(state.innerWndPos + node.pos, state.innerWndPos + node.pos + ImGui::GetWindowSize());
    node.lastCursor = ImGui::GetCursorPos();
    ImVec2 pos = ImVec2(style[Style_SlotRadius], node.lastCursor.y);
    draw_list->AddLine(
        offset + pos + ImVec2(style[Style_NodeBorderSize] * 0.5f, 0.f),
        offset + pos + ImVec2(node.size.x - style[Style_NodeBorderSize], 0),
        style[Style_SlotSeparatorColor], style[Style_SlotSeparatorSize]);
    draw_list->PopClipRect();
    ImGui::Spacing();
}

void NodeArea::EndSlot(NodeState &node, int inputType, int outputType) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->PushClipRect(state.innerWndPos + node.pos, state.innerWndPos + node.pos + ImGui::GetWindowSize());

    ImVec2 relativeInputPos = ImVec2(style[Style_SlotRadius], node.lastCursor.y + (ImGui::GetCursorPos().y - node.lastCursor.y) / 2.f);
    ImVec2 relativeOutputPos = relativeInputPos + ImVec2(node.size.x, 0);

    ImVec2 absoluteInputPos = relativeInputPos + node.pos;
    ImVec2 absoluteOutputPos = relativeOutputPos + node.pos;

    ImVec2 slotBoundsMin = ImVec2(style[Style_SlotRadius], node.lastCursor.y);
    ImVec2 slotBoundsMax = ImVec2(slotBoundsMin.x + node.size.x, ImGui::GetCursorPos().y);
    ImVec2 offset = ImGui::GetWindowPos();

    auto paintEdgeDock = [&](ImVec2 pos, ImColor col, bool input, int slotIdx, ImVec2 dragPos) {
#ifdef IMGUI_NODES_DEBUG
        debug
            << " edge " << (input ? "input " : "output ") << slotIdx << " pos: "
            << pos.x << ", " << pos.y << std::endl;
#endif
        draw_list->AddCircleFilled(offset + pos, style[Style_SlotRadius], col);

        bool hovered =
            state.outerWindowFocused &&
            ImLengthSqr(offset + pos - ImGui::GetMousePos()) <= style[Style_SlotMouseRadius] * style[Style_SlotMouseRadius];

        bool anotherItemIsActive = false;
        if (style.newEdgeFromSlot) {
            anotherItemIsActive = ImGui::IsAnyItemActive();
            ImVec2 oldCursor = ImGui::GetCursorPos();
            ImGui::SetCursorPos(slotBoundsMin);
            invisibleButtonNoResize("slot", slotBoundsMax - slotBoundsMin);
            ImGui::SetCursorPos(oldCursor);

            hovered = hovered
                || ImGui::IsItemHovered()
                || ImRect(slotBoundsMin, slotBoundsMax).Contains(ImGui::GetMousePos() - offset);
        }
        if (hovered) {
            Mode mode = input ? Mode::DraggingEdgeInput : Mode::DraggingEdgeOutput;
            bool validTarget =
                (state.mode == Mode::DraggingEdgeInput || state.mode == Mode::DraggingEdgeOutput) &&
                mode != state.mode &&           // makes sure, we cannot connect input and input or output and output
                state.edgeStartNode != node.id; // makes sure, we cannot connect inputs and outputs of the same node

            if (ImGui::IsMouseDown(0)
                && !ImGui::IsMouseDragging(0, 1.f)
                && state.mode == Mode::None
                && ImGui::IsWindowFocused()
                && !anotherItemIsActive)
            {
                state.mode = mode;
                state.dragStart = state.dragEnd = dragPos;
                state.edgeStartNode = node.id;
                state.edgeStartSlot = slotIdx;
            } else if (validTarget) {
                state.edgeEndNode = node.id;
                state.edgeEndSlot = slotIdx;
            }

            if (state.mode == Mode::None || validTarget) {
                draw_list->AddCircle(offset + pos, style[Style_SlotRadius], style[Style_SlotBorderHovered]);
                if (style.newEdgeFromSlot) {
                    draw_list->AddRect(offset + slotBoundsMin, offset + slotBoundsMax, style[Style_SlotBorderHovered]);
                }
            }
        }
    };

    if (inputType != -1) {
        node.inputSlots.push_back({ inputType, absoluteInputPos });
        paintEdgeDock(relativeInputPos, style[Style_InputEdgeColor], false, (int)node.inputSlots.size() - 1, absoluteInputPos);
    }
    if (outputType != -1) {
        node.outputSlots.push_back({ outputType, absoluteOutputPos });
        paintEdgeDock(relativeOutputPos, style[Style_OutputEdgeColor], true, (int)node.outputSlots.size() - 1, absoluteOutputPos);
    }

    draw_list->PopClipRect();
    node.lastCursor = ImGui::GetCursorPos();
}

bool NodeArea::DrawEdge(int edgeId, NodeState const &sourceNode, int sourceSlot, NodeState const &sinkNode, int sinkSlot) {
    if (sourceNode.outputSlots.size() <= sourceSlot || sinkNode.inputSlots.size() <= sinkSlot)
        return false;

    ImVec2 offset = ImGui::GetWindowPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p1 = sourceNode.outputSlots[sourceSlot].pos;
    ImVec2 cp1 = p1 + ImVec2(+50, 0);
    ImVec2 p2 = sinkNode.inputSlots[sinkSlot].pos;
    ImVec2 cp2 = p2 + ImVec2(-50, 0);

    if (state.mode == Mode::SelectAll) {
        state.selectedEdges.addToSelection(edgeId);
    }

    bool wouldSelect = handleEdgeDragSelection(*this, edgeId, p1, cp1, cp2, p2);

    p1 += offset; p2 += offset; cp1 += offset; cp2 += offset;

    bool hovered = state.outerWindowFocused && ImGui::IsWindowHovered() &&
        closeToBezier(ImGui::GetMousePos(), p1, cp1, cp2, p2, 8.f);
    if (state.selectedEdges.isSelected(edgeId) || wouldSelect) {
        draw_list->AddBezierCubic(p1, cp1, cp2, p2, style[Style_EdgeSelectedColor], style[Style_EdgeSelectedSize]);
    }

    ImColor color = style[Style_EdgeColor];
    if (hovered && state.mode == Mode::None) {
        state.hoveredEdge = edgeId;
        color = style[Style_EdgeHovered];
        if (ImGui::IsMouseClicked(0)) {
            if (!ImGui::GetIO().KeyShift) {
                clearAllSelections();
            }
            state.selectedEdges.toggleSelection(edgeId);
        }
    }

    draw_list->AddBezierCubic(p1, cp1, cp2, p2, color, style[Style_EdgeSize]);

    return true;
}

bool NodeArea::GetNewEdge(int *edgeSourceNode, int *edgeSourceNodeSlot, int *edgeSinkNode, int *edgeSinkNodeSlot) const
{
    if (ImGui::IsMouseReleased(0) && state.edgeStartNode != state.edgeEndNode &&
        ((state.edgeStartNode != -1 && state.edgeStartSlot != -1) ||
        (state.edgeEndNode != -1 && state.edgeEndSlot != -1)))
    {
        if (state.mode == Mode::DraggingEdgeInput) {
            *edgeSourceNode     = state.edgeStartNode;
            *edgeSourceNodeSlot = state.edgeStartSlot;
            *edgeSinkNode       = state.edgeEndNode;
            *edgeSinkNodeSlot   = state.edgeEndSlot;
            return true;
        } else if(state.mode == Mode::DraggingEdgeOutput) {
            *edgeSourceNode     = state.edgeEndNode;
            *edgeSourceNodeSlot = state.edgeEndSlot;
            *edgeSinkNode       = state.edgeStartNode;
            *edgeSinkNodeSlot   = state.edgeStartSlot;
            return true;
        }
    }
    return false;
}

ImVec2 NodeArea::GetAbsoluteMousePos() const
{
    return ImGui::GetMousePos() - ImGui::GetWindowPos();
}

ImVec2 NodeArea::GetContentSize(NodeState &node) const
{
    return node.size - style[Style_SlotRadius];
}

ImVec2 NodeArea::ConvertToNodeAreaPosition(ImVec2 outsidePosition) const
{
    return outsidePosition / state.zoom - state.innerWndPos;
}

#ifdef IMGUI_NODES_DEBUG
void NodeArea::ShowMetricsWindow(bool* p_open /*= nullptr*/)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, state.outerContext->Style.WindowBorderSize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, state.outerContext->Style.Colors[ImGuiCol_WindowBg]);
    ImGui::ShowMetricsWindow(p_open);
    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(1);

}

std::stringstream& NodeArea::Debug()
{
    return debug;
}
#endif

void Style::push(StyleFloat idx, float f)
{
    styleStack.emplace_back(idx, styleFloats[idx]);
    styleFloats[idx] = f;
}

void Style::push(StyleVec2 idx, ImVec2 const &vec2)
{
    styleStack.emplace_back(idx, styleVec2s[idx]);
    styleVec2s[idx] = vec2;
}

void Style::push(StyleColor idx, ImColor const &col)
{
    styleStack.emplace_back(idx, styleColors[idx]);
    styleColors[idx] = col;
}

void Style::pop(int count)
{
    while (count-- > 0) {
        IM_ASSERT(styleStack.size() > 0);
        uint32_t idx = styleStack.back().index();
        switch (styleStack.back().type()) {
        case StyleDataType_Float: styleFloats[idx] = styleStack.back().f; break;
        case StyleDataType_Vec2: styleVec2s[idx] = styleStack.back().vec2; break;
        case StyleDataType_Color: styleColors[idx] = styleStack.back().color; break;
        default: IM_ASSERT(false); break;
        }
        styleStack.pop_back();
    }
}

void Style::generate()
{
    ImGuiStyle const& style = ImGui::GetStyle();

    styleFloats = {
        3.f,  // Style_EdgeSize
        4.f,  // Style_EdgeSelectedSize
        3.f,  // Style_EdgeDraggingSize
        style.WindowRounding,  // Style_NodeRounding
        2.f,  // Style_NodeBorderSize
        4.f,  // Style_SlotRadius
        10.f, // Style_SlotMouseRadius
        1.5f  // Style_SlotSeparatorSize
    };

    styleVec2s = { {
        style.WindowPadding // Style_NodePadding
    } };

    styleColors = { {
        {  50,  50,  50,  50 }, //Style_SelectionFill
        { 255, 255, 255, 150 }, //Style_SelectionBorder
        { 255, 128,   0, 255 }, //Style_EdgeSelectedColor
        { 255, 128,   0, 255 }, //Style_EdgeDragging
        { 192, 192, 192, 255 }, //Style_EdgeHovered
        { 150, 150, 250, 255 }, //Style_EdgeColor
        { 150, 150, 250, 150 }, //Style_InputEdgeColor
        { 150, 150, 250, 150 }, //Style_OutputEdgeColor
        style.Colors[ImGuiCol_WindowBg], //Style_NodeFill
        style.Colors[ImGuiCol_PopupBg], //Style_NodeFillHovered
        style.Colors[ImGuiCol_Border], //Style_NodeBorder
        style.Colors[ImGuiCol_FrameBgActive], //Style_NodeBorderSelected
        { 255, 128,   0, 255 }, //Style_SlotBorderHovered
        style.Colors[ImGuiCol_Separator]  //Style_SlotSeparatorColor
    } };

    grid = {
        Grid{ 64, 1.f, ImColor(150, 150, 150, 200) },
        Grid{ 16, 1.f, ImColor(200, 200, 200, 100) },
        Grid{ 1, 1.f, ImColor(230, 230, 230, 40) } };
}


NodeState::NodeState(int id, ImVec2 initialPos)
    : id(id)
    , pos(initialPos)
    , posFloat(initialPos)
    , size(-1.f, -1.f)
    , skip(false)
    , forceRedraw(true)
{}

} // namespace nodes
