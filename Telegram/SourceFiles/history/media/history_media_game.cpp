/*
This file is part of Bettergram.

For license and copyright information please follow this link:
https://github.com/bettergram/bettergram/blob/master/LEGAL
*/
#include "history/media/history_media_game.h"

#include "lang/lang_keys.h"
#include "layout.h"
#include "history/history_item_components.h"
#include "history/history.h"
#include "history/view/history_view_element.h"
#include "history/view/history_view_cursor_state.h"
#include "history/media/history_media_common.h"
#include "ui/text_options.h"
#include "data/data_session.h"
#include "data/data_game.h"
#include "data/data_media_types.h"
#include "styles/style_history.h"

namespace {

using TextState = HistoryView::TextState;

} // namespace

HistoryGame::HistoryGame(
	not_null<Element*> parent,
	not_null<GameData*> data,
	const TextWithEntities &consumed)
: HistoryMedia(parent)
, _data(data)
, _title(st::msgMinWidth - st::webPageLeft)
, _description(st::msgMinWidth - st::webPageLeft) {
	if (!consumed.text.isEmpty()) {
		_description.setMarkedText(
			st::webPageDescriptionStyle,
			consumed,
			Ui::ItemTextOptions(parent->data()));
	}
	history()->owner().registerGameView(_data, _parent);
}

QSize HistoryGame::countOptimalSize() {
	auto lineHeight = unitedLineHeight();

	const auto item = _parent->data();
	if (!_openl && IsServerMsgId(item->id)) {
		const auto row = 0;
		const auto column = 0;
		_openl = std::make_shared<ReplyMarkupClickHandler>(
			row,
			column,
			item->fullId());
	}

	auto title = TextUtilities::SingleLine(_data->title);

	// init attach
	if (!_attach) {
		_attach = CreateAttach(_parent, _data->document, _data->photo);
	}

	// init strings
	if (_description.isEmpty() && !_data->description.isEmpty()) {
		auto text = _data->description;
		if (!text.isEmpty()) {
			if (!_attach) {
				text += _parent->skipBlock();
			}
			auto marked = TextWithEntities { text };
			auto parseFlags = TextParseLinks | TextParseMultiline | TextParseRichText;
			TextUtilities::ParseEntities(marked, parseFlags);
			_description.setMarkedText(
				st::webPageDescriptionStyle,
				marked,
				Ui::WebpageTextDescriptionOptions());
		}
	}
	if (_title.isEmpty() && !title.isEmpty()) {
		_title.setText(
			st::webPageTitleStyle,
			title,
			Ui::WebpageTextTitleOptions());
	}

	// init dimensions
	auto l = st::msgPadding.left() + st::webPageLeft, r = st::msgPadding.right();
	auto skipBlockWidth = _parent->skipBlockWidth();
	auto maxWidth = skipBlockWidth;
	auto minHeight = 0;

	auto titleMinHeight = _title.isEmpty() ? 0 : lineHeight;
	// enable any count of lines in game description / message
	auto descMaxLines = 4096;
	auto descriptionMinHeight = _description.isEmpty() ? 0 : qMin(_description.minHeight(), descMaxLines * lineHeight);

	if (!_title.isEmpty()) {
		accumulate_max(maxWidth, _title.maxWidth());
		minHeight += titleMinHeight;
	}
	if (!_description.isEmpty()) {
		accumulate_max(maxWidth, _description.maxWidth());
		minHeight += descriptionMinHeight;
	}
	if (_attach) {
		auto attachAtTop = !_titleLines && !_descriptionLines;
		if (!attachAtTop) minHeight += st::mediaInBubbleSkip;

		_attach->initDimensions();
		QMargins bubble(_attach->bubbleMargins());
		auto maxMediaWidth = _attach->maxWidth() - bubble.left() - bubble.right();
		if (isBubbleBottom() && _attach->customInfoLayout()) {
			maxMediaWidth += skipBlockWidth;
		}
		accumulate_max(maxWidth, maxMediaWidth);
		minHeight += _attach->minHeight() - bubble.top() - bubble.bottom();
	}
	maxWidth += st::msgPadding.left() + st::webPageLeft + st::msgPadding.right();
	auto padding = inBubblePadding();
	minHeight += padding.top() + padding.bottom();

	if (!_gameTagWidth) {
		_gameTagWidth = st::msgDateFont->width(lang(lng_game_tag).toUpper());
	}
	return { maxWidth, minHeight };
}

void HistoryGame::refreshParentId(not_null<HistoryItem*> realParent) {
	if (_openl) {
		_openl->setMessageId(realParent->fullId());
	}
	if (_attach) {
		_attach->refreshParentId(realParent);
	}
}

QSize HistoryGame::countCurrentSize(int newWidth) {
	accumulate_min(newWidth, maxWidth());
	auto innerWidth = newWidth - st::msgPadding.left() - st::webPageLeft - st::msgPadding.right();

	// enable any count of lines in game description / message
	auto linesMax = 4096;
	auto lineHeight = unitedLineHeight();
	auto newHeight = 0;
	if (_title.isEmpty()) {
		_titleLines = 0;
	} else {
		if (_title.countHeight(innerWidth) < 2 * st::webPageTitleFont->height) {
			_titleLines = 1;
		} else {
			_titleLines = 2;
		}
		newHeight += _titleLines * lineHeight;
	}

	if (_description.isEmpty()) {
		_descriptionLines = 0;
	} else {
		auto descriptionHeight = _description.countHeight(innerWidth);
		if (descriptionHeight < (linesMax - _titleLines) * st::webPageDescriptionFont->height) {
			_descriptionLines = (descriptionHeight / st::webPageDescriptionFont->height);
		} else {
			_descriptionLines = (linesMax - _titleLines);
		}
		newHeight += _descriptionLines * lineHeight;
	}

	if (_attach) {
		auto attachAtTop = !_titleLines && !_descriptionLines;
		if (!attachAtTop) newHeight += st::mediaInBubbleSkip;

		QMargins bubble(_attach->bubbleMargins());

		_attach->resizeGetHeight(innerWidth + bubble.left() + bubble.right());
		newHeight += _attach->height() - bubble.top() - bubble.bottom();
		if (isBubbleBottom() && _attach->customInfoLayout() && _attach->width() + _parent->skipBlockWidth() > innerWidth + bubble.left() + bubble.right()) {
			newHeight += bottomInfoPadding();
		}
	}
	auto padding = inBubblePadding();
	newHeight += padding.top() + padding.bottom();

	return { newWidth, newHeight };
}

TextSelection HistoryGame::toDescriptionSelection(
		TextSelection selection) const {
	return HistoryView::UnshiftItemSelection(selection, _title);
}

TextSelection HistoryGame::fromDescriptionSelection(
		TextSelection selection) const {
	return HistoryView::ShiftItemSelection(selection, _title);
}

void HistoryGame::draw(Painter &p, const QRect &r, TextSelection selection, crl::time ms) const {
	if (width() < st::msgPadding.left() + st::msgPadding.right() + 1) return;
	auto paintw = width(), painth = height();

	auto outbg = _parent->hasOutLayout();
	bool selected = (selection == FullSelection);

	auto &barfg = selected ? (outbg ? st::msgOutReplyBarSelColor : st::msgInReplyBarSelColor) : (outbg ? st::msgOutReplyBarColor : st::msgInReplyBarColor);
	auto &semibold = selected ? (outbg ? st::msgOutServiceFgSelected : st::msgInServiceFgSelected) : (outbg ? st::msgOutServiceFg : st::msgInServiceFg);
	auto &regular = selected ? (outbg ? st::msgOutDateFgSelected : st::msgInDateFgSelected) : (outbg ? st::msgOutDateFg : st::msgInDateFg);

	QMargins bubble(_attach ? _attach->bubbleMargins() : QMargins());
	auto padding = inBubblePadding();
	auto tshift = padding.top();
	auto bshift = padding.bottom();
	paintw -= padding.left() + padding.right();
	if (isBubbleBottom() && _attach && _attach->customInfoLayout() && _attach->width() + _parent->skipBlockWidth() > paintw + bubble.left() + bubble.right()) {
		bshift += bottomInfoPadding();
	}

	QRect bar(rtlrect(st::msgPadding.left(), tshift, st::webPageBar, height() - tshift - bshift, width()));
	p.fillRect(bar, barfg);

	auto lineHeight = unitedLineHeight();
	if (_titleLines) {
		p.setPen(semibold);
		auto endskip = 0;
		if (_title.hasSkipBlock()) {
			endskip = _parent->skipBlockWidth();
		}
		_title.drawLeftElided(p, padding.left(), tshift, paintw, width(), _titleLines, style::al_left, 0, -1, endskip, false, selection);
		tshift += _titleLines * lineHeight;
	}
	if (_descriptionLines) {
		p.setPen(outbg ? st::webPageDescriptionOutFg : st::webPageDescriptionInFg);
		auto endskip = 0;
		if (_description.hasSkipBlock()) {
			endskip = _parent->skipBlockWidth();
		}
		_description.drawLeftElided(p, padding.left(), tshift, paintw, width(), _descriptionLines, style::al_left, 0, -1, endskip, false, toDescriptionSelection(selection));
		tshift += _descriptionLines * lineHeight;
	}
	if (_attach) {
		auto attachAtTop = !_titleLines && !_descriptionLines;
		if (!attachAtTop) tshift += st::mediaInBubbleSkip;

		auto attachLeft = padding.left() - bubble.left();
		auto attachTop = tshift - bubble.top();
		if (rtl()) attachLeft = width() - attachLeft - _attach->width();

		auto attachSelection = selected ? FullSelection : TextSelection { 0, 0 };

		p.translate(attachLeft, attachTop);
		_attach->draw(p, r.translated(-attachLeft, -attachTop), attachSelection, ms);
		auto pixwidth = _attach->width();
		auto pixheight = _attach->height();

		auto gameW = _gameTagWidth + 2 * st::msgDateImgPadding.x();
		auto gameH = st::msgDateFont->height + 2 * st::msgDateImgPadding.y();
		auto gameX = pixwidth - st::msgDateImgDelta - gameW;
		auto gameY = pixheight - st::msgDateImgDelta - gameH;

		App::roundRect(p, rtlrect(gameX, gameY, gameW, gameH, pixwidth), selected ? st::msgDateImgBgSelected : st::msgDateImgBg, selected ? DateSelectedCorners : DateCorners);

		p.setFont(st::msgDateFont);
		p.setPen(st::msgDateImgFg);
		p.drawTextLeft(gameX + st::msgDateImgPadding.x(), gameY + st::msgDateImgPadding.y(), pixwidth, lang(lng_game_tag).toUpper());

		p.translate(-attachLeft, -attachTop);
	}
}

TextState HistoryGame::textState(QPoint point, StateRequest request) const {
	auto result = TextState(_parent);

	if (width() < st::msgPadding.left() + st::msgPadding.right() + 1) {
		return result;
	}
	auto paintw = width(), painth = height();

	QMargins bubble(_attach ? _attach->bubbleMargins() : QMargins());
	auto padding = inBubblePadding();
	auto tshift = padding.top();
	auto bshift = padding.bottom();
	if (isBubbleBottom() && _attach && _attach->customInfoLayout() && _attach->width() + _parent->skipBlockWidth() > paintw + bubble.left() + bubble.right()) {
		bshift += bottomInfoPadding();
	}
	paintw -= padding.left() + padding.right();

	auto inThumb = false;
	auto symbolAdd = 0;
	auto lineHeight = unitedLineHeight();
	if (_titleLines) {
		if (point.y() >= tshift && point.y() < tshift + _titleLines * lineHeight) {
			Text::StateRequestElided titleRequest = request.forText();
			titleRequest.lines = _titleLines;
			result = TextState(_parent, _title.getStateElidedLeft(
				point - QPoint(padding.left(), tshift),
				paintw,
				width(),
				titleRequest));
		} else if (point.y() >= tshift + _titleLines * lineHeight) {
			symbolAdd += _title.length();
		}
		tshift += _titleLines * lineHeight;
	}
	if (_descriptionLines) {
		if (point.y() >= tshift && point.y() < tshift + _descriptionLines * lineHeight) {
			Text::StateRequestElided descriptionRequest = request.forText();
			descriptionRequest.lines = _descriptionLines;
			result = TextState(_parent, _description.getStateElidedLeft(
				point - QPoint(padding.left(), tshift),
				paintw,
				width(),
				descriptionRequest));
		} else if (point.y() >= tshift + _descriptionLines * lineHeight) {
			symbolAdd += _description.length();
		}
		tshift += _descriptionLines * lineHeight;
	}
	if (inThumb) {
		if (!_parent->data()->isLogEntry()) {
			result.link = _openl;
		}
	} else if (_attach) {
		auto attachAtTop = !_titleLines && !_descriptionLines;
		if (!attachAtTop) tshift += st::mediaInBubbleSkip;

		auto attachLeft = padding.left() - bubble.left();
		auto attachTop = tshift - bubble.top();
		if (rtl()) attachLeft = width() - attachLeft - _attach->width();

		if (QRect(attachLeft, tshift, _attach->width(), height() - tshift - bshift).contains(point)) {
			if (_attach->isReadyForOpen()) {
				if (!_parent->data()->isLogEntry()) {
					result.link = _openl;
				}
			} else {
				result = _attach->textState(point - QPoint(attachLeft, attachTop), request);
			}
		}
	}

	result.symbol += symbolAdd;
	return result;
}

TextSelection HistoryGame::adjustSelection(TextSelection selection, TextSelectType type) const {
	if (!_descriptionLines || selection.to <= _title.length()) {
		return _title.adjustSelection(selection, type);
	}
	auto descriptionSelection = _description.adjustSelection(toDescriptionSelection(selection), type);
	if (selection.from >= _title.length()) {
		return fromDescriptionSelection(descriptionSelection);
	}
	auto titleSelection = _title.adjustSelection(selection, type);
	return { titleSelection.from, fromDescriptionSelection(descriptionSelection).to };
}

void HistoryGame::clickHandlerActiveChanged(const ClickHandlerPtr &p, bool active) {
	if (_attach) {
		_attach->clickHandlerActiveChanged(p, active);
	}
}

void HistoryGame::clickHandlerPressedChanged(const ClickHandlerPtr &p, bool pressed) {
	if (_attach) {
		_attach->clickHandlerPressedChanged(p, pressed);
	}
}

TextWithEntities HistoryGame::selectedText(TextSelection selection) const {
	auto titleResult = _title.originalTextWithEntities(
		selection,
		ExpandLinksAll);
	auto descriptionResult = _description.originalTextWithEntities(
		toDescriptionSelection(selection),
		ExpandLinksAll);
	if (titleResult.text.isEmpty()) {
		return descriptionResult;
	} else if (descriptionResult.text.isEmpty()) {
		return titleResult;
	}

	titleResult.text += '\n';
	TextUtilities::Append(titleResult, std::move(descriptionResult));
	return titleResult;
}

void HistoryGame::playAnimation(bool autoplay) {
	if (_attach) {
		if (autoplay) {
			_attach->autoplayAnimation();
		} else {
			_attach->playAnimation();
		}
	}
}

QMargins HistoryGame::inBubblePadding() const {
	auto lshift = st::msgPadding.left() + st::webPageLeft;
	auto rshift = st::msgPadding.right();
	auto bshift = isBubbleBottom() ? st::msgPadding.left() : st::mediaInBubbleSkip;
	auto tshift = isBubbleTop() ? st::msgPadding.left() : st::mediaInBubbleSkip;
	return QMargins(lshift, tshift, rshift, bshift);
}

int HistoryGame::bottomInfoPadding() const {
	if (!isBubbleBottom()) return 0;

	auto result = st::msgDateFont->height;

	// we use padding greater than st::msgPadding.bottom() in the
	// bottom of the bubble so that the left line looks pretty.
	// but if we have bottom skip because of the info display
	// we don't need that additional padding so we replace it
	// back with st::msgPadding.bottom() instead of left().
	result += st::msgPadding.bottom() - st::msgPadding.left();
	return result;
}

void HistoryGame::parentTextUpdated() {
	if (const auto media = _parent->data()->media()) {
		const auto consumed = media->consumedMessageText();
		if (!consumed.text.isEmpty()) {
			_description.setMarkedText(
				st::webPageDescriptionStyle,
				consumed,
				Ui::ItemTextOptions(_parent->data()));
		} else {
			_description = Text(st::msgMinWidth - st::webPageLeft);
		}
		history()->owner().requestViewResize(_parent);
	}
}

HistoryGame::~HistoryGame() {
	history()->owner().unregisterGameView(_data, _parent);
}
