// Aseprite Document Library
// Copyright (c) 2001-2015 David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "doc/layer.h"

#include "doc/cel.h"
#include "doc/image.h"
#include "doc/primitives.h"
#include "doc/sprite.h"

#include <algorithm>
#include <cstring>

namespace doc {

Layer::Layer(ObjectType type, Sprite* sprite)
  : WithUserData(type)
  , m_sprite(sprite)
  , m_parent(NULL)
  , m_flags(LayerFlags(
      int(LayerFlags::Visible) |
      int(LayerFlags::Editable)))
{
  ASSERT(type == ObjectType::LayerImage || type == ObjectType::LayerFolder);

  setName("Layer");
}

Layer::~Layer()
{
}

int Layer::getMemSize() const
{
  return sizeof(Layer);
}

Layer* Layer::getPrevious() const
{
  if (m_parent != NULL) {
    LayerConstIterator it =
      std::find(m_parent->getLayerBegin(),
                m_parent->getLayerEnd(), this);

    if (it != m_parent->getLayerEnd() &&
        it != m_parent->getLayerBegin()) {
      it--;
      return *it;
    }
  }
  return NULL;
}

Layer* Layer::getNext() const
{
  if (m_parent != NULL) {
    LayerConstIterator it =
      std::find(m_parent->getLayerBegin(),
                m_parent->getLayerEnd(), this);

    if (it != m_parent->getLayerEnd()) {
      it++;
      if (it != m_parent->getLayerEnd())
        return *it;
    }
  }
  return NULL;
}

std::shared_ptr<Cel> Layer::cel(frame_t frame) const
{
  return NULL;
}

//////////////////////////////////////////////////////////////////////
// LayerImage class

LayerImage::LayerImage(Sprite* sprite)
  : Layer(ObjectType::LayerImage, sprite)
  , m_blendmode(BlendMode::NORMAL)
  , m_opacity(255)
{
}

LayerImage::~LayerImage()
{
  destroyAllCels();
}

int LayerImage::getMemSize() const
{
  int size = sizeof(LayerImage);
  CelConstIterator it = getCelBegin();
  CelConstIterator end = getCelEnd();

  for (; it != end; ++it) {
    auto cel = *it;
    size += cel->getMemSize();

    const Image* image = cel->image();
    size += image->getMemSize();
  }

  return size;
}

void LayerImage::destroyAllCels()
{
  m_cels.clear();
}

std::shared_ptr<Cel> LayerImage::cel(frame_t frame) const
{
  CelConstIterator it = findCelIterator(frame);
  if (it != getCelEnd())
    return *it;
  else
    return nullptr;
}

void LayerImage::getCels(CelList& cels) const
{
  CelConstIterator it = getCelBegin();
  CelConstIterator end = getCelEnd();

  for (; it != end; ++it)
    cels.push_back(*it);
}

std::shared_ptr<Cel> LayerImage::getLastCel() const
{
  if (!m_cels.empty())
    return m_cels.back();
  else
    return NULL;
}

CelConstIterator LayerImage::findCelIterator(frame_t frame) const
{
  CelIterator it = const_cast<LayerImage*>(this)->findCelIterator(frame);
  return CelConstIterator(it);
}

CelIterator LayerImage::findCelIterator(frame_t frame)
{
  auto first = getCelBegin();
  auto end = getCelEnd();

  // Here we use a binary search to find the first cel equal to "frame" (or after frame)
  first = std::lower_bound(
    first, end, nullptr,
    [frame](auto cel, auto) -> bool {
      return cel->frame() < frame;
    });

  // We return the iterator only if it's an exact match
  if (first != end && (*first)->frame() == frame)
    return first;
  else
    return end;
}

CelIterator LayerImage::findFirstCelIteratorAfter(frame_t firstAfterFrame)
{
  auto first = getCelBegin();
  auto end = getCelEnd();

  // Here we use a binary search to find the first cel after the given frame
  first = std::lower_bound(
    first, end, nullptr,
    [firstAfterFrame](auto cel, auto) -> bool {
      return cel->frame() <= firstAfterFrame;
    });

  return first;
}

void LayerImage::addCel(std::shared_ptr<Cel> cel)
{
  ASSERT(cel);
  ASSERT(cel->data() && "The cel doesn't contain CelData");
  ASSERT(cel->image());
  ASSERT(sprite());
  ASSERT(cel->image()->pixelFormat() == sprite()->pixelFormat());

  CelIterator it = findFirstCelIteratorAfter(cel->frame());
  m_cels.insert(it, cel);

  cel->setParentLayer(this);
}

/**
 * Removes the cel from the layer.
 */
void LayerImage::removeCel(std::shared_ptr<Cel> cel)
{
  ASSERT(cel);
  CelIterator it = findCelIterator(cel->frame());
  ASSERT(it != m_cels.end());

  m_cels.erase(it);

  cel->setParentLayer(NULL);
}

void LayerImage::moveCel(std::shared_ptr<Cel> cel, frame_t frame)
{
  removeCel(cel);
  cel->setFrame(frame);
  addCel(cel);
}

/**
 * Configures some properties of the specified layer to make it as the
 * "Background" of the sprite.
 *
 * You can't use this routine if the sprite already has a background
 * layer.
 */
void LayerImage::configureAsBackground()
{
  ASSERT(sprite() != NULL);
  ASSERT(sprite()->backgroundLayer() == NULL);

  switchFlags(LayerFlags::BackgroundLayerFlags, true);
  setName("Background");

  sprite()->folder()->stackLayer(this, NULL);
}

void LayerImage::displaceFrames(frame_t fromThis, frame_t delta)
{
  Sprite* sprite = this->sprite();

  if (delta > 0) {
    for (frame_t c=sprite->lastFrame(); c>=fromThis; --c) {
      if (auto cel = this->cel(c))
        moveCel(cel, c+delta);
    }
  }
  else {
    for (frame_t c=fromThis; c<=sprite->lastFrame(); ++c) {
      if (auto cel = this->cel(c))
        moveCel(cel, c+delta);
    }
  }
}

//////////////////////////////////////////////////////////////////////
// LayerFolder class

LayerFolder::LayerFolder(Sprite* sprite)
  : Layer(ObjectType::LayerFolder, sprite)
{
  setName("Layer Set");
}

LayerFolder::~LayerFolder()
{
  destroyAllLayers();
}

void LayerFolder::destroyAllLayers()
{
  LayerIterator it = getLayerBegin();
  LayerIterator end = getLayerEnd();

  for (; it != end; ++it) {
    Layer* layer = *it;
    delete layer;
  }
  m_layers.clear();
}

int LayerFolder::getMemSize() const
{
  int size = sizeof(LayerFolder);
  LayerConstIterator it = getLayerBegin();
  LayerConstIterator end = getLayerEnd();

  for (; it != end; ++it) {
    const Layer* layer = *it;
    size += layer->getMemSize();
  }

  return size;
}

void LayerFolder::getCels(CelList& cels) const
{
  LayerConstIterator it = getLayerBegin();
  LayerConstIterator end = getLayerEnd();

  for (; it != end; ++it)
    (*it)->getCels(cels);
}

void LayerFolder::addLayer(Layer* layer)
{
  m_layers.push_back(layer);
  layer->setParent(this);
}

void LayerFolder::removeLayer(Layer* layer)
{
  LayerIterator it = std::find(m_layers.begin(), m_layers.end(), layer);
  ASSERT(it != m_layers.end());
  m_layers.erase(it);

  layer->setParent(NULL);
}

void LayerFolder::stackLayer(Layer* layer, Layer* after)
{
  ASSERT(layer != after);
  if (layer == after)
    return;

  LayerIterator it = std::find(m_layers.begin(), m_layers.end(), layer);
  ASSERT(it != m_layers.end());
  m_layers.erase(it);

  if (after) {
    LayerIterator after_it = std::find(m_layers.begin(), m_layers.end(), after);
    ASSERT(after_it != m_layers.end());
    after_it++;
    m_layers.insert(after_it, layer);
  }
  else
    m_layers.insert(m_layers.begin(), layer);
}

void LayerFolder::displaceFrames(frame_t fromThis, frame_t delta)
{
  for (Layer* layer : m_layers)
    layer->displaceFrames(fromThis, delta);
}

} // namespace doc
