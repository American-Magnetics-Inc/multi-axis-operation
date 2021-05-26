#pragma once

#include <QtWidgets/QTableWidget>

class QTableWidgetWithCopyPaste : public QTableWidget
{
public:
	QTableWidgetWithCopyPaste(int rows, int columns, QWidget *parent) :
	  QTableWidget(rows, columns, parent)
	  {minimumNumCols = 1;};

	  QTableWidgetWithCopyPaste(QWidget *parent) :
	  QTableWidget(parent)
	  {minimumNumCols = 1;};

	void saveToFile(QString filename, bool append);
	void loadFromFile(QString filename, int skipRowsCnt);
	void setMinimumNumCols(int numCols) { minimumNumCols = numCols; }
	int getMinimumNumCols(void) { return minimumNumCols; }
	void setColumnAlignment(int col, int alignment);

private:
	int minimumNumCols;
	void copy();
	void paste();
	void performDelete();

protected:
	virtual void keyPressEvent(QKeyEvent * event);
};
