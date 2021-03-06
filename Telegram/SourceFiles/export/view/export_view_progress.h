/*
This file is part of Bettergram.

For license and copyright information please follow this link:
https://github.com/bettergram/bettergram/blob/master/LEGAL
*/
#pragma once

#include "ui/rp_widget.h"
#include "export/view/export_view_content.h"

namespace Ui {
class VerticalLayout;
class RoundButton;
class FlatLabel;
} // namespace Ui

namespace Export {
namespace View {

class ProgressWidget : public Ui::RpWidget {
public:
	ProgressWidget(
		QWidget *parent,
		rpl::producer<Content> content);

	rpl::producer<> cancelClicks() const;
	rpl::producer<> doneClicks() const;

	~ProgressWidget();

private:
	void setupBottomButton(not_null<Ui::RoundButton*> button);
	void updateState(Content &&content);
	void showDone();

	Content _content;

	class Row;
	object_ptr<Ui::VerticalLayout> _body;
	std::vector<not_null<Row*>> _rows;

	QPointer<Ui::FlatLabel> _about;
	base::unique_qptr<Ui::RoundButton> _cancel;
	base::unique_qptr<Ui::RoundButton> _done;
	rpl::event_stream<> _doneClicks;

};

} // namespace View
} // namespace Export
