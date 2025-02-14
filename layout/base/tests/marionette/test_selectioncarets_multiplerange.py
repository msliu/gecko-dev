# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.by import By
from marionette_driver.marionette import Actions
from marionette import MarionetteTestCase
from marionette_driver.selection import SelectionManager
from marionette_driver.gestures import long_press_without_contextmenu


class CaretsMultipleRangeTestBase(object):
    _long_press_time = 1        # 1 second

    def setUp(self):
        # Code to execute before a tests are run.
        MarionetteTestCase.setUp(self)
        self.actions = Actions(self.marionette)

    def openTestHtml(self, enabled=True):
        # Open html for testing and enable selectioncaret and
        # non-editable support
        self.marionette.execute_async_script(
            'SpecialPowers.pushPrefEnv({"set": [["%s", %s]]}, marionetteScriptFinished);' %
            (self.carets_enabled_pref_name, ('true' if enabled else 'false')))

        test_html = self.marionette.absolute_url('test_selectioncarets_multiplerange.html')
        self.marionette.navigate(test_html)

        self._body = self.marionette.find_element(By.ID, 'bd')
        self._sel1 = self.marionette.find_element(By.ID, 'sel1')
        self._sel2 = self.marionette.find_element(By.ID, 'sel2')
        self._sel3 = self.marionette.find_element(By.ID, 'sel3')
        self._sel4 = self.marionette.find_element(By.ID, 'sel4')
        self._sel6 = self.marionette.find_element(By.ID, 'sel6')
        self._nonsel1 = self.marionette.find_element(By.ID, 'nonsel1')

    def openTestHtmlLongText(self, enabled=True):
        # Open html for testing and enable selectioncaret
        self.marionette.execute_script(
            'SpecialPowers.setBoolPref("%s", %s);' %
            (self.carets_enabled_pref_name, 'true' if enabled else 'false'))

        test_html = self.marionette.absolute_url('test_selectioncarets_longtext.html')
        self.marionette.navigate(test_html)

        self._body = self.marionette.find_element(By.ID, 'bd')
        self._longtext = self.marionette.find_element(By.ID, 'longtext')

    def openTestHtmlIframe(self, enabled=True):
        # Open html for testing and enable selectioncaret
        self.marionette.execute_script(
            'SpecialPowers.setBoolPref("%s", %s);' %
            (self.carets_enabled_pref_name, 'true' if enabled else 'false'))

        test_html = self.marionette.absolute_url('test_selectioncarets_iframe.html')
        self.marionette.navigate(test_html)

        self._iframe = self.marionette.find_element(By.ID, 'frame')

    def _long_press_to_select_word(self, el, wordOrdinal):
        sel = SelectionManager(el)
        original_content = sel.content
        words = original_content.split()
        self.assertTrue(wordOrdinal < len(words),
            'Expect at least %d words in the content.' % wordOrdinal)

        # Calc offset
        offset = 0
        for i in range(wordOrdinal):
            offset += (len(words[i]) + 1)

        # Move caret inside the word.
        el.tap()
        sel.move_caret_to_front()
        sel.move_caret_by_offset(offset)
        x, y = sel.caret_location()

        # Long press the caret position. Selection carets should appear, and the
        # word will be selected. On Windows, those spaces after the word
        # will also be selected.
        long_press_without_contextmenu(self.marionette, el, self._long_press_time, x, y)

    def _to_unix_line_ending(self, s):
        """Changes all Windows/Mac line endings in s to UNIX line endings."""

        return s.replace('\r\n', '\n').replace('\r', '\n')

    def test_long_press_to_select_non_selectable_word(self):
        '''Testing long press on non selectable field.
        We should not select anything when long press on non selectable fields.'''

        self.openTestHtml(enabled=True)
        halfY = self._nonsel1.size['height'] / 2
        long_press_without_contextmenu(self.marionette, self._nonsel1, self._long_press_time, 0, halfY)
        sel = SelectionManager(self._nonsel1)
        range_count = sel.range_count()
        self.assertEqual(range_count, 0)

    def test_drag_caret_over_non_selectable_field(self):
        '''Testing drag caret over non selectable field.
        So that the selected content should exclude non selectable field and
        end selection caret should appear in last range's position.'''
        self.openTestHtml(enabled=True)

        # Select target element and get target caret location
        self._long_press_to_select_word(self._sel4, 3)
        sel = SelectionManager(self._body)
        (_, _), (end_caret_x, end_caret_y) = sel.selection_carets_location()

        self._long_press_to_select_word(self._sel6, 0)
        (_, _), (end_caret2_x, end_caret2_y) = sel.selection_carets_location()

        # Select start element
        self._long_press_to_select_word(self._sel3, 3)

        # Drag end caret to target location
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.selection_carets_location()
        self.actions.flick(self._body, caret2_x, caret2_y, end_caret_x, end_caret_y, 1).perform()
        self.assertEqual(self._to_unix_line_ending(sel.selected_content.strip()),
            'this 3\nuser can select this')

        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.selection_carets_location()
        self.actions.flick(self._body, caret2_x, caret2_y, end_caret2_x, end_caret2_y, 1).perform()
        self.assertEqual(self._to_unix_line_ending(sel.selected_content.strip()),
            'this 3\nuser can select this 4\nuser can select this 5\nuser')

        # Drag first caret to target location
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.selection_carets_location()
        self.actions.flick(self._body, caret1_x, caret1_y, end_caret_x, end_caret_y, 1).perform()
        self.assertEqual(self._to_unix_line_ending(sel.selected_content.strip()),
            '4\nuser can select this 5\nuser')

    def test_drag_caret_to_beginning_of_a_line(self):
        '''Bug 1094056
        Test caret visibility when caret is dragged to beginning of a line
        '''
        self.openTestHtml(enabled=True)

        # Select the first word in the second line
        self._long_press_to_select_word(self._sel2, 0)
        sel = SelectionManager(self._body)
        (start_caret_x, start_caret_y), (end_caret_x, end_caret_y) = sel.selection_carets_location()

        # Select target word in the first line
        self._long_press_to_select_word(self._sel1, 2)

        # Drag end caret to the beginning of the second line
        (caret1_x, caret1_y), (caret2_x, caret2_y) = sel.selection_carets_location()
        self.actions.flick(self._body, caret2_x, caret2_y, start_caret_x, start_caret_y).perform()

        # Drag end caret back to the target word
        self.actions.flick(self._body, start_caret_x, start_caret_y, caret2_x, caret2_y).perform()

        self.assertEqual(self._to_unix_line_ending(sel.selected_content.strip()), 'select')

    def test_caret_position_after_changing_orientation_of_device(self):
        '''Bug 1094072
        If positions of carets are updated correctly, they should be draggable.
        '''
        # Skip running test on non-rotatable device ex.desktop browser
        if not self.marionette.session_capabilities['rotatable']:
            return

        self.openTestHtmlLongText(enabled=True)

        # Select word in portrait mode, then change to landscape mode
        self.marionette.set_orientation('portrait')
        self._long_press_to_select_word(self._longtext, 12)
        sel = SelectionManager(self._body)
        (p_start_caret_x, p_start_caret_y), (p_end_caret_x, p_end_caret_y) = sel.selection_carets_location()
        self.marionette.set_orientation('landscape')
        (l_start_caret_x, l_start_caret_y), (l_end_caret_x, l_end_caret_y) = sel.selection_carets_location()

        # Drag end caret to the start caret to change the selected content
        self.actions.flick(self._body, l_end_caret_x, l_end_caret_y, l_start_caret_x, l_start_caret_y).perform()

        # Change orientation back to portrait mode to prevent affecting
        # other tests
        self.marionette.set_orientation('portrait')

        self.assertEqual(self._to_unix_line_ending(sel.selected_content.strip()), 'o')

    def test_select_word_inside_an_iframe(self):
        '''Bug 1088552
        The scroll offset in iframe should be taken into consideration properly.
        In this test, we scroll content in the iframe to the bottom to cause a
        huge offset. If we use the right coordinate system, selection should
        work. Otherwise, it would be hard to trigger select word.
        '''
        self.openTestHtmlIframe(enabled=True)

        # switch to inner iframe and scroll to the bottom
        self.marionette.switch_to_frame(self._iframe)
        self.marionette.execute_script(
         'document.getElementById("bd").scrollTop += 999')

        # long press to select bottom text
        self._body = self.marionette.find_element(By.ID, 'bd')
        sel = SelectionManager(self._body)
        self._bottomtext = self.marionette.find_element(By.ID, 'bottomtext')
        long_press_without_contextmenu(self.marionette, self._bottomtext, self._long_press_time)

        self.assertNotEqual(self._to_unix_line_ending(sel.selected_content.strip()), '')


class SelectionCaretsMultipleRangeTest(CaretsMultipleRangeTestBase, MarionetteTestCase):
    def setUp(self):
        self.carets_enabled_pref_name = 'selectioncaret.enabled'
        CaretsMultipleRangeTestBase.setUp(self)


class AccessibleCaretCursorModeMultipleRangeTest(CaretsMultipleRangeTestBase, MarionetteTestCase):
    def setUp(self):
        self.carets_enabled_pref_name = 'layout.accessiblecaret.enabled'
        CaretsMultipleRangeTestBase.setUp(self)
