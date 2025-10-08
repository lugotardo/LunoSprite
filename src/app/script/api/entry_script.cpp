// LibreSprite
// Copyright (C) 2023  LibreSprite contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include "ui/entry.h"
#include "app/script/api/widget_script.h"
#include "app/script/app_scripting.h"

namespace script {
  void setStorage(const script::Value& value, const std::string& key, const std::string& domain);
}

class CustomEntry : public ui::Entry {
  std::string m_fileName;
public:

  CustomEntry(std::size_t maxsize, const std::string& fileName) : ui::Entry{maxsize, nullptr}, m_fileName(fileName) {}

  bool canRaiseEvent = true;

  void setTextSilent(const std::string& text) {
    canRaiseEvent = false;
    setText(text);
    script::setStorage(text, id(), m_fileName);
    canRaiseEvent = true;
  }

  void onChange() override {
    script::setStorage(text(), id(), m_fileName);
    if (canRaiseEvent)
      app::AppScripting::raiseEvent(m_fileName, {id() + "_change"});
  }
};

class EntryWidgetScriptObject : public WidgetScriptObject {
  unsigned int m_maxsize = 40;
public:
  EntryWidgetScriptObject() {
    addProperty("maxsize",
                [this]() -> unsigned int {return entry()->maxTextSize();},
                [this](unsigned int maxsize) {entry()->setMaxTextSize(maxsize); return maxsize;});
    addProperty("value",
                [this] {return entry()->text();},
                [this](const std::string& value) {entry()->setTextSilent(value); return value;});
  }

  CustomEntry* entry() {
    auto entry = getWidget<CustomEntry>();
    if (!entry)
      throw script::ObjectDestroyedException{};
    return entry;
  }

  DisplayType getDisplayType() override {return DisplayType::Block;}

  Handle build() override {
    return (new CustomEntry(m_maxsize, app::AppScripting::getFileName()))->handle();
  }
};

static script::ScriptObject::Regular<EntryWidgetScriptObject> _SO("EntryWidgetScriptObject", {
    "widget" + std::to_string(ui::kEntryWidget)
  });
