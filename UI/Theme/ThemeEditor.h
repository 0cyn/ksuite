//
// Created by serket on 7/18/23.
//

#include <QWidget>
#include <QDialog>
#include <QSettings>
#include <QPushButton>
#include <QColor>
#include <QColorDialog>
#include <QVBoxLayout>


#ifndef KSUITE_THEMEEDITOR_H
#define KSUITE_THEMEEDITOR_H

class SelectColorButton : public QPushButton
{
	Q_OBJECT
public:
	SelectColorButton( QWidget* parent );

	void setColor( const QColor& color );
	const QColor& getColor() const;

public slots:
	void updateColor();
	void changeColor();

signals:
	void colorChanged(const QColor& color);

private:
	QColor color;
};


class ThemeEditor : public QDialog {
    Q_OBJECT

	QSettings* m_settings;

	QColor m_windowColor;
	QColor m_alternateBaseColor;
	QColor m_textColor;
	QColor m_toolTipTextColor;
	QColor m_highlightColor;
	QColor m_highlightTextColor;
	QColor m_linkColor;
	QColor m_lightColor;

public:

	void UpdateTheme();

	ThemeEditor(QWidget* parent = nullptr);

	std::string GenerateStyleSheet();
	std::string GenerateThemeText();

	void SetWindowColor(const QColor& color);
	void SetAlternateBaseColor(const QColor& color);
	void SetTextColor(const QColor& color);
	void SetToolTipTextColor(const QColor& color);
	void SetHighlightColor(const QColor& color);
	void SetHighlightTextColor(const QColor& color);
	void SetLinkColor(const QColor& color);
	void SetLightColor(const QColor& color);


};


#endif //KSUITE_THEMEEDITOR_H
