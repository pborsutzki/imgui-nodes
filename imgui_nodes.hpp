#pragma once

//#define IMGUI_NODES_DEBUG

/*#ifndef IMGUI_DEFINE_MATH_OPERATORS
# define IMGUI_DEFINE_MATH_OPERATORS
#endif // IMGUI_DEFINE_MATH_OPERATORS*/

#include <imgui.h>
#include <vector>
#include <array>
#include <functional>
#include <cstdint>

namespace nodes {

enum class UserAction {
    None, Delete, Copy, Cut, Paste, Undo, Redo, NewEdge
};

typedef int NodeAreaFlags; // These flags should typically only be set for one frame.
enum NodeAreaFlags_ {
    NodeAreaFlags_UpdateStyle = 1 << 0, // Applies the current ImGui style to the node graph area.
    NodeAreaFlags_SnapToGrid  = 1 << 1, // Aligns all nodes to the configured snapGrid.
    NodeAreaFlags_ForceRedraw = 1 << 2, // Redraws all visible and invisible nodes. Use
                                        // after changing node positions/sizes programmatically.
    NodeAreaFlags_ZoomToFit   = 1 << 3, // Changes zoom level to fit the whole graph on the visible screen.
    NodeAreaFlags_NoCopy      = 1 << 4  // Do not copy draw commands (does not display). Useful for
                                        // re-rendering the node ui until automatic resizes converge.
};

struct SlotState {
    int type;
    ImVec2 pos;
};

struct NodeState {
    NodeState(int id, ImVec2 initialPos);
    NodeState(NodeState const &other) = default;
    NodeState& operator= (const NodeState& other) = default;

    int id;

    ImVec2 pos;         // position of the nodes imgui window
    ImVec2 posFloat;
    ImVec2 size;        // size of the nodes imgui window
    ImVec2 sizeConstraintMin;
    ImVec2 sizeConstraintMax;

    ImVec2 lastCursor;
    bool skip;
    bool forceRedraw;

    std::vector<SlotState> inputSlots;
    std::vector<SlotState> outputSlots;
};

struct Grid {
    int spacingMultiplier;
    float thickness;
    ImColor lineColor;
};

enum StyleColor : uint32_t {
    Style_SelectionFill = 0,
    Style_SelectionBorder,
    Style_EdgeSelectedColor,
    Style_EdgeDragging,
    Style_EdgeHovered,
    Style_EdgeColor,
    Style_InputEdgeColor,
    Style_OutputEdgeColor,
    Style_NodeFill,
    Style_NodeFillHovered,
    Style_NodeBorder,
    Style_NodeBorderSelected,
    Style_SlotBorderHovered,
    Style_SlotSeparatorColor,
    Style_Color_Count_
};

enum StyleVec2 : uint32_t {
    Style_NodePadding = 0,
    Style_Vec2_Count_
};

enum StyleFloat : uint32_t {
    Style_EdgeSize = 0,
    Style_EdgeSelectedSize,
    Style_EdgeDraggingSize,
    Style_NodeRounding,
    Style_NodeBorderSize,
    Style_SlotRadius,
    Style_SlotMouseRadius,
    Style_SlotSeparatorSize,
    Style_Float_Count_
};

enum StyleDataType : uint32_t {
    StyleDataType_Float = 0u << 30,
    StyleDataType_Vec2  = 1u << 30,
    StyleDataType_Color = 2u << 30,
    //StyleDataType_Reserved = 3u << 30,
    StyleDataType_Mask_ = 3u << 30
};

struct StyleMod {
    StyleMod(StyleFloat idx, float f)          : maskedStyleIndex(idx | StyleDataType_Float), f(f) {}
    StyleMod(StyleVec2  idx, ImVec2 const &v)  : maskedStyleIndex(idx | StyleDataType_Vec2), vec2(v) {}
    StyleMod(StyleColor idx, ImColor const &c) : maskedStyleIndex(idx | StyleDataType_Color), color(c) {}

    StyleDataType type() const { return (StyleDataType)(maskedStyleIndex & StyleDataType_Mask_); }
    uint32_t index() const { return maskedStyleIndex & ~StyleDataType_Mask_; }

    uint32_t maskedStyleIndex;
    union {
        float f;
        ImVec2 vec2;
        ImColor color;
    };
};

struct Style {
    bool newEdgeFromSlot; // enables creating new edges from the whole slot area instead of just from the slot connector circle

    // todo: sort this vector by descending spacing
    std::vector<Grid> grid;
    float gridSpacing;

    std::array<float, Style_Float_Count_>   styleFloats;
    std::array<ImVec2, Style_Vec2_Count_>   styleVec2s;
    std::array<ImColor, Style_Color_Count_> styleColors;
    std::vector<StyleMod> styleStack;

    float           operator[](StyleFloat idx) const { return styleFloats[idx]; }
    ImVec2  const & operator[](StyleVec2  idx) const { return styleVec2s[idx]; }
    ImColor const & operator[](StyleColor idx) const { return styleColors[idx]; }

    void push(StyleFloat idx, float f);
    void push(StyleVec2  idx, ImVec2 const &vec2);
    void push(StyleColor idx, ImColor const &col);
    void pop(int count = 1);

    Style()
        : styleFloats({
            3.f,  // Style_EdgeSize
            4.f,  // Style_EdgeSelectedSize
            3.f,  // Style_EdgeDraggingSize
            4.f,  // Style_NodeRounding
            2.f,  // Style_NodeBorderSize
            4.f,  // Style_SlotRadius
            10.f, // Style_SlotMouseRadius
            1.5f  // Style_SlotSeparatorSize
            })
        , styleVec2s({{
            { 8.f, 8.f } // Style_NodePadding
            }})
        , styleColors({{
            {  50,  50,  50,  50 }, // Style_SelectionFill
            { 255, 255, 255, 150 }, // Style_SelectionBorder
            { 255, 128,   0, 255 }, // Style_EdgeSelectedColor
            { 255, 128,   0, 255 }, // Style_EdgeDragging
            { 192, 192, 192, 255 }, // Style_EdgeHovered
            { 150, 150, 250, 255 }, // Style_EdgeColor
            { 150, 150, 250, 150 }, // Style_InputEdgeColor
            { 150, 150, 250, 150 }, // Style_OutputEdgeColor
            {  60,  60,  60, 255 }, // Style_NodeFill
            {  75,  75,  75, 255 }, // Style_NodeFillHovered
            { 100, 100, 100, 255 }, // Style_NodeBorder
            { 255, 128,   0, 255 }, // Style_NodeBorderSelected
            { 255, 128,   0, 255 }, // Style_SlotBorderHovered
            { 100, 100, 100, 255 }  // Style_SlotSeparatorColor
            }})
        , grid({
            Grid{ 64, 1.f, ImColor(150, 150, 150, 200) },
            Grid{ 16, 1.f, ImColor(200, 200, 200, 100) },
            Grid{ 1, 1.f, ImColor(230, 230, 230, 40) }})
        , gridSpacing(16)
        , newEdgeFromSlot(true)
    {}

    void generate();
};

struct NodeArea {
    enum class Mode {
        None,
        Selecting, SelectionCaptureAdd, SelectionCaptureRemove,
        DraggingNodes, ResizingNode,
        DraggingEdgeInput, DraggingEdgeOutput,
        Escaped, SelectAll
    };

    struct Selection {
        int selectedCount = 0;
        std::vector<bool> selectedItems; // there should be a specialization for this type ...

        void clearSelection();
        void addToSelection(int id);
        void removeFromSelection(int id);
        void toggleSelection(int id);
        bool isSelected(int id) const;
    };

    struct InternalState {
        bool initialized = false;
        ImVec2 nodeAreaSize = ImVec2(20000.f, 20000.f);
        ImVec2 zoomLimits = ImVec2(0.15f, 16.f);

        ImGuiContext* innerContext;
        ImGuiContext* outerContext;

        float zoom;
        float snapGrid = 16.f;
        ImVec2 innerWndPos;

        Mode mode;
        bool scrolling;

        ImVec2 dragStart;
        ImVec2 dragEnd;

        ImVec2 lowerBound;
        ImVec2 upperBound;

        int edgeStartNode;
        int edgeStartSlot;
        int edgeEndNode;
        int edgeEndSlot;

        int activeNode = -1;
        int hoveredNode = -1;
        int hoveredEdge = -1;
        Selection selectedNodes;
        Selection selectedEdges;

        NodeAreaFlags flags;

        bool outerWindowFocused;
        bool outerWindowHovered;
        bool anyItemActive;
        bool anySizeChanged;
    } state;

    Style style;

    void clearAllSelections() {
        state.selectedNodes.clearSelection();
        state.selectedEdges.clearSelection();
    }

    void BeginNodeArea(std::function<void(UserAction)> actionCallback, NodeAreaFlags flags);
    void EndNodeArea();

    bool BeginNode(NodeState &node, bool resizeable = false);
    void EndNode(NodeState &node);

    void BeginSlot(NodeState &node);
    void EndSlot(NodeState &node, int inputType = -1, int outputType = -1);

    bool DrawEdge(int edgeId, NodeState const &sourceNode, int sourceSlot, NodeState const &sinkNode, int sinkSlot);
    bool GetNewEdge(int *edgeSourceNode, int *edgeSourceNodeSlot, int *edgeSinkNode, int *edgeSinkNodeSlot) const;

    ImVec2 GetAbsoluteMousePos() const;
    ImVec2 GetContentSize(NodeState &node) const;
    ImVec2 ConvertToNodeAreaPosition(ImVec2 outsidePosition) const;

#ifdef IMGUI_NODES_DEBUG
    void ShowMetricsWindow(bool* p_open = nullptr);
    std::stringstream& Debug();
#endif
};

static_assert(std::is_default_constructible<NodeArea>::value, "");
static_assert(std::is_copy_constructible<NodeArea>::value, "");
static_assert(std::is_move_constructible<NodeArea>::value, "");
static_assert(std::is_copy_assignable<NodeArea>::value, "");
static_assert(std::is_move_assignable<NodeArea>::value, "");

} // namespace nodes
