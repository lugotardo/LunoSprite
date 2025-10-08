// Aseprite
// Copyright (C) 2001-2016  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>

#include "base/bind.h"
#include "base/memory.h"
#include "ui/ui.h"

#include "app/app.h"
#include "app/console.h"
#include "app/context.h"
#include "app/modules/gui.h"
#include "app/ui/status_bar.h"

namespace app {

using namespace ui;

static Window* wid_console{};
static Widget* wid_view{};
static Widget* wid_textbox{};
static int console_counter = 0;
static bool console_locked;
static bool want_close_flag = false;

Console::Console(Context* ctx)
  : m_withUI(false)
{
  if (ctx)
    m_withUI = (ctx->isUIAvailable());
  else
    m_withUI =
      (App::instance()->isGui() &&
       Manager::getDefault() &&
       Manager::getDefault()->getDisplay());

  if (!m_withUI)
    return;

  console_counter++;
  if (wid_console || console_counter > 1)
    return;

  Window* window = new Window(Window::WithTitleBar, "Errors Console");
  Grid* grid = new Grid(1, false);
  View* view = new View();
  TextBox* textbox = new TextBox("", WORDWRAP);
  Button* close = new Button("&Close");
  Button* clear = new Button("C&lear");

  if (!grid || !textbox || !close)
    return;

  // The "close" closes the console
  close->Click.connect([=](Event&){window->closeWindow(close);}); // base::Bind<void>(&Window::closeWindow, window, close));
  clear->Click.connect([=](Event&){textbox->setText("");});

  view->attachToView(textbox);

  close->setMinSize(gfx::Size(60, 0));
  clear->setMinSize(gfx::Size(60, 0));

  grid->addChildInCell(view, 1, 1, HORIZONTAL | VERTICAL);
  auto row = new Grid(2, false);
  row->addChildInCell(clear, 1, 1, CENTER);
  row->addChildInCell(close, 1, 1, CENTER);
  grid->addChildInCell(row, 1, 1, CENTER);
  window->addChild(grid);

  view->setVisible(false);
  close->setFocusMagnet(true);
  view->setExpansive(true);

  wid_console = window;
  wid_view = view;
  wid_textbox = textbox;
  console_locked = false;
  want_close_flag = false;
}

Console::~Console()
{
  if (!m_withUI)
    return;

  console_counter--;

  if ((wid_console) && (console_counter == 0)) {
    if (console_locked
        && !want_close_flag
        && wid_console->isVisible()) {
      // Open in foreground
      wid_console->openWindowInForeground();
    }
    if (wid_console->manager())
      delete wid_console;         // window
    wid_console = NULL;
    want_close_flag = false;
  }
}

void Console::printf(const char* format, ...)
{
  char buf[4096];               // TODO warning buffer overflow
  va_list ap;

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  if (!m_withUI || !wid_console) {
    fputs(buf, stdout);
    fflush(stdout);
    return;
  }

  // Open the window
  if (!wid_console->isVisible()) {
    wid_console->openWindow();
    ui::Manager::getDefault()->invalidate();
  }

  /* update the textbox */
  if (!console_locked) {
    console_locked = true;

    wid_view->setVisible(true);

    wid_console->remapWindow();
    wid_console->setBounds(gfx::Rect(0, 0, ui::display_w()*9/10, ui::display_h()*6/10));
    wid_console->centerWindow();
    wid_console->invalidate();
  }

  wid_textbox->setText(wid_textbox->text() + buf);
}

// static
void Console::showException(const std::exception& e)
{
  Console console;
  if (typeid(e) == typeid(std::bad_alloc))
    console.printf("There is not enough memory to complete the action.");
  else
    console.printf("A problem has occurred.\n\nDetails:\n%s\n", e.what());
}

} // namespace app
