// Aseprite UI Library
// Copyright (C) 2001-2013, 2015, 2016  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ui/overlay_manager.h"

#include "she/display.h"
#include "she/surface.h"
#include "ui/manager.h"
#include "ui/overlay.h"

#include <algorithm>

namespace ui {

static bool less_than(Overlay* x, Overlay* y) {
  return *x < *y;
}

OverlayManager* OverlayManager::m_singleton = NULL;

OverlayManager* OverlayManager::instance()
{
  if (m_singleton == NULL)
    m_singleton = new OverlayManager;
  return m_singleton;
}

void OverlayManager::destroyInstance()
{
  delete m_singleton;
}

OverlayManager::OverlayManager()
{
}

OverlayManager::~OverlayManager()
{
}

void OverlayManager::addOverlay(Overlay* overlay)
{
  iterator it = std::lower_bound(begin(), end(), overlay, less_than);
  m_overlays.insert(it, overlay);
}

void OverlayManager::removeOverlay(Overlay* overlay)
{
  iterator it = std::find(begin(), end(), overlay);
  ASSERT(it != end());
  if (it != end())
    m_overlays.erase(it);
}

void OverlayManager::captureOverlappedAreas()
{
  Manager* manager = Manager::getDefault();
  if (!manager)
    return;

  she::Surface* displaySurface = manager->getDisplay()->getSurface();
  she::SurfaceLock lock(displaySurface);
  for (Overlay* overlay : *this)
    overlay->captureOverlappedArea(displaySurface);
}

void OverlayManager::restoreOverlappedAreas()
{
  Manager* manager = Manager::getDefault();
  if (!manager)
    return;

  she::Surface* displaySurface = manager->getDisplay()->getSurface();
  she::SurfaceLock lock(displaySurface);
  for (Overlay* overlay : *this)
    overlay->restoreOverlappedArea(displaySurface);
}

void OverlayManager::drawOverlays()
{
  Manager* manager = Manager::getDefault();
  if (!manager)
    return;

  she::Surface* displaySurface = manager->getDisplay()->getSurface();
  she::SurfaceLock lock(displaySurface);
  for (Overlay* overlay : *this)
    overlay->drawOverlay(displaySurface);
}

} // namespace ui
