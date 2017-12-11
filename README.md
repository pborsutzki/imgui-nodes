Nodes for Dear ImGui
====================

This project wants to be a generic node based graph editor addon for [Dear ImGui](https://github.com/ocornut/imgui/) based projects.
It tries to use ImGui paradigmas where possible by keeping or duplicating as little state as possible. Like in ImGui the application is responsible for holding and updating its own data model - a node graph. Thus the library offers facilites to display, interact and manipulate such a node graph but does not store its state.

*This project is work in progress. Test carefully if you want to use it in productive software. Also note that the API is not final yet - be prepared for changes without preceding deprecation.*

Mouse Bindings
--------------
* Node Editor Background:
  * LMB: Deselect all nodes/connectors
  * Drag LMB: Add nodes/connectors touching the dragged box to the selection
  * ALT+Drag LMB: Add nodes/connectors enclosed in the dragged box to the selection
  * SHIFT+Drag LMB: Remove nodes/connectors touching the dragged box from the selection
  * SHIFT+ALT+Drag LMB: Remove nodes/connectors enclosed in the dragged box from the selection
  * Scroll Wheel: Zoom in/Zoom out at current cursor position
  * *RMB: Opens context menu for adding new nodes (in the sample)*
* Node body:
  * LMB: Select node and deselect all other nodes/connectors
  * CTRL+LMB: Toggle selection
  * Drag LMB: Add connector
* Connector:
  * LMB: Select connector and deselect all other nodes/connectors
* Connector socket:
  * Drag LMB: Add connector
  * CTRL+LMB: Toggle selection

Key Bindings
------------
* ESC: Cancel actions like adding a connector or selecting nodes
* HOME: Centers the node editor view and resets the zoom level
* *CTRL+A: Selects all nodes and connectors (in the sample)*
* *DEL: Deletes selected nodes and connectors (in the sample)*

Notes
-----
* The Nodes library itself requires C++11 to build, the sample currently requires C++17.
* Currently there are only project files for Microsoft Visual Studio 2017.
* Even though I tried to require no ImGui changes, I recommend to merge one change into ImGui: [The last commit of this branch](https://github.com/thedmd/imgui/tree/2016-09-fringe-scale) from the GitHub user `thedmd`. It adds fringe scaling to ImGui which reduces blurring, especially when zoomed in but also counteracts small lines when zoomed out. After merging the change add the define `IMGUI_ANTIALIASFRINGESCALE` to your build environment.
* The project is using some ImGui internals which can change at any time. Things might not work out of the box if you use a different ImGui version than the one referenced here.
* It is recommended to oversample the font loaded into imgui by the maximum amount of pixels (8) for best results of zoomed text. Example:
  ```
  ImGuiIO& io = ImGui::GetIO();

  ImFontConfig fontcfg;
  fontcfg.OversampleH = 8;
  fontcfg.OversampleV = 8;
  io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\SegoeUI.ttf", 18.0f, &fontcfg);
  ```
