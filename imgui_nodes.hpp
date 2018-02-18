#pragma once

#include <imgui.h>
#include <vector>
#include <array>
#include <functional>

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
    NodeAreaFlags_ZoomToFit   = 1 << 3  // Changes zoom level to fit the whole graph on the visible screen.
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

    ImVec2 pos;
    ImVec2 posFloat;
    ImVec2 size;

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

struct Style {
    ImColor selectionFill;
    ImColor selectionBorder;

    float   edgeSize;
    ImColor edgeSelectedColor;
    float   edgeSelectedSize;
    ImColor edgeDragging;
    float   edgeDraggingSize;
    ImColor edgeHovered;
    ImColor edgeInvalid;

    ImVec2  nodePadding;
    ImColor nodeFill;
    ImColor nodeFillHovered;
    ImColor nodeBorder;
    ImColor nodeBorderSelected;
    float   nodeRounding;
    float   nodeBorderSize;

    float   slotRadius;
    float   slotMouseRadius;
    ImColor slotBorderHovered;
    ImColor slotSeparatorColor;
    float   slotSeparatorSize;

    std::vector<ImColor> edgeTypeColor;

    // todo: sort this vector by descending spacing
    std::vector<Grid> grid;
    float gridSpacing;

    Style() 
        : selectionFill(50, 50, 50, 50)
        , selectionBorder(255, 255, 255, 150)
        , edgeSize(3.f)
        , edgeSelectedColor(255, 128, 0)
        , edgeSelectedSize(4.f)
        , edgeDragging(255, 128, 0)
        , edgeDraggingSize(3.f)
        , edgeHovered(192, 192, 192)
        , edgeInvalid(255, 0, 0)
        , nodePadding(8.f, 8.f)
        , nodeFill(60, 60, 60)
        , nodeFillHovered(75, 75, 75)
        , nodeBorder(100, 100, 100)
        , nodeBorderSelected(255, 128, 0)
        , nodeRounding(4.f)
        , nodeBorderSize(2.f)
        , slotRadius(4.f)
        , slotMouseRadius(10.f)
        , slotBorderHovered(255, 128, 0)
        , slotSeparatorColor(100, 100, 100)
        , slotSeparatorSize(1.5f)
        , edgeTypeColor({
            ImColor(150, 150, 250, 150),
            ImColor(250, 150, 250, 150),
            ImColor(250, 250, 150, 150),
            ImColor(150, 250, 250, 150) })
        , grid({
            Grid{ 64, 1.f, ImColor(150, 150, 150, 200) },
            Grid{ 16, 1.f, ImColor(200, 200, 200, 100) },
            Grid{ 1, 1.f, ImColor(230, 230, 230, 40) }})
        , gridSpacing(16)
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
