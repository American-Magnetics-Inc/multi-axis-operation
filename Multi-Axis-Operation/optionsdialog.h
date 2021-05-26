#pragma once

#include <QDialog>
#include "ui_optionsdialog.h"

//---------------------------------------------------------------------------
class OptionsDialog : public QDialog
{
	Q_OBJECT

public:
	OptionsDialog(QWidget *parent = Q_NULLPTR);
	~OptionsDialog();

	bool enterPersistence(void) { return m_enterPersistence; }
	int settlingTime(void) { return m_settlingTime; }
	QString magnetDAQLocation(void) { return m_magnetDAQLocation; }
	bool magnetDAQMinimized(void) { return m_magnetDAQMinimized; }
	bool disableAutoStability(void) { return m_disableAutoStability; }

signals:
	void configChanged(void);

private slots:
	void buttonClicked(QAbstractButton*);
	void findMagnetDAQ(void);

private:
	Ui::OptionsDialog ui;

	// preferences
	bool m_enterPersistence;		// if true, check Enter Persistence on file imports by default
	int m_settlingTime;				// settling time after HOLDING reached until persistence is auto-entered
	QString m_magnetDAQLocation;	// location of Magnet-DAQ app bundle or executable
	bool m_magnetDAQMinimized;		// if true, launch Magnet-DAQ instances in minimized (shrunk to taskbar icon) state
	bool m_disableAutoStability;	// if true, any manual Stability Setting is preserved for all connected Model 430's

	void restoreSettings(void);
	void saveSettings(void);
	bool readSettingsFromDialog(void);
	void showError(QString errMsg);
};

