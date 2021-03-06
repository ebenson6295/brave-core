/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/views/profiles/brave_profile_chooser_view.h"

#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"

class BraveProfileChooserViewTest : public InProcessBrowserTest {
 public:
  BraveProfileChooserViewTest() = default;
  ~BraveProfileChooserViewTest() override = default;

protected:
  void OpenProfileChooserView(Browser* browser) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    views::View* button = browser_view->toolbar()->avatar_button();
    DCHECK(button);

    ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
    button->OnMousePressed(e);
  }

  AvatarMenu* avatar_menu() {
    return ProfileChooserView::profile_bubble_->avatar_menu_.get();
  }

  views::LabelButton* current_profile_card() {
    return ProfileChooserView::profile_bubble_->current_profile_card_;
  }

private:
  DISALLOW_COPY_AND_ASSIGN(BraveProfileChooserViewTest);
};

IN_PROC_BROWSER_TEST_F(BraveProfileChooserViewTest, TestCurrentProfileView) {
  OpenProfileChooserView(browser());

  EXPECT_EQ(1u, avatar_menu()->GetNumberOfItems());
  ASSERT_EQ(avatar_menu()->GetItemAt(0).name,
            current_profile_card()->GetAccessibleName());
}
