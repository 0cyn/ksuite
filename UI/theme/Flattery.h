//
// Created by vr1s on 5/8/23.
//

#include <ui/theme.h>

#ifndef KSUITE_FLATTERY_H
#define KSUITE_FLATTERY_H

static std::string flatteryJson = R":3uwu(
{
  "name": "Flattery - Dark",
  "author": "cynder ( github.com/cxnder | cynder.me )",
"version": "0.1.1",
"style": "Fusion",
"styleSheet": "
SidebarWidgetAndHeader, SidebarWidgetAndHeader QWidget, SidebarWidgetAndHeader QWidget QWidget, CrossReferenceTree, MainWindow QSplitter,
StatusBarWidget, QPlainTextEdit, FilteredView, FilterEdit
{
background-color: #24273a;
}

MainWindow QSplitter {
        padding-left: 1px;
}

MainWindow, AddressIndicator {
background-color: #24273a;
}

ViewFrame QWidget {
        background: #181926;
}

ViewFrame > QWidget {
margin-bottom: 20px;
}

MainWindow QToolButton {
        color: #ccc;
        border-radius: 0px;
        padding: 5px;
}
MainWindow QToolButton:hover {
color: #fff;
background: #11000011;
border-radius: 0px;
padding: 5px;
}

Sidebar {
opacity:1;
color:#dedede;
}
SidebarWidget {
border: 0;
}
SidebarWidget > QWidget {
margin-right: 3px;
margin-left: 3px;
border: 0;
}
SidebarWidget > QWidget > QWidget {
margin-right: 5px;
margin-left: 5px;
border: 0;
}

StringsView, StackView {
border: 0;
margin-left: 10px;
}

StackView {
padding-right: 10px;
}

SearchFilter {
padding-top: 10px;
padding-bottom: 10px;
padding-left: 10px;
}

SidebarHeaderTitle {
padding-left: 10px;
margin-top: 5px;
margin-bottom: 5px;
padding-top: 5px;
padding-bottom: 5px;
padding-right: 5px;
border-radius: 10px;
color: #b5b5b5;
}

SidebarWidget QWidget {
        margin-right: 2px;
}

/* Un-background-colors the tabs in the GlobalAreaWidget ;_; */
SplitTabWidget > QWidget > QWidget > QWidget {
background-color: transparent;
}

QScrollBar:vertical {
border: 0px solid #fff;
background-color: transparent;
width:14px;
margin: 0px 0px 0px 3px;
}
QScrollBar::handle:vertical:active {
min-height: 0px;
border: 5px solid transparent;
/* this keeps it at a minimum of 10px so it doesn't look ugly */
padding-top: 10px;
border-radius: 5px;
background-color: #ffffff;
}
QScrollBar::handle:vertical {
min-height: 0px;
border: 5px solid transparent;
padding-top: 10px;
border-radius: 5px;
background-color: #c3c3c3;
}
QScrollBar::add-line:vertical {
height: 0px;
subcontrol-position: bottom;
subcontrol-origin: margin;
}
QScrollBar::sub-line:vertical {
height: 0 px;
subcontrol-position: top;
subcontrol-origin: margin;
}
QScrollBar::handle {
min-height: 0px;
border: 9px solid transparent;
padding-top: 10px;
border-radius: 7px;
background-color: #c3c3c3;
}
QScrollBar::handle:active {
min-height: 0px;
border: 9px solid transparent;
padding-top: 10px;
border-radius: 7px;
background-color: #ffffff;
}
QScrollBar::add-line {
height: 0px;
subcontrol-position: bottom;
subcontrol-origin: margin;
}
QScrollBar::sub-line {
height: 0 px;
subcontrol-position: top;
subcontrol-origin: margin;
}
ComponentTreeView {
background-color: #24273a;
border: solid 0px transparent;
}
FilterEdit, TypeFilterEdit {
border: solid 0px transparent;
color: #8b8b8b;
}
LogView QLineEdit {
        background-color: #24273a;
        margin-top: 5px;
        margin-bottom: 5px;
        padding-left: 10px;
        border-radius: 10px;
}
LogView QComboBox {
        background-color: #24273a;
        border: solid 0px transparent;
}
/* Horrible hack to create a downward arrow without a border that actually works on macOS */
LogView QComboBox::drop-down:down-arrow{
width: 0px;
height: 0px;
margin-top: 8px;
border-left: 5px solid #24273a;
border-right: 5px solid #24273a;
border-top: 5px solid white;
background-color:black;
}
LogView QListView {
        background-color: #131313;
}
QTreeView QHeaderView::section {
        background-color: #24273a;
        border: solid 0px transparent;
        padding-left: 15px;
}
QTableView QHeaderView::section {
        background-color: #24273a;
        border: solid 0px transparent;
}
Pane QStackedWidget {
        /*margin-right: 20px;*/
}
PaneHeader > QWidget {
margin-bottom: 10px;
margin-top: 10px;
margin-right: 10px;
}

StickyHeader {
margin-top:20px;
}
/* Linear/Graph View right-side, then Log/Console view right-side*/
ViewFrame QWidget, GlobalArea QWidget {
margin-right: 10px;
}
/* Exit button in GlobalArea */
GlobalArea CloseButton {
        margin-right: 20px;
        margin-bottom: 15px;
}
GlobalArea QListView {
        margin-bottom: 20px;
}
/* Un-do the greedy 20px margin from above in the console */
GlobalArea QListView, GlobalArea ScriptingConsoleOutput {
margin-right: 0px;
}
GlobalArea QComboBox {
        margin-right: 5px;
}

LogView ClickableIcon {
        margin-right: 10px;
}

ScriptingConsole QLabel {
        margin-right: 0px;
}

ScriptingConsoleOutput {
background-color: #1e2030;
}

ScriptingConsoleEdit {
background-color: #24273a;
margin-left: 0px;
border: 0;
}

NewTabWidget QWidget {
        background-color: transparent;
}
NewTabWidget QWidget {
        border: solid 0px transparent;
}
NewTabWidget QWidget QWidget {
border: solid 0px transparent;
color: #c7c7c7;
}

SettingsView QWidget {
        background-color: #24273a;
        border: solid 1px #24273a;
}
SettingsTreeView QWidget QWidget {
background-color: #2a2a2a;
}
SettingsTreeView QWidget {
        background-color: #2a2a2a;
}
SettingsTreeView QTextEdit {
        background-color: #1e2030;
}
SettingsScopeBar QWidget {
        margin-left: 10px;
}
MemoryMapSidebarWidget QWidget {
        border: 1px transparent;
}

/* literally cant do anything other than adjust
  padding, if you modify border or color in
  any way the selected state no longer
  has attributes */
QPushButton {
padding-left: 10px;
padding-right: 10px;
padding-top: 5px;
padding-bottom: 5px;
}

#remoteBrowser #textBrowser, #remoteBrowser #projectList, #remoteBrowser #remoteFileTree {
background-color: #24273a;
margin-top: 5px;
padding-left: 5px;
border: solid 0px transparent;
}
#remoteBrowser #textBrowser
{
background-color: #24273a;
margin-top: 10px;
padding-left: 4px;
border: solid 0px transparent;
}

#remoteBrowser #projectList
{
margin-left:1px;
}
#remoteBrowser #remoteInfoLabel
{
margin-top: 10px;
margin-left: 5px;
}
#remoteBrowser #remoteNameLabel, #remoteBrowser #remoteURLLabel {
margin-top: 10px;
}
#remoteBrowser #refreshButton
{
margin-left: 8px;
margin-right: 3px;
}
#remoteBrowser #refreshButton, #remoteBrowser #remoteButton
{
margin-top: 5px;
}

#remoteBrowser QToolButton
{
background-color: #292929;
padding-right: 10px;
padding-left: 10px;
}
#remoteBrowser #projectActionsButton, #remoteBrowser #fileActionsButton
{
padding-right: 14px;
}
#remoteBrowser QLabel#infoTitle
{
/* CANNOT use margin here due to Qt Css junk*/
padding-top: 3px;
}

/* <- this is here so I can edit the stylesheet as CSS in vscode with minimal red squiggles
",
"colors": {
        "base": [36, 39, 58],
        "text": [202, 211, 245],
        "sky": [145, 215, 227],
        "green": [166, 218, 149],
        "yellow": [238, 212, 159],
        "red": [237, 135, 150],
        "rosewater": [244, 219, 214],
        "lavender": [183, 189, 248],
        "blue": [138, 173, 244],
        "surface0": [54, 58, 79],
        "surface2": [91, 96, 120],
        "mantle": [30, 32, 48],
        "crust": [24, 25, 38]
    },

    "palette": {
        "Window":          "base",
        "WindowText":      "text",
        "Base":            "base",
        "AlternateBase":   "surface0",
        "ToolTipBase":     "base",
        "ToolTipText":     "surface2",
        "Text":            "text",
        "Button":          "base",
        "ButtonText":      "surface2",
        "BrightText":      "surface2",
        "Link":            "rosewater",
        "Highlight":       "sky",
        "HighlightedText": "base",
        "Light":           "surface2"
    },

    "theme-colors": {
        "addressColor":                         "green",
        "modifiedColor":                        "red",
        "insertedColor":                        "sky",
        "notPresentColor":                      "sky",
        "selectionColor":                       "surface2",
        "outlineColor":                         "green",
        "backgroundHighlightDarkColor":         "base",
        "backgroundHighlightLightColor":        "base",
        "boldBackgroundHighlightDarkColor":     "blue",
        "boldBackgroundHighlightLightColor":    "red",
        "alphanumericHighlightColor":           "sky",
        "printableHighlightColor":              "yellow",
        "graphBackgroundDarkColor":             "crust",
        "graphBackgroundLightColor":            "mantle",
        "graphNodeDarkColor":                   "base",
        "graphNodeLightColor":                  "base",
        "graphNodeOutlineColor":                "surface2",
        "trueBranchColor":                      "green",
        "falseBranchColor":                     "red",
        "unconditionalBranchColor":             "sky",
        "altTrueBranchColor":                   "sky",
        "altFalseBranchColor":                  "red",
        "altUnconditionalBranchColor":          "sky",
        "registerColor":                        "red",
        "numberColor":                          "yellow",
        "codeSymbolColor":                      "green",
        "dataSymbolColor":                      "lavender",
        "stackVariableColor":                   ["+", "text", "sky"],
        "importColor":                          ["+", "text","green"],
        "instructionHighlightColor":            ["~", "surface2", "base", 50],
        "tokenHighlightColor":                  "lavender",
        "annotationColor":                      "text",
        "opcodeColor":                          "surface2",
        "linearDisassemblyFunctionHeaderColor": "mantle",
        "linearDisassemblyBlockColor":          "mantle",
        "linearDisassemblyNoteColor":           "base",
        "linearDisassemblySeparatorColor":      "surface2",
        "stringColor":                          "yellow",
        "typeNameColor":                        "sky",
        "fieldNameColor":                       "blue",
        "keywordColor":                         "green",
        "uncertainColor":                       "sky",
        "scriptConsoleOutputColor":             "text",
        "scriptConsoleErrorColor":              "red",
        "scriptConsoleEchoColor":               "green",
        "blueStandardHighlightColor":           "blue",
        "greenStandardHighlightColor":          "green",
        "cyanStandardHighlightColor":           "sky",
        "redStandardHighlightColor":            "red",
        "magentaStandardHighlightColor":        "lavender",
        "yellowStandardHighlightColor":         "yellow",
        "orangeStandardHighlightColor":         ["+", "yellow", "red"],
        "whiteStandardHighlightColor":          "text",
        "blackStandardHighlightColor":          "base"
    }
}
):3uwu";


#endif //KSUITE_FLATTERY_H
