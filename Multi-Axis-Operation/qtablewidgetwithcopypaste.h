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

	void saveToFile(QString filename);
	void loadFromFile(QString filename, int skipRowsCnt);
	void setMinimumNumCols(int numCols) { minimumNumCols = numCols; }
	void setColumnAlignment(int col, int alignment);

private:
	int minimumNumCols;
	void copy();
	void paste();
	void performDelete();

protected:
	virtual void keyPressEvent(QKeyEvent * event);
};
