/* CategoriesEditor.cpp
 *
 * Copyright (C) 1993-2013 David Weenink, 2008,2015 Paul Boersma
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 djmw 1995
 djmw 19980225 repaired a memory leak, caused by wrong inheritance for
 	 CategoriesEditorInsert command.
 djmw 20020408 GPL
 djmw 20020408 Modified 'createMenus'
 djmw 20060111 Replaced Resources.h with Preferences.h
 djmw 20060328 Changed last argument to 0 in XtVaSetValues, XtVaGetValues and XtVaCreateManagedWidget
 	for 64-bit compatibility.
 djmw 20070620 Latest modification.
 pb 20080320 split off Help menu
 pb 20080321 new Editor API
 djmw 20090107 Removed a bug in update that caused editor to crash on replace
 djmw 20090203 Removed potential crashes in CategoriesEditor<command>_create.
 djmw 20110304 Thing_new
 djmw 20111110 Use autostringvector
*/

#define CategoriesEditor_TEXTMAXLENGTH 100

#include "CategoriesEditor.h"
//#include "Preferences.h"
#include "EditorM.h"

Thing_implement (CategoriesEditor, Editor, 0);

/* forward declarations */
static void update (I, long from, long to, const long *select, long nSelect);
static void update_dos (I);

const char32 *CategoriesEditor_EMPTYLABEL = U"(empty)";

static void menu_cb_help (EDITOR_ARGS) {
	EDITOR_IAM (CategoriesEditor);
	Melder_help (U"CategoriesEditor");
}

/**************** Some methods for Collection  ****************/

/* Preconditions: */
/*	1 <= (position[i], newpos) <= size; */
/*	newpos <= position[1] || newpos >= position[npos] */
static void Ordered_moveItems (I, long position[], long npos, long newpos) {
	iam (Ordered);
	long pos, min = position[1], max = position[1];

	for (long i = 2; i <= npos; i++) {
		if (position[i] > max) {
			max = position[i];
		} else if (position[i] < min) {
			min = position[i];
		}
	}

	Melder_assert (min >= 1 && max <= my size && (newpos <= min || newpos >= max));

	autoNUMvector<Daata> tmp (1, npos);

	// 'remove'

	for (long i = 1; i <= npos; i++) {
		tmp[i] = (Daata) my item[position[i]];
		my item[position[i]] = 0;
	}

	// create a contiguous 'hole'

	if (newpos <= min) {
		pos = max;
		for (long i = max; i >= newpos; i--) {
			if (my item[i]) {
				my item[pos--] = my item[i];
			}
		}
		pos = newpos;
	} else {
		pos = min;
		for (long i = min; i <= newpos; i++) {
			if (my item[i]) {
				my item[pos++] = my item[i];
			}
		}
		pos = newpos - npos + 1;
	}

	// fill the 'hole'

	for (long i = 1; i <= npos; i++) {
		my item[pos++] = tmp[i];
	}
}

static void Collection_replaceItemPos (I, Any item, long pos) {
	iam (Collection);
	if (pos < 1 || pos > my size) {
		return;
	}
	forget (((SimpleString *) my item) [pos]);
	my item[pos] = item;
}

/* Remove the item at position 'from' and insert it at position 'to'. */
static void Ordered_moveItem (I, long from, long to) {
	iam (Ordered);
	if (from < 1 || from > my size) {
		from = my size;
	}
	if (to < 1 || to > my size) {
		to = my size;
	}
	if (from == to) {
		return;
	}
	Daata tmp = (Daata) my item[from];
	if (from > to) {
		for (long i = from; i > to; i--) {
			my item[i] = my item[i - 1];
		}
	} else {
		for (long i = from; i < to; i++) {
			my item[i] = my item[i + 1];
		}
	}
	my item[to] = tmp;
}

/********************** General Command **********************/

Thing_define (CategoriesEditorCommand, Command) {
	// new data:
public:
	autoCategories categories;
	long *selection; long nSelected, newPos;
	// overridden methods:
	virtual void v_destroy ();
};

Thing_implement (CategoriesEditorCommand, Command, 0);

void structCategoriesEditorCommand :: v_destroy () {
	NUMvector_free (selection, 1);
	CategoriesEditorCommand_Parent :: v_destroy ();
}

static void CategoriesEditorCommand_init (CategoriesEditorCommand me, const char32 *name, Any data,
        int (*execute) (Any), int (*undo) (Any), int /*nCategories*/, int nSelected) {

	my nSelected = nSelected;
	Command_init (me, name, data, execute, undo);
	my categories = Categories_create();
	my selection = NUMvector<long> (1, nSelected);
}

/*********************** Insert Command ***********************/

Thing_define (CategoriesEditorInsert, CategoriesEditorCommand) {
};

Thing_implement (CategoriesEditorInsert, CategoriesEditorCommand, 0);

static int CategoriesEditorInsert_execute (I) {
	iam (CategoriesEditorInsert);
	CategoriesEditor editor = (CategoriesEditor) my data;
	Categories categories = (Categories) editor -> data;
	autoSimpleString str = Data_copy ((SimpleString) my categories -> item[1]);
	Ordered_addItemPos (categories, str.transfer(), my selection[1]);
	update (editor, my selection[1], 0, my selection, 1);
	return 1;
}

static int CategoriesEditorInsert_undo (I) {
	iam (CategoriesEditorInsert);
	CategoriesEditor editor = (CategoriesEditor) my data;
	Categories categories = (Categories) editor -> data;
	Collection_removeItem (categories, my selection[1]);
	update (editor, my selection[1], 0, my selection, 1);
	return 1;
}

static CategoriesEditorInsert CategoriesEditorInsert_create (Any data, Any str, int position) {
	try {
		autoCategoriesEditorInsert me = Thing_new (CategoriesEditorInsert);
		CategoriesEditorCommand_init (me.peek(), U"Insert", data, CategoriesEditorInsert_execute, CategoriesEditorInsert_undo, 1, 1);
		my selection[1] = position;
		Collection_addItem (my categories.peek(), (SimpleString) str);
		return me.transfer();
	} catch (MelderError) {
		Melder_throw (U"CategoriesEditorInsert not created.");
	}
}

/*********************** Remove Command ***********************/

Thing_define (CategoriesEditorRemove, CategoriesEditorCommand) {
};

Thing_implement (CategoriesEditorRemove, CategoriesEditorCommand, 0);

static int CategoriesEditorRemove_execute (I) {
	iam (CategoriesEditorRemove);
	CategoriesEditor editor = (CategoriesEditor) my data;
	Categories l_categories = (Categories) editor -> data;   // David, weer link: tweemaal dezelfde naam (categories): is ok, geen conflict. Naam toch maar aangepast

	for (long i = my nSelected; i >= 1; i--) {
		Ordered_addItemPos (my categories.peek(), (SimpleString) l_categories -> item[my selection[i]], 1);
		l_categories -> item[my selection[i]] = 0;
		Collection_removeItem (l_categories, my selection[i]);
	}
	update (editor, my selection[1], 0, 0, 0);
	return 1;
}

static int CategoriesEditorRemove_undo (I) {
	iam (CategoriesEditorRemove);
	CategoriesEditor editor = (CategoriesEditor) my data;
	Categories categories = (Categories) editor -> data;

	for (long i = 1; i <= my nSelected; i++) {
		autoSimpleString item = Data_copy ( (SimpleString) my categories -> item[i]);
		Ordered_addItemPos (categories, item.transfer(), my selection[i]);
	}
	update (editor, my selection[1], 0, my selection, my nSelected);
	return 1;
}

static CategoriesEditorRemove CategoriesEditorRemove_create (Any data, long *posList, long posCount) {
	try {
		autoCategoriesEditorRemove me = Thing_new (CategoriesEditorRemove);
		CategoriesEditorCommand_init (me.peek(), U"Remove", data, CategoriesEditorRemove_execute,
		                              CategoriesEditorRemove_undo, posCount, posCount);
		for (long i = 1; i <= posCount; i++) {
			my selection[i] = posList[i];
		}
		return me.transfer();
	} catch (MelderError) {
		Melder_throw (U"CategoriesEditorRemove not created.");
	}
}

//update (me);
/*********************** Replace Command ***********************/

Thing_define (CategoriesEditorReplace, CategoriesEditorCommand) {
};

Thing_implement (CategoriesEditorReplace, CategoriesEditorCommand, 0);

static int CategoriesEditorReplace_execute (I) {
	iam (CategoriesEditorReplace);
	CategoriesEditor editor = (CategoriesEditor) my data;
	Categories categories = (Categories) editor -> data;

	for (long i = my nSelected; i >= 1; i--) {
		autoSimpleString str = Data_copy ((SimpleString) my categories -> item[1]);
		Ordered_addItemPos (my categories.peek(), (SimpleString) categories -> item[my selection[i]], 2);
		categories -> item[my selection[i]] =  str.transfer();
	}
	update (editor, my selection[1], my selection[my nSelected], my selection, my nSelected);
	return 1;
}

static int CategoriesEditorReplace_undo (I) {
	iam (CategoriesEditorReplace);
	CategoriesEditor editor = (CategoriesEditor) my data;
	Categories categories = (Categories) editor -> data;

	for (long i = 1; i <= my nSelected; i++) {
		autoSimpleString str = Data_copy ( (SimpleString) my categories -> item[i + 1]);
		Collection_replaceItemPos (categories, str.transfer(), my selection[i]);
	}
	update (editor, my selection[1], my selection[my nSelected], my selection, my nSelected);
	return 1;
}

static CategoriesEditorReplace CategoriesEditorReplace_create (Any data, Any str, long *posList, long posCount) {
	try {
		autoCategoriesEditorReplace me = Thing_new (CategoriesEditorReplace);
		CategoriesEditorCommand_init (me.peek(), U"Replace", data, CategoriesEditorReplace_execute,
		                              CategoriesEditorReplace_undo, posCount + 1, posCount);
		for (long i = 1; i <= posCount; i++) {
			my selection[i] = posList[i];
		}
		Collection_addItem (my categories.peek(), (SimpleString) str);
		return me.transfer();
	} catch (MelderError) {
		Melder_throw (U"CategoriesEditorReplace not created.");
	}
}

/*********************** MoveUp Command ***********************/

Thing_define (CategoriesEditorMoveUp, CategoriesEditorCommand) {
};

Thing_implement (CategoriesEditorMoveUp, CategoriesEditorCommand, 0);

static int CategoriesEditorMoveUp_execute (I) {
	iam (CategoriesEditorMoveUp);
	CategoriesEditor editor = (CategoriesEditor) my data;
	Ordered_moveItems (editor->data, my selection, my nSelected, my newPos);
	autoNUMvector<long> selection (1, my nSelected);
	for (long i = 1; i <= my nSelected; i++) {
		selection[i] = my newPos + i - 1;
	}
	update (editor, my newPos, my selection[my nSelected], selection.peek(), my nSelected);
	return 1;
}

static int CategoriesEditorMoveUp_undo (I) {
	iam (CategoriesEditorMoveUp);
	CategoriesEditor editor = (CategoriesEditor) my data;

	for (long i = 1; i <= my nSelected; i++) {
		Ordered_moveItem (editor->data, my newPos, my selection[my nSelected]);
	}
	update (editor, my newPos, my selection[my nSelected], my selection, my nSelected);
	return 1;
}

static CategoriesEditorMoveUp CategoriesEditorMoveUp_create (Any data, long *posList,
        long posCount, long newPos) {
	try {
		autoCategoriesEditorMoveUp me = Thing_new (CategoriesEditorMoveUp);
		CategoriesEditorCommand_init (me.peek(), U"Move up", data, CategoriesEditorMoveUp_execute,
		                              CategoriesEditorMoveUp_undo, 0, posCount);
		for (long i = 1; i <= posCount; i++) {
			my selection[i] = posList[i];
		}
		my newPos = newPos;
		return me.transfer();
	} catch (MelderError) {
		Melder_throw (U"CategoriesEditorMoveUp not created.");
	}
}

/*********************** MoveDown Command ***********************/

Thing_define (CategoriesEditorMoveDown, CategoriesEditorCommand) {
};

Thing_implement (CategoriesEditorMoveDown, CategoriesEditorCommand, 0);

static int CategoriesEditorMoveDown_execute (I) {
	iam (CategoriesEditorMoveDown);
	CategoriesEditor editor = (CategoriesEditor) my data;
	Ordered_moveItems ( (Ordered) editor -> data, my selection, my nSelected, my newPos);
	autoNUMvector<long> selection (1, my nSelected);
	for (long i = 1; i <= my nSelected; i++) {
		selection[i] = my newPos - my nSelected + i;
	}
	update (editor, my selection[1], my newPos, selection.peek(), my nSelected);
	return 1;
}

static int CategoriesEditorMoveDown_undo (I) {
	iam (CategoriesEditorMoveDown);
	CategoriesEditor editor = (CategoriesEditor) my data;
	for (long i = 1; i <= my nSelected; i++) {
		Ordered_moveItem (editor -> data, my newPos, my selection[1]); // TODO 1 or i ??
	}
	long from = my selection[1];
	update (editor, (from > 1 ? from-- : from), my newPos, my selection, my nSelected);
	return 1;
}

static CategoriesEditorMoveDown CategoriesEditorMoveDown_create (Any data, long *posList,
        long posCount, long newPos) {
	try {
		autoCategoriesEditorMoveDown me = Thing_new (CategoriesEditorMoveDown);
		CategoriesEditorCommand_init (me.peek(), U"Move down", data, CategoriesEditorMoveDown_execute,
		                              CategoriesEditorMoveDown_undo, 0, posCount);
		for (long i = 1; i <= posCount; i++) {
			my selection[i] = posList[i];
		}
		my newPos = newPos;
		return me.transfer();
	} catch (MelderError) {
		Melder_throw (U"CategoriesEditorMoveDown not created.");
	}
}

/********************* Commands (End)  *************************************/

static void notifyOutOfView (I) {
	iam (CategoriesEditor);
	autoMelderString tmp;
	MelderString_copy (&tmp, U"");
	long posCount;
	autoNUMvector<long> posList (GuiList_getSelectedPositions (my list, & posCount), 1);
	if (posList.peek() != 0) {
		long outOfView = 0, top = GuiList_getTopPosition (my list), bottom = GuiList_getBottomPosition (my list);

		for (long i = posCount; i > 0; i--) {
			if (posList[i] < top || posList[i] > bottom) {
				outOfView++;
			}
		}
		if (outOfView > 0) {
			MelderString_append (&tmp, outOfView, U" selection(s) out of view");
		}
	}
	GuiLabel_setText (my outOfView, tmp.string);
}

static void update_dos (I) {
	iam (CategoriesEditor);
	bool undoSense = true, redoSense = true;

	// undo

	const char32 *name;
	if (! (name = CommandHistory_commandName (my history.peek(), 0))) {
		name = U"nothing"; undoSense = false;
	}

	GuiButton_setText (my undo, Melder_cat (U"Undo ", U"\"", name, U"\""));
	GuiThing_setSensitive (my undo, undoSense);

	// redo

	if (! (name = CommandHistory_commandName (my history.peek(), 1))) {
		name = U"nothing"; redoSense = false;
	}
	GuiButton_setText (my redo, Melder_cat (U"Redo ", U"\"", name, U"\""));
	GuiThing_setSensitive (my redo, redoSense);
}

static void updateWidgets (I) { /*all buttons except undo & redo */
	iam (CategoriesEditor);
	long size = ( (Categories) my data)->size;
	bool insert = false, insertAtEnd = true, replace = false, remove = false;
	bool moveUp = false, moveDown = false;
	long posCount;
	autoNUMvector<long> posList (GuiList_getSelectedPositions (my list, & posCount), 1);
	if (posList.peek() != 0) {
		long firstPos = posList[1], lastPos = posList[posCount];
		bool contiguous = lastPos - firstPos + 1 == posCount;
		moveUp = contiguous && firstPos > 1;
		moveDown = contiguous && lastPos < size;
		my position = firstPos;
		remove = true; replace = true; //insertAtEnd = False;
		if (posCount == 1) {
			insert = true;
			//if (posList[1] == size) insertAtEnd = True;
			if (size == 1 && ! str32cmp (CategoriesEditor_EMPTYLABEL,
			                           OrderedOfString_itemAtIndex_c ((OrderedOfString) my data, 1))) {
				remove = false;
			}
		}
	}
	GuiThing_setSensitive (my insert,      insert);
	GuiThing_setSensitive (my insertAtEnd, insertAtEnd);
	GuiThing_setSensitive (my replace,     replace);
	GuiThing_setSensitive (my remove,      remove);
	GuiThing_setSensitive (my moveUp,      moveUp);
	GuiThing_setSensitive (my moveDown,    moveDown);
	if (my history) {
		update_dos (me);
	}
	notifyOutOfView (me);
}

static void update (I, long from, long to, const long *select, long nSelect) {
	iam (CategoriesEditor);
	long size = ((Categories) my data) -> size;

	if (size == 0) {
		autoSimpleString str = SimpleString_create (CategoriesEditor_EMPTYLABEL);
		Collection_addItem ( (Categories) my data, str.transfer());
		update (me, 0, 0, 0, 0);
		return;
	}
	if (from == 0 && from == to) {
		from = 1; to = size;
	}
	if (from < 1 || from > size) {
		from = size;
	}
	if (to < 1 || to > size) {
		to = size;
	}
	if (from > to) {
		long ti = from; from = to; to = ti;
	}

	// Begin optimization: add the items from a table instead of separately.
	try {
		autostring32vector table (from, to);
		long itemCount = GuiList_getNumberOfItems (my list);
		for (long i = from; i <= to; i++) {
			char wcindex[20];
			snprintf (wcindex,20, "%5ld ", i);
			table[i] = Melder_dup_f (Melder_cat (Melder_peek8to32 (wcindex), OrderedOfString_itemAtIndex_c ((OrderedOfString) my data, i)));
		}
		if (itemCount > size) { // some items have been removed from Categories?
			for (long j = itemCount; j > size; j --) {
				GuiList_deleteItem (my list, j);
			}
			itemCount = size;
		}
		if (to > itemCount) {
			for (long j = 1; j <= to - itemCount; j ++) {
				GuiList_insertItem (my list, table [itemCount + j], 0);
			}
		}
		if (from <= itemCount) {
			long n = (to < itemCount ? to : itemCount);
			for (long j = from; j <= n; j++) {
				GuiList_replaceItem (my list, table[j], j);
			}
		}
	} catch (MelderError) {
		throw;
	}

	// End of optimization

	// HIGHLIGHT

	GuiList_deselectAllItems (my list);
	if (size == 1) { /* the only item is always selected */
		const char32 *catg = OrderedOfString_itemAtIndex_c ((OrderedOfString) my data, 1);
		GuiList_selectItem (my list, 1);
		updateWidgets (me);   // instead of "notify". BUG?
		GuiText_setString (my text, catg);
	} else if (nSelect > 0) {
		// Select but postpone highlighting

		for (long i = 1; i <= nSelect; i++) {
			GuiList_selectItem (my list, select[i] > size ? size : select[i]);
		}
	}

	// VIEWPORT

	{
		long top = GuiList_getTopPosition (my list), bottom = GuiList_getBottomPosition (my list);
		long visible = bottom - top + 1;
		if (nSelect == 0) {
			top = my position - visible / 2;
		} else if (select[nSelect] < top) {
			// selection above visible area
			top = select[1];
		} else if (select[1] > bottom) {
			// selection below visible area
			top = select[nSelect] - visible + 1;
		} else {
			long deltaTopPos = -1, nUpdate = to - from + 1;
			if ( (from == select[1] && to == select[nSelect]) || // Replace
			        (nUpdate > 2 && nSelect == 1) /* Inserts */) {
				deltaTopPos = 0;
			} else if (nUpdate == nSelect + 1 && select[1] == from + 1) { // down
				deltaTopPos = 1;
			}
			top += deltaTopPos;
		}
		if (top + visible > size) {
			top = size - visible + 1;
		}
		if (top < 1) {
			top = 1;
		}
		GuiList_setTopPosition (my list, top);
	}
}

static void gui_button_cb_remove (CategoriesEditor me, GuiButtonEvent /* event */) {
	long posCount;
	autoNUMvector<long> posList (GuiList_getSelectedPositions (my list, & posCount), 1);
	if (posList.peek() != 0) {
		autoCategoriesEditorRemove command = CategoriesEditorRemove_create (me, posList.peek(), posCount);
		if (! Command_do (command.peek())) {
			return;
		}
		if (my history) {
			CommandHistory_insertItem (my history.peek(), command.transfer());
		}
		updateWidgets (me);
	}
}

static void insert (CategoriesEditor me, int position) {
	autostring32 text = GuiText_getString (my text);
	if (str32len (text.peek()) != 0) {
		autoSimpleString str = SimpleString_create (text.peek());
		autoCategoriesEditorInsert command = CategoriesEditorInsert_create (me, str.transfer(), position);
		Command_do (command.peek());
		if (my history) {
			CommandHistory_insertItem (my history.peek(), command.transfer());
		}
		updateWidgets (me);
	}
}

static void gui_button_cb_insert (CategoriesEditor me, GuiButtonEvent /* event */) {
	insert (me, my position);
}

static void gui_button_cb_insertAtEnd (CategoriesEditor me, GuiButtonEvent /* event */) {
	Categories categories = (Categories) my data;
	insert (me, categories -> size + 1);
	my position = categories -> size;
}

static void gui_button_cb_replace (CategoriesEditor me, GuiButtonEvent /* event */) {
	long posCount;
	autoNUMvector<long> posList (GuiList_getSelectedPositions (my list, & posCount), 1);
	if (posCount > 0) {
		autostring32 text = GuiText_getString (my text);
		if (str32len (text.peek()) != 0) {
			autoSimpleString str = SimpleString_create (text.peek());
			autoCategoriesEditorReplace command = CategoriesEditorReplace_create (me, str.transfer(),
			                                      posList.peek(), posCount);
			Command_do (command.peek());
			if (my history) {
				CommandHistory_insertItem (my history.peek(), command.transfer());
			}
			updateWidgets (me);
		}
	}
}

/* Precondition: contiguous selection */
static void gui_button_cb_moveUp (CategoriesEditor me, GuiButtonEvent /* event */) {
	long posCount;
	autoNUMvector<long> posList (GuiList_getSelectedPositions (my list, & posCount), 1);
	if (posCount > 0) {
		autoCategoriesEditorMoveUp command = CategoriesEditorMoveUp_create
		                                     (me, posList.peek(), posCount, posList[1] - 1);
		Command_do (command.peek());
		if (my history) {
			CommandHistory_insertItem (my history.peek(), command.transfer());
		}
		updateWidgets (me);
	}
}

/* Precondition: contiguous selection */
static void gui_button_cb_moveDown (CategoriesEditor me, GuiButtonEvent /* event */) {
	long posCount;
	autoNUMvector<long> posList (GuiList_getSelectedPositions (my list, & posCount), 1);
	if (posCount > 0) {
		autoCategoriesEditorMoveDown command = CategoriesEditorMoveDown_create
		                                       (me, posList.peek(), posCount, posList[posCount] + 1);
		Command_do (command.peek());
		if (my history) {
			CommandHistory_insertItem (my history.peek(), command.transfer());
		}
		updateWidgets (me);
	}
}


static void gui_cb_scroll (GUI_ARGS) {
	GUI_IAM (CategoriesEditor);
	notifyOutOfView (me);
}

static void gui_list_cb_double_click (void *void_me, GuiListEvent /* event */) {
	iam (CategoriesEditor);
	const char32 *catg = OrderedOfString_itemAtIndex_c ((OrderedOfString) my data, my position);
	GuiText_setString (my text, catg);
}

static void gui_list_cb_extended (void *void_me, GuiListEvent /* event */) {
	iam (CategoriesEditor);
	updateWidgets (me);
}

static void gui_button_cb_undo (CategoriesEditor me, GuiButtonEvent /* event */) {
	if (CommandHistory_offleft (my history.peek())) {
		return;
	}
	Command_undo (CommandHistory_getItem (my history.peek()));
	CommandHistory_back (my history.peek());
	updateWidgets (me);
}

static void gui_button_cb_redo (CategoriesEditor me, GuiButtonEvent /* event */) {
	CommandHistory_forth (my history.peek());
	if (CommandHistory_offright (my history.peek())) {
		return;
	}
	Command_do (CommandHistory_getItem (my history.peek()));
	updateWidgets (me);
}

void structCategoriesEditor :: v_destroy () {
	CategoriesEditor_Parent :: v_destroy ();
}

void structCategoriesEditor :: v_createHelpMenuItems (EditorMenu menu) {
	CategoriesEditor_Parent :: v_createHelpMenuItems (menu);
	EditorMenu_addCommand (menu, U"CategoriesEditor help", '?', menu_cb_help);
}

// origin is at top left.
void structCategoriesEditor :: v_createChildren () {
	double menuBarOffset = 40;
	double button_width = 90, button_height = menuBarOffset, list_width = 260, list_height = 200, list_bottom;
	double delta_x = 15, delta_y = menuBarOffset / 2, text_button_height = button_height / 2;
	double left, right, top, bottom, buttons_left, buttons_top;

	left = 5; right = left + button_width; top = 3 + menuBarOffset; bottom = top + text_button_height;
	GuiLabel_createShown (d_windowForm, left, right, top, bottom, U"Positions:", 0);
	left = right + delta_x ; right = left + button_width;
	GuiLabel_createShown (d_windowForm, left, right, top, bottom, U"Values:", 0);

	left = 0; right = left + list_width; buttons_top = (top = bottom + delta_y); list_bottom = bottom = top + list_height;
	list = GuiList_create (d_windowForm, left, right, top, bottom, true, 0);
	GuiList_setSelectionChangedCallback (list, gui_list_cb_extended, this);
	GuiList_setDoubleClickCallback (list, gui_list_cb_double_click, this);
	GuiThing_show (list);

	buttons_left = left = right + 2 * delta_x; right = left + button_width; bottom = top + button_height;
	GuiLabel_createShown (d_windowForm, left, right, top, bottom, U"Value:", 0);
	left = right + delta_x; right = left + button_width;
	text = GuiText_createShown (d_windowForm, left, right, top, bottom, 0);
	GuiText_setString (text, CategoriesEditor_EMPTYLABEL);

	left = buttons_left; right = left + button_width; top = bottom + delta_y; bottom = top + button_height;
	insert = GuiButton_createShown (d_windowForm, left, right, top, bottom,	U"Insert", gui_button_cb_insert, this, GuiButton_DEFAULT);
	left = right + delta_x; right = left + button_width;
	replace = GuiButton_createShown (d_windowForm, left, right, top, bottom, U"Replace", gui_button_cb_replace, this, 0);
	left = buttons_left; right = left + 1.5 * button_width; top = bottom + delta_y; bottom = top + button_height;
	insertAtEnd = GuiButton_createShown (d_windowForm, left, right, top, bottom, U"Insert at end", gui_button_cb_insertAtEnd, this, 0);
	top = bottom + delta_y; bottom = top + button_height;
	undo = GuiButton_createShown (d_windowForm, left, right, top, bottom, U"Undo", gui_button_cb_undo, this, 0);
	top = bottom + delta_y; bottom = top + button_height;
	redo = GuiButton_createShown (d_windowForm, left, right, top, bottom, U"Redo", gui_button_cb_redo, this, 0);
	top = bottom + delta_y; bottom = top + button_height;
	remove = GuiButton_createShown (d_windowForm, left, right, top, bottom, U"Remove", gui_button_cb_remove, this, 0);
	top = bottom + delta_y; bottom = top + button_height;
	moveUp = GuiButton_createShown (d_windowForm, left, right, top, bottom, U"Move selection up", gui_button_cb_moveUp, this, 0);
	top = bottom + delta_y; bottom = top + button_height;
	moveDown = GuiButton_createShown (d_windowForm, left, right, top, bottom, U"Move selection down", gui_button_cb_moveDown, this, 0);

	top = list_bottom + delta_y; bottom = top + button_height; left = 5; right = left + 200;
	outOfView = GuiLabel_createShown (d_windowForm, left, right, top, bottom, U"", 0);
}

void structCategoriesEditor :: v_dataChanged () {
	update (this, 0, 0, 0, 0);
	updateWidgets (this);
}

autoCategoriesEditor CategoriesEditor_create (const char32 *title, Categories data) {
	try {
		autoCategoriesEditor me = Thing_new (CategoriesEditor);
		Editor_init (me.peek(), 20, 40, 600, 600, title, data);
		my history = CommandHistory_create (100);
		update (me.peek(), 0, 0, 0, 0);
		updateWidgets (me.peek());
		return me;
	} catch (MelderError) {
		Melder_throw (U"Categories window not created.");
	}
}

/* End of file CategoriesEditor.cpp */
