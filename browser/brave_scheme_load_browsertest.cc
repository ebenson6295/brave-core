/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/path_service.h"
#include "base/strings/pattern.h"
#include "brave/common/brave_paths.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

class BraveSchemeLoadBrowserTest : public InProcessBrowserTest,
                                   public BrowserListObserver,
                                   public TabStripModelObserver {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    brave::RegisterPathProvider();
    base::FilePath test_data_dir;
    base::PathService::Get(brave::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // BrowserListObserver overrides:
  void OnBrowserAdded(Browser* browser) override { popup_ = browser; }

  // TabStripModelObserver overrides:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kInserted) {
      WaitForLoadStop(active_contents());
    }
  }

  content::WebContents* active_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  bool NavigateToURLUntilLoadStop(const std::string& origin,
                                  const std::string& path) {
    ui_test_utils::NavigateToURL(browser(),
                                 embedded_test_server()->GetURL(origin, path));
    return WaitForLoadStop(active_contents());
  }

  // Check loading |url| in private window is redirected to normal
  // window.
  void TestURLIsNotLoadedInPrivateWindow(const GURL& url) {
    Browser* private_browser = CreateIncognitoBrowser(nullptr);
    TabStripModel* private_model = private_browser->tab_strip_model();

    // Check normal & private window have one blank tab.
    EXPECT_EQ("about:blank",
              private_model->GetActiveWebContents()->GetVisibleURL().spec());
    EXPECT_EQ(1, private_model->count());
    EXPECT_EQ("about:blank", active_contents()->GetVisibleURL().spec());
    EXPECT_EQ(1, browser()->tab_strip_model()->count());

    browser()->tab_strip_model()->AddObserver(this);

    // Load url to private window.
    NavigateParams params(private_browser, url, ui::PAGE_TRANSITION_TYPED);
    Navigate(&params);

    browser()->tab_strip_model()->RemoveObserver(this);

    EXPECT_EQ(url, active_contents()->GetVisibleURL());
    EXPECT_EQ(2, browser()->tab_strip_model()->count());
    // Private window stays as initial state.
    EXPECT_EQ("about:blank",
              private_model->GetActiveWebContents()->GetVisibleURL().spec());
    EXPECT_EQ(1, private_browser->tab_strip_model()->count());
  }

  Browser* popup_ = nullptr;
};

// Test whether brave page is not loaded from different host by window.open().
IN_PROC_BROWSER_TEST_F(BraveSchemeLoadBrowserTest, NotAllowedToLoadTest) {
  EXPECT_TRUE(
      NavigateToURLUntilLoadStop("example.com", "/brave_scheme_load.html"));
  content::ConsoleObserverDelegate console_delegate(
      active_contents(), "Not allowed to load local resource:*");
  active_contents()->SetDelegate(&console_delegate);

  ASSERT_TRUE(ExecuteScript(
      active_contents(),
      "window.domAutomationController.send(openBraveSettings())"));
  console_delegate.Wait();

  EXPECT_TRUE(base::MatchPattern(
      console_delegate.message(),
      "Not allowed to load local resource: brave://settings/"));
}

// Test whether brave page is not loaded from different host by window.open().
IN_PROC_BROWSER_TEST_F(BraveSchemeLoadBrowserTest,
                       NotAllowedToLoadTestByWindowOpenWithNoOpener) {
  BrowserList::GetInstance()->AddObserver(this);

  EXPECT_TRUE(
      NavigateToURLUntilLoadStop("example.com", "/brave_scheme_load.html"));

  ASSERT_TRUE(ExecuteScript(
      active_contents(),
      "window.domAutomationController.send(openBraveSettingsWithNoOpener())"));

  auto* popup_tab = popup_->tab_strip_model()->GetActiveWebContents();

  WaitForLoadStop(popup_tab);

  // Loading brave page should be blocked in new window.
  DCHECK_EQ(popup_tab->GetVisibleURL().spec(), content::kBlockedURL);

  BrowserList::GetInstance()->RemoveObserver(this);
}

// Test whether brave page is not loaded from different host directly by
// location.replace().
IN_PROC_BROWSER_TEST_F(BraveSchemeLoadBrowserTest,
                       NotAllowedToDirectReplaceTest) {
  EXPECT_TRUE(
      NavigateToURLUntilLoadStop("example.com", "/brave_scheme_load.html"));
  content::ConsoleObserverDelegate console_delegate(
      active_contents(), "Not allowed to load local resource:*");
  active_contents()->SetDelegate(&console_delegate);

  ASSERT_TRUE(ExecuteScript(
      active_contents(),
      "window.domAutomationController.send(replaceToBraveSettingsDirectly())"));
  console_delegate.Wait();
  EXPECT_TRUE(base::MatchPattern(
      console_delegate.message(),
      "Not allowed to load local resource: brave://settings/"));
}

// Test whether brave page is not loaded from different host indirectly by
// location.replace().
IN_PROC_BROWSER_TEST_F(BraveSchemeLoadBrowserTest,
                       NotAllowedToIndirectReplaceTest) {
  EXPECT_TRUE(
      NavigateToURLUntilLoadStop("example.com", "/brave_scheme_load.html"));
  auto* initial_active_tab = active_contents();
  content::ConsoleObserverDelegate console_delegate(
      initial_active_tab, "Not allowed to load local resource:*");

  ASSERT_TRUE(ExecuteScript(initial_active_tab,
                            "window.domAutomationController.send("
                            "replaceToBraveSettingsIndirectly())"));

  initial_active_tab->SetDelegate(&console_delegate);
  console_delegate.Wait();

  EXPECT_TRUE(base::MatchPattern(
      console_delegate.message(),
      "Not allowed to load local resource: brave://settings/"));
}

// Test whether brave page is not loaded from chrome page.
IN_PROC_BROWSER_TEST_F(BraveSchemeLoadBrowserTest,
                       NotAllowedToBraveFromChrome) {
  NavigateToURLBlockUntilNavigationsComplete(active_contents(),
                                             GURL("chrome://version"), 1);

  content::ConsoleObserverDelegate console_delegate(
      active_contents(), "Not allowed to load local resource:*");
  active_contents()->SetDelegate(&console_delegate);

  ASSERT_TRUE(
      ExecuteScript(active_contents(), "window.open(\"brave://settings\")"));
  console_delegate.Wait();
  EXPECT_TRUE(base::MatchPattern(
      console_delegate.message(),
      "Not allowed to load local resource: brave://settings/"));
}

// Test whether brave page is not loaded by click.
IN_PROC_BROWSER_TEST_F(BraveSchemeLoadBrowserTest, NotAllowedToBraveByClick) {
  EXPECT_TRUE(
      NavigateToURLUntilLoadStop("example.com", "/brave_scheme_load.html"));
  content::ConsoleObserverDelegate console_delegate(
      active_contents(), "Not allowed to load local resource:*");
  active_contents()->SetDelegate(&console_delegate);

  ASSERT_TRUE(ExecuteScript(
      active_contents(),
      "window.domAutomationController.send(gotoBraveSettingsByClick())"));
  console_delegate.Wait();
  EXPECT_TRUE(base::MatchPattern(
      console_delegate.message(),
      "Not allowed to load local resource: brave://settings/"));
}

// Test whether brave page is not loaded by middle click.
IN_PROC_BROWSER_TEST_F(BraveSchemeLoadBrowserTest,
                       NotAllowedToBraveByMiddleClick) {
  EXPECT_TRUE(
      NavigateToURLUntilLoadStop("example.com", "/brave_scheme_load.html"));
  content::ConsoleObserverDelegate console_delegate(
      active_contents(), "Not allowed to load local resource:*");
  active_contents()->SetDelegate(&console_delegate);

  ASSERT_TRUE(ExecuteScript(
      active_contents(),
      "window.domAutomationController.send(gotoBraveSettingsByMiddleClick())"));
  console_delegate.Wait();
  EXPECT_TRUE(base::MatchPattern(
      console_delegate.message(),
      "Not allowed to load local resource: brave://settings/"));
}

// Some webuis are not allowed to load in private window.
// Allowed url list are checked by IsURLAllowedInIncognito().
// So, corresponding brave scheme url should be filtered as chrome scheme.
// Ex, brave://settings should be loaded only in normal window because
// chrome://settings is not allowed. When tyring to loading brave://settings in
// private window, it should be loaded in normal window instead of private
// window.
IN_PROC_BROWSER_TEST_F(BraveSchemeLoadBrowserTest,
                       SettingsPageIsNotAllowedInPrivateWindow) {
  TestURLIsNotLoadedInPrivateWindow(GURL("brave://settings/"));
}

IN_PROC_BROWSER_TEST_F(BraveSchemeLoadBrowserTest,
                       SyncPageIsNotAllowedInPrivateWindow) {
  TestURLIsNotLoadedInPrivateWindow(GURL("brave://sync/"));
}

IN_PROC_BROWSER_TEST_F(BraveSchemeLoadBrowserTest,
                       RewardsPageIsNotAllowedInPrivateWindow) {
  TestURLIsNotLoadedInPrivateWindow(GURL("brave://rewards/"));
}
