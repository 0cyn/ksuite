//
// Created by serket on 7/18/23.
//


#include "ThemeEditor.h"
#include "CSSBuilder.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include "ui/theme.h"
#include <QApplication>
#include <QWindow>

SelectColorButton::SelectColorButton( QWidget* parent )
		: QPushButton(parent)
{
	connect( this, SIGNAL(clicked()), this, SLOT(changeColor()) );
}

void SelectColorButton::updateColor()
{
	setStyleSheet( "background-color: " + color.name() );
}

void SelectColorButton::changeColor()
{
	QColor newColor = QColorDialog::getColor(color, parentWidget());
	if ( newColor != color )
	{
		setColor( newColor );
		emit colorChanged(newColor);
	}
}

void SelectColorButton::setColor( const QColor& color )
{
	this->color = color;
	updateColor();
}

const QColor& SelectColorButton::getColor() const
{
	return color;
}

rapidjson::Value& r(std::string k, rapidjson::CrtAllocator& allocator) // the r stands for really stupid thanks rapidjson
{
	auto n = new rapidjson::Value(k.c_str(), allocator);
	return *n;
}

std::string ThemeEditor::GenerateStyleSheet()
{
	CSSBuilder builder;

	// 🧘🏻

	builder.CreateNewItem()
			->AddSelector("ViewFrame QWidget")

			//crust
			->AddRule("background", m_settings->value("window").value<QColor>().name().toStdString());

	builder.CreateNewItem()
			->AddSelector("SidebarWidgetAndHeader")
			->AddSelector("SidebarWidgetAndHeader QWidget")
			->AddSelector("SidebarWidgetAndHeader QWidget QWidget")
			->AddSelector("CrossReferenceTree")
			->AddSelector("MainWindow QSplitter")
			->AddSelector("StatusBarWidget")
			->AddSelector("QPlainTextEdit")
			->AddSelector("FilteredView")
			->AddSelector("FilterEdit")
			->AddSelector("MainWindow")
			->AddSelector("AddressIndicator")
			->AddSelector("ComponentTreeView")
			->AddSelector("LogView QLineEdit")
			->AddSelector("LogView QComboBox")
			->AddSelector("QTreeView QHeaderView::section")
			->AddSelector("QTableView QHeaderView::section")
			->AddSelector("ScriptingConsoleEdit")
			->AddSelector("SettingsView QWidget")
			->AddSelector("#remoteBrowser #textBrowser")
			->AddSelector("#remoteBrowser #projectList")
			->AddSelector("#remoteBrowser #remoteFileTree")
			->AddSelector("#remoteBrowser #textBrowser")

			//base
			->AddRule("background-color", m_settings->value("window").value<QColor>().name().toStdString());

	builder.CreateNewItem()
			->AddSelector("SettingsTreeView QWidget QWidget")
			->AddSelector("SettingsTreeView QWidget")
			// mantle
			->AddRule("background-color", "#2a2a2a");
	builder.CreateNewItem()
			->AddSelector("SettingsTreeView QTextEdit")
			->AddSelector("ScriptingConsoleOutput")
			//crust
			->AddRule("background-color", m_settings->value("window").value<QColor>().name().toStdString());

	builder.CreateNewItem()
			->AddSelector("#remoteBrowser QToolButton")
			->AddRule("background-color", "#292929")
			->AddRule("padding-right", "10px")
			->AddRule("padding-left", "10px");

	builder.CreateNewItem()
			->AddSelector("LogView QListView")
			->AddRule("background-color", "#131313");

	builder.CreateNewItem()
			->AddSelector("NewTabWidget QWidget")
			->AddRule("background-color", "transparent");

	builder.CreateNewItem()
			->AddSelector("SettingsView QWidget")
			//crust
			->AddRule("border", "solid 1px " + m_settings->value("window").value<QColor>().name().toStdString());

	builder.CreateNewItem()
			->AddSelector("ViewFrame > QWidget")

			->AddRule("margin-bottom", "20px");

	builder.CreateNewItem()
			->AddSelector("MainWindow QSplitter")

			->AddRule("padding-left", "1px");

	builder.CreateNewItem()
			->AddSelector("MainWindow QToolButton")

			->AddRule("color", "#ccc")
			->AddRule("border-radius", "0px")
			->AddRule("padding", "5px");

	builder.CreateNewItem()
			->AddSelector("MainWindow QToolButton:hover")

			->AddRule("color", "#fff")
			->AddRule("background", "#11000011")
			->AddRule("border-radius", "0px")
			->AddRule("padding", "5px");

	builder.CreateNewItem()
			->AddSelector("Sidebar")

			->AddRule("opacity", "1")
			->AddRule("color", "#dedede");


	builder.CreateNewItem()
			->AddSelector("SidebarWidget")

			->AddRule("border", "0");

	builder.CreateNewItem()
			->AddSelector("FilterEdit")
			->AddSelector("TypeFilterEdit")
			->AddRule("color", "#8b8b8b");

	builder.CreateNewItem()
			->AddSelector("NewTabWidget QWidget QWidget")
			->AddRule("color", "#c7c7c7");

	builder.CreateNewItem()
			->AddSelector("SettingsScopeBar QWidget")
			->AddRule("margin-left", "10px");

	builder.CreateNewItem()
			->AddSelector("MemoryMapSidebarWidget QWidget")
			->AddRule("border", "1px transparent");

	/* literally cant do anything other than adjust
	  padding, if you modify border or color in
	  any way the selected state no longer
	  has attributes */
	builder.CreateNewItem()
			->AddSelector("QPushButton")
			->AddRule("padding-left", "10px")
			->AddRule("padding-right", "10px")
			->AddRule("padding-top", "5px")
			->AddRule("padding-bottom", "5px");

	builder.CreateNewItem()
			->AddSelector("#remoteBrowser #textBrowser")
			->AddSelector("#remoteBrowser #projectList")
			->AddSelector("#remoteBrowser #remoteFileTree")
			->AddRule("margin-top", "5px")
			->AddRule("padding-left", "5px")
			->AddRule("border", "solid 0px transparent");

	builder.CreateNewItem()
			->AddSelector("#remoteBrowser #textBrowser")
			->AddRule("margin-top", "10px")
			->AddRule("padding-left", "4px")
			->AddRule("border", "solid 0px transparent");

	builder.CreateNewItem()
			->AddSelector("#remoteBrowser #projectList")
			->AddRule("margin-left", "1px");

	builder.CreateNewItem()
			->AddSelector("#remoteBrowser #remoteInfoLabel")
			->AddRule("margin-top", "10px")
			->AddRule("margin-left", "5px");

	builder.CreateNewItem()
			->AddSelector("#remoteBrowser #remoteNameLabel")
			->AddSelector("#remoteBrowser #remoteURLLabel")
			->AddRule("margin-top", "10px");

	builder.CreateNewItem()
			->AddSelector("#remoteBrowser #refreshButton")
			->AddRule("margin-left", "8px")
			->AddRule("margin-right", "3px");

	builder.CreateNewItem()
			->AddSelector("#remoteBrowser #refreshButton")
			->AddSelector("#remoteBrowser #remoteButton")
			->AddRule("margin-top", "5px");

	builder.CreateNewItem()
			->AddSelector("#remoteBrowser #projectActionsButton")
			->AddSelector("#remoteBrowser #fileActionsButton")
			->AddRule("padding-right", "14px");

	/* CANNOT use margin here due to Qt Css junk*/
	builder.CreateNewItem()
			->AddSelector("#remoteBrowser QLabel#infoTitle")
			->AddRule("padding-top", "3px");

	builder.CreateNewItem()
			->AddSelector("LogView QLineEdit")
			->AddRule("margin-top", "5px")
			->AddRule("margin-bottom", "5px")
			->AddRule("padding-left", "10px")
			->AddRule("border-radius", "10px");

	// Recreate the down arrow for the box
	builder.CreateNewItem()
			->AddSelector("LogView QComboBox::drop-down:down-arrow")
			->AddRule("width", "0px")
			->AddRule("height", "0px")
			->AddRule("margin-top", "8px")
			->AddRule("border-left", "5px solid #24273a")
			->AddRule("border-right", "5px solid #24273a")
			->AddRule("border-top", "5px solid white")
			->AddRule("background-color", "black");

	builder.CreateNewItem()
			->AddSelector("QTreeView QHeaderView::section")
			->AddRule("padding-left", "15px");

	builder.CreateNewItem()
			->AddSelector("PaneHeader > QWidget")
			->AddRule("margin-bottom", "10px")
			->AddRule("margin-top", "10px")
			->AddRule("margin-right", "10px");

	builder.CreateNewItem()
			->AddSelector("StickyHeader")
			->AddRule("margin-top", "20px");

	// Linear/Graph View right-side, then Log/Console view right-side
	builder.CreateNewItem()
			->AddSelector("ViewFrame QWidget")
			->AddSelector("GlobalArea QWidget")
			->AddRule("margin-right", "10px");

	builder.CreateNewItem()
			->AddSelector("GlobalArea CloseButton")
			->AddRule("margin-right", "20px")
			->AddRule("margin-bottom", "15px");

	builder.CreateNewItem()
			->AddSelector("GlobalArea QListView")
			->AddRule("margin-bottom", "20px");

	builder.CreateNewItem()
			->AddSelector("GlobalArea QListView")
			->AddSelector("GlobalArea ScriptingConsoleOutput")
			->AddRule("margin-bottom", "0px");

	builder.CreateNewItem()
			->AddSelector("GlobalArea QComboBox")
			->AddRule("margin-right", "5px");

	builder.CreateNewItem()
			->AddSelector("LogView ClickableIcon")
			->AddRule("margin-right", "10px");

	builder.CreateNewItem()
			->AddSelector("ScriptingConsole QLabel")
			->AddRule("margin-right", "0px");

	builder.CreateNewItem()
			->AddSelector("ScriptingConsoleEdit")
			->AddRule("margin-left", "0px")
			->AddRule("border", "0");

	builder.CreateNewItem()
			->AddSelector("SidebarWidget > QWidget")

			->AddRule("margin-right", "3px")
			->AddRule("margin-left", "3px")
			->AddRule("border", "0");

	builder.CreateNewItem()
			->AddSelector("SidebarWidget > QWidget > QWidget")
			->AddRule("margin-right", "5px")
			->AddRule("margin-left", "5px")
			->AddRule("border", "0");

	builder.CreateNewItem()
			->AddSelector("StringsView")
			->AddSelector("StackView")
			->AddRule("border", "0")
			->AddRule("margin-left", "10px");

	builder.CreateNewItem()
			->AddSelector("StackView")
			->AddRule("padding-right", "10px");

	builder.CreateNewItem()
			->AddSelector("SearchFilter")
			->AddRule("padding-top", "10px")
			->AddRule("padding-bottom", "10px")
			->AddRule("padding-left", "10px");

	builder.CreateNewItem()
			->AddSelector("SidebarHeaderTitle")
			->AddRule("padding-left", "10px")
			->AddRule("margin-top", "5px")
			->AddRule("margin-bottom", "5px")
			->AddRule("padding-top", "5px")
			->AddRule("padding-bottom", "5px")
			->AddRule("padding-right", "5px")
			->AddRule("border-radius", "10px")
			->AddRule("color", "#b5b5b5");

	builder.CreateNewItem()
			->AddSelector("SidebarWidget QWidget")
			->AddRule("margin-right", "2px");

	// Un-background-colors the tabs in the GlobalAreaWidget
	builder.CreateNewItem()
			->AddSelector("SplitTabWidget > QWidget > QWidget > QWidget")
			->AddRule("background-color", "transparent");

	builder.CreateNewItem()
			->AddSelector("QScrollBar:vertical")
			->AddRule("border", "0px solid #fff")
			->AddRule("background-color", "transparent")
			->AddRule("width", "14px")
			->AddRule("margin", "0px 0px 0px 3px");

	auto activeScrollBar = builder.CreateNewItem()
			->AddSelector("QScrollBar::handle:vertical:active")
			->AddRule("min-height", "0px")
			->AddRule("border", "5px solid transparent")
			->AddRule("padding-top", "10px")
			->AddRule("border-radius", "5px")
			->AddRule("background-color", "#ffffff");

	builder.CreateNewItem()
			->AddSelector("QScrollBar::handle:vertical")
			->CopyRules(activeScrollBar)
			->AddRule("background-color", "#c3c3c3");

	auto scrollBarButton = builder.CreateNewItem()
			->AddSelector("QScrollBar::add-line:vertical")
			->AddSelector("QScrollBar::add-line")
			->AddRule("height", "0px")
			->AddRule("subcontrol-position", "bottom")
			->AddRule("subcontrol-origin", "margin");

	builder.CreateNewItem()
			->AddSelector("QScrollBar::sub-line:vertical")
			->AddSelector("QScrollBar::sub-line")
			->CopyRules(scrollBarButton)
			->AddRule("subcontrol-position", "top");

	auto horHandle = builder.CreateNewItem()
			->AddSelector("QScrollBar::handle")
			->AddRule("min-height", "0px")
			->AddRule("border", "9px solid transparent")
			->AddRule("padding-top", "10px")
			->AddRule("border-radius", "7px")
			->AddRule("background-color", "#c3c3c3");

	builder.CreateNewItem()
			->AddSelector("QScrollBar::handle:active")
			->CopyRules(horHandle)
			->AddRule("background-color", "#ffffff");

	builder.CreateNewItem()
			->AddSelector("ComponentTreeView")
			->AddSelector("FilterEdit")
			->AddSelector("TypeFilterEdit")
			->AddSelector("LogView QComboBox")
			->AddSelector("QTreeView QHeaderView::section")
			->AddSelector("QTableView QHeaderView::section")
			->AddSelector("NewTabWidget QWidget")
			->AddSelector("NewTabWidget QWidget QWidget")
			->AddRule("border", "solid 0px transparent");


	return builder.Generate();
}

std::string ThemeEditor::GenerateThemeText()
{
	rapidjson::Document doc;
	doc.SetObject();
	auto a = doc.GetAllocator();
	doc.AddMember(r("name", a), r("KSUITE-INTERNALTHEME", a), a);
	doc.AddMember(r("author", a), r("cynder ( github.com/cxnder | cynder.me )", a) ,a);
	doc.AddMember(r("version", a), r("0.1.1", a),a);
	doc.AddMember(r("style", a), r("Fusion", a) ,a);
	doc.AddMember(r("styleSheet", a), r(GenerateStyleSheet(), a) ,a);

	rapidjson::Value colors(rapidjson::kObjectType);
	{
		colors.AddMember("base", m_settings->value("window").value<QColor>().name().toStdString(), doc.GetAllocator());
		colors.AddMember("text", m_settings->value("text").value<QColor>().name().toStdString(), doc.GetAllocator());
		colors.AddMember("sky", m_settings->value("highlight").value<QColor>().name().toStdString(), doc.GetAllocator());
		colors.AddMember("green", m_settings->value("text").value<QColor>().name().toStdString(), doc.GetAllocator());
		colors.AddMember("yellow", m_settings->value("text").value<QColor>().name().toStdString(), doc.GetAllocator());
		colors.AddMember("red", m_settings->value("text").value<QColor>().name().toStdString(), doc.GetAllocator());
		colors.AddMember("rosewater", m_settings->value("text").value<QColor>().name().toStdString(), doc.GetAllocator());
		colors.AddMember("lavender", m_settings->value("text").value<QColor>().name().toStdString(), doc.GetAllocator());
		colors.AddMember("blue", m_settings->value("text").value<QColor>().name().toStdString(), doc.GetAllocator());
		colors.AddMember("surface0", m_settings->value("alternatebase").value<QColor>().name().toStdString(), doc.GetAllocator());
		colors.AddMember("surface2", m_settings->value("tooltiptext").value<QColor>().name().toStdString(), doc.GetAllocator());
		colors.AddMember("mantle", m_settings->value("window").value<QColor>().name().toStdString(), doc.GetAllocator());
		colors.AddMember("crust", m_settings->value("window").value<QColor>().name().toStdString(), doc.GetAllocator());

		doc.AddMember("colors", colors, a);
	}

	rapidjson::Value palette(rapidjson::kObjectType);
	{
		palette.AddMember("Window", "base", a);
		palette.AddMember("WindowText", "text", a);
		palette.AddMember("Base", "base", a);
		palette.AddMember("AlternateBase", "surface0", a);
		palette.AddMember("ToolTipBase", "base", a);
		palette.AddMember("ToolTipText", "surface2", a);
		palette.AddMember("Text", "text", a);
		palette.AddMember("Button", "base", a);
		palette.AddMember("ButtonText", "surface2", a);
		palette.AddMember("BrightText", "surface2", a);
		palette.AddMember("Link", "rosewater", a);
		palette.AddMember("Highlight", "sky", a);
		palette.AddMember("HighlightedText", "base", a);
		palette.AddMember("Light", "surface2", a);

		doc.AddMember("palette", palette, a);
	}

	rapidjson::Value themeColors(rapidjson::kObjectType);
	{
		themeColors.AddMember("addressColor", "green", a);
		themeColors.AddMember("modifiedColor", "red", a);
		themeColors.AddMember("insertedColor", "sky", a);
		themeColors.AddMember("notPresentColor", "sky", a);
		themeColors.AddMember("selectionColor", "surface2", a);
		themeColors.AddMember("outlineColor", "green", a);
		themeColors.AddMember("backgroundHighlightDarkColor", "base", a);
		themeColors.AddMember("backgroundHighlightLightColor", "base", a);
		themeColors.AddMember("boldBackgroundHighlightDarkColor", "blue", a);
		themeColors.AddMember("boldBackgroundHighlightLightColor", "red", a);
		themeColors.AddMember("alphanumericHighlightColor", "sky", a);
		themeColors.AddMember("printableHighlightColor", "yellow", a);
		themeColors.AddMember("graphBackgroundDarkColor", "crust", a);
		themeColors.AddMember("graphBackgroundLightColor", "mantle", a);
		themeColors.AddMember("graphNodeDarkColor", "base", a);
		themeColors.AddMember("graphNodeLightColor", "base", a);
		themeColors.AddMember("graphNodeOutlineColor", "surface2", a);
		themeColors.AddMember("trueBranchColor", "green", a);
		themeColors.AddMember("falseBranchColor", "red", a);
		themeColors.AddMember("unconditionalBranchColor", "sky", a);
		themeColors.AddMember("altTrueBranchColor", "sky", a);
		themeColors.AddMember("altFalseBranchColor", "red", a);
		themeColors.AddMember("altUnconditionalBranchColor", "sky", a);
		themeColors.AddMember("registerColor", "red", a);
		themeColors.AddMember("numberColor", "yellow", a);
		themeColors.AddMember("codeSymbolColor", "green", a);
		themeColors.AddMember("dataSymbolColor", "lavender", a);
		themeColors.AddMember("stackVariableColor", "sky", a);
		themeColors.AddMember("importColor", "green", a);
		themeColors.AddMember("instructionHighlightColor", "surface2", a);
		themeColors.AddMember("tokenHighlightColor", "lavender", a);
		themeColors.AddMember("annotationColor", "text", a);
		themeColors.AddMember("opcodeColor", "surface2", a);
		themeColors.AddMember("linearDisassemblyFunctionHeaderColor", "mantle", a);
		themeColors.AddMember("linearDisassemblyBlockColor", "mantle", a);
		themeColors.AddMember("linearDisassemblyNoteColor", "base", a);
		themeColors.AddMember("linearDisassemblySeparatorColor", "surface2", a);
		themeColors.AddMember("stringColor", "yellow", a);
		themeColors.AddMember("typeNameColor", "sky", a);
		themeColors.AddMember("fieldNameColor", "blue", a);
		themeColors.AddMember("keywordColor", "green", a);
		themeColors.AddMember("uncertainColor", "sky", a);
		themeColors.AddMember("scriptConsoleOutputColor", "text", a);
		themeColors.AddMember("scriptConsoleErrorColor", "red", a);
		themeColors.AddMember("scriptConsoleEchoColor", "green", a);
		themeColors.AddMember("blueStandardHighlightColor", "blue", a);
		themeColors.AddMember("greenStandardHighlightColor", "green", a);
		themeColors.AddMember("cyanStandardHighlightColor", "sky", a);
		themeColors.AddMember("redStandardHighlightColor", "red", a);
		themeColors.AddMember("magentaStandardHighlightColor", "lavender", a);
		themeColors.AddMember("yellowStandardHighlightColor", "yellow", a);
		themeColors.AddMember("orangeStandardHighlightColor", "yellow", a);
		themeColors.AddMember("whiteStandardHighlightColor", "text", a);
		themeColors.AddMember("blackStandardHighlightColor", "base", a);

		doc.AddMember("theme-colors", themeColors, a);
	}

	rapidjson::StringBuffer strbuf;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(strbuf);
	doc.Accept(writer);

	std::string s = strbuf.GetString();
	return s;
}

ThemeEditor::ThemeEditor(QWidget* parent)
	: QDialog(parent)
{
	m_settings = new QSettings("ksuite.theme");
	if (!m_settings->contains("crust"))
	{
		QColor base(36, 39, 58);
		QColor text(202, 211, 245);
		QColor sky(145, 215, 227);
		QColor green(166, 218, 149);
		QColor yellow(238, 212, 159);
		QColor red(237, 135, 150);
		QColor rosewater(244, 219, 214);
		QColor lavender(183, 189, 248);
		QColor blue(138, 173, 244);
		QColor surface0(54, 58, 79);
		QColor surface2(91, 96, 120);
		QColor mantle(30, 32, 48);
		QColor crust(24, 25, 38);

		m_settings->setValue("window", base);
		m_settings->setValue("alternatebase", surface0);
		m_settings->setValue("text", text);
		m_settings->setValue("tooltiptext", surface2);
		m_settings->setValue("highlight", sky);
		m_settings->setValue("highlighttext", base);
		m_settings->setValue("link", rosewater);


	}

	setFixedWidth(500);
	setFixedHeight(300);

	auto lyt = new QHBoxLayout();
	setLayout(lyt);

	auto leftSideLayout = new QVBoxLayout();
	auto rightSideLayout = new QVBoxLayout();

	auto windowColorButton = new SelectColorButton(this);
	windowColorButton->setColor(m_settings->value("window").value<QColor>());
	connect(windowColorButton, &SelectColorButton::colorChanged, this, &ThemeEditor::SetWindowColor);
	leftSideLayout->addWidget(windowColorButton);

	auto textColorButton = new SelectColorButton(this);
	textColorButton->setColor(m_settings->value("text").value<QColor>());
	connect(textColorButton, &SelectColorButton::colorChanged, this, &ThemeEditor::SetTextColor);
	leftSideLayout->addWidget(textColorButton);

	auto alternateBase = new SelectColorButton(this);
	alternateBase->setColor(m_settings->value("alternatebase").value<QColor>());
	connect(alternateBase, &SelectColorButton::colorChanged, this, &ThemeEditor::SetAlternateBaseColor);
	leftSideLayout->addWidget(alternateBase);

	auto toolTipTextColorButton = new SelectColorButton(this);
	toolTipTextColorButton->setColor(m_settings->value("tooltiptext").value<QColor>());
	connect(toolTipTextColorButton, &SelectColorButton::colorChanged, this, &ThemeEditor::SetToolTipTextColor);
	leftSideLayout->addWidget(toolTipTextColorButton);

	lyt->addLayout(leftSideLayout);

	auto highlightColorButton = new SelectColorButton(this);
	highlightColorButton->setColor(m_settings->value("highlight").value<QColor>());
	connect(highlightColorButton, &SelectColorButton::colorChanged, this, &ThemeEditor::SetHighlightColor);
	rightSideLayout->addWidget(highlightColorButton);

	auto highlightTextColorButton = new SelectColorButton(this);
	highlightTextColorButton->setColor(m_settings->value("highlighttext").value<QColor>());
	connect(highlightTextColorButton, &SelectColorButton::colorChanged, this, &ThemeEditor::SetHighlightTextColor);
	rightSideLayout->addWidget(highlightTextColorButton);

	auto linkColorButton = new SelectColorButton(this);
	linkColorButton->setColor(m_settings->value("link").value<QColor>());
	connect(linkColorButton, &SelectColorButton::colorChanged, this, &ThemeEditor::SetLinkColor);
	rightSideLayout->addWidget(linkColorButton);

	auto lightColorButton = new SelectColorButton(this);
	lightColorButton->setColor(m_settings->value("light").value<QColor>());
	connect(lightColorButton, &SelectColorButton::colorChanged, this, &ThemeEditor::SetLightColor);
	rightSideLayout->addWidget(lightColorButton);

	lyt->addLayout(rightSideLayout);
}

void ThemeEditor::UpdateTheme()
{
	addJsonTheme(GenerateThemeText().c_str());
	refreshUserThemes();
	setActiveTheme("KSUITE-INTERNALTHEME");
	resetUserThemes();
	parentWidget()->windowHandle()->requestActivate();
}

void ThemeEditor::SetWindowColor(const QColor& color)
{
	m_settings->setValue("window", color);
	UpdateTheme();
}
void ThemeEditor::SetAlternateBaseColor(const QColor& color)
{
	m_settings->setValue("alternatebase", color);
	UpdateTheme();
}
void ThemeEditor::SetTextColor(const QColor& color)
{
	m_settings->setValue("text", color);
	UpdateTheme();
}
void ThemeEditor::SetToolTipTextColor(const QColor& color)
{
	m_settings->setValue("tooltiptext", color);
	UpdateTheme();
}
void ThemeEditor::SetHighlightColor(const QColor& color)
{
	m_settings->setValue("highlight", color);
	UpdateTheme();
}
void ThemeEditor::SetHighlightTextColor(const QColor& color)
{
	m_settings->setValue("highlighttext", color);
	UpdateTheme();
}
void ThemeEditor::SetLinkColor(const QColor& color)
{
	m_settings->setValue("link", color);
	UpdateTheme();
}
void ThemeEditor::SetLightColor(const QColor& color)
{
	m_settings->setValue("light", color);
	UpdateTheme();
}
