// QTableWidget with support for copy and paste added
// Here copy and paste can copy/paste the entire grid of cells
#include "stdafx.h"
#include "qtablewidgetwithcopypaste.h"

//---------------------------------------------------------------------------
void QTableWidgetWithCopyPaste::copy()
{
	QItemSelectionModel * selection = selectionModel();
	QModelIndexList indexes = selection->selectedIndexes();

	if(indexes.size() < 1)
		return;

	// QModelIndex::operator < sorts first by row, then by column.
	// this is what we need
	std::sort(indexes.begin(), indexes.end());

	// You need a pair of indexes to find the row changes
	QModelIndex previous = indexes.first();
	indexes.removeFirst();
	QString selected_text;
	QModelIndex current;

	Q_FOREACH(current, indexes)
	{
		QVariant data = model()->data(previous);
		QString text = data.toString();

		// At this point "text" contains the text in one cell
		selected_text.append(text);

		// If you are at the start of the row the row number of the previous index
		// isn't the same.  Text is followed by a row separator, which is a newline.
		if (current.row() != previous.row())
		{
			selected_text.append(QLatin1Char('\n'));
		}
		// Otherwise it's the same row, so append a column separator, which is a comma.
		else
		{
			selected_text.append(QLatin1Char(','));
		}
		previous = current;
	}

	// add last element
	selected_text.append(model()->data(current).toString());
	selected_text.append(QLatin1Char('\n'));
	qApp->clipboard()->setText(selected_text);
}

//---------------------------------------------------------------------------
void QTableWidgetWithCopyPaste::paste()
{
	// TO DO
}

//---------------------------------------------------------------------------
void QTableWidgetWithCopyPaste::performDelete()
{
	QItemSelectionModel * selection = selectionModel();
	QModelIndexList indexes = selection->selectedIndexes();

	if(indexes.size() < 1)
		return;

	// QModelIndex::operator < sorts first by row, then by column.
	// this is what we need
	std::sort(indexes.begin(), indexes.end());

	QModelIndex current;

	Q_FOREACH(current, indexes)
	{
		QTableWidgetItem *cell = item(current.row(), current.column());

		if (cell)
			cell->setText("");
	}
}

//---------------------------------------------------------------------------
void QTableWidgetWithCopyPaste::keyPressEvent(QKeyEvent * event)
{
	if(event->matches(QKeySequence::Copy))
	{
		copy();
	}
	else if(event->matches(QKeySequence::Paste))
	{
		paste();
	}
	else if(event->matches(QKeySequence::Delete))
	{
		performDelete();
	}
	else
	{
		QTableWidget::keyPressEvent(event);
	}

}

//---------------------------------------------------------------------------
void QTableWidgetWithCopyPaste::saveToFile(QString filename)
{
	FILE *outFile;
	outFile = fopen(filename.toLocal8Bit(), "a");

	if (outFile != NULL)
	{
		QTextStream out(outFile);

		// output horizontal header titles
		for (int i = 0; i < columnCount(); i++)
		{
			QTableWidgetItem *cell = horizontalHeaderItem(i);

			if (i > 0)
				out << ',';

			if (cell)
				out << cell->text();
		}
		out << '\n';

		// output table data
		for (int i = 0; i < rowCount(); ++i) 
		{
			if (i > 0)
				out << '\n';
			for (int j = 0; j < columnCount(); ++j) 
			{
				if (j > 0)
					out << ',';

				QTableWidgetItem *cell = item(i, j);

				if (cell)
					out << cell->text();
			}
		}

		flush(out);
		fclose(outFile);
	}
}

//---------------------------------------------------------------------------
void QTableWidgetWithCopyPaste::loadFromFile(QString filename, bool makeCheckable, int skipCnt)
{
	FILE *inFile;
	inFile = fopen(filename.toLocal8Bit(), "r");

	// read in table data, skip horizontal header titles in row 1
	if (inFile != NULL)
	{
		QTextStream in(inFile);
		QString str = in.readAll();
		QStringList rows = str.split('\n');

#ifdef DEBUG
		qDebug() << "Read in file";
#endif
		int numRows = rows.count();
		int numColumns = rows.at(1).count(',') + 1;

		// is last line in file empty?
		if (rows.last().isEmpty())
			numRows--;	// skip it

		setRowCount(numRows - skipCnt);
		qDebug() << "Set row count = " + QString::number(numRows - skipCnt);

		if (numColumns < minimumNumCols)
		{
			setColumnCount(minimumNumCols);
#ifdef DEBUG
			qDebug() << "Set col count = " + QString::number(minimumNumCols);
#endif			
			numColumns = minimumNumCols;
		}
		else
		{
			setColumnCount(numColumns);
#ifdef DEBUG
			qDebug() << "Set col count = " + QString::number(numColumns);
#endif		
		}

		int cellRow = 0;

		for (int i = skipCnt; i < numRows; ++i) 
		{
			if (rows[i].length() > 0)
			{
				QStringList columns = rows[i].split(',');

				for (int j = 0; j < numColumns; ++j)
				{
					if (j >= columns.count())
					{
						QTableWidgetItem *cell = item(cellRow, j);

						if (cell == NULL)
							setItem(cellRow, j, cell = new QTableWidgetItem(""));
						else
							cell->setText("");

						cell->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
					}
					else
					{
						QTableWidgetItem *cell = item(cellRow, j);
						QString cellStr = columns[j];

						if (cellStr.isEmpty())
							cellStr = "";

						if (cell == NULL)
							setItem(cellRow, j, cell = new QTableWidgetItem(cellStr));
						else
							cell->setText(cellStr);

						if (j == (columns.count() - 1))
							cell->setTextAlignment(Qt::AlignCenter);
						else
							cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

#ifdef DEBUG
						qDebug() << "Set text for item = " + QString::number(cellRow) + "," + QString::number(j);
#endif
						if (j == 0 && makeCheckable)
						{
							if (cell->text().length() > 0)
								cell->setCheckState(Qt::Checked);
							else
								cell->setCheckState(Qt::Unchecked);
						}
					}
				}
				
				cellRow++;
			}
		}

		fclose(inFile);

#ifdef DEBUG
		qDebug() << "Closed file";
#endif
	}
}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------