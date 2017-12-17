#include <imgui.h>

#include <imgui_nodes.hpp>

#include <vector>
#include <algorithm>
#include <exception>
#include <iostream>

#include <variant>
#include <array>
#include <cctype>

namespace {

// for_each_alternative: Helper to iterate over types in variant.
// Usage: 
// using VariantType = std::variant<int, bool>;
// for_each_alternative<VariantType>([](auto S) {
//     using type = decltype(S)::type;
// }
template<typename V, size_t I, typename Callable>
static typename std::enable_if_t<I == 0, void>
for_each_alternative(Callable&&) {}

template<typename V, size_t I = std::variant_size_v<V>, typename Callable>
static typename std::enable_if_t<I != 0, void>
for_each_alternative(Callable&& c) {
    for_each_alternative<V, I - 1>(std::forward<Callable>(c));
    struct S { using type = std::variant_alternative_t<I - 1, V>; };
    c(S{});
}

static bool findIgnoreCase(std::string_view const &haystack, std::string_view const &needle) {
    auto it = std::search(
        haystack.cbegin(), haystack.cend(),
        needle.cbegin(), needle.cend(),
        [](auto a, auto b) { return std::toupper(a) == std::toupper(b); });
    return it != haystack.cend();
}

inline ImVec2 operator+(const ImVec2& lhs, const ImVec2 rhs) { return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }
inline ImVec2 operator-(const ImVec2& lhs, const ImVec2 rhs) { return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y); }

} // unnamed namespace

struct Connection {
    int sourceNode;
    int sourceSlot;
    int sinkNode;
    int sinkSlot;
};

enum class DataType { None = -1, Float1 = 0, Float2 = 1, Float3 = 2, Float4 = 3 };

template<typename... T>
struct Graph
{
    using NodeType = std::variant<T...>;

    int getFreeNodeSlot() {
        for (int i = 0; i < (int)nodes.size(); ++i) {
            if (std::holds_alternative<std::monostate>(nodes[i]))
                return i;
        }
        nodes.emplace_back(std::monostate{});
        return (int)nodes.size() - 1;
    }

    bool newNodePopup(bool forceOpen = false) {
        ImVec2 windowPos = ImGui::GetWindowPos();
        static const char *popupStr = "item context menu";
        static char filter[64];
        if (forceOpen || nodeArea.state.hoveredLink == -1 && nodeArea.state.hoveredNode == -1 && ImGui::IsMouseClicked(1)) {
            ImGui::OpenPopup(popupStr);
            filter[0] = '\0';
        }

        if (ImGui::BeginPopup(popupStr))
        {
            ImVec2 popupPos = ImGui::GetMousePosOnOpeningCurrentPopup() - windowPos;
            ImGui::Text("Add Node:");
            ImGui::Separator();
            for_each_alternative<NodeType>([this, popupPos](auto S) {
                using type = decltype(S)::type;
                if constexpr(!std::is_same_v<type, std::monostate>) {
                    if (findIgnoreCase(type::name, filter)) {
                        if (ImGui::Selectable(type::name)) {
                            int nodeslot = getFreeNodeSlot();
                            nodes[nodeslot] = type{ nodes::NodeState(nodeslot, popupPos) };
                        }
                    }
                }
            });
            ImGui::Separator();
            if (!ImGui::IsAnyItemActive()) {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::InputText("filter", filter, sizeof(filter));
            ImGui::EndPopup();
            return true;
        }
        return false;
    }

    void deleteConnector(int index) {
        connections.erase(connections.begin() + index);
    }

    void deleteNode(int index) {
        // erasing changes positions of other nodes which would make connections wrong.
        // -> insert placeholder instead
        connections.erase(std::remove_if(connections.begin(), connections.end(), [index](Connection &c) {
            return c.sinkNode == index || c.sourceNode == index;
        }), connections.end());
        nodes[index] = std::monostate{};
    }

    void deleteSelectedItems() {
        for (int i = (int)nodeArea.state.selectedLinks.selectedItems.size() - 1; i >= 0 ; --i) {
            if (nodeArea.state.selectedLinks.selectedItems[i]) {
                deleteConnector(i);
            }
        }
        for (int i = (int)nodeArea.state.selectedNodes.selectedItems.size() - 1; i >= 0; --i) {
            if (nodeArea.state.selectedNodes.selectedItems[i]) {
                deleteNode(i);
            }
        }
        nodeArea.clearAllSelections();
    }

    void draw(bool updateStyle) {
        if (updateStyle) {
            nodeArea.style.generate();
        }

        bool forcePopup = false;

        auto userAction = [&](nodes::UserAction action) {
            switch (action) {
            case nodes::UserAction::NewConnection:
            {
                Connection newConnection;
                nodeArea.GetNewConnection(&newConnection.sourceNode, &newConnection.sourceSlot, &newConnection.sinkNode, &newConnection.sinkSlot);
                if (newConnection.sourceNode == -1 || newConnection.sinkNode == -1) {
                    forcePopup = true;
                } else {
                    auto replacedConnector = std::find_if(connections.begin(), connections.end(), [&newConnection](Connection const &connection) {
                        return connection.sinkNode == newConnection.sinkNode && 
                            connection.sinkSlot == newConnection.sinkSlot;
                    });
                    if (replacedConnector != connections.end()) {
                        deleteConnector((int)(replacedConnector - connections.begin()));
                    }
                    connections.emplace_back(std::move(newConnection));
                }
                break;
            }
            case nodes::UserAction::Delete:
                deleteSelectedItems();
                break;
            }
        };

        nodeArea.BeginNodeArea(userAction, updateStyle);

        for (int i = 0; i < (int)nodes.size(); ++i) {
            ImGui::PushID(i);
            std::visit([this](auto &node)->void { 
                if constexpr(!std::is_same_v<decltype(node), std::monostate&>) {
                    node.draw(nodeArea);
                }
            }, nodes[i]);
            ImGui::PopID();
        }

        for (int i = (int)connections.size() - 1; i >= 0; --i) {
            auto const &connection = connections[i];
            if (!nodeArea.ConnectNodeSlots(i,
                getNodeState(nodes[connection.sourceNode]), connection.sourceSlot,
                getNodeState(nodes[connection.sinkNode]), connection.sinkSlot))
            {
                deleteConnector(i);
            }
        }

        newNodePopup(forcePopup);

        nodeArea.EndNodeArea();

        //ImGui::SetCursorPos(ImVec2(10.f, 5.f));
        //ImGui::Text("x %.2f, y %.2f\nz %.2f", nodeArea.state.innerWndPos.x, nodeArea.state.innerWndPos.y, nodeArea.state.zoom);
    }

    static nodes::NodeState& getNodeState(NodeType &node) {
        return std::visit([](auto &n)->nodes::NodeState& { 
            if constexpr(std::is_same_v<decltype(n), std::monostate&>) {
                throw std::runtime_error("Attempted to query the state of a deleted node.");
            } else {
                return n.state;
            }
        }, node);
    };

    std::vector<NodeType> nodes;
    std::vector<Connection> connections;
    nodes::NodeArea nodeArea;
};

template<typename T>
struct BaseNode {
    BaseNode(nodes::NodeState &&s) : state(std::move(s)) {}
    nodes::NodeState state;

    void draw(nodes::NodeArea &) {}
};

struct InputNode : public BaseNode<InputNode> {
    using BaseNode::BaseNode;

    constexpr static char *name = "Input";

    DataType outputType = DataType::Float1;
    std::array<float, 4> floats = {};

    void draw(nodes::NodeArea &area) {
        if (area.BeginNode(state)) {
            ImGui::Text("%s (%d)", name, state.id);

            area.BeginSlot(state);
            ImGui::Combo("Type", (int*)&outputType, "float\0float2\0float3\0float4\0\0");
            ImGui::Spacing();
            switch (outputType) {
            case DataType::Float1: ImGui::InputFloat("Float", floats.data()); break;
            case DataType::Float2: ImGui::InputFloat2("Float2", floats.data()); break;
            case DataType::Float3: ImGui::InputFloat3("Float3", floats.data()); break;
            case DataType::Float4: ImGui::InputFloat4("Float4", floats.data()); break;
            }

            area.EndSlot(state);

            area.BeginSlot(state);
            ImGui::Text("Value");
            area.EndSlot(state, -1, (int)outputType);
        }
        area.EndNode(state);
    };
};

struct OutputNode : public BaseNode<OutputNode> {
    using BaseNode::BaseNode;

    constexpr static char *name = "Output";

    void draw(nodes::NodeArea &area) {
        if (area.BeginNode(state)) {
            ImGui::Text("%s (%d)", name, state.id);

            area.BeginSlot(state);
            ImGui::Text("Color");
            area.EndSlot(state, (int)DataType::Float3);

            area.BeginSlot(state);
            ImGui::Text("Alpha");
            area.EndSlot(state, (int)DataType::Float1);
        }
        area.EndNode(state);
    }
};

struct Combine : public BaseNode<Combine> {
    using BaseNode::BaseNode;

    constexpr static char *name = "Combine";

    int numInputs = 1;

    void draw(nodes::NodeArea &area) {
        if (area.BeginNode(state)) {
            ImGui::Text("%s (%d)", name, state.id);

            area.BeginSlot(state);
            ImGui::SliderInt("#", &numInputs, 1, 4);
            area.EndSlot(state);

            for (int i = 0; i < numInputs; ++i) {
                area.BeginSlot(state);
                ImGui::Text("Channel %d", i);
                area.EndSlot(state, (int)DataType::Float1);
            }

            area.BeginSlot(state);
            ImGui::Text("Combined");
            area.EndSlot(state, -1, (int)DataType::Float4);
        }
        area.EndNode(state);
    }
};

struct ConstantColor : public BaseNode<ConstantColor> {
    using BaseNode::BaseNode;

    constexpr static char *name = "Constant Color";

    float color[4] = {1.f, 0.f, 0.f, 1.f};

    void draw(nodes::NodeArea &area) {
        if (area.BeginNode(state)) {
            ImGui::Text("%s (%d)", name, state.id);

            area.BeginSlot(state);
            ImGui::ColorPicker4("Color", color);
            area.EndSlot(state);

            area.BeginSlot(state);
            ImGui::Text("Output Color");
            area.EndSlot(state, -1, (int)DataType::Float4);
        }
        area.EndNode(state);
    }
};

struct CodeNode : public BaseNode<CodeNode> {
    using BaseNode::BaseNode;

    constexpr static char *name = "Code Node";

    char code[1024*16] = "function(float4 input, int _) {\n  return input * 4.0;\n }\n";

    void draw(nodes::NodeArea &area) {
        if (area.BeginNode(state)) {
            ImGui::Text("%s (%d)", name, state.id);

            area.BeginSlot(state);
            ImGui::Text("Input -> Code -> Output");
            area.EndSlot(state, (int)DataType::Float4, (int)DataType::Float4);

            area.BeginSlot(state);
            ImGui::Text("Code");
            ImGui::InputTextMultiline("##source", code, IM_ARRAYSIZE(code), ImVec2(300.0f, ImGui::GetTextLineHeight() * 16), ImGuiInputTextFlags_AllowTabInput);
            area.EndSlot(state);
        }
        area.EndNode(state, true);
    }
};

struct Recursion : public BaseNode<Recursion> {
    using BaseNode::BaseNode;

    constexpr static char *name = "Recursion";

    Graph<std::monostate, InputNode, OutputNode, Combine, Recursion, ConstantColor, CodeNode> innerGraph;

    void draw(nodes::NodeArea &area) {
        if (area.BeginNode(state)) {
            ImGui::Text("%s (%d)", name, state.id);

            area.BeginSlot(state);
            ImGui::Text("Input");
            area.EndSlot(state, (int)DataType::Float4);

            area.BeginSlot(state);
            ImGui::Text("Putput");
            ImGui::Spacing();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, ImU32(ImColor(60, 60, 70, 200)));
            ImGui::PushID(this);
            ImGui::BeginChild("border2", ImVec2(200, 150), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
            innerGraph.draw(false);
            ImGui::EndChild();
            ImGui::PopID();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
            area.EndSlot(state);

            area.BeginSlot(state);
            ImGui::Text("Output");
            area.EndSlot(state, -1, (int)DataType::Float4);
        }
        area.EndNode(state, true);
    }
};

Graph<std::monostate, InputNode, OutputNode, Combine, Recursion, ConstantColor, CodeNode> graph;

void AddSomeNodes()
{
    ImVec2 offset(10000.f, 10000.f);
    graph.nodes.emplace_back(InputNode{ nodes::NodeState((int)graph.nodes.size(), ImVec2(-400, -200) + offset) });
    graph.nodes.emplace_back(Combine{ nodes::NodeState((int)graph.nodes.size(), ImVec2(-100, 0) + offset) });
    graph.nodes.emplace_back(OutputNode{ nodes::NodeState((int)graph.nodes.size(), ImVec2(300, 100) + offset) });

    graph.connections.emplace_back(Connection{ 0, 0, 1, 0 });
    graph.connections.emplace_back(Connection{ 1, 0, 2, 0 });
}

void imgui_nodes_example_window(bool updateStyle)
{
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiSetCond_FirstUseEver);
    ImGui::Begin("Node Editor Example");

    ImGui::Text("Just some space to test interaction");
    static char buf[256];
    ImGui::InputText("text input test", buf, IM_ARRAYSIZE(buf));

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, ImU32(ImColor(60, 60, 70, 200)));
    ImGui::BeginChild("nodes", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);
    ImGui::PopStyleVar(2);

    static bool initialized = false;
    if (!initialized) {
        AddSomeNodes();
        initialized = true;
    }

    // Display nodes
    graph.draw(updateStyle);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    ImGui::End();
}
